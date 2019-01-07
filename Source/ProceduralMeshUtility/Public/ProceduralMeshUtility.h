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

#pragma once

#include "ModuleManager.h"

class IProceduralMeshUtility : public IModuleInterface
{
public:

	FORCEINLINE static IProceduralMeshUtility& Get()
	{
		return FModuleManager::GetModuleChecked<IProceduralMeshUtility>("ProceduralMeshUtility");
	}

	FORCEINLINE static bool IsAvailable()
	{
        return FModuleManager::Get().IsModuleLoaded("ProceduralMeshUtility");
	}

	virtual bool IsGameModule() const override
	{
		return true;
	}

	virtual TSharedPtr<class FGWTAsyncThreadPool> GetThreadPool() = 0;

#ifdef PMU_VOXEL_USE_OCL

	virtual bool HasGPUProgram(const FName& ProgramName) const = 0;
	virtual TSharedPtr<class FOCLBProgram> GetGPUProgram(const FName& ProgramName) = 0;
	virtual bool SetGPUProgram(const FName& ProgramName, TSharedRef<class FOCLBProgram> Program) = 0;
	virtual bool RemoveGPUProgram(const FName& ProgramName) = 0;

	virtual bool HasGPUProgramWeak(const FName& ProgramName) const = 0;
	virtual TWeakPtr<class FOCLBProgram> GetGPUProgramWeak(const FName& ProgramName) const = 0;
	virtual bool SetGPUProgramWeak(const FName& ProgramName, TSharedRef<class FOCLBProgram> Program, bool bOverwrite = false) = 0;
	virtual bool RemoveGPUProgramWeak(const FName& ProgramName) = 0;

#endif // PMU_VOXEL_USE_OCL
};

DECLARE_LOG_CATEGORY_EXTERN(LogPMU, Verbose, All);
DECLARE_LOG_CATEGORY_EXTERN(UntPMU, Verbose, All);
DECLARE_STATS_GROUP(TEXT("ProceduralMeshUtility"), STATGROUP_ProceduralMeshUtility, STATCAT_Advanced);
