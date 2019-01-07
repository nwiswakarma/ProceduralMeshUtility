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
#include "DynamicMeshBuilder.h"
#include "PMUMeshTypes.generated.h"

// Struct used to specify a tangent vector for a vertex
// The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member.
USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshTangent
{
	GENERATED_BODY()

	// Direction of X tangent for this vertex
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Tangent)
	FVector TangentX;

	// Whether we should flip the Y tangent when we compute it using cross product
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Tangent)
	bool bFlipTangentY;

	FPMUMeshTangent()
		: TangentX(1.f, 0.f, 0.f)
		, bFlipTangentY(false)
	{}

	FPMUMeshTangent(float X, float Y, float Z)
		: TangentX(X, Y, Z)
		, bFlipTangentY(false)
	{}

	FPMUMeshTangent(FVector InTangentX, bool bInFlipTangentY)
		: TangentX(InTangentX)
		, bFlipTangentY(bInFlipTangentY)
	{}
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshVertex
{
	GENERATED_BODY()

	// Vertex position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vertex)
	FVector Position;

	// Vertex normal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vertex)
	FVector Normal;

	// Vertex color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vertex)
	FVector2D UV0;

	// Vertex color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vertex)
	FColor Color;

	FPMUMeshVertex()
		: Position(0.f)
		, Normal(0.f)
		, Color(0, 0, 0)
	{
    }

	FPMUMeshVertex(FVector InPosition)
        : Position(InPosition)
        , Normal(0.f)
        , Color(0, 0, 0)
    {
    }

	FPMUMeshVertex(FVector InPosition, FVector InNormal)
        : Position(InPosition)
        , Normal(InNormal)
        , Color(0, 0, 0)
    {
    }

	FPMUMeshVertex(FVector InPosition, FVector InNormal, FColor InColor)
        : Position(InPosition)
        , Normal(InNormal)
        , Color(InColor)
    {
    }

	FPMUMeshVertex(const FPMUMeshVertex& InVertex)
        : Position(InVertex.Position)
        , Normal(InVertex.Normal)
        , Color(InVertex.Color)
    {
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSection
{
	GENERATED_BODY()

	// Vertex buffer for this section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FPMUMeshVertex> VertexBuffer;

	// Index buffer for this section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<int32> IndexBuffer;

	// Local bounding box of section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	FBox LocalBox;

	// Should we display this section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	bool bUsePositionAsUV;

	// Should we build collision data for triangles in this section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	bool bEnableCollision;

	// Should we display this section
	UPROPERTY(BlueprintReadWrite, Category=Section)
	bool bSectionVisible;

	FPMUMeshSection()
		: LocalBox(ForceInitToZero)
		, bEnableCollision(false)
		, bUsePositionAsUV(true)
		, bSectionVisible(true)
	{}

	// Reset this section, clear all mesh info
	void Reset()
	{
		VertexBuffer.Empty();
		IndexBuffer.Empty();
		LocalBox.Init();
		bEnableCollision = false;
		bUsePositionAsUV = true;
		bSectionVisible = true;
	}

	void ClearGeometry()
	{
		VertexBuffer.Empty();
		IndexBuffer.Empty();
		LocalBox.Init();
	}

	void Shrink()
	{
		VertexBuffer.Shrink();
		IndexBuffer.Shrink();
    }

    FORCEINLINE int32 GetVertexCount() const
    {
        return VertexBuffer.Num();
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSectionBufferData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FVector> PositionBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FVector> NormalBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FVector> TangentBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FVector2D> UVBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<FColor> ColorBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	TArray<int32> IndexBuffer;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	FBox Bounds;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	bool bEnableCollision;

	UPROPERTY(BlueprintReadWrite, Category=Section)
	bool bSectionVisible;

	FPMUMeshSectionBufferData()
		: Bounds(ForceInitToZero)
		, bEnableCollision(false)
		, bSectionVisible(true)
	{
    }

	void Empty()
	{
        PositionBuffer.Empty();
        NormalBuffer.Empty();
        TangentBuffer.Empty();
        UVBuffer.Empty();
        ColorBuffer.Empty();
        IndexBuffer.Empty();
	}

	void Shrink()
	{
        PositionBuffer.Shrink();
        NormalBuffer.Shrink();
        TangentBuffer.Shrink();
        UVBuffer.Shrink();
        ColorBuffer.Shrink();
        IndexBuffer.Shrink();
    }

    FORCEINLINE int32 GetVertexCount() const
    {
        return PositionBuffer.Num();
    }

    FORCEINLINE int32 GetIndexCount() const
    {
        return IndexBuffer.Num();
    }

    FORCEINLINE int32 GetPrimitiveCount() const
    {
        return GetIndexCount() / 3;
    }

    FORCEINLINE bool HasGeometry() const
    {
        return GetVertexCount() >= 3 && GetIndexCount() >= 3;
    }

    FORCEINLINE bool HasNormalBuffer() const
    {
        return NormalBuffer.Num() == PositionBuffer.Num();
    }

    FORCEINLINE bool HasTangentBuffer() const
    {
        return TangentBuffer.Num() == PositionBuffer.Num();
    }

    FORCEINLINE bool HasUVBuffer() const
    {
        return UVBuffer.Num() == PositionBuffer.Num();
    }

    FORCEINLINE bool HasColorBuffer() const
    {
        return ColorBuffer.Num() == PositionBuffer.Num();
    }

    FORCEINLINE bool HasCompleteBuffers() const
    {
        return HasGeometry()
            && HasNormalBuffer()
            && HasTangentBuffer()
            && HasUVBuffer()
            && HasColorBuffer();
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshLOD
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category=LOD)
    TArray<FPMUMeshSection> Sections;

	UPROPERTY()
    TMap<uint64, int32> SectionMap;

    FORCEINLINE bool HasSection(int32 SectionIndex) const
    {
        return Sections.IsValidIndex(SectionIndex);
    }

    FORCEINLINE bool HasMapped(uint64 MappedIndex) const
    {
        return SectionMap.Contains(MappedIndex) ? HasSection(SectionMap.FindChecked(MappedIndex)) : false;
    }

    FORCEINLINE FPMUMeshSection* GetSectionSafe(int32 SectionIndex)
    {
        return HasSection(SectionIndex) ? &GetSection(SectionIndex) : nullptr;
    }

    FORCEINLINE FPMUMeshSection* GetMappedSafe(uint64 MappedIndex)
    {
        return HasMapped(MappedIndex) ? &GetMapped(MappedIndex) : nullptr;
    }

    FORCEINLINE FPMUMeshSection& GetSection(int32 SectionIndex)
    {
        return Sections[SectionIndex];
    }

    FORCEINLINE FPMUMeshSection& GetMapped(uint64 MappedIndex)
    {
        return Sections[SectionMap.FindChecked(MappedIndex)];
    }

    FORCEINLINE int32 GetNumSections() const
    {
        return Sections.Num();
    }

    FORCEINLINE FBox GetLocalBounds() const
    {
        FBox LocalBox(ForceInitToZero);
        
        for (const FPMUMeshSection& Section : Sections)
        {
            LocalBox += Section.LocalBox;
        }
        
        return LocalBox.IsValid ? LocalBox : FBox(FVector::ZeroVector, FVector::ZeroVector); // fallback to reset box sphere bounds
    }

    void ClearAllMeshSections()
    {
        Sections.Empty();
        SectionMap.Empty();
    }

    void CreateMappedSections(const TArray<uint64>& SectionIds)
    {
        // Resize section container
        Sections.SetNum(SectionIds.Num(), true);

        // Map sections
        for (int32 i=0; i<SectionIds.Num(); ++i)
        {
            SectionMap.Emplace(SectionIds[i], i);
        }
    }
};

struct FPMUMeshSectionResourceBuffer
{
    template<class FResourceType>
    class TBufferResourceArray : public TResourceArray<FResourceType, VERTEXBUFFER_ALIGNMENT>
    {
    public:
        virtual void Discard() override
        {
            // TResourceArray<>::Discard() only discard array content if not in editor or commandlet,
            // override to always discard if cpu access is not required
            if (! GetAllowCPUAccess())
            {
                Empty();
            }
        }
    };

    class FSectionVertexBuffer : public FVertexBuffer
    {
    public:

        typedef TBufferResourceArray<FDynamicMeshVertex> FResourceArray;

        FResourceArray ResourceArray;

        virtual void InitRHI() override
        {
            FRHIResourceCreateInfo CreateInfo(&ResourceArray);
            VertexBufferRHI = RHICreateVertexBuffer(ResourceArray.GetResourceDataSize(), BUF_Static, CreateInfo);
        }
    };

    class FSectionIndexBuffer : public FIndexBuffer
    {
    public:

        typedef TBufferResourceArray<uint32> FResourceArray;

        FResourceArray ResourceArray;

        virtual void InitRHI() override
        {
            FRHIResourceCreateInfo CreateInfo;

            const int32 ArrayCount = ResourceArray.Num();
            const int32 TypeSize   = ResourceArray.GetTypeSize();
            const int32 BufferSize = ArrayCount * TypeSize;

            void* Buffer = nullptr;
            IndexBufferRHI = RHICreateAndLockIndexBuffer(TypeSize, BufferSize, BUF_Static, CreateInfo, Buffer);
            FMemory::Memcpy(Buffer, ResourceArray.GetData(), BufferSize);
            RHIUnlockIndexBuffer(IndexBufferRHI);
        }
    };

    FORCEINLINE FSectionVertexBuffer& GetVB()
    {
        return VertexBuffer;
    }

    FORCEINLINE FSectionIndexBuffer& GetIB()
    {
        return IndexBuffer;
    }

    FORCEINLINE FSectionVertexBuffer::FResourceArray& GetVBArray()
    {
        return VertexBuffer.ResourceArray;
    }

    FORCEINLINE FSectionIndexBuffer::FResourceArray& GetIBArray()
    {
        return IndexBuffer.ResourceArray;
    }

	FSectionVertexBuffer VertexBuffer;
	FSectionIndexBuffer  IndexBuffer;
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSectionResource
{
	GENERATED_BODY()

	FPMUMeshSectionResourceBuffer Buffer;

    int32 VertexCount;
    int32 IndexCount;

	FBox LocalBounds;
	bool bEnableCollision;
	bool bSectionVisible;

	FPMUMeshSectionResource()
		: LocalBounds(ForceInitToZero)
		, bEnableCollision(false)
		, bSectionVisible(true)
	{
    }

    FORCEINLINE int32 GetVertexCount() const
    {
        return VertexCount;
    }

    FORCEINLINE int32 GetIndexCount() const
    {
        return IndexCount;
    }
};

USTRUCT(BlueprintType)
struct PROCEDURALMESHUTILITY_API FPMUMeshSectionResourceRef
{
	GENERATED_BODY()

	FPMUMeshSectionResource* SectionPtr;

	FPMUMeshSectionResourceRef()
        : SectionPtr(nullptr)
    {
    }

	FPMUMeshSectionResourceRef(FPMUMeshSectionResource& Section)
        : SectionPtr(&Section)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return SectionPtr != nullptr;
    }
};
