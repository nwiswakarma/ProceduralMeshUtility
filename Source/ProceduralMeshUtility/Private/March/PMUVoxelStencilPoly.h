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
#include "EarcutTypes.h"
#include "PMUUtilityLibrary.h"
#include "PMUVoxelStencilTri.h"
#include "March/PMUVoxelStencil.h"
#include "PMUVoxelStencilPoly.generated.h"

UCLASS(BlueprintType)
class UPMUVoxelStencilPolyRef : public UPMUVoxelStencilRef
{
    GENERATED_BODY()

    TArray<FPMUVoxelStencilTri> Stencils;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    int32 FillType = 0;

    virtual void Clear() override
    {
        Stencils.Reset();
    }

    virtual UPMUVoxelStencilRef* Copy(UObject* Outer) override
    {
        UPMUVoxelStencilPolyRef* Stencil = NewObject<UPMUVoxelStencilPolyRef>(Outer);

        Stencil->FillType = FillType;
        Stencil->Stencils = Stencils;

        return Stencil;
    }

    virtual TArray<int32> GetChunkIndices(UPMUVoxelMapRef* MapRef) override
    {
        TSet<int32> ChunkIndexSet;

        if (! IsValid(MapRef) || ! MapRef->IsInitialized())
        {
            return ChunkIndexSet.Array();
        }

        FPMUVoxelMap& Map(MapRef->GetMap());
        ChunkIndexSet.Reserve(Map.chunkResolution * Map.chunkResolution);

        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            TArray<int32> ChunkIndices;
            const FVector2D Center(Stencil.GetShiftedBoundsCenter());

            Stencil.FillTypeSetting = FillType;
            Stencil.GetChunkIndices(Map, Center, ChunkIndices);

            ChunkIndexSet.Append(ChunkIndices);
        }

        return ChunkIndexSet.Array();
    }

    virtual void EditStates(FPMUVoxelMap& Map) override
    {
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditStates(Map, Stencil.GetShiftedBoundsCenter());
        }
    }

    virtual void EditStatesAsync(FPSGWTAsyncTask& Task, FPMUVoxelMap& Map) override
    {
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            FVector2D Center(Stencil.GetShiftedBoundsCenter());

            FPMUVoxelStencilTri* StencilPtr = &Stencil;
            FPMUVoxelMap* MapPtr = &Map;

            Task->AddTask(
                [StencilPtr, MapPtr, Center]()
                {
                    StencilPtr->EditStates(*MapPtr, Center);
                } );
        }
    }

    virtual void EditCrossings(FPMUVoxelMap& Map) override
    {
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditCrossings(Map, Stencil.GetShiftedBoundsCenter());
        }
    }

    virtual void EditCrossingsAsync(FPSGWTAsyncTask& Task, FPMUVoxelMap& Map) override
    {
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            FVector2D Center(Stencil.GetShiftedBoundsCenter());

            FPMUVoxelStencilTri* StencilPtr = &Stencil;
            FPMUVoxelMap* MapPtr = &Map;

            Task->AddTask(
                [StencilPtr, MapPtr, Center]()
                {
                    StencilPtr->EditCrossings(*MapPtr, Center);
                } );
        }
    }

    FORCEINLINE virtual void EditMapAsync(FPSGWTAsyncTask& Task, FPMUVoxelMap& Map) override
    {
        Task->AddTask([this, &Map](){ EditMap(Map); });
    }

    void EditMap(FPMUVoxelMap& Map)
    {
        // Edit map states
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditStates(Map, Stencil.GetShiftedBoundsCenter());
        }

        // Edit map voxel crossings
        for (FPMUVoxelStencilTri& Stencil : Stencils)
        {
            Stencil.FillTypeSetting = FillType;
            Stencil.EditCrossings(Map, Stencil.GetShiftedBoundsCenter());
        }
    }

    UFUNCTION(BlueprintCallable)
    void EditMap(UPMUVoxelMapRef* MapRef)
    {
        if (IsValid(MapRef) && MapRef->IsInitialized())
        {
            EditMap(MapRef->GetMap());
        }
    }

    UFUNCTION(BlueprintCallable)
    int32 GetPolyCount() const
    {
        return Stencils.Num();
    }

    UFUNCTION(BlueprintCallable)
    void AddTri(FVector Pos0, FVector Pos1, FVector Pos2, bool bInversed = false)
    {
        if (bInversed)
        {
            Stencils.Emplace(Pos2, Pos1, Pos0, FillType);
        }
        else
        {
            Stencils.Emplace(Pos0, Pos1, Pos2, FillType);
        }
    }

    UFUNCTION(BlueprintCallable)
    void AddTris(const TArray<FVector>& Positions, const TArray<int32> Indices, bool bInversed = false)
    {
        const int32 TriCount = Indices.Num() / 3;

        for (int32 ti=0; ti<TriCount; ++ti)
        {
            int32 i = ti * 3;

            Stencils.Emplace(
                Positions[Indices[i  ]],
                Positions[Indices[i+1]],
                Positions[Indices[i+2]],
                FillType
                );
        }
    }

    UFUNCTION(BlueprintCallable)
    void AddTrisFromPoints(const TArray<FVector2D>& Points, bool bInversed = false)
    {
        TArray<int32> Indices;
        UPMUUtilityLibrary::PolyClip(Points, Indices, bInversed);

        const int32 TriCount = Indices.Num() / 3;

        for (int32 ti=0; ti<TriCount; ++ti)
        {
            int32 i = ti * 3;

            Stencils.Emplace(
                FVector(Points[Indices[i  ]], 0.f),
                FVector(Points[Indices[i+1]], 0.f),
                FVector(Points[Indices[i+2]], 0.f),
                FillType
                );
        }
    }

    UFUNCTION(BlueprintCallable)
    void AddTrisFromPoly(const TArray<FPMUPoints>& Polys, bool bInversed = false)
    {
        TArray<int32> Indices;
        TArray<FVector2D> Points;
        FECPolygon Polygon;

        for (const FPMUPoints& PolyPoints : Polys)
        {
            Polygon.Data.Emplace(PolyPoints.Data);
            Points.Reserve(Points.Num() + PolyPoints.Data.Num());
        }

        for (const FPMUPoints& PolyPoints : Polys)
        {
            Points.Append(PolyPoints.Data);
        }

        FECUtils::Earcut(Polygon, Indices, bInversed);
        const int32 TriCount = Indices.Num() / 3;

        Stencils.Reserve(TriCount);

        for (int32 ti=0; ti<TriCount; ++ti)
        {
            int32 i = ti * 3;

            Stencils.Emplace(
                FVector(Points[Indices[i  ]], 0.f),
                FVector(Points[Indices[i+1]], 0.f),
                FVector(Points[Indices[i+2]], 0.f),
                FillType
                );
        }
    }
};
