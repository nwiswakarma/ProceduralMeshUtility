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
#include "GameFramework/Actor.h"
#include "PMUDeferredDecalComponent.generated.h"

class UPMUDeferredDecalComponent;

USTRUCT(BlueprintType)
struct FPMUDeferredDecalLine
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    FVector2D LineStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    FVector2D LineEnd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    float Radius = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    float Height = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    float HeightOffset = 0.f;
};


USTRUCT(BlueprintType)
struct FPMUDeferredDecalLineGroup
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    TArray<FPMUDeferredDecalLine> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    FColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    FBox Bounds;
};

UCLASS(BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class UPMUDeferredDecalComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decal)
    TArray<FPMUDeferredDecalLineGroup> LineGroups;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override
    {
        return 1;
    }
	//~ End UMeshComponent Interface.

	UFUNCTION(BlueprintCallable, Category = "Components|PMU Decal")
	void UpdateRenderState();

	UFUNCTION(BlueprintCallable, Category = "Components|PMU Decal")
    void SetLineGroup(int32 GroupId, const FPMUDeferredDecalLineGroup& LineGroup, bool bUpdateRenderState);

	UFUNCTION(BlueprintCallable, Category = "Components|PMU Decal")
    void UpdateLineGroup(int32 GroupId);

	UFUNCTION(BlueprintCallable, Category = "Components|PMU Decal")
    static TArray<FPMUDeferredDecalLine> ConvertPointsToLines(const TArray<FVector2D>& InPoints, float Radius = 1.f, float Height = 1.f, FColor Color = FColor::Black, bool bClosePoints = false);

	UFUNCTION(BlueprintCallable, Category = "Components|PMU Decal")
    static TArray<FPMUDeferredDecalLine> ConvertPointsToLines3D(const TArray<FVector>& InPoints, float Radius = 1.f, float Height = 1.f, FColor Color = FColor::Black, bool bClosePoints = false);

protected:

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;

	void UpdateLocalBounds();

    static void ConvertPointPairToLine3D(FPMUDeferredDecalLine& Line, const FVector& P0, const FVector& P1, float Radius, float Height, FColor Color);

};
