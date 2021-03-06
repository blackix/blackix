// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "PhysDerivedData.h"
#include "TargetPlatform.h"

#if WITH_PHYSX && WITH_EDITOR

FDerivedDataPhysXCooker::FDerivedDataPhysXCooker( FName InFormat, UBodySetup* InBodySetup )
	: BodySetup( InBodySetup )
	, CollisionDataProvider( NULL )
	, Format( InFormat )
	, Cooker( NULL )
{
	check( BodySetup != NULL );
	CollisionDataProvider = BodySetup->GetOuter();
	DataGuid = BodySetup->BodySetupGuid;
	bGenerateNormalMesh = BodySetup->bGenerateNonMirroredCollision;
	bGenerateMirroredMesh = BodySetup->bGenerateMirroredCollision;
	IInterface_CollisionDataProvider* CDP = InterfaceCast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	if (CDP)
	{
		CDP->GetMeshId(MeshId);
	}
	InitCooker();
}

// This constructor only used by ULandscapeMeshCollisionComponent, which always only build TriMesh, not Convex...
FDerivedDataPhysXCooker::FDerivedDataPhysXCooker( FName InFormat, ULandscapeMeshCollisionComponent* InMeshCollision, bool bMirrored )
	: BodySetup( NULL )
	, CollisionDataProvider( InMeshCollision )
	, Format( InFormat )
	, bGenerateNormalMesh( !bMirrored )
	, bGenerateMirroredMesh( bMirrored )
	, Cooker( NULL )
{
	check( InMeshCollision != NULL );
	DataGuid = InMeshCollision->MeshGuid;
	InitCooker();
}

void FDerivedDataPhysXCooker::InitCooker()
{
	// static here as an optimization
	static ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		Cooker = TPM->FindPhysXFormat( Format );
	}
}

bool FDerivedDataPhysXCooker::Build( TArray<uint8>& OutData )
{
	check(Cooker != NULL);

	FMemoryWriter Ar( OutData );
	uint8 bLittleEndian = PLATFORM_LITTLE_ENDIAN;
	int32 NumConvexElementsCooked = 0;
	int32 NumMirroredElementsCooked = 0;
	uint8 bTriMeshCooked = false;
	uint8 bMirroredTriMeshCooked = false;
	Ar << bLittleEndian;
	int64 CookedMeshInfoOffset = Ar.Tell();
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << bTriMeshCooked;
	Ar << bMirroredTriMeshCooked;

	// Cook convex meshes, but only if we are not forcing complex collision to be used as simple collision as well
	if( BodySetup && BodySetup->CollisionTraceFlag != CTF_UseComplexAsSimple && BodySetup->AggGeom.ConvexElems.Num() > 0 )
	{
		if( bGenerateNormalMesh )
		{
			NumConvexElementsCooked = BuildConvex( OutData, false );
		}
		if ( bGenerateMirroredMesh )
		{
			NumMirroredElementsCooked = BuildConvex( OutData, true );
		}
	}

	// Cook trimeshes, but only if we do not frce simple collision to be used as complex collision as well
	bool bUsingAllTriData = BodySetup != NULL ? BodySetup->bMeshCollideAll : false;
	if( (BodySetup == NULL || BodySetup->CollisionTraceFlag != CTF_UseSimpleAsComplex) && ShouldGenerateTriMeshData(bUsingAllTriData) )
	{
		if( bGenerateNormalMesh )
		{
			bTriMeshCooked = (uint8)BuildTriMesh( OutData, false, bUsingAllTriData );
		}
		if( bGenerateMirroredMesh && ShouldGenerateNegXTriMeshData() )
		{
			bMirroredTriMeshCooked = (uint8)BuildTriMesh( OutData, true, bUsingAllTriData );
		}
	}

	// Update info on what actually got cooked
	Ar.Seek( CookedMeshInfoOffset );
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << bTriMeshCooked;
	Ar << bMirroredTriMeshCooked;

	// Whatever got cached return true. We want to cache 'failure' too.
	return true;
}

int32 FDerivedDataPhysXCooker::BuildConvex( TArray<uint8>& OutData, bool InMirrored )
{	
	check( BodySetup != NULL );
	for( int32 ElementIndex = 0; ElementIndex < BodySetup->AggGeom.ConvexElems.Num(); ElementIndex++ )
	{
		const TArray<FVector>* MeshVertices = NULL;
		TArray<FVector> MirroredVerts;
		const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[ElementIndex];

		if( InMirrored )
		{
			MirroredVerts.AddUninitialized(ConvexElem.VertexData.Num());
			for(int32 VertIdx=0; VertIdx<ConvexElem.VertexData.Num(); VertIdx++)
			{
				MirroredVerts[VertIdx] = ConvexElem.VertexData[VertIdx] * FVector(-1,1,1);
			}
			MeshVertices = &MirroredVerts;
		}
		else
		{
			MeshVertices = &ConvexElem.VertexData;
		}

		// Store info on the cooking result (1 byte)
		int32 ResultInfoOffset = OutData.Add( false );

		// Cook and store Result at ResultInfoOffset
		UE_LOG(LogPhysics, Log, TEXT("Cook Convex: %s %d (FlipX:%d)"), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored);		
		bool Result = Cooker->CookConvex( Format, *MeshVertices, OutData );
		if( !Result )
		{
			UE_LOG(LogPhysics, Warning, TEXT("Failed to cook convex: %s %d (FlipX:%d). The remaining elements will not get cooked."), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored);
		}
		OutData[ ResultInfoOffset ] = Result;
	}

	return BodySetup->AggGeom.ConvexElems.Num();
}

bool FDerivedDataPhysXCooker::ShouldGenerateTriMeshData(bool InUseAllTriData)
{
	check(Cooker != NULL);

	IInterface_CollisionDataProvider* CDP = InterfaceCast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	const bool bPerformCook = ( CDP != NULL ) ? CDP->ContainsPhysicsTriMeshData(InUseAllTriData) : false;
	return bPerformCook;
}

bool FDerivedDataPhysXCooker::ShouldGenerateNegXTriMeshData()
{
	IInterface_CollisionDataProvider* CDP = InterfaceCast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	const bool bWantsNegX = ( CDP != NULL ) ? CDP->WantsNegXTriMesh() : false;
	return bWantsNegX;
}


bool FDerivedDataPhysXCooker::BuildTriMesh( TArray<uint8>& OutData, bool bInMirrored, bool InUseAllTriData )
{
	check(Cooker != NULL);

	bool bResult = false;
	FTriMeshCollisionData TriangleMeshDesc;
	IInterface_CollisionDataProvider* CDP = InterfaceCast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	check(CDP != NULL); // It's all been checked before getting into this function
		
	bool bHaveTriMeshData = CDP->GetPhysicsTriMeshData(&TriangleMeshDesc, InUseAllTriData);
	if(bHaveTriMeshData)
	{
		// If any of the below checks gets hit this usually means 
		// IInterface_CollisionDataProvider::ContainsPhysicsTriMeshData did not work properly.
		const int32 NumIndices = TriangleMeshDesc.Indices.Num();
		const int32 NumVerts = TriangleMeshDesc.Vertices.Num();
		if(NumIndices == 0 || NumVerts == 0 || TriangleMeshDesc.MaterialIndices.Num() > NumIndices)
		{
			UE_LOG(LogPhysics, Warning, TEXT("FDerivedDataPhysXCooker::BuildTriMesh: Triangle data from '%s' invalid (%d verts, %d indices)."), *CollisionDataProvider->GetPathName(), NumVerts, NumIndices );
			return bResult;
		}

		TArray<FVector>* MeshVertices = NULL;
		TArray<FVector> MirroredVerts;

		if( bInMirrored )
		{
			MirroredVerts.AddUninitialized(NumVerts);
			for(int32 VertIdx=0; VertIdx<NumVerts; VertIdx++)
			{
				MirroredVerts[VertIdx] = TriangleMeshDesc.Vertices[VertIdx] * FVector(-1,1,1);
			}
			MeshVertices = &MirroredVerts;
		}
		else
		{
			MeshVertices = &TriangleMeshDesc.Vertices;
		}

		UE_LOG(LogPhysics, Log, TEXT("Cook TriMesh: %s (FlipX: %d)"), *CollisionDataProvider->GetPathName(), bInMirrored);
		bResult = Cooker->CookTriMesh( Format, *MeshVertices, TriangleMeshDesc.Indices, TriangleMeshDesc.MaterialIndices, bInMirrored ? !TriangleMeshDesc.bFlipNormals : TriangleMeshDesc.bFlipNormals, OutData );
		if( !bResult )
		{
			UE_LOG(LogPhysics, Warning, TEXT("Failed to cook TriMesh: %s (FlipX:%d)."), *CollisionDataProvider->GetPathName(), bInMirrored );
		}
	}

	return bResult;
}

#endif	//WITH_PHYSX && WITH_EDITOR
