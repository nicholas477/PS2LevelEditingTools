// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MaterialDomain.h"
#include "PS2LevelEditingDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(config = Editor, defaultconfig)
class PS2LEVELEDITINGTOOLS_API UPS2LevelEditingDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// Path to the PS2 manifest file. This should be MANIFEST.HST
	UPROPERTY(config, EditAnywhere, Category = "PS2 Level Editing Settings", meta=(ConfigRestartRequired = true))
		FFilePath ManifestPath;

	UPROPERTY(config, EditAnywhere, Category = "PS2 Level Editing Settings")
		TArray<FString> ModelFileFormat = { "mdl;PS2 Model File" };

	UPROPERTY(config, EditAnywhere, Category = "PS2 Level Editing Settings")
		TSoftObjectPtr<UMaterialInterface> ModelMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	static UPS2LevelEditingDeveloperSettings* Get() { return GetMutableDefault<ThisClass>(); }
};
