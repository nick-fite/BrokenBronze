// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubeObject.h"

#include "Generators/MarchingCubes.h"

// Sets default values
AMarchingCubeObject::AMarchingCubeObject()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	StaticMesh = CreateDefaultSubobject<UStaticMesh>(TEXT("StaticMesh"));
	
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

}

void AMarchingCubeObject::MakeHole(const FVector& Center, float Radius)
{
	FVector localCenter = GetActorTransform().InverseTransformPosition(Center);
	FVector startPos = GetActorLocation() - FVector(Size/2 * VoxelSize, Size/2 * VoxelSize, 0);
	
	for (int X = 0; X <= Size; ++X)
	{
		for (int Y = 0; Y <= Size; ++Y)
		{
			for (int Z = 0; Z <= Size; ++Z)
			{
				FVector WorldPos = startPos + FVector(X, Y, Z) * VoxelSize;
				//FVector localPos = GetActorTransform().InverseTransformPosition(WorldPos);

				float dist = FVector::Dist(WorldPos, Center);
				if (dist < Radius * MeshScale)
				{
					UE_LOG(LogTemp, Display, TEXT("dist: %f"), dist);
					Voxels[GetVoxelIndex(X,Y,Z)] = -VoxelSize;
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
	int hits = 0;
	FVector RayDir = FVector(1.f, 0.3f, 0.5f).GetSafeNormal();
	FVector RayEnd = P + RayDir * 10000.f;
	for (int i = 0; i < OriginalIndices.Num(); i+=3)
	{
		const FVector& A = OriginalVertices[OriginalIndices[i]];
		const FVector& B = OriginalVertices[OriginalIndices[i + 1]];
		const FVector& C = OriginalVertices[OriginalIndices[i + 2]];

		FVector hitPoint;
		FVector normal;
		if (FMath::SegmentTriangleIntersection(P, RayEnd, A, B, C, hitPoint, normal))
		{
			++hits;
		}
	}
	return (hits % 2) == 1;
}

// Called when the game starts or when spawned
void AMarchingCubeObject::BeginPlay()
{
	Super::BeginPlay();

	FBox meshBox = StaticMesh->GetBoundingBox();
	FVector meshDimensions = meshBox.GetSize();
	const float SizeX = FMath::CeilToInt(meshDimensions.X / VoxelSize);
	const float SizeY = FMath::CeilToInt(meshDimensions.Y / VoxelSize);
	const float SizeZ = FMath::CeilToInt(meshDimensions.Z / VoxelSize);
	Size = FMath::Max(FMath::Max(SizeX, SizeY), SizeZ);
	
	Voxels.SetNum((Size + 1) * (Size + 1) * (Size + 1));
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
	
}

// Called every frame
void AMarchingCubeObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMarchingCubeObject::GenerateData(const FVector& Position)
{
	//FVector MeshCenter = StaticMesh->GetBoundingBox().GetCenter();
	//FVector offset = Position - MeshCenter;
    FVector startPos = Position - FVector(Size/2 * VoxelSize, Size/2 * VoxelSize, 0);
	
	for (int X = 0; X <= Size; ++X)
	{
		for (int Y = 0; Y <= Size; ++Y)
		{
			for (int Z = 0; Z <= Size; ++Z)
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
				if (X == Size/2 && Y == Size/2 && Z == Size/2) {
                    // Debug center point
                    UE_LOG(LogTemp, Warning, TEXT("Center point (%f,%f,%f) - isInside: %s"),
                        localPos.X, localPos.Y, localPos.Z, 
                        isInside ? TEXT("true") : TEXT("false"));
                }
				if (X == Size && Y == Size && Z == Size) {
                    // Debug center point
                    UE_LOG(LogTemp, Warning, TEXT("End point (%f,%f,%f) - isInside: %s"),
                        localPos.X, localPos.Y, localPos.Z, 
                        isInside ? TEXT("true") : TEXT("false"));
                }

				Voxels[GetVoxelIndex(X,Y,Z)] = isInside ? dist : -dist;
			}
		}
	} // Count how many voxels are inside
    int insideCount = 0;
    for (float val : Voxels) {
        if (val > 0.0f) insideCount++;
    }
    UE_LOG(LogTemp, Warning, TEXT("Inside voxels: %d of %d"), insideCount, Voxels.Num());
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
	for (int X = 0; X < Size; ++X)
	{
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int Z = 0; Z < Size; ++Z)
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
		FVector V1  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i]] * MeshScale;
		FVector V2  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 1]] * MeshScale;
		FVector V3  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 2]] * MeshScale;

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
	return  Z * (Size + 1) * (Size + 1) + Y * (Size + 1) + X;
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
