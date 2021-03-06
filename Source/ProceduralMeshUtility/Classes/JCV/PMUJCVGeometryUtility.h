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
#include "PMUJCVGeometryUtility.generated.h"

class UJCVDiagramAccessor;
struct FJCVDualGeometry;

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUJCVGeometryUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable)
    static void GenerateDualGeometryByFeature(
        UPARAM(ref) FPMUMeshSection& Section,
        UJCVDiagramAccessor* Accessor,
        uint8 FeatureType,
        int32 FeatureIndex = -1
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void DrawDualGeometryByFeature(
        UObject* WorldContextObject,
        UTextureRenderTarget2D* RenderTarget,
        UJCVDiagramAccessor* Accessor,
        uint8 FeatureType,
        int32 FeatureIndex = -1
        );

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static void DrawPolyGeometryByFeature(
        UObject* WorldContextObject,
        UTextureRenderTarget2D* RenderTarget,
        UJCVDiagramAccessor* Accessor,
        uint8 FeatureType,
        int32 FeatureIndex = -1,
        bool bUseCellAverageValue = false
        );
};
