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
#include "PMUUtilityLibrary.h"
#include "PMUGradientData.generated.h"

typedef TMap<int32, FLinearColor> FPMUGradientValueMap;

class FPMUGradientUtility
{
public:

    FORCEINLINE static bool IsValidValue(const uint8 ValueId)
    {
        return ValueId < 3;
    }

    FORCEINLINE static void AverageRatio(FLinearColor& Grad)
    {
        const float Ratio = 1.f / (Grad.R + Grad.G + Grad.B);
        Grad.R = FMath::Clamp(Grad.R * Ratio, 0.f, 1.f);
        Grad.G = FMath::Clamp(Grad.G * Ratio, 0.f, 1.f);
        Grad.B = FMath::Clamp(Grad.B * Ratio, 0.f, 1.f);
    }

    FORCEINLINE static FLinearColor GetRatio(const uint8 ValueId, const float Value)
    {
        check(IsValidValue(ValueId));
        FLinearColor Ratio(ForceInitToZero);
        Ratio.Component(ValueId)       = 1.f;
        Ratio.Component((ValueId+1)%2) = Value;
        AverageRatio(Ratio);
        return Ratio;
    }

    FORCEINLINE static FColor GetRatioAsColor(const uint8 ValueId, const float Value)
    {
        check(IsValidValue(ValueId));
        return GetRatio(ValueId, Value).ToFColor(false);
    }
};

class FPMUGradientDataMap
{
    TMap<int32, uint8> ValueMap;

public:

    void ClearValues()
    {
        ValueMap.Empty();
    }

    void AddValues(const uint8 ValueId, const TArray<int32>& Indices)
    {
        if (FPMUGradientUtility::IsValidValue(ValueId))
        {
            ValueMap.Reserve(ValueMap.Num()+Indices.Num());

            for (int32 i : Indices)
            {
                ValueMap.Emplace(i, ValueId);
            }
        }
    }

    FORCEINLINE bool HasValue(const int32 Index) const
    {
        return ValueMap.Contains(Index);
    }

    FORCEINLINE const uint8* GetValue(const int32 Index) const
    {
        return ValueMap.Find(Index);
    }

    FORCEINLINE const uint8& GetValueChecked(const int32 Index) const
    {
        return ValueMap.FindChecked(Index);
    }
};

struct FPMUGradientData
{
    FIntPoint Dimension;

    TArray<FPMUGradientDataMap> GradientMaps;
    TMap<FName, int32> GradientNameMap;

    void Empty()
    {
        GradientMaps.Empty();
        GradientNameMap.Empty();
    }

    FORCEINLINE void SetDimension(const FIntPoint& InDimension)
    {
        Dimension = InDimension;
    }

    FORCEINLINE bool HasValidDimension() const
    {
        return Dimension.X > 0 && Dimension.Y > 0;
    }

    FORCEINLINE bool HasMap(int32 MapId) const
    {
        return GradientMaps.IsValidIndex(MapId);
    }

    FORCEINLINE FPMUGradientDataMap& GetMapChecked(int32 MapId)
    {
        return GradientMaps[MapId];
    }

    FORCEINLINE const FPMUGradientDataMap& GetMapChecked(int32 MapId) const
    {
        return GradientMaps[MapId];
    }

    FORCEINLINE bool HasNamedMap(const FName& MapName) const
    {
        return GradientNameMap.Contains(MapName)
            ? HasMap(GradientNameMap.FindChecked(MapName))
            : false;
    }

    FORCEINLINE int32 GetNamedMapId(const FName& MapName) const
    {
        return HasNamedMap(MapName)
            ? GradientNameMap.FindChecked(MapName)
            : -1;
    }

    FORCEINLINE FPMUGradientDataMap* GetNamedMap(const FName& MapName)
    {
        return HasNamedMap(MapName)
            ? &GetNamedMapChecked(MapName)
            : nullptr;
    }

    FORCEINLINE const FPMUGradientDataMap* GetNamedMap(const FName& MapName) const
    {
        return HasNamedMap(MapName)
            ? &GetNamedMapChecked(MapName)
            : nullptr;
    }

    FORCEINLINE FPMUGradientDataMap& GetNamedMapChecked(const FName& MapName)
    {
        return GetMapChecked(GradientNameMap.FindChecked(MapName));
    }

    FORCEINLINE const FPMUGradientDataMap& GetNamedMapChecked(const FName& MapName) const
    {
        return GetMapChecked(GradientNameMap.FindChecked(MapName));
    }

    int32 CreateMap()
    {
        return CreateMap(GradientMaps.Num());
    }

    int32 CreateMap(const int32 MapId)
    {
        if (! GradientMaps.IsValidIndex(MapId))
        {
            GradientMaps.SetNum(MapId+1, false);
        }

        return MapId;
    }
};

UCLASS(BlueprintType)
class PROCEDURALMESHUTILITY_API UPMUGradientDataObject : public UObject
{
    GENERATED_BODY()

public:

    FPMUGradientData GradientData;

    UFUNCTION(BlueprintCallable)
    FIntPoint GetDimension() const
    {
        return GradientData.Dimension;
    }

    UFUNCTION(BlueprintCallable)
    void SetDimension(const FIntPoint& InDimension)
    {
        GradientData.SetDimension(InDimension);
    }

    UFUNCTION(BlueprintCallable)
    void Empty()
    {
        GradientData.Empty();
    }

    UFUNCTION(BlueprintCallable)
    void CreateMap(int32 MapId)
    {
        GradientData.CreateMap(MapId);
    }

    UFUNCTION(BlueprintCallable)
    void ClearValues(int32 MapId)
    {
        if (GradientData.HasMap(MapId))
        {
            FPMUGradientDataMap& Map(GradientData.GetMapChecked(MapId));
            Map.ClearValues();
        }
    }

    UFUNCTION(BlueprintCallable)
    void AddValues(int32 MapId, uint8 ValueId, const TArray<int32>& Indices)
    {
        if (GradientData.HasMap(MapId))
        {
            FPMUGradientDataMap& Map(GradientData.GetMapChecked(MapId));
            Map.AddValues(ValueId, Indices);
        }
    }

    UFUNCTION(BlueprintCallable)
    bool HasNamedMap(FName MapName) const
    {
        return GradientData.HasNamedMap(MapName);
    }

    UFUNCTION(BlueprintCallable)
    int32 GetNamedMapId(FName MapName) const
    {
        return GradientData.GetNamedMapId(MapName);
    }

    UFUNCTION(BlueprintCallable)
    int32 CreateNamedMap(FName MapName)
    {
        int32 MapId = -1;

        if (! HasNamedMap(MapName))
        {
            MapId = GradientData.CreateMap();
            SetMapName(MapName, MapId);
        }

        return MapId;
    }

    UFUNCTION(BlueprintCallable)
    void SetMapName(FName MapName, int32 MapId)
    {
        GradientData.GradientNameMap.Emplace(MapName, MapId);
    }

    UFUNCTION(BlueprintCallable)
    void RemoveMapName(FName MapName)
    {
        GradientData.GradientNameMap.Remove(MapName);
    }
};
