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

			//TArray<FVector3f> Positions;
			//TArray<FVector2f> UVs;
			//TArray<FVector3f> Normals;
			//TArray<FVector4f> Colors;

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
			
			{
				UInterchangeResultDisplay_Generic* Result = AddMessage<UInterchangeResultDisplay_Generic>();
				Result->SourceAssetName = FPaths::GetBaseFilename(GetSourceData()->GetFilename());
				Result->Text = NSLOCTEXT("InterchangePS2Translator", "ImportingVertices", "Importing vertices...");
			}

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

			{
				UInterchangeResultDisplay_Generic* Result = AddMessage<UInterchangeResultDisplay_Generic>();
				Result->SourceAssetName = FPaths::GetBaseFilename(GetSourceData()->GetFilename());
				Result->Text = NSLOCTEXT("InterchangePS2Translator", "ImportingUVs", "Importing UVs...");
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
			const FString MaterialName = UMaterial::GetDefaultMaterial(MD_Surface)->GetName();
			ensure(!MaterialName.IsEmpty());

			Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroupIndex] = FName(MaterialName);

			MeshDescription.ReserveNewTriangles(MeshHeader->pos.num_elements() - 2);
			MeshDescription.ReserveNewPolygons(MeshHeader->pos.num_elements() - 2);

			TArray<FVertexInstanceID, TInlineAllocator<3>> VertexInstanceIDs;
			for (int32 i = 2; i < MeshHeader->pos.num_elements(); ++i)
			{
				VertexInstanceIDs.Reset();
				MeshDescription.ReserveNewVertexInstances(3);

				for (int32 j = i; j >= (i - 2); --j)
				{
					FVertexID VertexID = j;
					FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
					VertexInstanceIDs.Add(VertexInstanceID);

					//if (VertexData.NormalIndex != INDEX_NONE && Normals.IsValidIndex(VertexData.NormalIndex))
					//{
					//	FVector3f& Normal = Attributes.GetVertexInstanceNormals()[VertexInstanceID];
					//	Normal = PositionToUEBasis(Normals[VertexData.NormalIndex]);
					//	TransformPosition(TotalMatrixForNormal, Normal);
					//}

					FVector3f& Normal = Attributes.GetVertexInstanceNormals()[VertexInstanceID];
					Normal = PositionToUEBasis(MeshHeader->nrm[VertexID]);
					TransformPosition(MeshGlobalTransform.ToMatrixNoScale(), Normal);
						
					//if (VertexData.UVIndex != INDEX_NONE && UVs.IsValidIndex(VertexData.UVIndex))
					//{
					//	Attributes.GetVertexInstanceUVs()[VertexInstanceID] = UVToUEBasis(UVs[VertexData.UVIndex]);
					//}
				}

				// Reverse every other triangle in the vertex list to flip mapping
				if (i % 2 == 1)
				{
					Algo::Reverse(VertexInstanceIDs);
				}

				MeshDescription.CreatePolygon(PolygonGroupIndex, VertexInstanceIDs);
			}

			MeshDescription.ResumeVertexInstanceIndexing();
			MeshDescription.ResumePolygonIndexing();
			MeshDescription.ResumePolygonGroupIndexing();
			MeshDescription.ResumeUVIndexing();

			return TOptional<FMeshPayloadData>(Payload);
		}
	);
}
