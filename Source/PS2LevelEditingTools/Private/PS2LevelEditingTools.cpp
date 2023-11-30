// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS2LevelEditingTools.h"
#include "Modules/ModuleManager.h"

#include "InterchangeManager.h"
#include "InterchangePS2ModelTranslator.h"

#define LOCTEXT_NAMESPACE "FPS2LevelEditingToolsModule"

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
}

void FPS2LevelEditingToolsModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPS2LevelEditingToolsModule, PS2LevelEditingTools)
