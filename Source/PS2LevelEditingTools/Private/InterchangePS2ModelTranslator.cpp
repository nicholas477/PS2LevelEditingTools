// Fill out your copyright notice in the Description page of Project Settings.


#include "InterchangePS2ModelTranslator.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Misc/FileHelper.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "PS2LevelEditingDeveloperSettings.h"

#include "meshoptimizer.h"

#include "egg/mesh_header.hpp"

UInterchangePS2ModelTranslator::UInterchangePS2ModelTranslator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshHeader = nullptr;
}

TArray<FString> UInterchangePS2ModelTranslator::GetSupportedFormats() const
{
	return UPS2LevelEditingDeveloperSettings::Get()->ModelFileFormat;
}

EInterchangeTranslatorAssetType UInterchangePS2ModelTranslator::GetSupportedAssetTypes() const
{
	return EInterchangeTranslatorAssetType::Meshes;
}

bool UInterchangePS2ModelTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	FString Filename = GetSourceData()->GetFilename();
	if (!FPaths::FileExists(Filename))
	{
		return false;
	}

	FFileHelper::LoadFileToArray(const_cast<ThisClass*>(this)->MeshFile, *Filename);

	const_cast<ThisClass*>(this)->MeshHeader = (MeshFileHeader*)MeshFile.GetData();

	const FString GroupName = Filename;
	const FString NodeUid = Filename;

	UInterchangeMeshNode* MeshNode = NewObject<UInterchangeMeshNode>(&BaseNodeContainer);
	MeshNode->InitializeNode(NodeUid, GroupName, EInterchangeNodeContainerType::TranslatedAsset);
	BaseNodeContainer.AddNode(MeshNode);

	MeshNode->SetPayLoadKey(GroupName, EInterchangeMeshPayLoadType::STATIC);

	MeshNode->SetCustomHasVertexNormal(true);
	MeshNode->SetCustomHasVertexBinormal(false);
	MeshNode->SetCustomHasVertexTangent(false);
	MeshNode->SetCustomHasSmoothGroup(false);
	MeshNode->SetCustomHasVertexColor(true);
	MeshNode->SetCustomUVCount(1);
	MeshNode->SetCustomPolygonCount(MeshHeader->pos.num_elements() - 2);
	MeshNode->SetCustomVertexCount(MeshHeader->pos.num_elements());

	return true;
}

static FVector3f PositionToUEBasis(const Vector& InVector)
{
	return FVector3f(InVector.x, InVector.z, InVector.y);
}

TFuture<TOptional<UE::Interchange::FMeshPayloadData>> UInterchangePS2ModelTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey, MeshGlobalTransform]
		{
			using namespace UE::Interchange;

			check(MeshHeader != nullptr);

			FMeshPayloadData Payload;

			TArray<uint32> OutputIndicies;
			{
				// Unstrippify
				TArray<uint32> InputIndicies;
				InputIndicies.SetNum(MeshHeader->pos.num_elements());
				for (size_t i = 0; i < MeshHeader->pos.num_elements(); ++i)
				{
					InputIndicies[i] = i;
				}
				OutputIndicies.SetNumZeroed(meshopt_unstripifyBound(MeshHeader->pos.num_elements()));
				OutputIndicies.SetNum(meshopt_unstripify(OutputIndicies.GetData(), InputIndicies.GetData(), InputIndicies.Num(), 0));

				// Remap indices
				//TArray<uint32> Remap;
				//Remap.SetNum(OutputIndicies.Num());

				//struct Vertex
				//{
				//	Vector pos;
				//	Vector nrm;
				//	Vector2 uvs;
				//	Vector colors;
				//};

				//TArray<Vertex> Vertices;
				//Vertices.SetNumZeroed(MeshHeader->pos.num_elements());
				//for (size_t i = 0; i < MeshHeader->pos.num_elements(); ++i)
				//{
				//	Vertex& NewVertex = Vertices[i];
				//	NewVertex.pos = MeshHeader->pos[i];
				//	NewVertex.nrm = MeshHeader->nrm[i];
				//	NewVertex.uvs = MeshHeader->uvs[i];
				//	NewVertex.colors = MeshHeader->colors[i];
				//}

				//size_t total_vertices = meshopt_generateVertexRemap(Remap.GetData(), OutputIndicies.GetData(), OutputIndicies.Num(), Vertices.GetData(), Vertices.Num(), sizeof(Vertex));

				//OutputIndicies.SetNum(total_indices);
				//meshopt_remapIndexBuffer(&result.indices[0], NULL, total_indices, &remap[0]);

				//result.vertices.resize(total_vertices);
				//meshopt_remapVertexBuffer(&result.vertices[0], &vertices[0], total_indices, sizeof(Vertex), &remap[0]);
			}



			FMeshDescription& MeshDescription = Payload.MeshDescription;
			FStaticMeshAttributes Attributes(MeshDescription);
			Attributes.Register();

			MeshDescription.SuspendVertexInstanceIndexing();
			MeshDescription.SuspendEdgeIndexing();
			MeshDescription.SuspendPolygonIndexing();
			MeshDescription.SuspendPolygonGroupIndexing();
			MeshDescription.SuspendUVIndexing();

			auto TransformPosition = [](const FMatrix& Matrix, FVector3f& Position)
			{
				const FVector TransformedPosition = Matrix.TransformPosition(FVector(Position));
				Position = static_cast<FVector3f>(TransformedPosition);
			};
			
			TVertexAttributesRef<FVector3f> MeshPositions = Attributes.GetVertexPositions();
			MeshDescription.ReserveNewVertices(MeshHeader->pos.num_elements());
			for (Vector pos : MeshHeader->pos)
			{
				FVertexID VertexIndex = MeshDescription.CreateVertex();
				if (MeshPositions.GetRawArray().IsValidIndex(VertexIndex))
				{
					FVector3f& Position = Attributes.GetVertexPositions()[VertexIndex];
					Position = PositionToUEBasis(pos);

					TransformPosition(MeshGlobalTransform.ToMatrixWithScale(), Position);
				}
			}

			// Create UVs and initialize values
			MeshDescription.SetNumUVChannels(1);
			MeshDescription.ReserveNewUVs(1);
			for (Vector2 uv : MeshHeader->uvs)
			{
				FUVID UVIndex = MeshDescription.CreateUV(0);
				Attributes.GetUVCoordinates(0)[UVIndex].X = uv.x;
				Attributes.GetUVCoordinates(0)[UVIndex].Y = uv.y;
			}

			FPolygonGroupID PolygonGroupIndex = MeshDescription.CreatePolygonGroup();
			const FString MaterialName = UPS2LevelEditingDeveloperSettings::Get()->ModelMaterial.GetAssetName();
			ensure(!MaterialName.IsEmpty());

			Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroupIndex] = FName(MaterialName);

			MeshDescription.ReserveNewTriangles(MeshHeader->pos.num_elements() - 2);
			MeshDescription.ReserveNewPolygons(MeshHeader->pos.num_elements() - 2);

			TArray<FVertexInstanceID, TInlineAllocator<3>> VertexInstanceIDs;
			for (int32 TriangleIndex = 0; TriangleIndex < (OutputIndicies.Num() / 3); ++TriangleIndex)
			{
				VertexInstanceIDs.Reset();
				MeshDescription.ReserveNewVertexInstances(3);

				for (int i = TriangleIndex * 3; i < ((TriangleIndex * 3) + 3); ++i)
				{
					FVertexID VertexID = OutputIndicies[i];
					FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
					VertexInstanceIDs.Add(VertexInstanceID);

					check(Attributes.GetVertexInstanceNormals().IsValid() && Attributes.GetVertexInstanceNormals().GetNumChannels() > 0);

					FVector3f& Normal = Attributes.GetVertexInstanceNormals()[VertexInstanceID];
					Normal = PositionToUEBasis(MeshHeader->nrm[VertexID]);
					Normal.Z *= -1.f;
					TransformPosition(MeshGlobalTransform.ToMatrixNoScale(), Normal);

					FVector4f& Color = Attributes.GetVertexInstanceColors()[VertexInstanceID];
					Color.X = FMath::Pow(MeshHeader->colors[VertexID].x, 2.2);
					Color.Y = FMath::Pow(MeshHeader->colors[VertexID].y, 2.2);
					Color.Z = FMath::Pow(MeshHeader->colors[VertexID].z, 2.2);
					Color.W = FMath::Pow(MeshHeader->colors[VertexID].w, 2.2);

					FVector2f& UVs = Attributes.GetVertexInstanceUVs()[VertexInstanceID];
					UVs.X = MeshHeader->uvs[VertexID].x;
					UVs.Y = MeshHeader->uvs[VertexID].y;
				}

				MeshDescription.CreatePolygon(PolygonGroupIndex, VertexInstanceIDs);
			}

			MeshDescription.ResumeEdgeIndexing();

			for (const FEdgeID& EdgeID : MeshDescription.Edges().GetElementIDs())
			{
				// Don't look at internal edges (edges within a polygon which break it into triangles)
				if (!MeshDescription.IsEdgeInternal(EdgeID))
				{
					Attributes.GetEdgeHardnesses()[EdgeID] = false;

					//bool bAllNormalsEqual = true;
					//TArray<FPolygonID, TInlineAllocator<2>> EdgePolygonIDs = MeshDescription.GetEdgeConnectedPolygons<TInlineAllocator<2>>(EdgeID);

					//for (const FVertexID& EdgeVertexID : MeshDescription.GetEdgeVertices(EdgeID))
					//{
					//	if (bAllNormalsEqual)
					//	{
					//		// This is the vertex index as it appears in the .obj
					//		int32 ObjVertexIndex = OutputIndicies[EdgeVertexID];

					//		// For the vertex we are considering, look at the first face adjacent to the edge, and find the vertex data which includes it
					//		// This is a baseline we will use to compare all the other adjacent faces
					//		const FVertexData& VertexData = Polygons[EdgePolygonIDs[0]]->GetVertexDataContainingVertexIndex(ObjVertexIndex);

					//		for (int32 Index = 1; Index < EdgePolygonIDs.Num(); Index++)
					//		{
					//			// For all other adjacent faces, find the vertex data which includes the vertex we are considering.
					//			// If the normal index is not the same as the baseline, we know this must be a hard edge.
					//			const FVertexData& VertexDataToCompare = Polygons[EdgePolygonIDs[Index]]->GetVertexDataContainingVertexIndex(ObjVertexIndex);

					//			if (VertexData.NormalIndex != VertexDataToCompare.NormalIndex)
					//			{
					//				bAllNormalsEqual = false;
					//				break;
					//			}
					//		}
					//	}
					//}

					//if (!bAllNormalsEqual)
					//{
					//	Attributes.GetEdgeHardnesses()[EdgeID] = false;
					//}

				}
			}

			MeshDescription.ResumeVertexInstanceIndexing();
			MeshDescription.ResumePolygonIndexing();
			MeshDescription.ResumePolygonGroupIndexing();
			MeshDescription.ResumeUVIndexing();

			return TOptional<FMeshPayloadData>(Payload);
		}
	);
}
