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

#include "Shaders/PMUUtilityShaderLibrary.h"

#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "PipelineStateCache.h"
#include "ShaderParameterUtils.h"
#include "ScreenRendering.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ProceduralMeshUtility.h"
#include "Rendering/PMURenderingLibrary.h"
#include "RHI/PMURHIUtilityLibrary.h"
#include "Shaders/PMUShaderDefinitions.h"
#include "Shaders/Filters/PMUFilterShaderBase.h"

#include "EarcutTypes.h"
#include "AJCUtilityLibrary.h"

static TGlobalResource<FPMUFilterVertexBuffer> GPMUFilterVertexBuffer;

void FPMUFilterVertexBuffer::InitRHI()
{
    // Create a static vertex buffer
    FRHIResourceCreateInfo CreateInfo;
    VertexBufferRHI = RHICreateVertexBuffer(sizeof(FScreenVertex) * 4, BUF_Static, CreateInfo);
    void* DataPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FScreenVertex) * 4, RLM_WriteOnly);
    // Generate the vertices used
    FScreenVertex* Vertices = reinterpret_cast<FScreenVertex*>(DataPtr);
	Vertices[0] = { { -1.0f,  1.0f }, { 0.f, 1.f } };
	Vertices[1] = { {  1.0f,  1.0f }, { 1.f, 1.f } };
	Vertices[2] = { { -1.0f, -1.0f }, { 0.f, 0.f } };
	Vertices[3] = { {  1.0f, -1.0f }, { 1.f, 0.f } };
    RHIUnlockVertexBuffer(VertexBufferRHI);
}

class FPMUUtilityShaderLibraryDrawScreenVS : public FPMUBaseVertexShader
{
    typedef FPMUBaseVertexShader FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(FPMUUtilityShaderLibraryDrawScreenVS, Global, true)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

class FPMUUtilityShaderLibraryDrawScreenMaterialPS : public FPMUBasePixelMaterialShader
{
public:

    typedef FPMUBasePixelMaterialShader FBaseType;

    DECLARE_SHADER_TYPE(FPMUUtilityShaderLibraryDrawScreenMaterialPS, Material);

public:

    static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material)
    {
		return Material->IsUIMaterial() || Material->GetMaterialDomain() == MD_PostProcess;
    }

    static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
    {
        FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("PMU_USE_SOURCE_MAP"), 1);
        OutEnvironment.SetDefine(TEXT("PMU_FILTER_HAS_VALID_MATERIAL_DOMAIN"), ShouldCompilePermutation(Platform, Material) ? 1 : 0);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(FPMUUtilityShaderLibraryDrawScreenMaterialPS)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "SourceMap", SourceMap
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "SourceMapSampler", SourceMapSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

IMPLEMENT_SHADER_TYPE(, FPMUUtilityShaderLibraryDrawScreenVS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUUtilityShaderLibraryDrawGeometryVSPS.usf"), TEXT("DrawScreenVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FPMUUtilityShaderLibraryDrawScreenMaterialPS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUUtilityShaderLibraryDrawScreenMaterialPS.usf"), TEXT("MainPS"), SF_Pixel);


class FPMUUtilityShaderLibraryDrawGeometryVS : public FPMUBaseVertexShader
{
    typedef FPMUBaseVertexShader FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(FPMUUtilityShaderLibraryDrawGeometryVS, Global, true)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

class FPMUUtilityShaderLibraryDrawGeometryPS : public FPMUBasePixelShader
{
    typedef FPMUBasePixelShader FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(FPMUUtilityShaderLibraryDrawGeometryPS, Global, true)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

IMPLEMENT_SHADER_TYPE(, FPMUUtilityShaderLibraryDrawGeometryVS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUUtilityShaderLibraryDrawGeometryVSPS.usf"), TEXT("DrawGeometryVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FPMUUtilityShaderLibraryDrawGeometryPS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUUtilityShaderLibraryDrawGeometryVSPS.usf"), TEXT("DrawGeometryPS"), SF_Pixel);

class FPMUUtilityShaderLibraryGetTexturePointValues : public FPMUBaseComputeShader<256,1,1>
{
public:

    typedef FPMUBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(FPMUUtilityShaderLibraryGetTexturePointValues, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(FPMUUtilityShaderLibraryGetTexturePointValues)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "SrcTexture", SrcTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "samplerSrcTexture", SrcTextureSampler
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "PointData", PointData
        )

    PMU_DECLARE_SHADER_PARAMETERS_1(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutValueData", OutValueData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_Dim",        Params_Dim,
        "_PointCount", Params_PointCount
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUUtilityShaderLibraryGetTexturePointValues, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUUtilityShaderLibraryTexturePointValues.usf"), TEXT("MainCS"), SF_Compute);

FVertexBufferRHIRef& UPMUUtilityShaderLibrary::GetFilterVertexBuffer()
{
    return GPMUFilterVertexBuffer.VertexBufferRHI;
}

void UPMUUtilityShaderLibrary::DrawPolyPoints(
    UObject* WorldContextObject,
    UTextureRenderTarget2D* RenderTarget,
    FIntPoint DrawSize,
    const TArray<FVector2D>& Points,
    float ExpandRadius
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* TextureResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    if (Points.Num() < 3)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, VERTEX COUNT < 3"));
        return;
    }

    if (DrawSize.X <= 0 || DrawSize.Y <= 0)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, INVALID DRAW SIZE"));
        return;
    }

    TextureResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! TextureResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::DrawPolyPoints() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* TextureResource;
        TArray<FVector> Vertices;
        TArray<int32>   Indices;
    };

    const FBox2D MapBounds(FVector2D(0,0), FVector2D(DrawSize.X, DrawSize.Y));
    const FVector2D Center(MapBounds.GetCenter());
    const FVector2D Extents(MapBounds.GetExtent());
    const FVector2D ExtentsInv = FVector2D::UnitVector / Extents;

    const int32 PointCount = Points.Num();

    // Construct shader vertex data

    if (FMath::IsNearlyZero(ExpandRadius))
    {
        TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
        RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
        RenderParameter->TextureResource = TextureResource;

        TArray<FVector>& Vertices(RenderParameter->Vertices);
        TArray<int32>& Indices(RenderParameter->Indices);

        FECUtils::Earcut(Points, Indices, false);

        Vertices.SetNumUninitialized(PointCount);

        for (int32 i=0; i<PointCount; ++i)
        {
            FVector2D Point = (FVector2D(Points[i])-Center) * ExtentsInv;
            Vertices[i] = FVector(Point, 1.f);
        }

        // Enqueue render command

        ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
            PMUUtilityShaderLibrary_DrawPolyPoints,
            TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
            {
                UPMUUtilityShaderLibrary::DrawGeometry_RT(
                    RHICmdList,
                    RenderParameterRef->FeatureLevel,
                    RenderParameterRef->TextureResource,
                    RenderParameterRef->Vertices,
                    RenderParameterRef->Indices,
                    true
                    );
            } );
    }
    else
    {
        // Generate offset points

        ExpandRadius = -FMath::Abs(ExpandRadius);

        FAJCVectorPath PointVectorPath({ Points, true });
        FAJCPathRef PointPath;
        FAJCUtilityLibrary::ConvertVectorPathToPointPath(PointVectorPath, PointPath);

        FAJCOffsetClipperConfig OffsetConfig;
        OffsetConfig.Delta = ExpandRadius;
        OffsetConfig.ArcTolerance = 30.f;

        FAJCPointPaths OffsetPointPaths;
        FAJCUtilityLibrary::OffsetClip(PointPath, OffsetConfig, OffsetPointPaths, true);

        FAJCVectorPaths OffsetVectorPaths;
        FAJCUtilityLibrary::ConvertPointPathsToVectorPaths(OffsetPointPaths, OffsetVectorPaths);

        // Generate vertices

        TArray< TArray<FVector> > VertexGroups;
        VertexGroups.SetNum(OffsetVectorPaths.Num() + 1);

        VertexGroups[0].SetNumUninitialized(Points.Num());

        for (int32 i=0; i<Points.Num(); ++i)
        {
            FVector2D Point = (FVector2D(Points[i])-Center) * ExtentsInv;
            VertexGroups[0][i] = FVector(Point, 0.f);
        }

        for (int32 i=0; i<OffsetVectorPaths.Num(); ++i)
        {
            const int32 vgi = i+1;

            TArray<FVector2D>& OffsetVectorPath(OffsetVectorPaths[i]);
            TArray<FVector>&   Vertices(VertexGroups[vgi]);

            Vertices.SetNumUninitialized(OffsetVectorPath.Num());

            for (int32 vi=0; vi<OffsetVectorPath.Num(); ++vi)
            {
                FVector2D Point = (FVector2D(OffsetVectorPath[vi])-Center) * ExtentsInv;
                Vertices[vi] = FVector(Point, 1.f);
            }
        }

        // Draw inner geometry

        for (int32 i=0; i<OffsetVectorPaths.Num(); ++i)
        {
            // Generate render parameter

            TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
            RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
            RenderParameter->TextureResource = TextureResource;

            TArray<FVector2D>& OffsetVectorPath(OffsetVectorPaths[i]);
            TArray<FVector>&   Vertices(RenderParameter->Vertices);
            TArray<int32>&     Indices(RenderParameter->Indices);

            FECUtils::Earcut(OffsetVectorPath, Indices, false);

            Vertices = VertexGroups[i+1];

            // Enqueue render command

            ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
                PMUUtilityShaderLibrary_DrawPolyPoints,
                TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
                {
                    UPMUUtilityShaderLibrary::DrawGeometry_RT(
                        RHICmdList,
                        RenderParameterRef->FeatureLevel,
                        RenderParameterRef->TextureResource,
                        RenderParameterRef->Vertices,
                        RenderParameterRef->Indices,
                        false
                        );
                } );
        }

        // Draw inner geometry

        {
            // Generate render parameter

            TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
            RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
            RenderParameter->TextureResource = TextureResource;

            TArray<FVector>& Vertices(RenderParameter->Vertices);
            TArray<int32>&   Indices(RenderParameter->Indices);

            Vertices = VertexGroups[0];

            FECPolygon TriangulationPolygon;
            TriangulationPolygon.Data.Reserve(VertexGroups.Num());
            TriangulationPolygon.Data.Emplace(Points);

            for (int32 i=0; i<OffsetVectorPaths.Num(); ++i)
            {
                const int32 idx = i+1;
                Vertices.Append(VertexGroups[idx]);
                TriangulationPolygon.Data.Emplace(OffsetVectorPaths[i]);
            }

            FECUtils::Earcut(TriangulationPolygon, Indices, false);

            // Enqueue render command

            ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
                PMUUtilityShaderLibrary_DrawPolyPoints,
                TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
                {
                    UPMUUtilityShaderLibrary::DrawGeometry_RT(
                        RHICmdList,
                        RenderParameterRef->FeatureLevel,
                        RenderParameterRef->TextureResource,
                        RenderParameterRef->Vertices,
                        RenderParameterRef->Indices,
                        true
                        );
                } );
        }
    }
}

void UPMUUtilityShaderLibrary::DrawGeometry_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FTextureRenderTarget2DResource* TextureResource,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Indices,
    bool bResolveRenderTarget
    )
{
    check(IsInRenderingThread());

    if (! TextureResource)
    {
        return;
    }

    const int32 VCount = Vertices.Num();
    const int32 ICount = Indices.Num();
    const int32 TCount = ICount / 3;

    check(VCount >= 3);
    check(ICount >= 3);
    check(TCount >= 1);

    check(Vertices.Num() == VCount);

    // Set render target

    FTexture2DRHIRef CurrentTexture = TextureResource->GetRenderTargetTexture();
    SetRenderTarget(RHICmdList, CurrentTexture, FTextureRHIRef());

    // Prepare graphics pipelane

    TShaderMapRef<FPMUUtilityShaderLibraryDrawGeometryVS> VSShader(GetGlobalShaderMap(FeatureLevel));
    TShaderMapRef<FPMUUtilityShaderLibraryDrawGeometryPS> PSShader(GetGlobalShaderMap(FeatureLevel));

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleList;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector3();
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PSShader);
    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Draw primitives

    FPMURHIUtilityLibrary::DrawIndexedPrimitiveVolatile(
        RHICmdList,
        PT_TriangleList,
        0,
        VCount,
        TCount,
        Indices.GetData(),
        Indices.GetTypeSize(),
        Vertices.GetData(),
        Vertices.GetTypeSize()
        );

    // Resolve render target

    if (bResolveRenderTarget)
    {
        RHICmdList.CopyToResolveTarget(
            TextureResource->GetRenderTargetTexture(),
            TextureResource->TextureRHI,
            FResolveParams()
            );
    }

    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}

void UPMUUtilityShaderLibrary::GetTexturePointValues(
    UObject* WorldContextObject,
    FPMUShaderTextureParameterInput TextureInput,
    const FVector2D Dimension,
    const TArray<FVector2D>& Points,
    TArray<FLinearColor>& Values
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FPMUShaderTextureParameterInputResource TextureResource(TextureInput.GetResource_GT());

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::GetTexturePointValues() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::GetTexturePointValues() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (Points.Num() < 1)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::GetTexturePointValues() ABORTED, EMPTY POINTS"));
        return;
    }

    if (Dimension.X <= 0 || Dimension.Y <= 0)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::GetTexturePointValues() ABORTED, INVALID DRAW SIZE"));
        return;
    }

    if (! TextureResource.HasValidResource())
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::GetTexturePointValues() ABORTED, INVALID TEXTURE INPUT"));
        return;
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FPMUShaderTextureParameterInputResource TextureResource;
        FVector2D Dimension;
        const TArray<FVector2D>* Points;
        TArray<FLinearColor>* Values;
    };

    TSharedRef<FRenderParameter> RenderParameter(new FRenderParameter);
    RenderParameter->FeatureLevel = World->Scene->GetFeatureLevel();
    RenderParameter->TextureResource = TextureResource;
    RenderParameter->Dimension = Dimension;
    RenderParameter->Points = &Points;
    RenderParameter->Values = &Values;

    Values.SetNumZeroed(Points.Num(), true);

    // Enqueue render command

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        PMUUtilityShaderLibrary_DrawPolyPoints,
        TSharedRef<FRenderParameter>, RenderParameterRef, RenderParameter,
        {
            UPMUUtilityShaderLibrary::GetTexturePointValues_RT(
                RHICmdList,
                RenderParameterRef->FeatureLevel,
                RenderParameterRef->TextureResource,
                RenderParameterRef->Dimension,
                *RenderParameterRef->Points,
                *RenderParameterRef->Values
                );
        } );
}

void UPMUUtilityShaderLibrary::GetTexturePointValues_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FPMUShaderTextureParameterInputResource& TextureInput,
    const FVector2D Dimension,
    const TArray<FVector2D>& Points,
    TArray<FLinearColor>& Values
    )
{
    check(IsInRenderingThread());
    check(Dimension.X > 0.f);
    check(Dimension.Y > 0.f);

    FTexture2DRHIParamRef TextureResource = TextureInput.GetTextureParamRef_RT();

    if (! TextureResource)
    {
        return;
    }

    const int32 PointCount = Points.Num();

    typedef TResourceArray<FAlignedVector2D, VERTEXBUFFER_ALIGNMENT> FPointData;

    FPointData PointCPUData(false);
    PointCPUData.SetNumUninitialized(PointCount);

    for (int32 i=0; i<PointCount; ++i)
    {
        PointCPUData[i] = Points[i];
    }
    
    FPMURWBufferStructured PointData;
    PointData.Initialize(
        sizeof(FPointData::ElementType),
        PointCount,
        &PointCPUData,
        BUF_Static,
        TEXT("PointData")
        );
    
    FPMURWBufferStructured ValueData;
    ValueData.Initialize(
        sizeof(FLinearColor),
        PointCount,
        BUF_Static,
        TEXT("ValueData")
        );

    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());

    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();

    TShaderMapRef<FPMUUtilityShaderLibraryGetTexturePointValues> ComputeShader(GetGlobalShaderMap(FeatureLevel));
    ComputeShader->SetShader(RHICmdList);
    ComputeShader->BindTexture(RHICmdList, TEXT("SrcTexture"), TEXT("samplerSrcTexture"), TextureResource, TextureSampler);
    ComputeShader->BindSRV(RHICmdList, TEXT("PointData"), PointData.SRV);
    ComputeShader->BindUAV(RHICmdList, TEXT("OutValueData"), ValueData.UAV);
    ComputeShader->SetParameter(RHICmdList, TEXT("_Dim"), Dimension);
    ComputeShader->SetParameter(RHICmdList, TEXT("_PointCount"), PointCount);
    ComputeShader->DispatchAndClear(RHICmdList, PointCount, 1, 1);

    Values.SetNumUninitialized(PointCount, true);

    void* ValueDataPtr = RHILockStructuredBuffer(ValueData.Buffer, 0, ValueData.Buffer->GetSize(), RLM_ReadOnly);
    FMemory::Memcpy(Values.GetData(), ValueDataPtr, ValueData.Buffer->GetSize());
    RHIUnlockStructuredBuffer(ValueData.Buffer);

#if 0
    for (int32 i=0; i<PointCount; ++i)
    {
        UE_LOG(UntPMU,Warning, TEXT("Values[%d]: %s"), i, *Values[i].ToString());
    }
#endif
}

void UPMUUtilityShaderLibrary::ApplyFilter(
    UObject* WorldContextObject,
    UPMUFilterShaderBase* FilterShader,
    UTextureRenderTarget2D* RenderTarget,
    UTextureRenderTarget2D* SwapTarget,
    UGWTTickEvent* CallbackEvent
    )
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* RenderTargetResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    if (! IsValid(FilterShader))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID FILTER SHADER"));
        return;
    }

    RenderTargetResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! RenderTargetResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    // Get swap render target resource if present

    FTextureRenderTarget2DResource* SwapTargetResource = nullptr;

    if (IsValid(SwapTarget))
    {
        if (UPMURenderingLibrary::IsRenderTargetDimensionAndFormatEqual(RenderTarget, SwapTarget))
        {
            SwapTargetResource = static_cast<FTextureRenderTarget2DResource*>(SwapTarget->GameThread_GetRenderTargetResource());
        }
        else
        {
            UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() INVALID SWAP RENDER TARGET DIMENSION / FORMAT"));
        }
    }

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* RenderTargetResource;
        FTextureRenderTarget2DResource* SwapTargetResource;
        TSharedPtr<FPMUFilterRenderThreadWorkerBase> FilterShaderWorker;
        FGWTTickEventRef CallbackRef;
    };

    FRenderParameter RenderParameter = {
        World->Scene->GetFeatureLevel(),
        RenderTargetResource,
        SwapTargetResource,
        FilterShader->GetRenderThreadWorker(),
        { CallbackEvent }
        };

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        PMUUtilityShaderLibrary_ApplyFilter,
        FRenderParameter, RenderParameter, RenderParameter,
        {
            UPMUUtilityShaderLibrary::ApplyFilter_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                RenderParameter.RenderTargetResource,
                RenderParameter.SwapTargetResource,
                RenderParameter.FilterShaderWorker.Get()
                );
            RenderParameter.CallbackRef.EnqueueCallback();
        } );
}

void UPMUUtilityShaderLibrary::ApplyFilter_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FTextureRenderTarget2DResource* RenderTargetResource,
    FTextureRenderTarget2DResource* SwapTargetResource,
    FPMUFilterRenderThreadWorkerBase* FilterShaderWorker
    )
{
    check(IsInRenderingThread());

    if (! RenderTargetResource || ! FilterShaderWorker)
    {
        return;
    }

    FTexture2DRHIRef SwapTextureRTV;
    FTexture2DRHIRef SwapTextureRSV;
    FTexture2DRHIRef TargetTexture = RenderTargetResource->GetRenderTargetTexture();

    if (! TargetTexture.IsValid())
    {
        return;
    }

    const int32 RepeatCount = FilterShaderWorker->GetRepeatCount();
    const bool bIsValidMultiPass = FilterShaderWorker->IsMultiPass() && RepeatCount > 0;

    // Create swap texture for multi-pass filter
    if (bIsValidMultiPass)
    {
        if (SwapTargetResource)
        {
            if (UPMURenderingLibrary::IsRenderTargetSwapCapable_RT(TargetTexture, SwapTargetResource->GetRenderTargetTexture()))
            {
                SwapTextureRTV = SwapTargetResource->GetRenderTargetTexture();
            }
            else
            {
                UE_LOG(LogPMU,Warning, TEXT("SUPPLIED SWAP RENDER TARGET IS NOT SWAP CAPABLE (RT)"));
                FDebug::DumpStackTraceToLog();
            }
        }

        if (! SwapTextureRTV.IsValid())
        {
            const uint32 FlagsMask = ~(TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_ShaderResource);
            FRHIResourceCreateInfo CreateInfo(TargetTexture->GetClearBinding());
            RHICreateTargetableShaderResource2D(
                TargetTexture->GetSizeX(),
                TargetTexture->GetSizeY(),
                TargetTexture->GetFormat(),
                1,
                TargetTexture->GetFlags() & FlagsMask,
                TexCreate_RenderTargetable,
                false,
                CreateInfo,
                SwapTextureRTV,
                SwapTextureRSV,
                TargetTexture->GetNumSamples()
                );
        }
    }

    // Prepare render target texture and resolve target
    FTextureRHIParamRef TextureRTV = TargetTexture;
    FTextureRHIParamRef TextureRSV = RenderTargetResource->TextureRHI;

    // Prepare graphics pipelane

    TShaderMapRef<FPMUUtilityShaderLibraryDrawScreenVS> VSShader(GetGlobalShaderMap(FeatureLevel));

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = FilterShaderWorker->GetPixelShader(FeatureLevel);

    // Set render target and set graphics pipeline

    SetRenderTarget(RHICmdList, TextureRTV, FTextureRHIRef());
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Bind shader parameters
    FilterShaderWorker->BindPixelShaderParameters(RHICmdList, FeatureLevel);

    // Draw first pass

    RHICmdList.SetStreamSource(0, UPMUUtilityShaderLibrary::GetFilterVertexBuffer(), 0);
    RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);

    // Draw multi-pass if required
    if (bIsValidMultiPass)
    {
        FTextureRHIParamRef TextureRTV0 = SwapTextureRTV;
        FTextureRHIParamRef TextureRTV1 = TextureRTV;

        check(TextureRTV0 != nullptr);
        check(TextureRTV1 != nullptr);

        for (int32 i=0; i<RepeatCount; ++i)
        {
            Swap(TextureRTV0, TextureRTV1);

            SetRenderTarget(RHICmdList, TextureRTV1, FTextureRHIRef());
            FilterShaderWorker->BindSwapTexture(RHICmdList, FeatureLevel, TextureRTV0);
            RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);
        }

        // Make sure TextureRTV has the last drawn render target
        if (TextureRTV1 != TextureRTV)
        {
            Swap(TextureRTV1, TextureRTV);
        }
    }

    // Unbind shader parameters
    FilterShaderWorker->UnbindPixelShaderParameters(RHICmdList, FeatureLevel);

    // Resolve render target

    RHICmdList.CopyToResolveTarget(
        TextureRTV,
        TextureRSV,
        FResolveParams()
        );

    // Release render target

    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}

void UPMUUtilityShaderLibrary::ApplyMaterialFilter(
    UObject* WorldContextObject,
    UMaterialInterface* Material,
    int32 RepeatCount,
    FPMUShaderTextureParameterInput SourceTexture,
    UTextureRenderTarget2D* RenderTarget,
    UTextureRenderTarget2D* SwapTarget,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* RenderTargetResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! IsValid(Material))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID MATERIAL"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    RenderTargetResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! RenderTargetResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    // Get swap render target resource if present

    FTextureRenderTarget2DResource* SwapTargetResource = nullptr;

    if (IsValid(SwapTarget))
    {
        if (UPMURenderingLibrary::IsRenderTargetDimensionAndFormatEqual(RenderTarget, SwapTarget))
        {
            SwapTargetResource = static_cast<FTextureRenderTarget2DResource*>(SwapTarget->GameThread_GetRenderTargetResource());
        }
        else
        {
            UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() INVALID SWAP RENDER TARGET DIMENSION / FORMAT"));
        }
    }

    ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
    const FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(0);

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        int32 RepeatCount;
        FPMUShaderTextureParameterInputResource SourceTextureResource;
        FTextureRenderTarget2DResource* RenderTargetResource;
        FTextureRenderTarget2DResource* SwapTargetResource;
        const FMaterialRenderProxy* MaterialRenderProxy;
        FGWTTickEventRef CallbackRef;
    };

    FRenderParameter RenderParameter = {
        FeatureLevel,
        RepeatCount,
        SourceTexture.GetResource_GT(),
        RenderTargetResource,
        SwapTargetResource,
        MaterialRenderProxy,
        { CallbackEvent }
        };

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        PMUUtilityShaderLibrary_ApplyFilter,
        FRenderParameter, RenderParameter, RenderParameter,
        {
            UPMUUtilityShaderLibrary::ApplyMaterialFilter_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                RenderParameter.RepeatCount,
                RenderParameter.SourceTextureResource,
                RenderParameter.RenderTargetResource,
                RenderParameter.SwapTargetResource,
                RenderParameter.MaterialRenderProxy
                );
            RenderParameter.CallbackRef.EnqueueCallback();
        } );
}

void UPMUUtilityShaderLibrary::ApplyMaterialFilter_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    int32 RepeatCount,
    FPMUShaderTextureParameterInputResource& SourceTextureResource,
    FTextureRenderTarget2DResource* RenderTargetResource,
    FTextureRenderTarget2DResource* SwapTargetResource,
    const FMaterialRenderProxy* MaterialRenderProxy
    )
{
    check(IsInRenderingThread());

    const FMaterial* MaterialResource = MaterialRenderProxy->GetMaterial(FeatureLevel);

    if (! RenderTargetResource || ! MaterialRenderProxy || ! MaterialResource)
    {
        return;
    }

    FTexture2DRHIRef SwapTextureRTV;
    FTexture2DRHIRef SwapTextureRSV;
    FTexture2DRHIRef SourceTexture = SourceTextureResource.GetTextureParamRef_RT();
    FTexture2DRHIRef TargetTexture = RenderTargetResource->GetRenderTargetTexture();

    if (! TargetTexture.IsValid())
    {
        return;
    }

    const bool bIsMultiPass = RepeatCount > 0;

    // Prepare swap texture for multi-pass filter
    if (bIsMultiPass)
    {
        // Get render resource texture if swap texture is supplied
        if (SwapTargetResource)
        {
            if (UPMURenderingLibrary::IsRenderTargetSwapCapable_RT(TargetTexture, SwapTargetResource->GetRenderTargetTexture()))
            {
                SwapTextureRTV = SwapTargetResource->GetRenderTargetTexture();
            }
            else
            {
                UE_LOG(LogPMU,Warning, TEXT("SUPPLIED SWAP RENDER TARGET IS NOT SWAP CAPABLE (RT)"));
                FDebug::DumpStackTraceToLog();
            }
        }

        // Create new swap texture if no swap texture is supplied
        if (! SwapTextureRTV.IsValid())
        {
            const uint32 FlagsMask = ~(TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_ShaderResource);
            FRHIResourceCreateInfo CreateInfo(TargetTexture->GetClearBinding());
            RHICreateTargetableShaderResource2D(
                TargetTexture->GetSizeX(),
                TargetTexture->GetSizeY(),
                TargetTexture->GetFormat(),
                1,
                TargetTexture->GetFlags() & FlagsMask,
                TexCreate_RenderTargetable,
                false,
                CreateInfo,
                SwapTextureRTV,
                SwapTextureRSV,
                TargetTexture->GetNumSamples()
                );
        }
    }

    // Prepare render target texture and resolve target
    FTextureRHIParamRef TextureRTV = TargetTexture;
    FTextureRHIParamRef TextureRSV = RenderTargetResource->TextureRHI;

	// Create a new view family

	FSceneViewFamily ViewFamily(
        FSceneViewFamily::ConstructionValues(
            RenderTargetResource,
            nullptr,
            FEngineShowFlags(ESFIM_Game)
            )
            .SetWorldTimes(0.f, 0.f, 0.f)
            .SetGammaCorrection(RenderTargetResource->GetDisplayGamma())
        );

	// Create a new view

	FIntRect ViewRect(FIntPoint(0, 0), RenderTargetResource->GetSizeXY());
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = FMatrix::Identity;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView View(ViewInitOptions);

    // Prepare graphics pipelane

    TShaderMapRef<FPMUUtilityShaderLibraryDrawScreenVS> VSShader(GetGlobalShaderMap(FeatureLevel));

	const FMaterialShaderMap* MaterialShaderMap = MaterialResource->GetRenderingThreadShaderMap();
	auto PSShader = MaterialShaderMap->GetShader<FPMUUtilityShaderLibraryDrawScreenMaterialPS>();

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PSShader->GetPixelShader();

    // Set render target and set graphics pipeline

    SetRenderTarget(RHICmdList, TextureRTV, FTextureRHIRef());
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Bind shader parameters

    FSamplerStateRHIParamRef TextureSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
    PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), SourceTexture, TextureSampler);
    PSShader->SetParameters(
        RHICmdList,
        PSShader->GetPixelShader(),
        MaterialRenderProxy,
        *MaterialRenderProxy->GetMaterial(FeatureLevel),
        View,
        View.ViewUniformBuffer,
        ESceneTextureSetupMode::None
        );

	{
		auto& PrimitiveVS = VSShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		auto& PrimitivePS = PSShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();

		// Uncomment to track down usage of the Primitive uniform buffer
		//check(! PrimitiveVS.IsBound());
		//check(! PrimitivePS.IsBound());

		// To prevent potential shader error (UE-18852 ElementalDemo crashes due to nil constant buffer)
		SetUniformBufferParameter(RHICmdList, VSShader->GetVertexShader(), PrimitiveVS, GIdentityPrimitiveUniformBuffer);
        SetUniformBufferParameter(RHICmdList, PSShader->GetPixelShader() , PrimitivePS, GIdentityPrimitiveUniformBuffer);
	}

    // Draw primitives

    RHICmdList.SetStreamSource(0, UPMUUtilityShaderLibrary::GetFilterVertexBuffer(), 0);
    RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);

    // Draw multi-pass if required
    if (bIsMultiPass)
    {
        FTextureRHIParamRef TextureRTV0 = SwapTextureRTV;
        FTextureRHIParamRef TextureRTV1 = TextureRTV;

        check(TextureRTV0 != nullptr);
        check(TextureRTV1 != nullptr);

        for (int32 i=0; i<RepeatCount; ++i)
        {
            Swap(TextureRTV0, TextureRTV1);

            SetRenderTarget(RHICmdList, TextureRTV1, FTextureRHIRef());

            // Bind swap texture
            PSShader->BindTexture(RHICmdList, TEXT("SourceMap"), TEXT("SourceMapSampler"), TextureRTV0, TextureSampler);

            RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);
        }

        // Make sure TextureRTV has the last drawn render target
        if (TextureRTV1 != TextureRTV)
        {
            Swap(TextureRTV1, TextureRTV);
        }
    }

    // Unbind shader parameters
    PSShader->UnbindBuffers(RHICmdList);

    // Resolve render target

    RHICmdList.CopyToResolveTarget(
        TextureRTV,
        TextureRSV,
        FResolveParams()
        );

    // Release render target

    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}

void UPMUUtilityShaderLibrary::ApplyMaterial(
    UObject* WorldContextObject,
    UMaterialInterface* Material,
    UTextureRenderTarget2D* RenderTarget,
    UGWTTickEvent* CallbackEvent
    )
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    FTextureRenderTarget2DResource* RenderTargetResource = nullptr;

    if (! IsValid(World))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD CONTEXT OBJECT"));
        return;
    }

    if (! World->Scene)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID WORLD SCENE"));
        return;
    }

    if (! IsValid(Material))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID MATERIAL"));
        return;
    }

    if (! IsValid(RenderTarget))
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET"));
        return;
    }

    RenderTargetResource = static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource());

    if (! RenderTargetResource)
    {
        UE_LOG(LogPMU,Warning, TEXT("UPMUUtilityShaderLibrary::ApplyFilter() ABORTED, INVALID RENDER TARGET TEXTURE RESOURCE"));
        return;
    }

    ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
    const FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(0);

    struct FRenderParameter
    {
        ERHIFeatureLevel::Type FeatureLevel;
        FTextureRenderTarget2DResource* RenderTargetResource;
        const FMaterialRenderProxy* MaterialRenderProxy;
        FGWTTickEventRef CallbackRef;
    };

    FRenderParameter RenderParameter = {
        FeatureLevel,
        RenderTargetResource,
        MaterialRenderProxy,
        { CallbackEvent }
        };

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        PMUUtilityShaderLibrary_ApplyFilter,
        FRenderParameter, RenderParameter, RenderParameter,
        {
            UPMUUtilityShaderLibrary::ApplyMaterial_RT(
                RHICmdList,
                RenderParameter.FeatureLevel,
                RenderParameter.RenderTargetResource,
                RenderParameter.MaterialRenderProxy
                );
            RenderParameter.CallbackRef.EnqueueCallback();
        } );
}

void UPMUUtilityShaderLibrary::ApplyMaterial_RT(
    FRHICommandListImmediate& RHICmdList,
    ERHIFeatureLevel::Type FeatureLevel,
    FTextureRenderTarget2DResource* RenderTargetResource,
    const FMaterialRenderProxy* MaterialRenderProxy
    )
{
    check(IsInRenderingThread());

    const FMaterial* MaterialResource = MaterialRenderProxy->GetMaterial(FeatureLevel);

    if (! RenderTargetResource || ! MaterialRenderProxy || ! MaterialResource)
    {
        return;
    }

    // Prepare render target texture and resolve target
    FTextureRHIParamRef TextureRTV = RenderTargetResource->GetRenderTargetTexture();
    FTextureRHIParamRef TextureRSV = RenderTargetResource->TextureRHI;

    if (! TextureRTV || ! TextureRSV)
    {
        return;
    }

	// Create a new view family

	FSceneViewFamily ViewFamily(
        FSceneViewFamily::ConstructionValues(
            RenderTargetResource,
            nullptr,
            FEngineShowFlags(ESFIM_Game)
            )
            .SetWorldTimes(0.f, 0.f, 0.f)
            .SetGammaCorrection(RenderTargetResource->GetDisplayGamma())
        );

	// Create a new view

	FIntRect ViewRect(FIntPoint(0, 0), RenderTargetResource->GetSizeXY());
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = FMatrix::Identity;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView View(ViewInitOptions);

    // Prepare graphics pipelane

    TShaderMapRef<FPMUUtilityShaderLibraryDrawScreenVS> VSShader(GetGlobalShaderMap(FeatureLevel));

	const FMaterialShaderMap* MaterialShaderMap = MaterialResource->GetRenderingThreadShaderMap();
	auto PSShader = MaterialShaderMap->GetShader<FPMUUtilityShaderLibraryDrawScreenMaterialPS>();

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PSShader->GetPixelShader();

    // Set render target and set graphics pipeline

    SetRenderTarget(RHICmdList, TextureRTV, FTextureRHIRef());
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Bind shader parameters

    PSShader->SetParameters(
        RHICmdList,
        PSShader->GetPixelShader(),
        MaterialRenderProxy,
        *MaterialRenderProxy->GetMaterial(FeatureLevel),
        View,
        View.ViewUniformBuffer,
        ESceneTextureSetupMode::None
        );

	{
		auto& PrimitiveVS = VSShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		auto& PrimitivePS = PSShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();

		// Uncomment to track down usage of the Primitive uniform buffer
		//check(! PrimitiveVS.IsBound());
		//check(! PrimitivePS.IsBound());

		// To prevent potential shader error (UE-18852 ElementalDemo crashes due to nil constant buffer)
		SetUniformBufferParameter(RHICmdList, VSShader->GetVertexShader(), PrimitiveVS, GIdentityPrimitiveUniformBuffer);
        SetUniformBufferParameter(RHICmdList, PSShader->GetPixelShader() , PrimitivePS, GIdentityPrimitiveUniformBuffer);
	}

    // Draw primitives

    RHICmdList.SetStreamSource(0, UPMUUtilityShaderLibrary::GetFilterVertexBuffer(), 0);
    RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);

    // Unbind shader parameters
    PSShader->UnbindBuffers(RHICmdList);

    // Resolve render target

    RHICmdList.CopyToResolveTarget(
        TextureRTV,
        TextureRSV,
        FResolveParams()
        );

    // Release render target

    SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}

void UPMUUtilityShaderLibrary::TestGPUCompute(UObject* WorldContextObject, int32 TestCount, UGWTTickEvent* CallbackEvent)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    if (! World || TestCount < 1)
    {
        return;
    }

    TFunction<void()> Command(
        [TestCount]()
        {
            check(IsInRenderingThread());

            FPMURWBufferStructured SourceData;
            FPMURWBufferStructured OffsetData;
            FPMURWBufferStructured SumData;

            typedef TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> FResourceType;

            FResourceType SourceArr(false);
            SourceArr.Init(1, TestCount);

            SourceData.Initialize(
                sizeof(FResourceType::ElementType),
                TestCount,
                &SourceArr,
                BUF_Static,
                TEXT("SourceData")
                );

            const int32 ScanBlockCount = FPMUPrefixSumScan::ExclusiveScan<1>(
                SourceData.SRV,
                sizeof(FResourceType::ElementType),
                TestCount,
                OffsetData,
                SumData,
                BUF_Static
                );

            UE_LOG(LogTemp,Warning, TEXT("ScanBlockCount: %d"), ScanBlockCount);

            if (SumData.Buffer->GetStride() > 0 && ScanBlockCount > 0)
            {
                const int32 SumDataCount = SumData.Buffer->GetSize() / SumData.Buffer->GetStride();

                FResourceType SumArr;
                SumArr.SetNumUninitialized(SumDataCount);

                void* SumDataPtr = RHILockStructuredBuffer(SumData.Buffer, 0, SumData.Buffer->GetSize(), RLM_ReadOnly);
                FMemory::Memcpy(SumArr.GetData(), SumDataPtr, SumData.Buffer->GetSize());
                RHIUnlockStructuredBuffer(SumData.Buffer);

                FResourceType::ElementType ScanSum = SumArr[ScanBlockCount];

                UE_LOG(LogTemp,Warning, TEXT("SumDataCount: %d"), SumDataCount);
                UE_LOG(LogTemp,Warning, TEXT("ScanSum: %d"), ScanSum);
            }
        } );

    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        PMUUtilityShaderLibrary_TestGPUReplication,
        TFunction<void()>, Command, Command,
        {
            Command();
        } );
}
