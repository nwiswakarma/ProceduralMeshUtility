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
#include "GWTAsyncTypes.h"
#include "GWTAsyncThreadPool.h"
#include "PMUUtilityLibrary.generated.h"

// Grid Info

namespace FPMUGridInfo
{
    static const int32 NDIR[8][2] =
    {
        { -1,  0 }, // W
        { -1, -1 }, // NW
        {  0, -1 }, // N
        {  1, -1 }, // NE
        {  1,  0 }, // E
        {  1,  1 }, // SE
        {  0,  1 }, // S
        { -1,  1 }  // SW
    };

    static const int32 NCOUNT      = 8;
    static const int32 AXIS_DIR[4] = { 0, 2, 4, 6 };
    enum { MASK_THRESHOLD = 127 };
};

// Geometry data

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUPoints
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector2D> Data;
};

// Map Generation Info

UENUM(BlueprintType)
enum class EPMUHeightBlendType : uint8
{
    REPLACE,
    MAX,
    ADD,
    MUL
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMapGenerationInfo
{
	GENERATED_BODY()

    /**
     * Source Height Map ID
     */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName = "Source ID"))
    int32 SrcID = -1;

    /**
     * Target Height Map ID
     */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName = "Target ID"))
    int32 DstID = -1;

    /**
     * Height Map Blend Type
     *
     * 0 : MAX
     * 1 : ADD
     * 2 : MUL
     * 3 : REPLACE
     */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPMUHeightBlendType BlendType = EPMUHeightBlendType::REPLACE;
};

// Utility Library

UCLASS()
class PROCEDURALMESHUTILITY_API UPMUUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

    using FPointMask = TArray<uint8>;
    using FIndices        = TArray<int32>;
    using FIndexContainer = TArray<FIndices>;

    static TArray<int32> FindGridIslands(TArray<TDoubleLinkedList<int32>>& BorderPaths, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold, int32 MaxIslandCount, bool bFilterStrands);

public:

    static void FindBorders(TSet<int32>& BorderIndices, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold = 0, const int32 Iteration = 1);

    static TArray<int32> FindGridIslands(FIndexContainer& IndexContainer, const FPointMask& GridMasks, int32 SizeX, int32 SizeY, const uint8 MaskThreshold = 0, int32 MaxIslandCount = 50, bool bFilterStrands = true);

    FORCEINLINE static bool IsPointOnTri(const FVector2D& p, const FVector2D& tp0, const FVector2D& tp1, const FVector2D& tp2)
    {
        return IsPointOnTri(p.X, p.Y, tp0.X, tp0.Y, tp1.X, tp1.Y, tp2.X, tp2.Y);
    }

    FORCEINLINE static bool IsPointOnTri(const FVector& p, const FVector& tp0, const FVector& tp1, const FVector& tp2)
    {
        return IsPointOnTri(p.X, p.Y, tp0.X, tp0.Y, tp1.X, tp1.Y, tp2.X, tp2.Y);
    }

    FORCEINLINE static bool IsPointOnTri(float px, float py, float tpx0, float tpy0, float tpx1, float tpy1, float tpx2, float tpy2)
    {
        float dX = px-tpx2;
        float dY = py-tpy2;
        float dX21 = tpx2-tpx1;
        float dY12 = tpy1-tpy2;
        float D = dY12*(tpx0-tpx2) + dX21*(tpy0-tpy2);
        float s = dY12*dX + dX21*dY;
        float t = (tpy2-tpy0)*dX + (tpx0-tpx2)*dY;
        return (D<0.f)
            ? (s<=0.f && t<=0.f && s+t>=D)
            : (s>=0.f && t>=0.f && s+t<=D);
    }

    // POLY POINTS UTILITY

    UFUNCTION(BlueprintCallable)
    static void PolyClip(const TArray<FVector2D>& Points, TArray<int32>& OutIndices, bool bInversed = false);

    UFUNCTION(BlueprintCallable)
    static TArray<int32> MaskPoints(const TArray<FVector2D>& Points, const TArray<uint8>& Mask, const int32 SizeX, const int32 SizeY, const uint8 Threshold = 127);

    UFUNCTION(BlueprintCallable)
    static void GeneratePolyCircle(UPARAM(ref) TArray<FVector2D>& Points, const FVector2D& Center, float Radius, int32 SegmentCount = 4);

    UFUNCTION(BlueprintCallable)
    static void CollapseAccuteAngles(TArray<FVector2D>& OutPoints, const TArray<FVector2D>& Points, float Threshold = -.25f);

    UFUNCTION(BlueprintCallable)
    static void AssignInstancesAlongPoly(int32 Seed, const TArray<FVector>& Points, const TArray<FVector>& InstanceDimensions, float HeightOffsetMin, float HeightOffsetMax, TArray<int32>& InstanceIds, TArray<FVector>& Positions, TArray<FVector>& Directions);

    // SUBSTANCE UTILITY

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static UObject* CreateSubstanceImageInputFromTexture2D(UObject* WorldContextObject, UTexture2D* SrcTexture, int32 MipLevel = 0, FString InstanceName = TEXT(""));

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
    static UObject* CreateSubstanceImageInputFromRTT2D(UObject* WorldContextObject, UTextureRenderTarget2D* SrcTexture, uint8 MipLevel = 0, FString InstanceName = TEXT(""));
};
