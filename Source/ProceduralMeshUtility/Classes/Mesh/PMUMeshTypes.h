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
#include "DynamicRHIResourceArray.h"
#include "RenderResource.h"
#include "PMUMeshTypes.generated.h"

struct PROCEDURALMESHUTILITY_API FPMUPackedVertex
{
    FVector Position;
    FVector2D TextureCoordinate;
    FPackedNormal TangentX;
    FPackedNormal TangentZ;
    FColor Color;
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUEdgeVertexPair
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Point0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Point1;
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUStaticMeshSectionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LODIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SectionIndex = 0;
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSection
{
	GENERATED_USTRUCT_BODY()

	/** Position buffer */
	UPROPERTY(BlueprintReadWrite)
	TArray<FVector> Positions;

	/** TangentX buffer */
	UPROPERTY(BlueprintReadWrite)
	TArray<FVector> TangentsX;

	/** TangentZ buffer */
	UPROPERTY(BlueprintReadWrite)
	TArray<FVector> TangentsZ;

	/** Packed Tangents buffer: TangentX = 2*i+0 and TangentZ = 2*i+1 */
	UPROPERTY()
	TArray<uint32> Tangents;

	/** Texture coordinate buffer */
	UPROPERTY(BlueprintReadWrite)
	TArray<FVector2D> UVs;

	/** Color buffer */
	UPROPERTY(BlueprintReadWrite)
	TArray<FColor> Colors;

	/** Index buffer */
	UPROPERTY()
	TArray<uint32> Indices;

	/** Local bounding box of section */
	UPROPERTY(BlueprintReadWrite)
	FBox SectionLocalBox;

	/** Should we build collision data for triangles in this section */
	UPROPERTY(BlueprintReadWrite)
	bool bEnableCollision;

	/** Should we display this section */
	UPROPERTY(BlueprintReadWrite)
	bool bSectionVisible;

	/** Enable fast UV copy if supported */
	UPROPERTY(BlueprintReadWrite)
	bool bEnableFastUVCopy;

	/** Enable fast tangents copy if supported */
	UPROPERTY(BlueprintReadWrite)
	bool bEnableFastTangentsCopy;

	/** Initialize invalid vertex data */
	UPROPERTY(BlueprintReadWrite)
	bool bInitializeInvalidVertexData;

	FPMUMeshSection()
		: SectionLocalBox(ForceInitToZero)
		, bEnableCollision(false)
		, bSectionVisible(true)
		, bEnableFastUVCopy(true)
		, bEnableFastTangentsCopy(true)
        , bInitializeInvalidVertexData(true)
	{
    }

	/** Reset this section, clear all mesh info. */
	void Reset()
	{
        Positions.Empty();
        TangentsX.Empty();
        TangentsZ.Empty();
        Tangents.Empty();
        UVs.Empty();
        Colors.Empty();
        Indices.Empty();

		SectionLocalBox.Init();
		bEnableCollision = false;
		bSectionVisible = true;
	}

    FORCEINLINE bool HasGeometry() const
    {
        return Positions.Num() >= 3 && Indices.Num() >= 3;
    }
};

typedef TSharedPtr<FPMUMeshSection, ESPMode::ThreadSafe> FPMUMeshSectionShared;

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSectionRef
{
	GENERATED_BODY()

	FPMUMeshSection* SectionPtr;

	FPMUMeshSectionRef()
        : SectionPtr(nullptr)
    {
    }

	FPMUMeshSectionRef(FPMUMeshSection& Section)
        : SectionPtr(&Section)
    {
    }

    FORCEINLINE bool HasValidSection() const
    {
        return SectionPtr != nullptr;
    }

    FORCEINLINE bool HasGeometry() const
    {
        return HasValidSection() && SectionPtr->HasGeometry();
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSectionSharedRef
{
	GENERATED_BODY()

	FPMUMeshSectionShared Section;

    FORCEINLINE bool HasValidSection() const
    {
        return Section.IsValid();
    }
};
