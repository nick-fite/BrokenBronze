// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubeObject.h"

// Sets default values
AMarchingCubeObject::AMarchingCubeObject()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

}

// Called when the game starts or when spawned
void AMarchingCubeObject::BeginPlay()
{
	Super::BeginPlay();

	Voxels.SetNum((Size + 1) * (Size + 1) * (Size + 1));
	Colors.SetNum((Size + 1) * (Size + 1) * (Size + 1));
	Vertices = TArray<FVector>();
	Triangles = TArray<int>();
	Normals = TArray<FVector>();
	UV0 = TArray<FVector2D>();

	UE_LOG(LogTemp, Warning, TEXT("Starting Data Generation"));
	GenerateData(GetActorLocation() / 100);
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

void AMarchingCubeObject::Setup()
{
}

void AMarchingCubeObject::GenerateData(const FVector& Position)
{
	for (int X = 0; X <= Size; X++)
	{
		for (int Y = 0; Y <= Size; Y++)
		{
			for (int Z = 0; Z < Size; Z++)
			{
				Voxels[GetVoxelIndex(X,Y,Z)] = 0.5;
			}
			UProceduralMeshComponent
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
	int VertexMask = 0;
	FVector EdgeVertex[12];
	for (int i = 0; i < 8; ++i)
	{
		if (Cube[i] <= SurfaceLevel)
				VertexMask |= (1 << i);
	}
	const int EdgeMask = CubeEdgeFlags[VertexMask];

	if (EdgeMask == 0 ) return;

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
		FVector V1  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i]] * 100;
		FVector V2  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 1]] * 100;
		FVector V3  = EdgeVertex[TriangleConnectionTable[VertexMask][3*i + 2]] * 100;

		FVector Normal = FVector::CrossProduct(V2 - V1, V3 - V1);

		FColor Color = FColor::MakeRandomColor();

		Normal.Normalize();

		Vertices.Append({V1, V2, V3});

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
	Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, Colors, TArray<FProcMeshTangent>(), true);
}
