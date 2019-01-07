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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mesh/PMUMeshTypes.h"
#include "Shaders/PMUShaderParameters.h"
#include "PMUGridData.h"
#include "PMUGridUtility.generated.h"

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUGridUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    static void CreateGPUMeshSection(
        ERHIFeatureLevel::Type FeatureLevel,
        FPMUGridData& Grid,
        FPMUMeshSectionResource& Section,
        FPMUShaderTextureParameterInputResource TextureInput,
        float HeightScale = 1.f,
        float BoundsHeightMin = -1.f,
        float BoundsHeightMax =  1.f,
        bool bReverseWinding = false,
        UGWTTickEvent* CallbackEvent = nullptr
        );

private:

    static void CreateGPUMeshSection_RT(
        FRHICommandListImmediate& RHICmdList,
        ERHIFeatureLevel::Type FeatureLevel,
        FPMUGridData& Grid,
        FPMUMeshSectionResource& Section,
        FPMUShaderTextureParameterInputResource& TextureInput,
        float HeightScale,
        FBox Bounds,
        bool bReverseWinding
        );
};
