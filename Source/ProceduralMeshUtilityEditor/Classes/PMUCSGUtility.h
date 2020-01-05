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
#include "PMUCSGUtility.generated.h"

class UPMUCSGBrush;
class UStaticMesh;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITYEDITOR_API FPMUCSGBrushInput
{
	GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPMUCSGBrush* Brush;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform Transform;
};

UCLASS()
class PROCEDURALMESHUTILITYEDITOR_API UPMUCSGUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    static bool IsValidMeshSection(UStaticMesh* InMesh, int32 LODIndex, int32 SectionIndex);
    static void ValidateBrush(UModel* Brush, bool ForceValidate, bool DoStatusUpdate);
    FORCEINLINE static bool FVerticesEqual(const FVector& V1, const FVector& V2);

public:

    UFUNCTION(BlueprintCallable)
    static UPMUCSGBrush* ConvertStaticMeshToBrush(UStaticMesh* InMesh, UMaterialInterface* Material, int32 LODIndex, int32 SectionIndex);

    UFUNCTION(BlueprintCallable)
    static void GenerateSectionsFromBSPBrushes(TArray<FPMUMeshSection>& Sections, const TArray<FPMUCSGBrushInput>& BrushInputs);

    static void CreateStaticMeshFromBrush(TArray<FPMUMeshSection>& Sections, UModel* Model);
};

FORCEINLINE bool UPMUCSGUtility::FVerticesEqual(const FVector& V1, const FVector& V2)
{
	if (FMath::Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.f)
	{
		return false;
	}

	if (FMath::Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.f)
	{
		return false;
	}

	if (FMath::Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.f)
	{
		return false;
	}

	return true;
}
