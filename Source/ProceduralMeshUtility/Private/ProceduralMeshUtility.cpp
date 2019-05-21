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
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ProceduralMeshUtility"

class FProceduralMeshUtility : public IProceduralMeshUtility
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
    {
        // This code will execute after your module is loaded into memory;
        // the exact timing is specified in the .uplugin file per-module

        FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ProceduralMeshUtility"))->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/ProceduralMeshUtility"), PluginShaderDir);
    }

	virtual void ShutdownModule() override
    {
        // This function may be called during shutdown to clean up your module.
        // For modules that support dynamic reloading,
        // we call this function before unloading the module.
    }
};

DEFINE_LOG_CATEGORY(LogPMU);
DEFINE_LOG_CATEGORY(UntPMU);
IMPLEMENT_MODULE(FProceduralMeshUtility, ProceduralMeshUtility)

#undef LOCTEXT_NAMESPACE
