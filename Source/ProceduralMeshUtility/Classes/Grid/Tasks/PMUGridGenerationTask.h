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

#include "CoreMinimal.h"
#include "PMUGridData.h"
#include "PMUGridGenerationTask.generated.h"

UCLASS(Blueprintable, BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUGridGenerationTask : public UObject
{
    GENERATED_BODY()

	DECLARE_DYNAMIC_DELEGATE(FOnExecuteTask);

protected:

    typedef class FPMUPolyPath    FPolyPath;
    typedef FPMUGridData          FGridData;
    typedef FGridData::FMapInfo   FMapInfo;
    typedef FGridData::FPointSet  FPointSet;
    typedef FGridData::FHeightMap FHeightMap;

    FGridData* GridData;

	virtual void ExecuteTask_Impl(FGridData& GridData)
        PURE_VIRTUAL(UPMUGridGenerationTask::ExecuteTask_Impl, );

public:

	UPROPERTY()
	FOnExecuteTask OnExecuteTask;

	UPROPERTY(BlueprintReadWrite)
	TArray<UObject*> Objects;

	UPROPERTY(BlueprintReadWrite)
	bool bAsyncSimultaneous = false;

    virtual bool SetupTask(FGridData& InGridData)
    {
        // Blank default implementation
        return true;
    }

    virtual void AsyncSetupTask(FGridData& InGridData)
    {
        // Blank default implementation
    }

	void ExecuteTask(FGridData& InGridData)
    {
        GridData = &InGridData;

        ExecuteTask_Impl(*GridData);
        OnExecuteTask.ExecuteIfBound();

        GridData = nullptr;
    }

    UFUNCTION(BlueprintCallable)
    FPMUGridDataRef GetGridRef()
    {
        return GridData ? FPMUGridDataRef(*GridData) : FPMUGridDataRef();
    }
};
