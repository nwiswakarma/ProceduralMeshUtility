////////////////////////////////////////////////////////////////////////////////
//
// MIT License
// 
// Copyright (c) 2018-2019 Nuraga Wiswakarma
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////
//

#include "ProceduralMeshUtility.h"
#include "Interfaces/IPluginManager.h"
#include "ProceduralMeshUtilitySettings.h"
#include "GenericWorkerThread.h"
#include "GWTAsyncThreadManager.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif // WITH_EDITOR

#ifdef PMU_VOXEL_USE_OCL

#include "OCLBError.h"
#include "OCLBProgram.h"

namespace bc = boost::compute;

#pragma warning( push )
#pragma warning( disable : 4628 4668 )

#include "boost/compute/system.hpp"
#include "boost/compute/container/typed_buffer.hpp"
#include "boost/compute/image/image2d.hpp"
#include "boost/compute/random/default_random_engine.hpp"
#include "boost/compute/random/uniform_real_distribution.hpp"

#pragma warning( pop )

#endif // PMU_VOXEL_USE_OCL

#define LOCTEXT_NAMESPACE "IProceduralMeshUtility"

class FProceduralMeshUtility : public IProceduralMeshUtility
{
    FGWTAsyncThreadPoolWeakInstance ThreadPool;
    bool bThreadPoolRequireUpdate = true;

#if WITH_EDITOR

    bool HandleSettingsModified()
    {
        UpdateThreadPool();
        return false;
    }

	void RegisterSettings()
	{
        // Register settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
		    ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "ProceduralMeshUtility",
		        LOCTEXT("RuntimeGeneralSettingsName", "Procedural Mesh Utility"),
		        LOCTEXT("RuntimeGeneralSettingsDescription", "Procedural mesh utility plug-in configuration settings."),
		        GetMutableDefault<UProceduralMeshUtilitySettings>()
		        );
 
		    if (SettingsSection.IsValid())
		    {
                SettingsSection->OnModified().BindRaw(this, &FProceduralMeshUtility::HandleSettingsModified);
		    }
		}
	}

    void UnregisterSettings()
    {
		// Unregister settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
            SettingsModule->UnregisterSettings("Project", "Plugins", "ProceduralMeshUtility");
		}
    }

#endif // WITH_EDITOR

    void UpdateThreadPool()
    {
        const UProceduralMeshUtilitySettings& Settings = *GetDefault<UProceduralMeshUtilitySettings>();
        ThreadPool = FGWTAsyncThreadPoolWeakInstance(Settings.ThreadCount);

        bThreadPoolRequireUpdate = false;
    }

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
    {
        // This code will execute after your module is loaded into memory;
        // the exact timing is specified in the .uplugin file per-module

        IProceduralMeshUtility::StartupModule();
        FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ProceduralMeshUtility"))->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/ProceduralMeshUtility"), PluginShaderDir);

#if WITH_EDITOR
        RegisterSettings();
#endif // WITH_EDITOR

        bThreadPoolRequireUpdate = true;
    }

	virtual void ShutdownModule() override
    {
        // This function may be called during shutdown to clean up your module.
        // For modules that support dynamic reloading,
        // we call this function before unloading the module.

#if WITH_EDITOR
        UnregisterSettings();
#endif // WITH_EDITOR
    }

	FORCEINLINE virtual FPSGWTAsyncThreadPool GetThreadPool() override
    {
        if (bThreadPoolRequireUpdate)
        {
            UpdateThreadPool();
        }
        return ThreadPool.Pin(IGenericWorkerThread::Get().GetAsyncThreadManager());
    }

#ifdef PMU_VOXEL_USE_OCL

    // GPU PROGRAM STORAGE

	FORCEINLINE virtual bool HasGPUProgram(const FName& ProgramName) const
    {
        return GPUProgramMap.Contains(ProgramName);
    }

	FORCEINLINE virtual TSharedPtr<FOCLBProgram> GetGPUProgram(const FName& ProgramName)
    {
        return HasGPUProgram(ProgramName)
            ? GPUProgramMap.FindChecked(ProgramName)
            : TSharedPtr<FOCLBProgram>();
    }

	virtual bool SetGPUProgram(const FName& ProgramName, TSharedRef<class FOCLBProgram> Program)
    {
        if (ProgramName.IsNone())
        {
            return false;
        }

        if (! HasGPUProgram(ProgramName) && Program->HasValidProgram())
        {
            GPUProgramMap.Emplace(ProgramName, Program);
            return true;
        }

        return false;
    }

	virtual bool RemoveGPUProgram(const FName& ProgramName)
    {
        if (HasGPUProgram(ProgramName))
        {
            GPUProgramMap.Remove(ProgramName);
            return true;
        }

        return false;
    }

    // GPU PROGRAM STORAGE (Weak pointer storage)

	FORCEINLINE virtual bool HasGPUProgramWeak(const FName& ProgramName) const
    {
        return GPUProgramMapWeak.Contains(ProgramName);
    }

	FORCEINLINE virtual TWeakPtr<FOCLBProgram> GetGPUProgramWeak(const FName& ProgramName) const
    {
        return HasGPUProgramWeak(ProgramName)
            ? GPUProgramMapWeak.FindChecked(ProgramName)
            : nullptr;
    }

	virtual bool SetGPUProgramWeak(const FName& ProgramName, TSharedRef<class FOCLBProgram> Program, bool bOverwrite)
    {
        if (ProgramName.IsNone())
        {
            return false;
        }

        bool bResult = false;

        if (bOverwrite)
        {
            GPUProgramMapWeak.Remove(ProgramName);
        }

        if (! HasGPUProgramWeak(ProgramName))
        {
            if (Program->HasValidProgram())
            {
                GPUProgramMapWeak.Emplace(ProgramName, Program);
                bResult = true;
            }
        }

        return bResult;
    }

	virtual bool RemoveGPUProgramWeak(const FName& ProgramName)
    {
        if (HasGPUProgramWeak(ProgramName))
        {
            GPUProgramMapWeak.Remove(ProgramName);
            return true;
        }

        return false;
    }

private:

    TMap<FName, TSharedRef<FOCLBProgram>> GPUProgramMap;
    TMap<FName, TWeakPtr<FOCLBProgram>> GPUProgramMapWeak;

#endif // PMU_VOXEL_USE_OCL
};

DEFINE_LOG_CATEGORY(LogPMU);
DEFINE_LOG_CATEGORY(UntPMU);
IMPLEMENT_MODULE(FProceduralMeshUtility, ProceduralMeshUtility)

#undef LOCTEXT_NAMESPACE
