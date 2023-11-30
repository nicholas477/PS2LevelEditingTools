// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PS2LevelEditingDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(config = Editor, defaultconfig)
class PS2LEVELEDITINGTOOLS_API UPS2LevelEditingDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// Path to the PS2 assets folder
	UPROPERTY(config, EditAnywhere, Category = "PS2 Level Editing Settings")
		FDirectoryPath AssetFolderPath;

	UPROPERTY(config, EditAnywhere, Category = "PS2 Level Editing Settings")
		TArray<FString> ModelFileFormat = { "mdl;PS2 Model File" };

	static UPS2LevelEditingDeveloperSettings* Get() { return GetMutableDefault<ThisClass>(); }
};
