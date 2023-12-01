// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS2LevelEditingTools.h"
#include "Modules/ModuleManager.h"

#include "InterchangeManager.h"
#include "InterchangePS2ModelTranslator.h"
#include "PS2LevelEditingDeveloperSettings.h"
#include "LevelEditor.h"

#include "egg/asset.hpp"

FDelegateHandle LevelViewportExtenderHandle;

DEFINE_LOG_CATEGORY(LogPS2LevelEditingTools);

#define LOCTEXT_NAMESPACE "FPS2LevelEditingToolsModule"

static void LoadManifest()
{
	FFilePath AssetManifestPath = UPS2LevelEditingDeveloperSettings::Get()->ManifestPath;
	if (FPaths::FileExists(AssetManifestPath.FilePath))
	{
		const uint64 FileSize = FPlatformFileManager::Get().GetPlatformFile().FileSize(*AssetManifestPath.FilePath);
		const uint64 ExpectedManifestSize = sizeof(Asset::AssetHashMapT);

		if (FileSize != ExpectedManifestSize)
		{
			UE_LOG(LogPS2LevelEditingTools, Warning, TEXT("Asset manifest file size mismatch! File size: %lu, Expected size: %lu"), FileSize, ExpectedManifestSize);
			return;
		}

		TArray64<uint8> AssetManifestBytes;
		AssetManifestBytes.Reserve(FileSize);
		FFileHelper::LoadFileToArray(AssetManifestBytes, *AssetManifestPath.FilePath);

		Asset::load_asset_table((std::byte*)AssetManifestBytes.GetData(), FileSize);
		UE_LOG(LogPS2LevelEditingTools, Log, TEXT("Loaded asset manifest succesfully"));
	}
	else
	{
		UE_LOG(LogPS2LevelEditingTools, Warning, TEXT("Unable to open asset manifest file: %s"), *AssetManifestPath.FilePath);
	}
}

void FPS2LevelEditingToolsModule::StartupModule()
{
	auto RegisterItems = []()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

		//Register the mesh translator
		InterchangeManager.RegisterTranslator(UInterchangePS2ModelTranslator::StaticClass());
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}

	LoadManifest();

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&FPS2LevelEditingToolsModule::OnExtendLevelEditorActorContextMenu));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FPS2LevelEditingToolsModule::ShutdownModule()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

static void CreateExportMapMenu(FMenuBuilder& MenuBuilder, const TArray<AActor*> SelectedActors)
{
	FName ExtensionName = "LiveLinkSourceSubMenu";

	//MenuBuilder.AddSubMenu(
	//	NSLOCTEXT("TakeRecorderSources", "LiveLinkList_Label", "From LiveLink"),
	//	NSLOCTEXT("TakeRecorderSources", "LiveLinkList_Tip", "Add a new recording source from a Live Link Subject"),
	//	FNewMenuDelegate::CreateStatic(PopulateLiveLinkSubMenu, Sources),
	//	FUIAction(),
	//	ExtensionName,
	//	EUserInterfaceActionType::Button
	//);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportPS2Map", "Export PS2 Map"),
		LOCTEXT("ExportPS2MapToolTip", "Exports the selected static meshes to a PS2 map."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.OpenProject"),
		FUIAction(
			FExecuteAction::CreateLambda([]()
				{
					UE_LOG(LogPS2LevelEditingTools, Log, TEXT("Test!!!!!!"));
				}
			)
		)
	);
}

TSharedRef<FExtender> FPS2LevelEditingToolsModule::OnExtendLevelEditorActorContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		"ActorOptions",
		EExtensionHook::After,
		CommandList,
		FMenuExtensionDelegate::CreateStatic(CreateExportMapMenu, SelectedActors)
	);

	return Extender;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPS2LevelEditingToolsModule, PS2LevelEditingTools)
