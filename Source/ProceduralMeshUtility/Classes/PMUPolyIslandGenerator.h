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
#include "PMUPolyIslandGenerator.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Poly Island Params"))
struct PROCEDURALMESHUTILITY_API FPMUPolyIslandParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 RandomSeed = 1337;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector2D Size;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector2D DisplacementRange;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 SideCount = 3;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 SubdivCount = 3;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float SubdivLimit = .1f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float PolyScale = .985f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float PolyAngleOffset = 0.f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float MinArea = 10000.f;

    FPMUPolyIslandParams() = default;

    FPMUPolyIslandParams(
        const FVector2D& pSize,
        int32 pSideCount,
        int32 pSubdivCount,
        float pSubdivLimit,
        float pPolyScale,
        float pPolyAngleOffset,
        const FVector2D& pDisplacementRange,
        const float pMinArea = 10000.f
    ) {
        Set(pSize,
            pSideCount,
            pSubdivCount,
            pSubdivLimit,
            pPolyScale,
            pPolyAngleOffset,
            pDisplacementRange,
            pMinArea);
    }

    void Set(
        const FVector2D& pSize,
        int32 pSideCount,
        int32 pSubdivCount,
        float pSubdivLimit,
        float pPolyScale,
        float pPolyAngleOffset,
        const FVector2D& pDisplacementRange,
        const float pMinArea
    ) {
        Size = pSize;
        SideCount = pSideCount;
        SubdivCount = pSubdivCount;
        SubdivLimit = pSubdivLimit;
        PolyScale = pPolyScale;
        PolyAngleOffset = pPolyAngleOffset;
        DisplacementRange = pDisplacementRange;
        MinArea = pMinArea;
    }

    FORCEINLINE bool IsValid() const
    {
        return SideCount >= 3
            && SubdivCount >= 0
            && SubdivLimit > 0.f
            && Size.GetMin() > KINDA_SMALL_NUMBER;
    }

    FORCEINLINE float GetMinAreaSq() const
    {
        return MinArea * MinArea;
    }

    FORCEINLINE FString ToString() const
    {
        return FString::Printf(
            TEXT("(RandomSeed: %d, Size: %s, Side Count: %d, Subdivision Count: %d, Subdivision Limit: %f, Poly Scale: %f, Poly Angle Offset: %f, Displacement Range: %s, Minimum Area: %f)"),
            RandomSeed,
            *Size.ToString(),
            SideCount,
            SubdivCount,
            SubdivLimit,
            PolyScale,
            PolyAngleOffset,
            *DisplacementRange.ToString(),
            MinArea
            );
    }
};

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUPolyIslandGenerator : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    struct FRoughV2D
    {
        FVector2D Pos;
        FVector2D Nrm;
        float Balance;
        float MaxOffset;

        FORCEINLINE FRoughV2D() = default;

        FORCEINLINE FRoughV2D(const FRoughV2D& v)
            : Pos(v.Pos)
            , Nrm(v.Nrm)
            , Balance(v.Balance)
            , MaxOffset(v.MaxOffset)
        {
        }

        FORCEINLINE FRoughV2D(float x, float y, float b, float m)
            : Pos(x, y)
            , Balance(b)
            , MaxOffset(m)
        {
        }

        FORCEINLINE FRoughV2D(float x, float y, FRandomStream& rand, float rmin, float rmax)
            : Pos(x, y)
        {
            GenerateBalance(rand, rmin, rmax);
        }

        FORCEINLINE FRoughV2D(const FVector2D& p, FRandomStream& rand, float rmin, float rmax)
            : Pos(p)
        {
            GenerateBalance(rand, rmin, rmax);
        }

        void GenerateBalance(FRandomStream& rand, float rmin, float rmax)
        {
            Balance = rand.GetFraction();
            MaxOffset = FMath::Max(.05f, rand.GetFraction()) * rand.FRandRange(rmin, rmax);
        }
    };

    typedef TDoubleLinkedList<FRoughV2D>     FPolyList;
    typedef FPolyList::TDoubleLinkedListNode FPolyNode;

    static bool GeneratePoly(FPolyList& PolyList, FBox2D& Bounds, const FPMUPolyIslandParams& Params, FRandomStream& Rand, TArray<FVector2D>& OutVerts);

public:

    static bool GeneratePoly(const FPMUPolyIslandParams& Params, FRandomStream& Rand, TArray<FVector2D>& OutVerts);
    static bool GeneratePoly(const FPMUPolyIslandParams& Params, const TArray<FVector2D>& InitialPoints, FRandomStream& Rand, TArray<FVector2D>& OutVerts);
    static void FitPoints(TArray<FVector2D>& Points, const FVector2D& Dimension, float FitScale = 1.f);

    UFUNCTION(BlueprintCallable, Category="Procedural Mesh Utility|Poly Island", meta=(DisplayName="Generate Poly"))
    static TArray<FVector2D> K2_GeneratePoly(FPMUPolyIslandParams Params);

    UFUNCTION(BlueprintCallable, Category="Procedural Mesh Utility|Poly Island", meta=(DisplayName="Generate Poly With Initial Points"))
    static TArray<FVector2D> K2_GeneratePolyWithInitialPoints(FPMUPolyIslandParams Params, const TArray<FVector2D>& Points);

    UFUNCTION(BlueprintCallable, Category="Procedural Mesh Utility|Poly Island", meta=(DisplayName="Generate Point Offsets"))
    static TArray<FVector2D> K2_GeneratePointOffsets(int32 Seed, const TArray<FVector2D>& Points, float PointRadius);

    UFUNCTION(BlueprintCallable, Category="Procedural Mesh Utility|Poly Island", meta=(DisplayName="Fit Points"))
    static TArray<FVector2D> K2_FitPoints(const TArray<FVector2D>& Points, FVector2D Dimension, float FitScale = 1.f);

    UFUNCTION(BlueprintCallable, Category="Procedural Mesh Utility|Poly Island", meta=(DisplayName="Flip Points"))
    static TArray<FVector2D> K2_FlipPoints(const TArray<FVector2D>& Points, FVector2D Dimension);
};
