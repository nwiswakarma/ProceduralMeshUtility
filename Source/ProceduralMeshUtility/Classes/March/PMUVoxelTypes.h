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
#include "PMUGradientData.h"
#include "DistanceField/PMUDFGeometry.h"
#include "Mesh/Simplify/PMUMeshSimplifier.h"
#include "PMUVoxelTypes.generated.h"

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUVoxelGradientConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPMUDFGeometryDataObject* DistanceFieldData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> DistanceFieldNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 DistanceFieldPartitionDepth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPMUGradientDataObject* GradientData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="1", UIMin="3", UIMax="3"))
    uint8 GradientType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GradientExponent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName GradientMapName;

    FORCEINLINE void ClearData()
    {
        if (IsValid(DistanceFieldData))
        {
            DistanceFieldData->Empty();
        }

        if (IsValid(GradientData))
        {
            GradientData->Empty();
        }
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUVoxelSurfaceState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bGenerateExtrusion = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExtrusionSurface  = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUMeshSimplifierOptions SimplifierOptions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPMUVoxelGradientConfig GradientConfig;
};

struct FPMUVoxelSurfaceConfig
{
    FVector2D Position;
    int32 VoxelResolution;
    int32 MapSize;
    float ChunkSize;
    float ExtrusionHeight;
    bool bGenerateExtrusion;
    bool bExtrusionSurface;
    FPMUMeshSimplifierOptions SimplifierOptions;
    FPMUVoxelGradientConfig GradientConfig;
    FPMUGridData* GridData = nullptr;

#ifdef PMU_VOXEL_USE_OCL
    class FOCLBProgram* GPUProgram = nullptr;
    FString GPUProgramKernelName;
#endif
};

struct FPMUVoxelGridConfig
{
    FVector2D Position;
    TArray<FPMUVoxelSurfaceState> States;
    int32 VoxelResolution;
    int32 MapSize;
    float ChunkSize;
    float MaxFeatureAngle;
    float MaxParallelAngle;
    float ExtrusionHeight;
    FPMUGridData* GridData = nullptr;

#ifdef PMU_VOXEL_USE_OCL
    class FOCLBProgram* GPUProgram = nullptr;
    FString GPUProgramKernelName;
#endif
};
