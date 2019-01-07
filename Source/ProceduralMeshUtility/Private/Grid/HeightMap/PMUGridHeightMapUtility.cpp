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

#include "Grid/HeightMap/PMUGridHeightMapUtility.h"
#include "Grid/HeightMap/PMUGridHeightMapGenerator.h"
#include "Grid/HeightMap/PMUGridUFNHeightMapGenerator.h"

#include "Engine/TextureRenderTarget2D.h"

#ifdef PMU_SUBSTANCE_ENABLED
#include "Engine/Texture2DDynamic.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceImageInput.h"
#include "SubstanceTexture2D.h"
#include "SubstanceUtility.h"
#endif

#include "Rendering/PMURenderingLibrary.h"

void UPMUGridHeightMapUtility::GetHeightNormal(const float X, const float Y, const int32 IX, const int32 IY, const FPMUGridData& GD, const int32 MapId, float& OutHeight, FVector& OutNormal)
{
    check(GD.HasHeightMap(MapId));

    const TArray<float>& HeightMap(GD.GetHeightMapChecked(MapId));
    const FIntPoint& Dim(GD.Dimension);
    const int32 Stride = Dim.X;
    const uint8 GridAxis = (FMath::IsNearlyZero(X-IX) ? 1 : 0) |
                           (FMath::IsNearlyZero(Y-IY) ? 1 : 0) << 1;

    switch (GridAxis)
    {
        case 0: // Grid Free
        {
            check((IX+1) < Dim.X);
            check((IY+1) < Dim.Y);

            float hv = GetBiLerp(X, Y, IX, IY, Stride, HeightMap);
            float hW = (IX > 0)         ? GetBiLerp(X-1.f, Y,     IX-1, IY,   Stride, HeightMap) : hv;
            float hN = (IY > 0)         ? GetBiLerp(X,     Y-1.f, IX,   IY-1, Stride, HeightMap) : hv;
            float hE = ((IX+1) < Dim.X) ? GetBiLerp(X+1.f, Y,     IX+1, IY,   Stride, HeightMap) : hv;
            float hS = ((IY+1) < Dim.Y) ? GetBiLerp(X,     Y+1.f, IX,   IY+1, Stride, HeightMap) : hv;
            const FVector n(-(hE-hW), -(hS-hN), 1.f);

            OutNormal = n.GetSafeNormal();
            OutHeight = hv;
        } break;

        case 1: // Interpolate Sampling Along Y-Axis
        {
            check((IY+1) < Dim.Y);

            float hv = GetLerpY(Y, IX, IY, Stride, HeightMap);
            float hW = (IX > 0)         ? GetLerpY(Y,     IX-1, IY,   Stride, HeightMap) : hv;
            float hN = (IY > 0)         ? GetLerpY(Y-1.f, IX,   IY-1, Stride, HeightMap) : hv;
            float hE = ((IX+1) < Dim.X) ? GetLerpY(Y,     IX+1, IY,   Stride, HeightMap) : hv;
            float hS = ((IY+1) < Dim.Y) ? GetLerpY(Y+1.f, IX,   IY+1, Stride, HeightMap) : hv;
            const FVector n(-(hE-hW), -(hS-hN), 1.f);

            OutNormal = n.GetSafeNormal();
            OutHeight = hv;
        } break;

        case 2: // Interpolate Sampling Along X-Axis
        {
            check((IX+1) < Dim.X);

            float hv = GetLerpX(Y, IX, IY, Stride, HeightMap);
            float hW = (IX > 0)         ? GetLerpX(X-1.f, IX-1, IY,   Stride, HeightMap) : hv;
            float hN = (IY > 0)         ? GetLerpX(X,     IX,   IY-1, Stride, HeightMap) : hv;
            float hE = ((IX+1) < Dim.X) ? GetLerpX(X+1.f, IX+1, IY,   Stride, HeightMap) : hv;
            float hS = ((IY+1) < Dim.Y) ? GetLerpX(X,     IX,   IY+1, Stride, HeightMap) : hv;
            const FVector n(-(hE-hW), -(hS-hN), 1.f);

            OutNormal = n.GetSafeNormal();
            OutHeight = hv;
        } break;

        case 3: // Grid Aligned, No Sampling Interpolation
        {
            const int32 hi = IY*Stride + IX;
            const float hv = HeightMap[hi];
            const float hW = (IX > 0)         ? HeightMap[hi+GD.NDIR[0]] : hv;
            const float hN = (IY > 0)         ? HeightMap[hi+GD.NDIR[2]] : hv;
            const float hE = ((IX+1) < Dim.X) ? HeightMap[hi+GD.NDIR[4]] : hv;
            const float hS = ((IY+1) < Dim.Y) ? HeightMap[hi+GD.NDIR[6]] : hv;
            const FVector n(-(hE-hW), -(hS-hN), 1.f);

            OutNormal = n.GetSafeNormal();
            OutHeight = hv;
        } break;
    }
}

// Height map edit tools

void UPMUGridHeightMapUtility::K2_CreateHeightMap(FPMUGridDataRef GridRef, int32 MapId)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && ! Grid->HasHeightMap(MapId))
    {
        Grid->CreateHeightMap(MapId);
    }
}

void UPMUGridHeightMapUtility::K2_SetHeightMapNum(FPMUGridDataRef GridRef, int32 MapNum, bool bShrink)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid)
    {
        Grid->SetHeightMapNum(MapNum, bShrink);
    }
}

void UPMUGridHeightMapUtility::K2_GenerateHeightMap(FPMUGridDataRef GridRef, int32 MapId, FPMUGridHeightMapGeneratorRef GeneratorRef)
{
    if (MapId >= 0 && GeneratorRef.IsValid() && GridRef.Grid)
    {
        GeneratorRef.Generator->GenerateHeightMap(*GridRef.Grid, MapId);
    }
}

void UPMUGridHeightMapUtility::K2_GenerateHeightMapFromRef(FPMUGridDataRef GridRef, int32 MapId, FPMUGridHeightMapGeneratorData GeneratorRef)
{
    if (MapId >= 0 && GeneratorRef.IsValid() && GridRef.Grid)
    {
        GeneratorRef.Generator->GenerateHeightMap(*GridRef.Grid, MapId);
    }
}

void GenerateHeightMapFromRenderTarget_RenderThread(
    FRHICommandListImmediate& RHICmdList,
    FPMUGridData& Grid,
    int32 MapId,
    FTexture2DRHIRef RenderTargetTexture,
    TArray<FLinearColor>& ColorBuffer
    )
{
    check(IsInRenderingThread());

    FIntPoint GridDim(Grid.Dimension);
    FIntPoint RTTDim(RenderTargetTexture->GetSizeX(), RenderTargetTexture->GetSizeY());

    ColorBuffer.Empty(Grid.CalcSize());

    if (GridDim != RTTDim)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUGridHeightMapUtility::GenerateHeightMapFromRenderTarget_RenderThread() ABORTED, INVALID GRID / RTT DIMENSION"));
        return;
    }

    EPixelFormat Format = RenderTargetTexture->GetFormat();

    if (Format == PF_R16F)
    {
        uint32 Stride = 0;
        void* TextureData = RHICmdList.LockTexture2D(RenderTargetTexture, 0, EResourceLockMode::RLM_ReadOnly, Stride, false);
        uint8* TextureDataPtr = reinterpret_cast<uint8*>(TextureData);

        for (int32 y=0; y < RTTDim.Y; ++y)
        {
            FFloat16* PixelPtr = reinterpret_cast<FFloat16*>(TextureDataPtr);
            
            for (int32 x=0; x<RTTDim.X; ++x)
            {
                FFloat16 EncodedPixel = *PixelPtr;
                PixelPtr++;

                ColorBuffer.Emplace(EncodedPixel.GetFloat(),0.f,0.f,1.f);
            }

            TextureDataPtr += Stride;
        }

        RHICmdList.UnlockTexture2D(RenderTargetTexture, 0, false);
    }
    else
    if (Format == PF_R32_FLOAT)
    {
        uint32 Stride = 0;
        void* TextureData = RHICmdList.LockTexture2D(RenderTargetTexture, 0, EResourceLockMode::RLM_ReadOnly, Stride, false);
        float* TextureDataPtr = reinterpret_cast<float*>(TextureData);

        for (int32 y=0, i=0; y<RTTDim.Y; ++y)
        for (int32 x=0     ; x<RTTDim.X; ++x, ++i)
        {
            ColorBuffer.Emplace(*(TextureDataPtr+i),0.f,0.f,1.f);
        }

        RHICmdList.UnlockTexture2D(RenderTargetTexture, 0, false);
    }
}

void UPMUGridHeightMapUtility::K2_GenerateHeightMapFromRenderTarget(FPMUGridDataRef GridRef, int32 MapId, UTextureRenderTarget2D* RenderTarget)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (! Grid || ! Grid->HasHeightMap(MapId) || ! IsValid(RenderTarget))
    {
        return;
    }

    TArray<FLinearColor> ColorBuffer;
    FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
		FPMUGridHeightMapUtility_K2_GenerateHeightMapFromRenderTarget,
		FPMUGridData*, GridPtr, Grid,
        int32, TargetMapId, MapId,
		UTextureRenderTarget2D*, RenderTargetPtr, RenderTarget,
		TArray<FLinearColor>*, ColorBufferPtr, &ColorBuffer,
		{
            FTexture2DRHIRef RenderTargetTexture = RenderTargetPtr->GetRenderTargetResource()->GetRenderTargetTexture();
            GenerateHeightMapFromRenderTarget_RenderThread(RHICmdList, *GridPtr, TargetMapId, RenderTargetTexture, *ColorBufferPtr);
        } );

    FlushRenderingCommands();

    if (ColorBuffer.Num() == HeightMap.Num())
    {
        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] = ColorBuffer[i].R;
        }
    }
}

void UPMUGridHeightMapUtility::K2_CopyHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(SrcId))
    {
        if (! Grid->HasHeightMap(DstId))
        {
            K2_CreateHeightMap(GridRef, DstId);
        }

        const FPMUGridData::FHeightMap& SrcMap(Grid->GetHeightMapChecked(SrcId));
        FPMUGridData::FHeightMap& DstMap(Grid->GetHeightMapChecked(DstId));

        DstMap = SrcMap;
    }
}

void UPMUGridHeightMapUtility::K2_MulHeightValue(FPMUGridDataRef GridRef, int32 MapId, float Value)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] *= Value;
        }
    }
}

void UPMUGridHeightMapUtility::K2_MulHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(SrcId) && Grid->HasHeightMap(DstId))
    {
        const FPMUGridData::FHeightMap& SrcMap(Grid->GetHeightMapChecked(SrcId));
        FPMUGridData::FHeightMap& DstMap(Grid->GetHeightMapChecked(DstId));

        for (int32 i=0; i<DstMap.Num(); ++i)
        {
            DstMap[i] *= SrcMap[i];
        }
    }
}

void UPMUGridHeightMapUtility::K2_AddHeightValue(FPMUGridDataRef GridRef, int32 MapId, float Value)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] += Value;
        }
    }
}

void UPMUGridHeightMapUtility::K2_AddHeightMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(SrcId) && Grid->HasHeightMap(DstId))
    {
        const FPMUGridData::FHeightMap& SrcMap(Grid->GetHeightMapChecked(SrcId));
        FPMUGridData::FHeightMap& DstMap(Grid->GetHeightMapChecked(DstId));

        for (int32 i=0; i<DstMap.Num(); ++i)
        {
            DstMap[i] += SrcMap[i];
        }
    }
}

void UPMUGridHeightMapUtility::K2_ClampMinValue(FPMUGridDataRef GridRef, int32 MapId, float MinValue)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] = FMath::Max(HeightMap[i], MinValue);
        }
    }
}

void UPMUGridHeightMapUtility::K2_ClampMaxValue(FPMUGridDataRef GridRef, int32 MapId, float MaxValue)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] = FMath::Min(HeightMap[i], MaxValue);
        }
    }
}

void UPMUGridHeightMapUtility::K2_ClampHeightMap(FPMUGridDataRef GridRef, int32 MapId)
{
    K2_ClampHeightMapRange(GridRef, MapId, 0.f, 1.f);
}

void UPMUGridHeightMapUtility::K2_ClampHeightMapRange(FPMUGridDataRef GridRef, int32 MapId, float InMin, float InMax)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

        for (int32 i=0; i<HeightMap.Num(); ++i)
        {
            HeightMap[i] = FMath::Clamp(HeightMap[i], InMin, InMax);
        }
    }
}

void UPMUGridHeightMapUtility::K2_ScaleHeightMapRange(FPMUGridDataRef GridRef, int32 MapId, float InMin, float InMax)
{
    FPMUGridData* Grid(GridRef.Grid);

    // Invalid grid reference, map or range, abort
    if (! Grid || ! Grid->HasHeightMap(MapId))
    {
        return;
    }

    FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));

    float MinVal;
    float MaxVal;

    GetExtremas(HeightMap, MinVal, MaxVal);

    const float SrcMin = MinVal;
    const float SrcMax = MaxVal - SrcMin;

    const float MinRange = FMath::Min(InMin, InMax);
    const float MaxRange = FMath::Max(InMin, InMax);

    const float ExtMin = MinRange;
    const float ExtMax = MaxRange - ExtMin;

    for (int32 i=0; i<HeightMap.Num(); ++i)
    {
        HeightMap[i] = ExtMin + (HeightMap[i] - SrcMin) * ExtMax / SrcMax;
    }
}

void UPMUGridHeightMapUtility::K2_ApplyClipMap(FPMUGridDataRef GridRef, int32 SrcId, int32 DstId, float Threshold)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(SrcId) && Grid->HasHeightMap(DstId))
    {
        const FPMUGridData::FHeightMap& SrcMap(Grid->GetHeightMapChecked(SrcId));
        FPMUGridData::FHeightMap& DstMap(Grid->GetHeightMapChecked(DstId));

        const int32 MapSize = SrcMap.Num();

        for (int32 i=0; i<MapSize; ++i)
        {
            if (SrcMap[i] < Threshold)
            {
                DstMap[i] = 0.f;
            }
        }
    }
}

void UPMUGridHeightMapUtility::K2_ApplyCurve(FPMUGridDataRef GridRef, int32 MapId, UCurveFloat* Curve, bool bNormalize)
{
    FPMUGridData* Grid(GridRef.Grid);

    if (Grid && Grid->HasHeightMap(MapId) && IsValid(Curve))
    {
        FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));
        const int32 MapSize = HeightMap.Num();

        if (bNormalize)
        {
            float MinValue;
            float MaxValue;

            GetExtremas(HeightMap, MinValue, MaxValue);

            for (int32 i=0; i<MapSize; ++i)
            {
                const float CurveAlpha = FMath::GetRangePct(MinValue, MaxValue, HeightMap[i]);
                const float ValueAlpha = Curve->GetFloatValue(CurveAlpha);
                HeightMap[i] = FMath::Lerp(MinValue, MaxValue, ValueAlpha);
            }
        }
        else
        {
            for (int32 i=0; i<MapSize; ++i)
            {
                HeightMap[i] = Curve->GetFloatValue(HeightMap[i]);
            }
        }
    }
}

void UPMUGridHeightMapUtility::GetExtremas(FPMUGridData::FHeightMap& HeightMap, float& OutMin, float& OutMax)
{
    float MinVal = TNumericLimits<float>::Max();
    float MaxVal = TNumericLimits<float>::Lowest();

    for (int32 i=0; i<HeightMap.Num(); ++i)
    {
        float Value = HeightMap[i];

        if (Value < MinVal)
        {
            MinVal = Value;
        }

        if (Value > MaxVal)
        {
            MaxVal = Value;
        }
    }

    OutMin = MinVal;
    OutMax = MaxVal;
}

// Height map user edit tools

void UPMUGridHeightMapUtility::K2_RadialGradient(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float InnerHeight, float OuterHeight, bool bUnbound)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    // Invalid parameter, abort
    if (! GridPtr || ! GridPtr->HasHeightMap(MapId) || Radius < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FPMUGridData& Grid(*GridPtr);
    FPMUGridData::FHeightMap& HeightMap(Grid.GetHeightMapChecked(MapId));

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dim(IntDim);

    check(0.f <= Center.X && Center.X < Dim.X);
    check(0.f <= Center.Y && Center.Y < Dim.Y);
    
    float HeightDelta = OuterHeight - InnerHeight;
    float RadiusSq = Radius * Radius;

    for (int32 y=0; y<IntDim.Y; ++y)
    for (int32 x=0; x<IntDim.X; ++x)
    {
        const float DistSq = (Center - FVector2D(x, y)).SizeSquared();

        if (DistSq < RadiusSq)
        {
            HeightMap[x + IntDim.X * y] = InnerHeight + HeightDelta*FMath::Sqrt(DistSq) / Radius;
        }
        else
        if (bUnbound)
        {
            HeightMap[x + IntDim.X * y] = InnerHeight + HeightDelta;
        }
    }
}

void UPMUGridHeightMapUtility::K2_SmoothRadial(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float FalloffRadius, float Strength, float HeightOverride, bool bUseHeightOverride)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    // Invalid parameter, abort
    if (! GridPtr || ! GridPtr->HasHeightMap(MapId) || Radius < KINDA_SMALL_NUMBER || Strength < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FPMUGridData& Grid(*GridPtr);
    FPMUGridData::FHeightMap& HeightMap(Grid.GetHeightMapChecked(MapId));

    const float InnerRadius = Radius;
    const float OuterRadius = Radius + FalloffRadius;

    const float OuterRadiusSq = OuterRadius * OuterRadius;
    const float InnerRadiusSq = InnerRadius * InnerRadius;

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dimension(Grid.Dimension);
    const FVector2D Extents(OuterRadius, OuterRadius);

    const FBox2D Bounds(
        FMath::Clamp(Center-Extents, FVector2D::ZeroVector, Dimension),
        FMath::Clamp(Center+Extents, FVector2D::ZeroVector, Dimension)
        );

    // Invalid bounds, abort
    if (Bounds.Max.X < Bounds.Min.X || Bounds.Max.Y < Bounds.Min.Y)
    {
        return;
    }

    const FIntPoint IntMin(Bounds.Min.X, Bounds.Min.Y);
    const FIntPoint IntMax(Bounds.Max.X, Bounds.Max.Y);

    TSet<int32> InnerSet;
    TSet<int32> OuterSet;
    TMap<int32, float> RangeMap;

    float Sum = 0.f;
    float Avg = 0.f;

    for (int32 y=IntMin.Y; y<IntMax.Y; ++y)
    for (int32 x=IntMin.X; x<IntMax.X; ++x)
    {
        int32 i = x + y*IntDim.X;
        FVector2D Pos(x, y);
        float DistSq = (Center-Pos).SizeSquared();

        if (DistSq < OuterRadiusSq)
        {
            if (DistSq < InnerRadiusSq)
            {
                InnerSet.Emplace(i);
                Sum += HeightMap[i];
            }
            else
            {
                OuterSet.Emplace(i);
                RangeMap.Emplace(i, FMath::GetRangePct(InnerRadiusSq, OuterRadiusSq, DistSq));
            }
        }
    }

    if (InnerSet.Num() <= 0)
    {
        return;
    }

    Avg = bUseHeightOverride ? HeightOverride : (Sum / InnerSet.Num());

    for (int32 i : InnerSet)
    {
        HeightMap[i] += (Avg - HeightMap[i]) * Strength;
    }

    for (int32 i : OuterSet)
    {
        if (RangeMap.Contains(i))
        {
            HeightMap[i] += FMath::Lerp((Avg - HeightMap[i]) * Strength, 0.f, RangeMap.FindChecked(i));
        }
    }
}

void UPMUGridHeightMapUtility::K2_SmoothBox(FPMUGridDataRef GridRef, int32 MapId, FVector2D Center, float Radius, float FalloffRadius, float Strength, float HeightOverride, bool bUseHeightOverride)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    // Invalid parameter, abort
    if (! GridPtr || ! GridPtr->HasHeightMap(MapId) || Radius < KINDA_SMALL_NUMBER || Strength < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FPMUGridData& Grid(*GridPtr);
    FPMUGridData::FHeightMap& HeightMap(Grid.GetHeightMapChecked(MapId));

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dimension(Grid.Dimension);

    const float InnerRadius = Radius;
    const float OuterRadius = Radius + FalloffRadius;

    const FVector2D InnerExtents(InnerRadius, InnerRadius);
    const FVector2D OuterExtents(OuterRadius, OuterRadius);
    const FBox2D InnerBox(Center-InnerExtents, Center+InnerExtents);
    const FBox2D OuterBox(Center-OuterExtents, Center+OuterExtents);

    const FBox2D Bounds(
        FMath::Clamp(Center-OuterExtents, FVector2D::ZeroVector, Dimension),
        FMath::Clamp(Center+OuterExtents, FVector2D::ZeroVector, Dimension)
        );

    // Invalid bounds, abort
    if (Bounds.Max.X < Bounds.Min.X || Bounds.Max.Y < Bounds.Min.Y)
    {
        return;
    }

    const FIntPoint IntMin(Bounds.Min.X, Bounds.Min.Y);
    const FIntPoint IntMax(Bounds.Max.X, Bounds.Max.Y);
    const float FalloffSq = FalloffRadius * FalloffRadius;

    TSet<int32> InnerSet;
    TSet<int32> OuterSet;
    TMap<int32, float> RangeMap;

    float Sum = 0.f;
    float Avg = 0.f;

    for (int32 y=IntMin.Y; y<IntMax.Y; ++y)
    for (int32 x=IntMin.X; x<IntMax.X; ++x)
    {
        int32 i = x + y*IntDim.X;
        FVector2D Pos(x, y);
        float DistSq = InnerBox.ComputeSquaredDistanceToPoint(Pos);

        if (DistSq > 0.f)
        {
            if (DistSq < FalloffSq)
            {
                OuterSet.Emplace(i);
                RangeMap.Emplace(i, FMath::GetRangePct(0.f, FalloffSq, DistSq));
            }
        }
        else
        {
            InnerSet.Emplace(i);
            Sum += HeightMap[i];
        }
    }

    if (InnerSet.Num() <= 0)
    {
        return;
    }

    Avg = bUseHeightOverride ? HeightOverride : (Sum / InnerSet.Num());

    for (int32 i : InnerSet)
    {
        HeightMap[i] += (Avg - HeightMap[i]) * Strength;
    }

    for (int32 i : OuterSet)
    {
        if (RangeMap.Contains(i))
        {
            HeightMap[i] += FMath::Lerp((Avg - HeightMap[i]) * Strength, 0.f, RangeMap.FindChecked(i));
        }
    }
}

void UPMUGridHeightMapUtility::K2_SmoothMultiBox(FPMUGridDataRef GridRef, int32 MapId, const TArray<FBox2D>& MultiBox, float Height, float FalloffRadius, float Strength)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    // Invalid parameter, abort
    if (! GridPtr || ! GridPtr->HasHeightMap(MapId) || Strength < KINDA_SMALL_NUMBER)
    {
        return;
    }

    FPMUGridData& Grid(*GridPtr);
    FPMUGridData::FHeightMap& HeightMap(Grid.GetHeightMapChecked(MapId));

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dimension(Grid.Dimension);
    const float ClampedStrength = FMath::Clamp(Strength, 0.f, 1.f);

    TSet<int32> InnerSet;
    TSet<int32> OuterSet;
    TMap<int32, float> RangeMap;

    for (const FBox2D& InnerBounds : MultiBox)
    {
        // Invalid bounds, skip
        if (! InnerBounds.bIsValid)
        {
            continue;
        }

        const FBox2D OuterBounds(InnerBounds.ExpandBy(FalloffRadius));
        const FBox2D ClampedBounds(
            FMath::Clamp(OuterBounds.Min, FVector2D::ZeroVector, Dimension),
            FMath::Clamp(OuterBounds.Max, FVector2D::ZeroVector, Dimension)
            );

        const FIntPoint IntMin(ClampedBounds.Min.X, ClampedBounds.Min.Y);
        const FIntPoint IntMax(ClampedBounds.Max.X, ClampedBounds.Max.Y);
        const float FalloffSq = FalloffRadius * FalloffRadius;

        for (int32 y=IntMin.Y; y<IntMax.Y; ++y)
        for (int32 x=IntMin.X; x<IntMax.X; ++x)
        {
            int32 i = x + y*IntDim.X;

            if (InnerSet.Contains(i))
            {
                continue;
            }

            FVector2D Pos(x, y);
            float DistSq = InnerBounds.ComputeSquaredDistanceToPoint(Pos);

            if (DistSq > 0.f)
            {
                if (DistSq < FalloffSq)
                {
                    OuterSet.Emplace(i);

                    float* MappedRange = RangeMap.Find(i);
                    float CurrentRange = FMath::GetRangePct(0.f, FalloffSq, DistSq);

                    if (MappedRange)
                    {
                        RangeMap.Emplace(i, FMath::Min(CurrentRange, *MappedRange));
                    }
                    else
                    {
                        RangeMap.Emplace(i, CurrentRange);
                    }
                }
            }
            else
            {
                InnerSet.Emplace(i);

                if (OuterSet.Contains(i))
                {
                    OuterSet.Remove(i);
                }

                HeightMap[i] += (Height - HeightMap[i]) * ClampedStrength;
            }
        }
    }

    for (int32 i : OuterSet)
    {
        if (RangeMap.Contains(i))
        {
            HeightMap[i] += FMath::Lerp((Height - HeightMap[i]) * ClampedStrength, 0.f, RangeMap.FindChecked(i));
        }
    }
}

void UPMUGridHeightMapUtility::K2_Smooth(FPMUGridDataRef GridRef, int32 MapId, int32 Radius)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    if (GridPtr && GridPtr->HasHeightMap(MapId) && Radius > 0)
    {
        FPMUGridData& Grid(*GridPtr);

        SmoothX(Grid, MapId, Radius);
        SmoothY(Grid, MapId, Radius);
    }
}

void UPMUGridHeightMapUtility::K2_SmoothX(FPMUGridDataRef GridRef, int32 MapId, int32 Radius)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    if (GridPtr && GridPtr->HasHeightMap(MapId) && Radius > 0)
    {
        FPMUGridData& Grid(*GridPtr);

        SmoothX(Grid, MapId, Radius);
    }
}

void UPMUGridHeightMapUtility::K2_SmoothY(FPMUGridDataRef GridRef, int32 MapId, int32 Radius)
{
    FPMUGridData* GridPtr(GridRef.Grid);

    if (GridPtr && GridPtr->HasHeightMap(MapId) && Radius > 0)
    {
        FPMUGridData& Grid(*GridPtr);

        SmoothY(Grid, MapId, Radius);
    }
}

void UPMUGridHeightMapUtility::SmoothX(FPMUGridData& Grid, int32 MapId, int32 Radius)
{
    check(Grid.HasHeightMap(MapId));
    check(Radius > 0);

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dimension(Grid.Dimension);

    FPMUGridData::FHeightMap& SrcHeightMap(Grid.GetHeightMapChecked(MapId));
    FPMUGridData::FHeightMap DstHeightMap(SrcHeightMap);

    for (int32 y=0; y<IntDim.Y; y++)
    {
        // Prefill the window with value of the left edge + n leftmost values (where n is Radius)
        float KernelSize = Radius * 2.f + 1;
        float KernelSum = SrcHeightMap[y*IntDim.X] * Radius;

        for (int32 x=0; x<Radius; x++)
        {
            KernelSum += SrcHeightMap[x + y*IntDim.X];
        }

        // In every step shift the window one tile to the right (= subtract its leftmost cell and add
        // value of rightmost + 1). i represents position of the central cell of the window
        for (int32 x=0; x<IntDim.X; x++)
        {
            /* If the window is approaching right border, use the rightmost value as fill. */
            if (x < Radius)
            {
                KernelSum += SrcHeightMap[(x+Radius) + y*IntDim.X] - SrcHeightMap[y*IntDim.X];
            }
            else
            if ((x+Radius) < IntDim.X)
            {
                KernelSum += SrcHeightMap[(x+Radius) + y*IntDim.X] - SrcHeightMap[(x-Radius) + y*IntDim.X];
            }
            else
            {
                KernelSum += SrcHeightMap[(IntDim.X-1) + y*IntDim.X] - SrcHeightMap[(x-Radius) + y*IntDim.X];
            }

            // Set the value of current tile to arithmetic average of window tiles
            DstHeightMap[x + y*IntDim.X] = KernelSum / KernelSize;
        }
    }

    SrcHeightMap = MoveTemp(DstHeightMap);
}

void UPMUGridHeightMapUtility::SmoothY(FPMUGridData& Grid, int32 MapId, int32 Radius)
{
    check(Grid.HasHeightMap(MapId));
    check(Radius > 0);

    const FIntPoint IntDim(Grid.Dimension);
    const FVector2D Dimension(Grid.Dimension);

    FPMUGridData::FHeightMap& SrcHeightMap(Grid.GetHeightMapChecked(MapId));
    FPMUGridData::FHeightMap DstHeightMap(SrcHeightMap);

    for (int32 x=0; x<IntDim.X; x++)
    {
        // Prefill the window with value of the left edge + n topmost values (where n is Radius)
        float KernelSize = Radius * 2.f + 1;
        float KernelSum = SrcHeightMap[x] * Radius;

        for (int32 y=0; y<Radius; y++)
        {
            KernelSum += SrcHeightMap[x + y*IntDim.X];
        }

        // In every step shift the window one tile to the bottom  (= subtract its topmost cell and add
        // value of bottommost + 1). i represents position of the central cell of the window
        for (int32 y=0; y<IntDim.Y; y++)
        {
            // If the window is approaching right border, use the rightmost value as fill
            if (y < Radius)
            {
                KernelSum += SrcHeightMap[x + (y+Radius)*IntDim.X] - SrcHeightMap[x];
            }
            else
            if ((y+Radius) < IntDim.Y)
            {
                KernelSum += SrcHeightMap[x + (y+Radius)*IntDim.X] - SrcHeightMap[x + (y-Radius)*IntDim.X];
            }
            else
            {
                KernelSum += SrcHeightMap[x + (IntDim.Y-1)*IntDim.X] - SrcHeightMap[x + (y-Radius)*IntDim.X];
            }

            /* Set the value of current tile to arithmetic average of window tiles. */
            DstHeightMap[x + y*IntDim.X] = KernelSum / KernelSize;
        }
    }

    SrcHeightMap = MoveTemp(DstHeightMap);
}

// Height map query tools

FVector2D UPMUGridHeightMapUtility::K2_GetCorners2DHeightExtrema(FPMUGridDataRef GridRef, int32 MapId, const FBox2D& Bounds)
{
    FPMUGridData* Grid(GridRef.Grid);
    FVector2D Extrema(ForceInitToZero);

    if (Grid && Grid->HasHeightMap(MapId) && Bounds.bIsValid)
    {
        const FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));
        const FIntPoint IntDim(Grid->Dimension);
        const int32 SX = IntDim.X;
        const int32 SY = IntDim.Y;

        FIntPoint BoundsMin(Bounds.Min.X, Bounds.Min.Y);
        FIntPoint BoundsMax(Bounds.Max.X, Bounds.Max.Y);
        FIntPoint BoundsCenter;

        BoundsMin.X = FMath::Clamp(BoundsMin.X, 0, IntDim.X-1);
        BoundsMin.Y = FMath::Clamp(BoundsMin.Y, 0, IntDim.Y-1);

        BoundsMax.X = FMath::Clamp(BoundsMax.X, 0, IntDim.X-1);
        BoundsMax.Y = FMath::Clamp(BoundsMax.Y, 0, IntDim.Y-1);

        BoundsCenter.X = BoundsMin.X + (BoundsMax.X-BoundsMin.X) * .5f;
        BoundsCenter.Y = BoundsMin.Y + (BoundsMax.Y-BoundsMin.Y) * .5f;

        const float HeightValues[5] = {
            HeightMap[BoundsMin.X+BoundsMin.Y*SX],
            HeightMap[BoundsMax.X+BoundsMin.Y*SX],
            HeightMap[BoundsMax.X+BoundsMax.Y*SX],
            HeightMap[BoundsMin.X+BoundsMax.Y*SX],
            HeightMap[BoundsCenter.X+BoundsCenter.Y*SX]
            };

        Extrema.Set(
            TNumericLimits<float>::Max(),
            TNumericLimits<float>::Lowest()
            );

        for (int32 i=0; i<5; ++i)
        {
            if (HeightValues[i] < Extrema.X)
            {
                Extrema.X = HeightValues[i];
            }

            if (HeightValues[i] > Extrema.Y)
            {
                Extrema.Y = HeightValues[i];
            }
        }
    }

    return Extrema;
}

FVector2D UPMUGridHeightMapUtility::K2_GetLine2DHeightExtrema(FPMUGridDataRef GridRef, int32 MapId, const FVector2D& LineStart, const FVector2D& LineEnd)
{
    FPMUGridData* Grid(GridRef.Grid);
    FVector2D Extrema(ForceInitToZero);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        const FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));
        const FIntPoint IntDim(Grid->Dimension);
        const int32 SX = IntDim.X;
        const int32 SY = IntDim.Y;

        FIntPoint P0(LineStart.X, LineStart.Y);
        FIntPoint P1(LineEnd.X, LineEnd.Y);

        P0.X = FMath::Clamp(P0.X, 0, IntDim.X-1);
        P0.Y = FMath::Clamp(P0.Y, 0, IntDim.Y-1);

        P1.X = FMath::Clamp(P1.X, 0, IntDim.X-1);
        P1.Y = FMath::Clamp(P1.Y, 0, IntDim.Y-1);

        Extrema.X = HeightMap[P0.X+P0.Y*SX];
        Extrema.Y = HeightMap[P1.X+P1.Y*SX];
    }

    return Extrema;
}

FVector UPMUGridHeightMapUtility::K2_GetPointHeight(FPMUGridDataRef GridRef, int32 MapId, const FVector2D& Point)
{
    FPMUGridData* Grid(GridRef.Grid);
    FVector Point3D(Point, 0.f);

    if (Grid && Grid->HasHeightMap(MapId))
    {
        const FPMUGridData::FHeightMap& HeightMap(Grid->GetHeightMapChecked(MapId));
        const FIntPoint IntDim(Grid->Dimension);

        int32 IX = Point3D.X;
        int32 IY = Point3D.Y;

        IX = FMath::Clamp(IX, 0, IntDim.X-1);
        IY = FMath::Clamp(IY, 0, IntDim.Y-1);

        Point3D.Z = HeightMap[IX+IY*IntDim.X];
    }

    return Point3D;
}

UTextureRenderTarget2D* UPMUGridHeightMapUtility::K2_CreateHeightMapRTT(UObject* WorldContextObject, FPMUGridDataRef GridRef, int32 MapId)
{
    FPMUGridData* Grid(GridRef.Grid);
    UTextureRenderTarget2D* RenderTarget = nullptr;

    if (! Grid || ! Grid->HasHeightMap(MapId))
    {
        return RenderTarget;
    }

    RenderTarget = UPMURenderingLibrary::CreateRenderTarget2D(
        WorldContextObject,
        Grid->Dimension.X,
        Grid->Dimension.Y,
        TA_Clamp,
        TA_Clamp,
        RTF_R32f,
        true,
        false,
        false
        );

    if (RenderTarget)
    {
        FTextureRenderTarget2DResource* TextureResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

        ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
            FPMUGridHeightMapUtility_K2_CreateHeightMapRTT,
            FTextureRenderTarget2DResource*, TextureResource, TextureResource,
            FPMUGridData*, Grid, Grid,
            int32, MapId, MapId,
            {
                const FPMUGridData::FHeightMap& Map(Grid->GetHeightMapChecked(MapId));
                FTexture2DRHIParamRef TextureRHI = (FTexture2DRHIParamRef) TextureResource->TextureRHI.GetReference();

                check(TextureRHI != nullptr);

                uint32 Stride;
                float* TextureDataPtr = (float*) RHICmdList.LockTexture2D(TextureRHI, 0, EResourceLockMode::RLM_WriteOnly, Stride, false, false);

                if (TextureDataPtr)
                {
                    FMemory::Memcpy(TextureDataPtr, Map.GetData(), Map.Num() * Map.GetTypeSize());
                }

                RHICmdList.UnlockTexture2D(TextureRHI, 0, false, false);
            } );
    }

    return RenderTarget;
}

// Height map generator functions

FPMUGridHeightMapGeneratorRef UPMUGridHeightMapUtility::CreateUFNHeightMapGenerator(UUFNNoiseGenerator* NoiseGenerator)
{
    if (NoiseGenerator)
    {
        TSharedPtr<IPMUGridHeightMapGenerator> Generator(new FPMUGridUFNHeightMapGenerator(NoiseGenerator));
        return FPMUGridHeightMapGeneratorRef(MoveTemp(Generator));
    }

    return FPMUGridHeightMapGeneratorRef();
}

// Mesh height map functions

void UPMUGridHeightMapUtility::AssignHeightMapToMeshSection(FPMUMeshSection& Section, const UPMUGridInstance* Grid, const int32 MapId)
{
    if (! IsValid(Grid) || ! Grid->HasHeightMap(MapId))
    {
        return;
    }

    check(Grid->HasHeightMap(MapId));

    const FPMUGridData& GridData(Grid->GridData);
    const FPMUGridData::FHeightMap& HeightMap(GridData.GetHeightMapChecked(MapId));
    const FIntPoint Dimension(GridData.Dimension);

    TArray<FPMUMeshVertex>& Vertices(Section.VertexBuffer);
    FBox& Bounds(Section.LocalBox);

    FVector2D GridRangeMin(1.f, 1.f);
    FVector2D GridRangeMax(Dimension.X-2, Dimension.Y-2);

    if ((GridRangeMin.X > GridRangeMax.X) || (GridRangeMin.Y > GridRangeMax.Y))
    {
        return;
    }

    check(GridRangeMin.X > 0.f);
    check(GridRangeMin.Y > 0.f);
    check(GridRangeMax.X > 0.f);
    check(GridRangeMax.Y > 0.f);
    check(GridRangeMax.X > GridRangeMin.X);
    check(GridRangeMax.Y > GridRangeMin.Y);

    for (int32 i=0; i<Vertices.Num(); ++i)
    {
        FPMUMeshVertex& Vertex(Vertices[i]);
        FVector& Position(Vertex.Position);

        const float PX = Position.X;
        const float PY = Position.Y;
        const float GridX = FMath::Clamp(PX, GridRangeMin.X, GridRangeMax.X);
        const float GridY = FMath::Clamp(PY, GridRangeMin.Y, GridRangeMax.Y);
        const int32 IX = GridX;
        const int32 IY = GridY;

        FVector& Normal(Vertex.Normal);
        float& Height(Position.Z);

        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, GridData, MapId, Height, Normal);

        Bounds += Position;
    }
}

// Substance map tools

void UPMUGridHeightMapUtility::CreateSubstanceHeightMap(FPMUGridDataRef GridRef, int32 MapId, UObject* InSubstanceTexture)
{
#ifdef PMU_SUBSTANCE_ENABLED
    FPMUGridData* Grid(GridRef.Grid);
    USubstanceTexture2D* SubstanceTexture = Cast<USubstanceTexture2D>(InSubstanceTexture);

    // Invalid grid data, abort
    if (! Grid || ! IsValid(SubstanceTexture))
    {
        if (! Grid)
        {
            UE_LOG(LogPMU,Error, TEXT("Invalid grid data reference"));
        }

        if (! IsValid(SubstanceTexture))
        {
            UE_LOG(LogPMU,Error, TEXT("Invalid substance texture"));
        }

        return;
    }

    const FIntPoint GridDim(Grid->Dimension);
    const int32 MapSize = Grid->CalcSize();

    EPixelFormat PixelFormat;
    FTexture2DMipMap* MipData = nullptr;
    float HeightRatio = 1.f;
    int32 BPP = 1;
    int32 DataSize = 0;

    // Validate substance texture size
    {
        PixelFormat = SubstanceTexture->Format;

        // Make sure texture pixel format is supported
        switch (PixelFormat)
        {
            case EPixelFormat::PF_G8:
                HeightRatio = 1.f / 255.f;
                BPP = 1;
                break;

            case EPixelFormat::PF_G16:
                HeightRatio = 1.f / 65535.f;
                BPP = 2;
                break;

            // Format not supported
            default:
                return;
        }

        DataSize = MapSize * BPP;

        for (int32 LOD=0; LOD<SubstanceTexture->Mips.Num(); ++LOD)
        {
            FTexture2DMipMap& TexMip(SubstanceTexture->Mips[LOD]);
            int32 BulkSize = TexMip.BulkData.GetBulkDataSize();

            if (DataSize == BulkSize && GridDim.X >= TexMip.SizeX && GridDim.Y >= TexMip.SizeY)
            {
                if (GridDim.X == TexMip.SizeX && GridDim.Y == TexMip.SizeY)
                {
                    MipData = &TexMip;
                }

                break;
            }
        }

        // No substance texture with the valid mip, abort
        if (! MipData)
        {
            return;
        }
    }

    check(DataSize > 0);

    // No existing height map, create a new one
    if (! Grid->HasHeightMap(MapId))
    {
        Grid->CreateHeightMap(MapId);
    }

    TArray<float>& HeightMap(Grid->GetHeightMapChecked(MapId));

    TArray<uint8> TextureByteData;
    TextureByteData.SetNumZeroed(DataSize);

    const void* MipBulkData(MipData->BulkData.LockReadOnly());
    FMemory::Memcpy(TextureByteData.GetData(), MipBulkData, DataSize);
    MipData->BulkData.Unlock();

    switch (PixelFormat)
    {
        case EPixelFormat::PF_G8:
        {
            uint8* TexPtr = TextureByteData.GetData();
            for (int32 i=0; i<MapSize; ++i)
                HeightMap[i] = *(TexPtr+i) * HeightRatio;
        }
        break;

        case EPixelFormat::PF_G16:
        {
            uint16* TexPtr = reinterpret_cast<uint16*>(TextureByteData.GetData());
            for (int32 i=0; i<MapSize; ++i)
                HeightMap[i] = *(TexPtr+i) * HeightRatio;
        }
        break;
    }
#endif
}

UObject* UPMUGridHeightMapUtility::CreatePointMaskSubstanceImage(FPMUGridDataRef GridRef)
{
#ifdef PMU_SUBSTANCE_ENABLED

    FPMUGridData* Grid(GridRef.Grid);

    // Invalid grid data, abort
    if (! Grid)
    {
        UE_LOG(LogPMU,Error, TEXT("Invalid grid data reference"));
        return nullptr;
    }

    const TArray<uint8>& PointMask(Grid->PointMask);
    const int32 MapSize = Grid->CalcSize();
    const int32 PointCount = PointMask.Num();
    const int32 ByteSize = PointCount * 4;

    if (MapSize != PointCount)
    {
        UE_LOG(LogPMU,Error, TEXT("Invalid Grid PointMask size, grid size and point mask size mismatch"));
        return nullptr;
    }

    USubstanceImageInput* Image = NewObject<USubstanceImageInput>(GetTransientPackage(), TEXT(""), RF_Transient);

    if (IsValid(Image))
    {
        Image->CompressionLevelRGB = 0;
        Image->CompressionLevelAlpha = 0;
        Image->NumComponents = 1;
        Image->SizeX = Grid->Dimension.X;
        Image->SizeY = Grid->Dimension.Y;
            
        FByteBulkData& ImageRGB(Image->ImageRGB);

        ImageRGB.RemoveBulkData();
        ImageRGB.Lock(LOCK_READ_WRITE);

        uint8* ImagePtr = reinterpret_cast<uint8*>(ImageRGB.Realloc(ByteSize));
        FMemory::Memset(ImagePtr, 0, ByteSize);
        for (int32 i=0; i<PointCount; ++i)
        {
            int32 Idx = i*4;
            uint8 Value = PointMask[i];
            ImagePtr[Idx  ] = Value;
            ImagePtr[Idx+1] = Value;
            ImagePtr[Idx+2] = Value;
            ImagePtr[Idx+3] = Value;
        }

        ImageRGB.Unlock();

        return Image;
    }

#endif

    return nullptr;
}
