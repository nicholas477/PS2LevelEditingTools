// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS2LevelEditingTools.h"
#include "PS2LevelEditingDeveloperSettings.h"
#include "EditorFramework/AssetImportData.h"

#include "FoliageInstancedStaticMeshComponent.h"

#include "egg/math_types.hpp"
#include "egg/level.hpp"
#include "egg/asset.hpp"

static void SwitchTransform(FTransform& Transform, EAxis::Type FrontAxis, EAxis::Type RightAxis, EAxis::Type UpAxis)
{
	FVector Location = Transform.GetLocation();
	FVector NewLocation;
	NewLocation[0] = Location.GetComponentForAxis(FrontAxis);
	NewLocation[1] = Location.GetComponentForAxis(RightAxis);
	NewLocation[2] = Location.GetComponentForAxis(UpAxis);

	FVector Scale = Transform.GetScale3D();
	FVector NewScale;
	NewScale[0] = Scale.GetComponentForAxis(FrontAxis);
	NewScale[1] = Scale.GetComponentForAxis(RightAxis);
	NewScale[2] = Scale.GetComponentForAxis(UpAxis);

	FQuat Orientation = Transform.GetRotation();
	FQuat NewOrientation;
	switch (FrontAxis)
	{
	case EAxis::X:		NewOrientation.X = Orientation.X;	break;
	//case EAxis::XNeg:	NewOrientation.X = -Orientation.X;	break;
	case EAxis::Y:		NewOrientation.X = Orientation.Y;	break;
	//case EAxis::YNeg:	NewOrientation.X = -Orientation.Y;	break;
	case EAxis::Z:		NewOrientation.X = Orientation.Z;	break;
	//case EAxis::ZNeg:	NewOrientation.X = -Orientation.Z;	break;
	}
	switch (RightAxis)
	{
	case EAxis::X:		NewOrientation.Y = Orientation.X;	break;
	//case EAxis::XNeg:	NewOrientation.Y = -Orientation.X;	break;
	case EAxis::Y:		NewOrientation.Y = Orientation.Y;	break;
	//case EAxis::YNeg:	NewOrientation.Y = -Orientation.Y;	break;
	case EAxis::Z:		NewOrientation.Y = Orientation.Z;	break;
	//case EAxis::ZNeg:	NewOrientation.Y = -Orientation.Z;	break;
	}
	switch (UpAxis)
	{
	case EAxis::X:		NewOrientation.Z = Orientation.X;	break;
	//case EAxis::XNeg:	NewOrientation.Z = -Orientation.X;	break;
	case EAxis::Y:		NewOrientation.Z = Orientation.Y;	break;
	//case EAxis::YNeg:	NewOrientation.Z = -Orientation.Y;	break;
	case EAxis::Z:		NewOrientation.Z = Orientation.Z;	break;
	//case EAxis::ZNeg:	NewOrientation.Z = -Orientation.Z;	break;
	}

	// Calculate the Levi-Civita symbol for W scaling according to the following table
	// +1 xyz yzx zxy
	// -1 xzy yxz zyx
	// 0 if any axes are duplicated (x == y, y == z, or z == x)
	float LCSymbol = 0.0f;
	EAxis::Type FrontAxisAbs = FrontAxis;
	EAxis::Type RightAxisAbs = RightAxis;
	EAxis::Type UpAxisAbs = UpAxis;
	if (((FrontAxisAbs == EAxis::X) && (RightAxisAbs == EAxis::Y) && (UpAxisAbs == EAxis::Z)) ||
		((FrontAxisAbs == EAxis::Y) && (RightAxisAbs == EAxis::Z) && (UpAxisAbs == EAxis::X)) ||
		((FrontAxisAbs == EAxis::Z) && (RightAxisAbs == EAxis::X) && (UpAxisAbs == EAxis::Y)))
	{
		LCSymbol = 1.0f;
	}
	else if (((FrontAxisAbs == EAxis::X) && (RightAxisAbs == EAxis::Z) && (UpAxisAbs == EAxis::Y)) ||
		((FrontAxisAbs == EAxis::Y) && (RightAxisAbs == EAxis::X) && (UpAxisAbs == EAxis::Z)) ||
		((FrontAxisAbs == EAxis::Z) && (RightAxisAbs == EAxis::Y) && (UpAxisAbs == EAxis::X)))
	{
		LCSymbol = -1.0f;
	}

	// Make sure the handedness is corrected to match the remapping
	const float FlipSign = 1.f;

	NewOrientation.W = Orientation.W * LCSymbol * FlipSign;

	Transform.SetComponents(NewOrientation.GetNormalized(), NewLocation, NewScale);
}

static Asset::Reference LookupAssetReference(const FString& Path)
{
	Filesystem::Path p = TCHAR_TO_ANSI(*Path);
	return Asset::Reference(p);
}

static bool AssetExists(Asset::Reference Ref)
{
	for (Asset::Reference TableKey : Asset::get_asset_table().keys)
	{
		if (TableKey == Ref)
		{
			return true;
		}
	}
	return false;
}

static void WriteoutLevel(const TArray<UE::Math::TMatrix<float>>& MeshTransforms, const TArray<Asset::Reference>& MeshFileReferences)
{
	std::vector<std::byte> out;

	LevelFileHeader NewLevel;
	NewLevel.meshes.mesh_files.set((intptr_t)MeshFileReferences.GetData(), MeshFileReferences.Num() * sizeof(Asset::Reference));
	NewLevel.meshes.mesh_transforms.set((intptr_t)MeshTransforms.GetData(), MeshTransforms.Num() * sizeof(Matrix));

	Serializer s(out);
	serialize(s, NewLevel, 1);
	s.finish_serialization();

	TArrayView<uint8> OutView((uint8*)out.data(), out.size());

	FFilePath AssetManifestPath = UPS2LevelEditingDeveloperSettings::Get()->ManifestPath;

	FString AssetManifestDirectory(FPaths::GetPath(AssetManifestPath.FilePath));

	FFileHelper::SaveArrayToFile(OutView, *(AssetManifestDirectory / "assets" / "new_level.lvl"));
}

static void CollectStaticMeshes(AActor* Actor, TArray<UE::Math::TMatrix<float>>& MeshTransforms, TArray<Asset::Reference>& MeshFileReferences)
{
	TArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		FTransform Transform = Mesh->GetComponentTransform();
		SwitchTransform(Transform, EAxis::X, EAxis::Z, EAxis::Y);
		if (Mesh->GetStaticMesh())
		{
			UAssetImportData* MeshImportData = Mesh->GetStaticMesh()->GetAssetImportData();
			if (MeshImportData->GetSourceData().SourceFiles.Num() > 0)
			{
				FAssetImportInfo::FSourceFile SourceFile = MeshImportData->GetSourceData().SourceFiles[0];

				FFilePath AssetManifestPath = UPS2LevelEditingDeveloperSettings::Get()->ManifestPath;
				FString AssetFilePath{ SourceFile.RelativeFilename };

				FDirectoryPath AssetManifestDirectory{ FPaths::GetPath(AssetManifestPath.FilePath) };

				if (FPaths::IsUnderDirectory(AssetFilePath, AssetManifestDirectory.Path))
				{
					FPaths::MakePathRelativeTo(AssetFilePath, *(AssetManifestDirectory.Path + "/"));

					UE_LOG(LogPS2LevelEditingTools, Log, TEXT("Asset path: %s"), *AssetFilePath);
					Asset::Reference AssetReference = LookupAssetReference(AssetFilePath);
					ensure(AssetExists(AssetReference));

					if (UInstancedStaticMeshComponent* InstancedMesh = Cast<UInstancedStaticMeshComponent>(Mesh))
					{
						int32 InstanceID = 0;
						for (const FInstancedStaticMeshInstanceData& InstanceData : InstancedMesh->PerInstanceSMData)
						{
							FTransform InstanceTransform;
							InstancedMesh->GetInstanceTransform(InstanceID, InstanceTransform, true);
							SwitchTransform(InstanceTransform, EAxis::X, EAxis::Z, EAxis::Y);

							UE::Math::TMatrix<double> UnrealMeshMatDouble = InstanceTransform.ToMatrixWithScale();

							UE::Math::TMatrix<float>& MeshMatrix = MeshTransforms.AddDefaulted_GetRef();
							for (int i = 0; i < 4; ++i)
							{
								for (int j = 0; j < 4; ++j)
								{
									MeshMatrix.M[i][j] = UnrealMeshMatDouble.M[i][j];
								}
							}
							MeshFileReferences.Add(AssetReference);

							InstanceID++;
						}
					}
					else
					{
						UE::Math::TMatrix<float>& MeshMatrix = MeshTransforms.AddDefaulted_GetRef();
						UE::Math::TMatrix<double> UnrealMeshMatDouble = Transform.ToMatrixWithScale();
						for (int i = 0; i < 4; ++i)
						{
							for (int j = 0; j < 4; ++j)
							{
								MeshMatrix.M[i][j] = UnrealMeshMatDouble.M[i][j];
							}
						}
						MeshFileReferences.Add(AssetReference);
					}
				}
			}
		}
	}
}

static void CollectFoliageMeshes(AActor* Actor, TArray<UE::Math::TMatrix<float>>& MeshTransforms, TArray<Asset::Reference>& MeshFileReferences)
{
	TArray<UInstancedStaticMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);

	for (UInstancedStaticMeshComponent* Mesh : Meshes)
	{
		//FTransform Transform = Mesh->GetComponentTransform();
		//SwitchTransform(Transform, EAxis::X, EAxis::Z, EAxis::Y, false, FVector::Zero(), false, FRotator::ZeroRotator);

		//Mesh->PerInstanceSMData

		//if (Mesh->GetStaticMesh())
		//{
		//	UAssetImportData* MeshImportData = Mesh->GetStaticMesh()->GetAssetImportData();
		//	if (MeshImportData->GetSourceData().SourceFiles.Num() > 0)
		//	{
		//		FAssetImportInfo::FSourceFile SourceFile = MeshImportData->GetSourceData().SourceFiles[0];

		//		FFilePath AssetManifestPath = UPS2LevelEditingDeveloperSettings::Get()->ManifestPath;
		//		FString AssetFilePath{ SourceFile.RelativeFilename };

		//		FDirectoryPath AssetManifestDirectory{ FPaths::GetPath(AssetManifestPath.FilePath) };

		//		if (FPaths::IsUnderDirectory(AssetFilePath, AssetManifestDirectory.Path))
		//		{
		//			FPaths::MakePathRelativeTo(AssetFilePath, *(AssetManifestDirectory.Path + "/"));

		//			UE_LOG(LogPS2LevelEditingTools, Log, TEXT("Asset path: %s"), *AssetFilePath);
		//			Asset::Reference AssetReference = LookupAssetReference(AssetFilePath);
		//			ensure(AssetExists(AssetReference));

		//			UE::Math::TMatrix<float>& MeshMatrix = MeshTransforms.AddDefaulted_GetRef();
		//			UE::Math::TMatrix<double> UnrealMeshMatDouble = Transform.ToMatrixWithScale();//(Transform * UPS2LevelEditingDeveloperSettings::Get()->ExportTransform).ToMatrixWithScale();
		//			for (int i = 0; i < 4; ++i)
		//			{
		//				for (int j = 0; j < 4; ++j)
		//				{
		//					MeshMatrix.M[i][j] = UnrealMeshMatDouble.M[i][j];
		//				}
		//			}
		//			MeshFileReferences.Add(AssetReference);
		//		}
		//	}
		//}
	}
}

void FPS2LevelEditingToolsModule::ExportMap(TArray<AActor*> SelectedActors)
{
	TArray<UE::Math::TMatrix<float>> MeshTransforms;
	TArray<Asset::Reference> MeshFileReferences;

	for (AActor* Actor : SelectedActors)
	{
		CollectStaticMeshes(Actor, MeshTransforms, MeshFileReferences);
		CollectFoliageMeshes(Actor, MeshTransforms, MeshFileReferences);
	}

	WriteoutLevel(MeshTransforms, MeshFileReferences);
}