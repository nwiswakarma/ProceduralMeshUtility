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

#include "JCV/PMUJCVGeometryUtility.h"

#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RenderResource.h"
#include "PipelineStateCache.h"
#include "ShaderParameterUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ProceduralMeshUtility.h"
#include "JCVDiagramAccessor.h"
#include "Shaders/PMUUtilityShaderLibrary.h"

#if 0
struct FPMUJCVDrawShaderVertex
{
	FVector2D Position;
	FVector4  Values;

    FPMUJCVDrawShaderVertex() = default;

    FPMUJCVDrawShaderVertex(const FVector2D& InPosition, const FVector4& InValues)
        : Position(InPosition)
        , Values(InValues)
    {
    }
};

class FPMUJCVDrawShaderVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FPMUJCVDrawShaderVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FPMUJCVDrawShaderVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FPMUJCVDrawShaderVertex, Values),   VET_Float4, 1, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FPMUJCVDrawShaderVertexDeclaration> GPMUJCVDrawShaderVertexDeclaration;

class FPMUJCVDrawShaderVS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FPMUJCVDrawShaderVS, Global);

public:

    static bool ShouldCache(EShaderPlatform Platform)
    {
        return true;
    }

    FPMUJCVDrawShaderVS() = default;

    FPMUJCVDrawShaderVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
    }
};

IMPLEMENT_SHADER_TYPE(, FPMUJCVDrawShaderVS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUJCVDrawShader.usf"), TEXT("MainVS"), SF_Vertex);

class FPMUJCVDrawShaderPS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FPMUJCVDrawShaderPS, Global);

public:

    FPMUJCVDrawShaderPS() = default;

    explicit FPMUJCVDrawShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
    }

    static bool ShouldCache(EShaderPlatform Platform)
    {
        return RHISupportsComputeShaders(Platform);
    }
};

IMPLEMENT_SHADER_TYPE(, FPMUJCVDrawShaderPS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUJCVDrawShader.usf"), TEXT("MainPS"), SF_Pixel);
#endif

void UPMUJCVGeometryUtility::GenerateDualGeometryByFeature(
    FPMUMeshSection& Section,
    UJCVDiagramAccessor* Accessor,
    uint8 FeatureType,
    int32 FeatureIndex
    )
{
    if (! IsValid(Accessor))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::GenerateDualGeometryByFeature() ABORTED, INVALID ACCESSOR"));
        return;
    }

    FJCVDualGeometry Geometry;
    Accessor->GenerateDualGeometryByFeature(Geometry, FJCVFeatureId(FeatureType, FeatureIndex));

    if (Geometry.Points.Num() >= 3 && Geometry.PolyIndices.Num() >= 3)
    {
        check(Geometry.Points.Num() == Geometry.CellIndices.Num());

        const TArray<FVector2D>& Points(Geometry.Points);
        const int32 PointCount = Points.Num();

        TArray<FPMUMeshVertex>& VertexBuffer(Section.VertexBuffer);
        VertexBuffer.Reserve(PointCount);

        FBox& Bounds(Section.LocalBox);

        for (int32 i=0; i<PointCount; ++i)
        {
            FVector Position(Points[i], 0.f);
            VertexBuffer.Emplace(Position, FVector::UpVector);
            Bounds += Position;
        }

        Section.IndexBuffer = Geometry.PolyIndices;
    }
}

void UPMUJCVGeometryUtility::DrawDualGeometryByFeature(
    UObject* WorldContextObject,
    UTextureRenderTarget2D* RenderTarget,
    UJCVDiagramAccessor* Accessor,
    uint8 FeatureType,
    int32 FeatureIndex
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* TextureResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! IsValid(Accessor))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID ACCESSOR"));
        return;
    }

    if (! Accessor->HasValidMap())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID ACCESSOR MAP"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    TextureResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! TextureResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* TextureResource;
        FJCVDualGeometry Geometry;
        TArray<FVector>  Vertices;
    };

    TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
    RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
    RenderParameter->TextureResource = TextureResource;

    FJCVDualGeometry& Geometry(RenderParameter->Geometry);
    Accessor->GenerateDualGeometryByFeature(Geometry, FJCVFeatureId(FeatureType, FeatureIndex));

    // Not enough elements to draw any geometry, return
    if (Geometry.Points.Num() < 3 && Geometry.PolyIndices.Num() < 3)
    {
        return;
    }

    check(Geometry.Points.Num() == Geometry.CellIndices.Num());
    check(World->Scene != nullptr);

    // Find diagram bounds center and extents for points normalization

    const FJCVDiagramMap& Map(Accessor->GetMap());
    const FBox2D MapBounds = Map.GetBounds();
    const FVector2D Center(MapBounds.GetCenter());
    const FVector2D Extents(MapBounds.GetExtent());

    if (Extents.X < KINDA_SMALL_NUMBER || Extents.Y < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID MAP BOUNDS SIZE"));
        return;
    }

    const FVector2D ExtentsInv = FVector2D::UnitVector / Extents;

    // Construct shader vertex data

    const TArray<FVector2D>& Points(Geometry.Points);
    const TArray<int32>& CellIndices(Geometry.CellIndices);

    const int32 PointCount = Points.Num();

    TArray<FVector>& Vertices(RenderParameter->Vertices);
    Vertices.SetNumUninitialized(PointCount);

    for (int32 i=0; i<PointCount; ++i)
    {
        check(Map.IsValidIndex(CellIndices[i]));

        const FJCVCell& Cell(Map.GetCell(CellIndices[i]));
        const FVector2D& Point(Points[i]);

        Vertices[i] = FVector((Point-Center)*ExtentsInv, Cell.Value);
    }

    // Enqueue render command

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        UPMUJCVGeometryUtility_DrawDualGeometryByFeature,
        TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
        {   
            UPMUUtilityShaderLibrary::DrawGeometry_RT(
                RHICmdList,
                RenderParameterRef->FeatureLevel,
                RenderParameterRef->TextureResource,
                RenderParameterRef->Vertices,
                RenderParameterRef->Geometry.PolyIndices
                );
        } );
}

void UPMUJCVGeometryUtility::DrawPolyGeometryByFeature(
    UObject* WorldContextObject,
    UTextureRenderTarget2D* RenderTarget,
    UJCVDiagramAccessor* Accessor,
    uint8 FeatureType,
    int32 FeatureIndex,
    bool bUseCellAverageValue
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* TextureResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawPolyGeometryByFeature() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! IsValid(Accessor))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawPolyGeometryByFeature() ABORTED, INVALID ACCESSOR"));
        return;
    }

    if (! Accessor->HasValidMap())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawPolyGeometryByFeature() ABORTED, INVALID ACCESSOR MAP"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawPolyGeometryByFeature() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    TextureResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! TextureResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawPolyGeometryByFeature() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* TextureResource;
        FJCVPolyGeometry Geometry;
        TArray<FVector>  Vertices;
    };

    TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
    RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
    RenderParameter->TextureResource = TextureResource;

    FJCVPolyGeometry& Geometry(RenderParameter->Geometry);
    Accessor->GeneratePolyGeometryByFeature(Geometry, FJCVFeatureId(FeatureType, FeatureIndex), bUseCellAverageValue);

    // Not enough elements to draw any geometry, return
    if (Geometry.Points.Num() < 3 && Geometry.PolyIndices.Num() < 3)
    {
        return;
    }

    check(World->Scene != nullptr);

    // Find diagram bounds center and extents for points normalization

    const FJCVDiagramMap& Map(Accessor->GetMap());
    const FBox2D MapBounds = Map.GetBounds();
    const FVector2D Center(MapBounds.GetCenter());
    const FVector2D Extents(MapBounds.GetExtent());

    if (Extents.X < KINDA_SMALL_NUMBER || Extents.Y < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUJCVGeometryUtility::DrawDualGeometryByFeature() ABORTED, INVALID MAP BOUNDS SIZE"));
        return;
    }

    const FVector2D ExtentsInv = FVector2D::UnitVector / Extents;

    // Construct shader vertex data

    const TArray<FVector>& Points(Geometry.Points);
    const TArray<int32>& CellIndices(Geometry.CellIndices);
    const int32 PointCount = Points.Num();

    TArray<FVector>& Vertices(RenderParameter->Vertices);
    Vertices.SetNumUninitialized(PointCount);

    for (int32 i=0; i<PointCount; ++i)
    {
        const FVector& Point(Points[i]);
        Vertices[i] = FVector((FVector2D(Point)-Center) * ExtentsInv, Point.Z);
    }

    // Enqueue render command

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        UPMUJCVGeometryUtility_DrawPolyGeometryByFeature,
        TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
        {   
            UPMUUtilityShaderLibrary::DrawGeometry_RT(
                RHICmdList,
                RenderParameterRef->FeatureLevel,
                RenderParameterRef->TextureResource,
                RenderParameterRef->Vertices,
                RenderParameterRef->Geometry.PolyIndices
                );
        } );
}
