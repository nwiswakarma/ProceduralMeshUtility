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

#include "PMUMarchingSquaresStencilPoly.h"

#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"

#include "ProceduralMeshUtility.h"
#include "RHI/PMURHIUtilityLibrary.h"
#include "RHI/PMURWBuffer.h"
#include "Shaders/PMUShaderDefinitions.h"

#include "EarcutTypes.h"

// VERTEX DECLARATIONS

class FPMUMSqStencilPolyDrawMaskVD : public FRenderResource
{
public:

    FVertexDeclarationRHIRef VertexDeclarationRHI;

    virtual void InitRHI() override
    {
        FVertexDeclarationElementList Elements;
        Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2D)));
        VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
    }

    virtual void ReleaseRHI() override
    {
        VertexDeclarationRHI.SafeRelease();
    }
};

static TGlobalResource<FPMUMSqStencilPolyDrawMaskVD> GPMUMSqStencilPolyDrawMaskVD;

// VERTEX SHADER DEFINITIONS

class FPMUMSqStencilPolyDrawMaskVS : public FPMUBaseVertexShader
{
    typedef FPMUBaseVertexShader FBaseType;

    DECLARE_SHADER_TYPE(FPMUMSqStencilPolyDrawMaskVS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(FPMUMSqStencilPolyDrawMaskVS)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Value,
        FShaderParameter,
        FParameterId,
        "_DrawExts", Params_DrawSize
        )
};

class FPMUMSqStencilPolyDrawEdgeVS : public FPMUBaseVertexShader
{
    typedef FPMUBaseVertexShader FBaseType;

    DECLARE_SHADER_TYPE(FPMUMSqStencilPolyDrawMaskVS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(FPMUMSqStencilPolyDrawEdgeVS)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)

    PMU_DECLARE_SHADER_PARAMETERS_1(
        Value,
        FShaderParameter,
        FParameterId,
        "_DrawExts", Params_DrawSize
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyDrawMaskVS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresPolyStencilDrawVSPS.usf"), TEXT("DrawStencilMaskVS"), SF_Vertex);

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyDrawEdgeVS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresPolyStencilDrawVSPS.usf"), TEXT("DrawStencilEdgeVS"), SF_Vertex);

// PIXEL SHADER DEFINITIONS

class FPMUMSqStencilPolyDrawMaskPS : public FPMUBasePixelShader
{
    typedef FPMUBasePixelShader FBaseType;

    DECLARE_SHADER_TYPE(FPMUMSqStencilPolyDrawMaskVS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(FPMUMSqStencilPolyDrawMaskPS)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

class FPMUMSqStencilPolyDrawEdgePS : public FPMUBasePixelShader
{
    typedef FPMUBasePixelShader FBaseType;

    DECLARE_SHADER_TYPE(FPMUMSqStencilPolyDrawMaskVS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
    }

    PMU_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(FPMUMSqStencilPolyDrawEdgePS)

    PMU_DECLARE_SHADER_PARAMETERS_0(SRV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(UAV,,)
    PMU_DECLARE_SHADER_PARAMETERS_0(Value,,)
};

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyDrawMaskPS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresPolyStencilDrawVSPS.usf"), TEXT("DrawStencilMaskPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyDrawEdgePS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresPolyStencilDrawVSPS.usf"), TEXT("DrawStencilEdgePS"), SF_Pixel);

// COMPUTE SHADER DEFINITIONS

class FPMUMSqStencilPolyWriteVoxelStateCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUMSqStencilPolyWriteVoxelStateCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "StencilTexture", StencilTexture,
        "LineGeomData", LineGeomData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVoxelStateData", OutVoxelStateData,
        "OutDebugTexture",   OutDebugTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_MapDim",   Params_MapDimension,
        "_FillType", Params_FillType
        )
};

class FPMUMSqStencilPolyWriteVoxelFeatureCS : public FPMUBaseComputeShader<16,16,1>
{
    typedef FPMUBaseComputeShader<16,16,1> FBaseType;

    PMU_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FPMUMSqStencilPolyWriteVoxelFeatureCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "VoxelStateData", VoxelStateData,
        "LineGeomData", LineGeomData
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutVoxelFeatureData", OutVoxelFeatureData,
        "OutDebugTexture",     OutDebugTexture
        )

    PMU_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_MapDim",   Params_MapDimension,
        "_FillType", Params_FillType
        )
};

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyWriteVoxelStateCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresStencilPolyCS.usf"), TEXT("VoxelWriteStateKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FPMUMSqStencilPolyWriteVoxelFeatureCS, TEXT("/Plugin/ProceduralMeshUtility/Private/PMUMarchingSquaresStencilPolyCS.usf"), TEXT("VoxelWriteFeatureKernel"), SF_Compute);

void FPMUMarchingSquaresStencilPoly::DrawStencilMask_RT()
{
    check(IsInRenderingThread());
    check(RHICmdListPtr != nullptr);
    check(Dimension.X > 1);
    check(Dimension.Y > 1);

    FRHICommandListImmediate& RHICmdList(*RHICmdListPtr);
    const TArray<FVector2D>& Points(StencilPoints);

    // No valid rtt or not enough geometry to form a poly, abort
    if (Points.Num() < 3)
    {
        return;
    }

    // Construct poly faces

    TArray<int32> Indices;
    FECUtils::Earcut(Points, Indices, false);

    const int32 TriCount = Indices.Num() / 3;

#if 0
    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresStencilPoly::DrawStencilMask_RT() TriCount: %d"), TriCount);

    for (int32 ti=0; ti<TriCount; ++ti)
    {
        int32 i = ti*3;
        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresStencilPoly::DrawStencilMask_RT() Tri[%d]: %d, %d, %d"), ti, Indices[i], Indices[i+1], Indices[i+2]);
    }
#endif

    // Assign render target

    SetRenderTarget(RHICmdList, StencilTextureRTV, FTextureRHIRef());

    // Prepare graphics pipeline

    TShaderMapRef<FPMUMSqStencilPolyDrawMaskVS> VSShader(RHIShaderMap);
    TShaderMapRef<FPMUMSqStencilPolyDrawMaskPS> PSShader(RHIShaderMap);

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
    RHICmdList.SetViewport(0, 0, 0.0f, Dimension.X, Dimension.Y, 1.0f);
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleList;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GPMUMSqStencilPolyDrawMaskVD.VertexDeclarationRHI;
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PSShader);
    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Draw render target

    FVector2D DrawExts = FVector2D(Dimension.X, Dimension.Y) / 2.f;
    VSShader->SetParameter(RHICmdList, TEXT("_DrawExts"), DrawExts);

    FPMURHIUtilityLibrary::DrawIndexedPrimitiveVolatile(
        RHICmdList,
        PT_TriangleList,
        0,
        Points.Num(),
        TriCount,
        Indices.GetData(),
        Indices.GetTypeSize(),
        Points.GetData(),
        Points.GetTypeSize()
        );

    VSShader->UnbindBuffers(RHICmdList);

    // Unassign render target

    SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

void FPMUMarchingSquaresStencilPoly::DrawStencilEdge_RT()
{
    check(IsInRenderingThread());
    check(RHICmdListPtr != nullptr);
    check(Dimension.X > 1);
    check(Dimension.Y > 1);
    check(VoxelCount > 1);

    FRHICommandListImmediate& RHICmdList(*RHICmdListPtr);

    const TArray<FVector2D>& Points(StencilPoints);
    const int32 PointCount = Points.Num();

    const float LineWidth = StencilEdgeRadius;
    const float MiterLimit = LineWidth * 5;

    // No valid rtt target or not enough geometry to form a poly, abort
    if (PointCount < 3 || LineWidth < KINDA_SMALL_NUMBER)
    {
        return;
    }

    const int32 VCount = PointCount * 4;
    const int32 ICount = PointCount * 6;
    const int32 TCount = ICount / 3;
    const int32 LineDataCount = PointCount + 1;

    TArray<FVector> Vertices;
    TArray<int32> Indices;

    Vertices.SetNumUninitialized(VCount);
    Indices.SetNumUninitialized(ICount);

    FLineGeomData LineGeomArr;
    LineGeomArr.SetNumUninitialized(LineDataCount, true);
    LineGeomArr[0] = FAlignedLineGeom();

    for (int32 i=0; i<PointCount; ++i)
    {
        int32 idx = (i > 0) ? (i-1) : (PointCount-1);

        int32 i0 = idx;
        int32 i1 = (i0+1) % PointCount;
        int32 i2 = (i0+2) % PointCount;
        int32 i3 = (i0+3) % PointCount;

        const FVector2D& p0(Points[i0]);
        const FVector2D& p1(Points[i1]);
        const FVector2D& p2(Points[i2]);
        const FVector2D& p3(Points[i3]);

        FVector2D HeadTangent;
        FVector2D TailTangent;
        float HeadWidth = LineWidth * .5f;
        float TailWidth = LineWidth * .5f;

        FVector2D Dir01((p1-p0).GetSafeNormal());
        FVector2D Dir12((p2-p1).GetSafeNormal());
        FVector2D Dir23((p3-p2).GetSafeNormal());

        // Calculate line head miter

        {
            HeadTangent = (Dir01+Dir12).GetSafeNormal();
            FVector2D Ortho(-Dir01.Y, Dir01.X);
            FVector2D Miter(-HeadTangent.Y, HeadTangent.X);
            HeadWidth = FMath::Clamp(HeadWidth / (Miter | Ortho), 0.f, MiterLimit);
        }

        // Calculate line tail miter

        {
            TailTangent = (Dir12+Dir23).GetSafeNormal();
            FVector2D Ortho(-Dir12.Y, Dir12.X);
            FVector2D Miter(-TailTangent.Y, TailTangent.X);
            TailWidth = FMath::Clamp(TailWidth / (Miter | Ortho), 0.f, MiterLimit);
        }

        // Construct line points

        FVector2D TailNormal(-TailTangent.Y, TailTangent.X);
        FVector2D HeadNormal(-HeadTangent.Y, HeadTangent.X);

        FVector2D lp0 = p1 + HeadNormal *  HeadWidth;
        FVector2D lp1 = p1 + HeadNormal * -HeadWidth;
        FVector2D lp2 = p2 + TailNormal *  TailWidth;
        FVector2D lp3 = p2 + TailNormal * -TailWidth;

        int32 lid = i+1;

        int32 vi0 = i*4;
        int32 vi1 = vi0+1;
        int32 vi2 = vi0+2;
        int32 vi3 = vi0+3;

        // Assign vertex position:
        // - XY: Vertex position
        // - Z : Line id

        Vertices[vi0] = FVector(lp0.X, lp0.Y, lid);
        Vertices[vi1] = FVector(lp1.X, lp1.Y, lid);
        Vertices[vi2] = FVector(lp2.X, lp2.Y, lid);
        Vertices[vi3] = FVector(lp3.X, lp3.Y, lid);

        // Assign geometry indices

        int32 ti = i*6;
        Indices[ti+0] = vi0;
        Indices[ti+1] = vi3;
        Indices[ti+2] = vi1;
        Indices[ti+3] = vi0;
        Indices[ti+4] = vi2;
        Indices[ti+5] = vi3;

        // Line points
        //
        // Figure showing line points layout:
        //
        // E2 ---- E0 ---- E1 ---- E3
        // |       |       |       |
        // P0 ---- P1 ---- P2 ---- P3

        FAlignedLineGeom lg;

        lg.P0 = p0;
        lg.P1 = p1;
        lg.P2 = p2;
        lg.P3 = p3;

        lg.E0 = lp0;
        lg.E1 = lp2;

        LineGeomArr[lid] = lg;
    }

    // Assign neighbour line extrusion points

    for (int32 i=0; i<PointCount; ++i)
    {
        int32 lid = i+1;
        int32 li0 = (i>0) ? (i-1) : (PointCount-1);
        int32 li1 = (i+1) % PointCount;
        li0 = (li0+1);
        li1 = (li1+1);
        FAlignedLineGeom& lg(LineGeomArr[lid]);
        lg.E2 = LineGeomArr[li0].E0;
        lg.E3 = LineGeomArr[li1].E1;
        LineGeomArr[lid] = lg;
    }

    // Construct line geom data

    LineGeomData.Release();
    LineGeomData.Initialize(
        sizeof(FLineGeomData::ElementType),
        LineGeomArr.Num(),
        &LineGeomArr,
        BUF_Static,
        TEXT("LineGeomData")
        );

    // Assign render target

    SetRenderTarget(RHICmdList, StencilTextureRTV, FTextureRHIRef());

    // Prepare graphics pipeline

    TShaderMapRef<FPMUMSqStencilPolyDrawEdgeVS> VSShader(RHIShaderMap);
    TShaderMapRef<FPMUMSqStencilPolyDrawEdgePS> PSShader(RHIShaderMap);

    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
    RHICmdList.SetViewport(0, 0, 0.0f, Dimension.X, Dimension.Y, 1.0f);
    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.PrimitiveType = PT_TriangleList;
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector3();
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VSShader);
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PSShader);
    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

    // Draw render target

    FVector2D DrawExts = FVector2D(Dimension.X, Dimension.Y) / 2.f;
    VSShader->SetParameter(RHICmdList, TEXT("_DrawExts"), DrawExts);

    FPMURHIUtilityLibrary::DrawIndexedPrimitiveVolatile(
        RHICmdList,
        PT_TriangleList,
        0,
        Vertices.Num(),
        TCount,
        Indices.GetData(),
        Indices.GetTypeSize(),
        Vertices.GetData(),
        Vertices.GetTypeSize()
        );

    VSShader->UnbindBuffers(RHICmdList);

    // Unassign render target

    SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

void FPMUMarchingSquaresStencilPoly::ResolveStencilTexture_RT()
{
    check(IsInRenderingThread());
    check(RHICmdListPtr != nullptr);
    check(IsValidRef(StencilTextureRTV));
    check(IsValidRef(StencilTextureRSV));

    FRHICommandListImmediate& RHICmdList(*RHICmdListPtr);

    RHICmdList.CopyToResolveTarget(
        StencilTextureRTV,
        StencilTextureRSV,
        FResolveParams()
        );

    UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresStencilPoly::ResolveStencilTexture_RT() CopyToResolveTarget()"));
}

void FPMUMarchingSquaresStencilPoly::ReleaseResources_RT()
{
    if (IsValidRef(StencilTextureRTV) || IsValidRef(StencilTextureRSV) || IsValidRef(StencilTextureSRV))
    {
        StencilTextureRTV.SafeRelease();
        StencilTextureRSV.SafeRelease();
        StencilTextureSRV.SafeRelease();

        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresStencilPoly::ReleaseResources_RT() RELEASE RESOURCES"));
    }
}

void FPMUMarchingSquaresStencilPoly::GenerateVoxelFeatures_RT(FRHICommandListImmediate& RHICmdList, const FGenerateVoxelFeatureParameter& Parameter)
{
    check(IsInRenderingThread());
    check(Parameter.Map != nullptr);
    check(Parameter.Map->HasValidDimension_RT());

    RHICmdListPtr = &RHICmdList;
    RHIShaderMap  = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    FPMUMarchingSquaresMap& Map(*Parameter.Map);

    if (Dimension != Map.GetDimension_RT())
    {
        ReleaseResources_RT();

        Dimension  = Map.GetDimension_RT();
        VoxelCount = Map.GetVoxelCount_RT();
    }

    FillType          = Parameter.FillType;
    StencilPoints     = Parameter.StencilPoints;
    StencilEdgeRadius = Parameter.StencilEdgeRadius;

    // Create resolve targetable texture if required

    if (! StencilTextureRTV || ! StencilTextureRSV)
    {
        // Make sure if one the render resource is invalid then all of them are
        check(! IsValidRef(StencilTextureRTV));
        check(! IsValidRef(StencilTextureRSV));
        check(! IsValidRef(StencilTextureSRV));

        FRHIResourceCreateInfo CreateInfo;
        RHICreateTargetableShaderResource2D(
            Dimension.X,
            Dimension.Y,
            PF_R16_UINT,
            1,
            TexCreate_None,
            TexCreate_RenderTargetable,
            false,
            CreateInfo,
            StencilTextureRTV,
            StencilTextureRSV
            );

        // Create shader resourve view
        StencilTextureSRV = RHICreateShaderResourceView(StencilTextureRSV, 0);

        UE_LOG(UntMSq,Warning, TEXT("FPMUMarchingSquaresStencilPoly::GenerateVoxelFeatures_RT() CREATE TEXTURE"));
    }

    // Clear stencil texture

    RHICmdList.SetViewport(0, 0, 0.0f, Dimension.X, Dimension.Y, 1.0f);
    SetRenderTarget(
        RHICmdList,
        StencilTextureRTV,
        FTextureRHIRef(),
        ESimpleRenderTargetMode::EClearColorExistingDepth,
        FExclusiveDepthStencil::DepthWrite_StencilWrite
        );

    SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());

    // Draw stencil texture

    DrawStencilMask_RT();
    DrawStencilEdge_RT();

    // Resolve stencil texture

    ResolveStencilTexture_RT();

    // Get data SRV & UAV

    FPMURWBuffer VoxelStateData(Map.GetVoxelStateData());
    FPMURWBuffer VoxelFeatureData(Map.GetVoxelFeatureData());

    FShaderResourceViewRHIParamRef  VoxelStateDataSRV = VoxelStateData.SRV;
    FUnorderedAccessViewRHIParamRef VoxelStateDataUAV = VoxelStateData.UAV;

    FShaderResourceViewRHIParamRef  VoxelFeatureDataSRV = VoxelFeatureData.SRV;
    FUnorderedAccessViewRHIParamRef VoxelFeatureDataUAV = VoxelFeatureData.UAV;

    FUnorderedAccessViewRHIRef& DebugTextureUAV(Map.GetDebugRTTUAV());

    FShaderResourceViewRHIParamRef LineGeomDataSRV = LineGeomData.SRV;

    // Write voxel state data

    {
        TShaderMapRef<FPMUMSqStencilPolyWriteVoxelStateCS> ComputeShader(RHIShaderMap);
        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindSRV(RHICmdList, TEXT("StencilTexture"), StencilTextureSRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("LineGeomData"), LineGeomDataSRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVoxelStateData"), VoxelStateDataUAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->SetParameter(RHICmdList, TEXT("_MapDim"), Dimension);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }

    // Write cell feature data

    {
        TShaderMapRef<FPMUMSqStencilPolyWriteVoxelFeatureCS> ComputeShader(RHIShaderMap);
        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelStateData"), VoxelStateDataSRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("LineGeomData"), LineGeomDataSRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutVoxelFeatureData"), VoxelFeatureDataUAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->SetParameter(RHICmdList, TEXT("_MapDim"), Dimension);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }

    LineGeomData.Release();

    RHICmdListPtr = nullptr;
    RHIShaderMap  = nullptr;
}

void FPMUMarchingSquaresStencilPoly::ClearStencil_RT(FRHICommandListImmediate& RHICmdList)
{
    Dimension  = FIntPoint::ZeroValue;
    VoxelCount = 0;

    ReleaseResources_RT();
}

void FPMUMarchingSquaresStencilPoly::GenerateVoxelFeatures(const FGenerateVoxelFeatureParameter& Parameter)
{
    check(Parameter.Map != nullptr);
    check(Parameter.Map->HasValidDimension());

    Parameter.Map->InitializeVoxelData();

    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        FPMUMarchingSquaresStencilPoly_GenerateVoxelFeatures,
        FPMUMarchingSquaresStencilPoly*, Stencil, this,
        FGenerateVoxelFeatureParameter, Parameter, Parameter,
        {
            Stencil->GenerateVoxelFeatures_RT(RHICmdList, Parameter);
        } );
}

void FPMUMarchingSquaresStencilPoly::ClearStencil()
{
    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        FPMUMarchingSquaresStencilPoly_GenerateVoxelFeatures,
        FPMUMarchingSquaresStencilPoly*, Stencil, this,
        {
            Stencil->ClearStencil_RT(RHICmdList);
        } );
}

void UPMUMarchingSquaresStencilPolyRef::ClearStencil()
{
    Stencil.ClearStencil();
}

void UPMUMarchingSquaresStencilPolyRef::ApplyStencilToMap(UPMUMarchingSquaresMapRef* MapRef, int32 FillType)
{
    if (FillType >= 0                           &&
        StencilPoints.Num() > 0                 &&
        StencilEdgeRadius > KINDA_SMALL_NUMBER  &&
        IsValid(MapRef)                         &&
        MapRef->HasValidMap()
        )
    {
        FPMUMarchingSquaresMap& Map(MapRef->GetMap());
        uint32 uFillType = FMath::Max(0, FillType);

        typedef FPMUMarchingSquaresStencilPoly::FGenerateVoxelFeatureParameter FParameterType;

        FParameterType Parameter( { &Map, uFillType, StencilPoints, StencilEdgeRadius } );
        Stencil.GenerateVoxelFeatures(Parameter);
    }
}
