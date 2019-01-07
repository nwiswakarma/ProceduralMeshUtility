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
#include "Mesh/PMUMeshTypes.h"
#include "PMUMarchingSquaresMap.h"
#include "PMUMarchingSquaresTypes.h"
#include "PMUMarchingSquaresMapRef.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPMUMarchingSquaresMapRef_OnBuildMapDone, bool, bResult, int32, FillType);

UCLASS(BlueprintType, Blueprintable)
class UPMUMarchingSquaresMapRef : public UObject
{
    GENERATED_BODY()

    FPMUMarchingSquaresMap Map;

    void ApplyMapSettings();
    void OnBuildMapDoneCallback(bool bBuildMapResult, uint32 FillType);

public:

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1", DisplayName="Dimension X"))
    int32 DimX = 256;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1", DisplayName="Dimension Y"))
    int32 DimY = 256;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1"))
    int32 BlockSize = 64;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    bool bOverrideBoundsZ = false;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    float BoundsSurfaceOverrideZ = 0.f;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    float BoundsExtrudeOverrideZ = 0.f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float SurfaceHeightScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float ExtrudeHeightScale = -1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float BaseHeightOffset = 0.5f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float BaseOffsetScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float SurfaceOffsetScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float ExtrudeOffsetScale = -1.0f;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite)
    int32 SurfaceHeightTextureMipLevel = 0;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite)
    int32 ExtrudeHeightTextureMipLevel = 0;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite)
    int32 HeightOffsetTextureMipLevel = 0;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite, meta=(DisplayName="Height Offset Texture Sample Offset"))
    FVector2D HeightOffsetSampleOffset;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite, meta=(DisplayName="Height Offset Texture Sample Scale"))
    float HeightOffsetSampleScale = 1.f;

    UPROPERTY(BlueprintAssignable, Category="Map Settings")
    FPMUMarchingSquaresMapRef_OnBuildMapDone OnBuildMapDone;

    UPROPERTY(BlueprintReadWrite, Category="Prefabs")
    TArray<class UStaticMesh*> MeshPrefabs;

    UPROPERTY(BlueprintReadWrite, Category="Debug")
    UTextureRenderTarget2D* DebugRTT;

    UPMUMarchingSquaresMapRef(const FObjectInitializer& ObjectInitializer);

    FORCEINLINE FPMUMarchingSquaresMap& GetMap()
    {
        return Map;
    }

    FORCEINLINE const FPMUMarchingSquaresMap& GetMap() const
    {
        return Map;
    }

    void SetHeightTexture(FTexture2DRHIParamRef HeightTexture, TEnumAsByte<EPMUMarchingSquaresHeightTextureType::Type> HeightTextureType);

    UFUNCTION(BlueprintCallable)
    bool HasValidMap() const
    {
        return Map.HasValidDimension();
    }

    UFUNCTION(BlueprintCallable, DisplayName="Apply Map Settings")
    void ApplyMapSettingsBP()
    {
        ApplyMapSettings();
    }

    UFUNCTION(BlueprintCallable)
    FIntPoint GetMapDimension() const
    {
        return Map.GetDimension();
    }

    UFUNCTION(BlueprintCallable)
    void BuildMap(int32 FillType, bool bGenerateWalls);

    UFUNCTION(BlueprintCallable)
    void ClearMap();

    UFUNCTION(BlueprintCallable)
    void SetHeightTextureFromUTexture2D(UTexture2D* HeightTexture, TEnumAsByte<EPMUMarchingSquaresHeightTextureType::Type> HeightTextureType);

    UFUNCTION(BlueprintCallable)
    void SetHeightTextureFromRTT(UTextureRenderTarget2D* HeightTexture, TEnumAsByte<EPMUMarchingSquaresHeightTextureType::Type> HeightTextureType);

    UFUNCTION(BlueprintCallable)
    void SetHeightTextureFromSubstanceTexture(UObject* HeightTexture, TEnumAsByte<EPMUMarchingSquaresHeightTextureType::Type> HeightTextureType);

    // SECTION FUNCTIONS

    UFUNCTION(BlueprintCallable)
    bool HasSection(int32 FillType, int32 Index) const;

    UFUNCTION(BlueprintCallable)
    FPMUMeshSectionResourceRef GetSectionResource(int32 FillType, int32 Index);

    UFUNCTION(BlueprintCallable)
    void CopySectionResource(FPMUMeshSectionResourceRef TargetSection, int32 FillType, int32 Index) const;

    UFUNCTION(BlueprintCallable)
    void ClearStateSectionResource(int32 FillType);

    // PREFAB FUNCTIONS

    UFUNCTION(BlueprintCallable)
    bool HasPrefab(int32 PrefabIndex) const;

    UFUNCTION(BlueprintCallable)
    TArray<FBox2D> GetPrefabBounds(int32 PrefabIndex) const;

    UFUNCTION(BlueprintCallable)
    bool ApplyPrefab(
        int32 PrefabIndex,
        int32 SectionIndex,
        FVector Center,
        bool bApplyHeightMap,
        bool bInverseHeightMap,
        UPARAM(ref) FPMUMeshSection& OutSection
        );
};
