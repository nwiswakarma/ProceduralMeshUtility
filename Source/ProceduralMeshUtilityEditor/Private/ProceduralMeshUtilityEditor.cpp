// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ProceduralMeshUtilityEditor.h"

class FProceduralMeshUtilityEditor : public IProceduralMeshUtilityEditor
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
    {
        // This code will execute after your module is loaded into memory;
        // the exact timing is specified in the .uplugin file per-module
    }

	virtual void ShutdownModule() override
    {
        // This function may be called during shutdown to clean up your module.
        // For modules that support dynamic reloading,
        // we call this function before unloading the module.
    }
};

IMPLEMENT_MODULE(FProceduralMeshUtilityEditor, ProceduralMeshUtilityEditor)
