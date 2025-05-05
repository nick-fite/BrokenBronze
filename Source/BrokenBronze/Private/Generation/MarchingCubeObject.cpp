// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubeObject.h"

#include "NavigationSystem.h"
#include "Serialization/BufferArchive.h"

// Sets default values
AMarchingCubeObject::AMarchingCubeObject()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComponent->SetupAttachment(RootComponent);
}

void AMarchingCubeObject::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
}

void AMarchingCubeObject::MakeHole(const FVector& Center, float Radius)
{
	for (int X = 0; X <= SizeX; ++X)
	{
		for (int Y = 0; Y <= SizeY; ++Y)
		{
			for (int Z = 0; Z <= SizeZ; ++Z)
			{
				//FVector WorldPos = FVector(X, Y, Z) * VoxelSize;
				//FVector localPos = GetActorTransform().InverseTransformPosition(WorldPos);
				FVector WorldPos = GetVoxelWorldPosition(X,Y,Z);
				
				float dist = FVector::Dist(WorldPos, Center);
				if (dist < Radius * VoxelSize)
				{
					UE_LOG(LogTemp, Display, TEXT("dist: %f"), dist);
                    int index = GetVoxelIndex(X,Y,Z);
                    float currentValue = Voxels[index];
                    Voxels[index] = FMath::Min(currentValue, -VoxelSize * 2);
					VoxelsHitStatus[index] = true;
				}
			}
		}
	}
	
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    Colors.Reset();
    UVs.Reset();
    VertexCount = 0;
    GenerateMesh();
    ApplyMesh();

	UpdateNavmesh();
}

float AMarchingCubeObject::ClosestTriangleDistance(const FVector& P)
{
	float minDist = FLT_MAX;
	for (int i = 0; i < OriginalIndices.Num(); i+=3)
	{
		const FVector& A = OriginalVertices[OriginalIndices[i]];
		const FVector& B = OriginalVertices[OriginalIndices[i + 1]];
		const FVector& C = OriginalVertices[OriginalIndices[i + 2]];

		FVector closest = FMath::ClosestPointOnTriangleToPoint(P, A, B, C);
		float Dist = FVector::Dist(P, closest);
		minDist = FMath::Min(minDist, Dist);
	}
	return minDist;
}

bool AMarchingCubeObject::IsInsideMesh(const FVector& P)
{
	 // Use more ray directions for better coverage of flat surfaces
    const TArray<FVector> RayDirections = {
        FVector(1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f),
        FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.0f, -1.0f, 0.0f),
        FVector(0.0f, 0.0f, -1.0f),
        FVector(1.0f, 1.0f, 1.0f).GetSafeNormal(),
        FVector(-1.0f, 1.0f, 1.0f).GetSafeNormal(),
        FVector(1.0f, -1.0f, 1.0f).GetSafeNormal(),
        FVector(1.0f, 1.0f, -1.0f).GetSafeNormal()
    };

    // Count the number of rays indicating the point is inside
    int insideCount = 0;
    int totalRays = RayDirections.Num();

    // Small epsilon to offset ray start position slightly
    const float Epsilon = 0.0001f;

    for (const FVector& RayDir : RayDirections)
    {
        // Slightly offset ray starting point to avoid numerical precision issues
        FVector RayStart = P + RayDir * Epsilon;
        FVector RayEnd = P + RayDir * 10000.0f;
        
        int hits = 0;
        bool hasHit = false;

        for (int i = 0; i < OriginalIndices.Num(); i += 3)
        {
            const FVector& A = OriginalVertices[OriginalIndices[i]];
            const FVector& B = OriginalVertices[OriginalIndices[i + 1]];
            const FVector& C = OriginalVertices[OriginalIndices[i + 2]];

            FVector hitPoint;
            FVector normal;
            
            if (FMath::SegmentTriangleIntersection(RayStart, RayEnd, A, B, C, hitPoint, normal))
            {
                hasHit = true;
                hits++;
            }
        }

        // If no hits detected with this ray, try the opposite direction
        if (!hasHit)
        {
            FVector oppositeRayEnd = P - RayDir * 10000.0f;
            
            for (int i = 0; i < OriginalIndices.Num(); i += 3)
            {
                const FVector& A = OriginalVertices[OriginalIndices[i]];
                const FVector& B = OriginalVertices[OriginalIndices[i + 1]];
                const FVector& C = OriginalVertices[OriginalIndices[i + 2]];

                FVector hitPoint;
                FVector normal;
                
                if (FMath::SegmentTriangleIntersection(RayStart, oppositeRayEnd, A, B, C, hitPoint, normal))
                {
                    hits++;
                }
            }
        }

        if ((hits % 2) == 1)
        {
            insideCount++;
        }
    }

    // Use weighted voting - require more than half the rays to agree
    return insideCount > (totalRays / 2);
}


FVector AMarchingCubeObject::GetVoxelWorldPosition(int X, int Y, int Z) const
{
	FVector localPos = FVector(X, Y, Z) * VoxelSize;
	return GetActorLocation() + GetActorRotation().RotateVector(localPos);
}


bool AMarchingCubeObject::SaveVoxelsToFile(const FString& Filename)
{
    // Create binary archive
    FBufferArchive BinaryData;
    
    // Write metadata
    int32 SavedSize = SizeX;
    BinaryData.Serialize(&SavedSize, sizeof(SavedSize));
    SavedSize = SizeY;
    BinaryData.Serialize(&SavedSize, sizeof(SavedSize));
    SavedSize = SizeZ;
    BinaryData.Serialize(&SavedSize, sizeof(SavedSize));
    
    float SavedVoxelSize = VoxelSize;
    BinaryData.Serialize(&SavedVoxelSize, sizeof(SavedVoxelSize));
    
    // Write array size
    int32 NumVoxels = Voxels.Num();
    BinaryData.Serialize(&NumVoxels, sizeof(NumVoxels));
    
    // Write voxel data
    if (NumVoxels > 0)
    {
        BinaryData.Serialize(Voxels.GetData(), NumVoxels * sizeof(float));
    }
    
    // Save to file
    FString FilePath = FPaths::ProjectSavedDir() + Filename;
    bool bSuccess = FFileHelper::SaveArrayToFile(BinaryData, *FilePath);
    
    // Clean up
    BinaryData.FlushCache();
    BinaryData.Empty();
    
    if (bSuccess)
    {
        UE_LOG(LogTemp, Display, TEXT("Saved %d voxels to %s"), NumVoxels, *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save to %s"), *FilePath);
    }
    
    return bSuccess;
}

bool AMarchingCubeObject::LoadVoxelsFromFile(const FString& Filename)
{
    FString FilePath = FPaths::ProjectSavedDir() + Filename;
    
    // Load file content
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
        return false;
    }
    
    // Create reader
    FMemoryReader Reader(FileData, true);
    
    // Read metadata
    int32 LoadedSizeX;
    Reader.Serialize(&LoadedSizeX, sizeof(LoadedSizeX));
    int32 LoadedSizeY;
    Reader.Serialize(&LoadedSizeY, sizeof(LoadedSizeY));
    int32 LoadedSizeZ;
    Reader.Serialize(&LoadedSizeZ, sizeof(LoadedSizeZ));
    
    float LoadedVoxelSize;
    Reader.Serialize(&LoadedVoxelSize, sizeof(LoadedVoxelSize));
    
    // Read array size
    int32 NumVoxels;
    Reader.Serialize(&NumVoxels, sizeof(NumVoxels));
    
    if (NumVoxels <= 0 || NumVoxels > 10000000) // Sanity check
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid voxel count: %d"), NumVoxels);
        return false;
    }
    
    // Read voxel data
    TArray<float> LoadedVoxels;
    LoadedVoxels.SetNumUninitialized(NumVoxels);
    Reader.Serialize(LoadedVoxels.GetData(), NumVoxels * sizeof(float));
    
    // Update object state
    //Size = LoadedSize;
	SizeX = LoadedSizeX;
	SizeY = LoadedSizeY;
	SizeZ = LoadedSizeZ;
    VoxelSize = LoadedVoxelSize;
    Voxels = MoveTemp(LoadedVoxels);
    
    // Reset hit status
    VoxelsHitStatus.Init(false, NumVoxels);
    
    // Rebuild mesh
    GenerateMesh();
    ApplyMesh();
    
    UE_LOG(LogTemp, Display, TEXT("Loaded %d voxels from %s"), NumVoxels, *FilePath);
    return true;
}

//Blake added function :)
void AMarchingCubeObject::UpdateNavmesh()
{
	if (NavigationSystem)
	{
		UE_LOG(LogTemp, Display, TEXT("UPDATING NAVMESH"));
		NavigationSystem->UpdateComponentInNavOctree(*Mesh);
		NavigationSystem->AddDirtyArea(Mesh->Bounds.GetBox(), ENavigationDirtyFlag::All);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("NAVMESH INVALID!"));
}


// Called when the game starts or when spawned
void AMarchingCubeObject::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;

	if(StaticMesh)
	{
		FBox meshBox = StaticMesh->GetBoundingBox();
		FVector meshDimensions = meshBox.GetSize();
		const float NewSizeX = FMath::CeilToInt(meshDimensions.X / VoxelSize) * 4;
		const float NewSizeY = FMath::CeilToInt(meshDimensions.Y / VoxelSize) * 2;
		const float NewSizeZ = FMath::CeilToInt(meshDimensions.Z / VoxelSize) * 4;
		//Size = FMath::Max(FMath::Max(SizeX, SizeY), SizeZ);
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		SizeZ = NewSizeZ;
		
		Voxels.SetNum((SizeX + 1) * (SizeY + 1) * (SizeZ + 1));
		VoxelsHitStatus.SetNum((SizeX + 1) * (SizeY + 1) * (SizeZ + 1));
		//Colors.SetNum((Size + 1) * (Size + 1) * (Size + 1));
		//Voxels = TArray<float>();
		Colors = TArray<FColor>();
		Vertices = TArray<FVector>();
		Triangles = TArray<int>();
		Normals = TArray<FVector>();
		UVs = TArray<FVector2D>();
	
		OriginalColors = TArray<FColor>();
		OriginalVertices = TArray<FVector>();
		OriginalTriangles = TArray<int>();
		OriginalNormals = TArray<FVector>();
		OriginalUVs = TArray<FVector2D>();
		OriginalIndices = TArray<int>();
	}

	if (ShouldLoad)
	{
		LoadVoxelsFromFile(VoxelDataFilename);
		UpdateNavmesh();
		return;
	}

	if (StaticMesh)
	{
		UE_LOG(LogTemp, Display, TEXT("Mesh is initialized"));
		//UStaticMesh* tempStatic = StaticMesh->GetStaticMesh();
		FStaticMeshRenderData* renderData = StaticMesh->GetRenderData();
		if (renderData)
		{
			UE_LOG(LogTemp, Warning, TEXT("Getting Data"));
			const FStaticMeshLODResources& LODResources = renderData->LODResources[0];
			const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
			const FStaticMeshVertexBuffer& VertexBuffer = LODResources.VertexBuffers.StaticMeshVertexBuffer;
			const FIndexArrayView& Indices = LODResources.IndexBuffer.GetArrayView();
			const FRawStaticIndexBuffer& IndicesBuffer = LODResources.IndexBuffer;
			//IndicesBuffer.InitPreRHIResources();
			

			for (uint32 i = 0; i < PositionBuffer.GetNumVertices(); i++)
			{
				FVector3f temp = PositionBuffer.VertexPosition(i);
				OriginalVertices.Add(FVector(temp.X, temp.Y, temp.Z));
			}
			
			for (uint32 i = 0; i < VertexBuffer.GetNumVertices(); i++)
			{
				FVector3f temp = VertexBuffer.VertexTangentZ(i);
				OriginalNormals.Add(FVector(temp.X, temp.Y, temp.Z));
			}
		
			for (uint32 i = 0; i < VertexBuffer.GetNumVertices(); i++)
			{
				FVector2f temp = VertexBuffer.GetVertexUV(i, 0) ;
				OriginalUVs.Add(FVector2D(temp.X, temp.Y));
			}
			
			for (int32 i = 0; i < Indices.Num(); i+=3)
			{
			    if (i+2 < Indices.Num())
			    {
			        OriginalIndices.Add(IndicesBuffer.GetIndex(i));
			        OriginalIndices.Add(IndicesBuffer.GetIndex(i+1));
			        OriginalIndices.Add(IndicesBuffer.GetIndex(i+2));
			    }
			}

			//Vertices = OriginalVertices;
			//Normals = OriginalNormals;
			//Triangles = OriginalTriangles;
			//Normals = OriginalNormals;
			//UVs = OriginalUVs;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not get Data Data"));
		}
	}
	else
	{
			UE_LOG(LogTemp, Warning, TEXT("mesh not initialized"));
	}
	

	//UE_LOG(LogTemp, Warning, TEXT("Starting Data Generation"));

	GenerateData(GetActorLocation());
	UE_LOG(LogTemp, Warning, TEXT("Generate Data Done"));
	GenerateMesh();
	UE_LOG(LogTemp, Warning, TEXT("Generate Mesh Done"));
	ApplyMesh();
	UE_LOG(LogTemp, Warning, TEXT("Apply Mesh Done"));
	if (ShouldSave)
	{
		SaveVoxelsToFile(VoxelDataFilename);
	}

	UpdateNavmesh();
}

// Called every frame
void AMarchingCubeObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMarchingCubeObject::GenerateData(const FVector& Position)
{
	UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
	if(!StaticMesh)
	{
		return;
	}
	FVector MeshCenter = StaticMesh->GetBoundingBox().GetCenter();
	FVector offset = Position - MeshCenter;
    FVector startPos = Position - FVector(SizeX/4 * VoxelSize, SizeY/2 * VoxelSize,0);
    const float SurfaceProximityThreshold = VoxelSize * 0.1f;
	
	for (int X = 0; X <= SizeX; ++X)
	{
		for (int Y = 0; Y <= SizeY; ++Y)
		{
			for (int Z = 0; Z <= SizeZ; ++Z)
			{
				FVector pos = startPos + FVector(X, Y, Z) * VoxelSize;
				FVector localPos = GetActorTransform().InverseTransformPosition(pos);
				
				float dist = ClosestTriangleDistance(localPos);
				bool isInside = IsInsideMesh(localPos);

				if (X == 0 && Y == 0 && Z == 0) {
                    // Debug center point
                    UE_LOG(LogTemp, Warning, TEXT("Start point (%f,%f,%f) - isInside: %s"),
                        localPos.X, localPos.Y, localPos.Z, 
                        isInside ? TEXT("true") : TEXT("false"));
                }
				if (X == SizeX/2 && Y == SizeY/2 && Z == SizeZ/2) {
                    // Debug center point
                    UE_LOG(LogTemp, Warning, TEXT("Center point (%f,%f,%f) - isInside: %s"),
                        localPos.X, localPos.Y, localPos.Z, 
                        isInside ? TEXT("true") : TEXT("false"));
                }
				if (X == SizeX && Y == SizeY && Z == SizeZ) {
                    // Debug center point
                    UE_LOG(LogTemp, Warning, TEXT("End point (%f,%f,%f) - isInside: %s"),
                        localPos.X, localPos.Y, localPos.Z, 
                        isInside ? TEXT("true") : TEXT("false"));
                }

				Voxels[GetVoxelIndex(X,Y,Z)] = isInside ? dist : -dist;
				VoxelsHitStatus[GetVoxelIndex(X,Y,Z)] = false;
			}
		}
	}
}

void AMarchingCubeObject::GenerateMesh()
{
	if (SurfaceLevel > 0.0f)
	{
		TriangleOrder[0] = 0;
		TriangleOrder[1] = 1;
		TriangleOrder[2] = 2;
	}
	else
	{
		TriangleOrder[0] = 2;
		TriangleOrder[1] = 1;
		TriangleOrder[2] = 0;
	}

	float Cube[8];
	for (int X = 0; X < SizeX; ++X)
	{
		for (int Y = 0; Y < SizeY; ++Y)
		{
			for (int Z = 0; Z < SizeZ; ++Z)
			{
				for (int i = 0; i < 8; ++i)
				{
					Cube[i] = Voxels[GetVoxelIndex(X + VertexOffset[i][0], Y + VertexOffset[i][1], Z + VertexOffset[i][2])];
				}
				March(X,Y,Z,Cube);
			}
		}
	}
}

void AMarchingCubeObject::March(int X, int Y, int Z, const float Cube[8])
{
	UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
	if(!StaticMesh)
	{
		return;
	}
	FBox boundingBox = StaticMesh->GetBoundingBox();
	FVector meshMin = boundingBox.Min;
	FVector meshMax = boundingBox.Max;
	const float meshWidth = meshMax.X - meshMin.X;
	const float meshHeight = meshMax.Y - meshMin.Y;
	
	int VertexMask = 0;
	FVector EdgeVertex[12];
	for (int i = 0; i < 8; ++i)
	{
		if (Cube[i] <= SurfaceLevel)
				VertexMask |= (1 << i);
	}
	const int EdgeMask = CubeEdgeFlags[VertexMask];

	if (EdgeMask == 0 ) return;
	
	//Makes cubes showing alll the marching cubes in the object. Debugging.
    /*if (EdgeMask != 0)
    {
        FVector CubeMin = FVector(X, Y, Z) * VoxelSize;
        FVector CubeMax = CubeMin + FVector(VoxelSize, VoxelSize, VoxelSize);
        FColor BoxColor = FColor::Yellow;
        FColor BoxHitColor = FColor::Red;
        
        // Draw debug box with actor transform applied
        DrawDebugBox(
            GetWorld(),
            GetActorLocation() + GetActorRotation().RotateVector((CubeMin + CubeMax) * 0.5f),
            (CubeMax - CubeMin) * 0.5f,
            GetActorRotation().Quaternion(),
            VoxelsHitStatus[GetVoxelIndex(X,Y,Z)] ? BoxHitColor : BoxColor,
            false,
            10000000.0f,  // Lifetime (0 = one frame)
            0,     // DepthPriority
            1.0f   // Thickness
        );
    }*/

	//UE_LOG(LogTemp, Warning, TEXT("Making Edges"))
	for (int i = 0; i < 12; ++i)
	{
		if ((EdgeMask & (1 << i)) != 0)
		{
			const float Offset = Interpolation ? GetInterpolationOffset(Cube[EdgeConnection[i][0]], Cube[EdgeConnection[i][1]]) : 0.5f;

			EdgeVertex[i].X = X + (VertexOffset[EdgeConnection[i][0]][0] + Offset * EdgeDirection[i][0]);
			EdgeVertex[i].Y = Y + (VertexOffset[EdgeConnection[i][0]][1] + Offset * EdgeDirection[i][1]);
			EdgeVertex[i].Z = Z + (VertexOffset[EdgeConnection[i][0]][2] + Offset * EdgeDirection[i][2]);
		}
	}

	for (int i = 0; i < 5; ++i)
	{
		if (TriangleConnectionTable[VertexMask][3*i] < 0) break;
		FVector V1  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i]] * VoxelSize;
		FVector V2  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 1]] * VoxelSize;
		FVector V3  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 2]] * VoxelSize;

		FVector Normal = FVector::CrossProduct(V2 - V1, V3 - V1);

		FColor Color = FColor::MakeRandomColor();

		Normal.Normalize();

		Vertices.Append({V1, V2, V3});

		FVector2D UV1 = FVector2D(V1.X / meshWidth, V1.Y / meshHeight);
        FVector2D UV2 = FVector2D(V2.X / meshWidth, V2.Y / meshHeight);
        FVector2D UV3 = FVector2D(V3.X / meshWidth, V3.Y / meshHeight);
		
        UVs.Append({ UV1, UV2, UV3 });
		
		Triangles.Append({
			VertexCount + TriangleOrder[0],
			VertexCount + TriangleOrder[1],
			VertexCount + TriangleOrder[2]});

		Normals.Append({Normal, Normal, Normal});
		
		Colors.Append({Color, Color, Color});
		VertexCount += 3;		
	}
}

int AMarchingCubeObject::GetVoxelIndex(int X, int Y, int Z) const
{
    return Z * (SizeX + 1) * (SizeY + 1) + Y * (SizeX + 1) + X;
}

float AMarchingCubeObject::GetInterpolationOffset(float V1, float V2) const
{
	const float Delta = V2 - V1;
	return Delta == 0.0f ? SurfaceLevel : (SurfaceLevel - V1) / Delta;
}

void AMarchingCubeObject::ApplyMesh()
{
	UE_LOG(LogTemp, Warning, TEXT("%d"), Vertices.Num());
	UE_LOG(LogTemp, Warning, TEXT("%d"), Triangles.Num());
	UE_LOG(LogTemp, Warning, TEXT("%d"), Normals.Num());
	UE_LOG(LogTemp, Warning, TEXT("%d"), Colors.Num());
	Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
}

