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

#include "PMUBrushBuilder.h"

#include "Editor.h"
#include "EditorStyleSet.h"
#include "BSPOps.h"
#include "SnappingUtils.h"
#include "Engine/Brush.h"
#include "Engine/Polys.h"
#include "Engine/Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "PMUCubeBrushBuilder.h"

#define LOCTEXT_NAMESPACE "PMUBrushBuilder"

/*-----------------------------------------------------------------------------
    UPMUBrushBuilder.
-----------------------------------------------------------------------------*/

UPMUBrushBuilder::UPMUBrushBuilder(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BitmapFilename = TEXT("BBGeneric");
    ToolTip = TEXT("BrushBuilderName_Generic");
    NotifyBadParams = true;
}

void UPMUBrushBuilder::BeginBrush(bool InMergeCoplanars, FName InLayer)
{
    Layer = InLayer;
    MergeCoplanars = InMergeCoplanars;
    Vertices.Empty();
    Polys.Empty();
}

bool UPMUBrushBuilder::EndBrush( UWorld* InWorld, ABrush* InBrush )
{
    //!!validate
    check( InWorld != nullptr );
    ABrush* BuilderBrush = (InBrush != nullptr) ? InBrush : InWorld->GetDefaultBrush();

    // Ensure the builder brush is unhidden.
    BuilderBrush->bHidden = false;
    BuilderBrush->bHiddenEdLayer = false;

    AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
    FVector Location;
    if ( InBrush == nullptr )
    {
        Location = Actor ? Actor->GetActorLocation() : BuilderBrush->GetActorLocation();
    }
    else
    {
        Location = InBrush->GetActorLocation();
    }

    UModel* Brush = BuilderBrush->Brush;
    if (Brush == nullptr)
    {
        return true;
    }

    Brush->Modify();
    BuilderBrush->Modify();

    FRotator Temp(0.0f,0.0f,0.0f);
    FSnappingUtils::SnapToBSPVertex( Location, FVector::ZeroVector, Temp );
    BuilderBrush->SetActorLocation(Location, false);
    BuilderBrush->SetPivotOffset( FVector::ZeroVector );

    // Try and maintain the materials assigned to the surfaces. 
    TArray<FPoly> CachedPolys;
    UMaterialInterface* CachedMaterial = nullptr;
    if( Brush->Polys->Element.Num() == Polys.Num() )
    {
        // If the number of polygons match we assume its the same shape.
        CachedPolys.Append(Brush->Polys->Element);
    }
    else if( Brush->Polys->Element.Num() > 0 )
    {
        // If the polygons have changed check if we only had one material before. 
        CachedMaterial = Brush->Polys->Element[0].Material;
        if (CachedMaterial != NULL)
        {
            for( auto Poly : Brush->Polys->Element )
            {
                if( CachedMaterial != Poly.Material )
                {
                    CachedMaterial = NULL;
                    break;
                }
            }
        }
    }

    // Clear existing polys.
    Brush->Polys->Element.Empty();

    const bool bUseCachedPolysMaterial = CachedPolys.Num() > 0;
    int32 CachedPolyIdx = 0;
    for( TArray<FBuilderPoly>::TIterator It(Polys); It; ++It )
    {
        if( It->Direction<0 )
        {
            for( int32 i=0; i<It->VertexIndices.Num()/2; i++ )
            {
                Exchange( It->VertexIndices[i], It->VertexIndices.Last(i) );
            }
        }

        FPoly Poly;
        Poly.Init();
        Poly.ItemName = It->ItemName;
        Poly.Base = Vertices[It->VertexIndices[0]];
        Poly.PolyFlags = It->PolyFlags;

        // Try and maintain the polygons material where possible
        Poly.Material = ( bUseCachedPolysMaterial ) ? CachedPolys[CachedPolyIdx++].Material : CachedMaterial;

        for( int32 j=0; j<It->VertexIndices.Num(); j++ )
        {
            new(Poly.Vertices) FVector(Vertices[It->VertexIndices[j]]);
        }
        if( Poly.Finalize( BuilderBrush, 1 ) == 0 )
        {
            new(Brush->Polys->Element)FPoly(Poly);
        }
    }

    if( MergeCoplanars )
    {
        GEditor->bspMergeCoplanars( Brush, 0, 1 );
        //FBSPOps::bspValidateBrush( Brush, 1, 1 );
        ValidateBrush( Brush, 1, 1 );
    }
    Brush->Linked = 1;
    //FBSPOps::bspValidateBrush( Brush, 0, 1 );
    ValidateBrush( Brush, 0, 1 );
    Brush->BuildBound();

    GEditor->RedrawLevelEditingViewports();
    GEditor->SetPivot( BuilderBrush->GetActorLocation(), false, true );

    BuilderBrush->ReregisterAllComponents();

    return true;
}

int32 UPMUBrushBuilder::GetVertexCount() const
{
    return Vertices.Num();
}

FVector UPMUBrushBuilder::GetVertex(int32 i) const
{
    return Vertices.IsValidIndex(i) ? Vertices[i] : FVector::ZeroVector;
}

int32 UPMUBrushBuilder::GetPolyCount() const
{
    return Polys.Num();
}

void UPMUBrushBuilder::ValidateBrush(UModel* Brush, bool ForceValidate, bool DoStatusUpdate)
{
    check(Brush != nullptr);
    Brush->Modify();
    if( ForceValidate || !Brush->Linked )
    {
        Brush->Linked = 1;
        for( int32 i=0; i<Brush->Polys->Element.Num(); i++ )
        {
            Brush->Polys->Element[i].iLink = i;
        }
        int32 n=0;
        for( int32 i=0; i<Brush->Polys->Element.Num(); i++ )
        {
            FPoly* EdPoly = &Brush->Polys->Element[i];
            if( EdPoly->iLink==i )
            {
                for( int32 j=i+1; j<Brush->Polys->Element.Num(); j++ )
                {
                    FPoly* OtherPoly = &Brush->Polys->Element[j];
                    if
                    (   OtherPoly->iLink == j
                    &&  OtherPoly->Material == EdPoly->Material
                    &&  OtherPoly->TextureU == EdPoly->TextureU
                    &&  OtherPoly->TextureV == EdPoly->TextureV
                    &&  OtherPoly->PolyFlags == EdPoly->PolyFlags
                    &&  (OtherPoly->Normal | EdPoly->Normal)>0.9999 )
                    {
                        float Dist = FVector::PointPlaneDist( OtherPoly->Vertices[0], EdPoly->Vertices[0], EdPoly->Normal );
                        if( Dist>-0.001 && Dist<0.001 )
                        {
                            OtherPoly->iLink = i;
                            n++;
                        }
                    }
                }
            }
        }
//      UE_LOG(LogBSPOps, Log,  TEXT("BspValidateBrush linked %i of %i polys"), n, Brush->Polys->Element.Num() );
    }

    // Build bounds.
    Brush->BuildBound();
}

bool UPMUBrushBuilder::BadParameters(const FText& Msg)
{
    if ( NotifyBadParams )
    {
        FFormatNamedArguments Arguments;
        Arguments.Add(TEXT("Msg"), Msg);
        FNotificationInfo Info( FText::Format( LOCTEXT( "BadParameters", "Bad parameters in brush builder\n{Msg}" ), Arguments ) );
        Info.bFireAndForget = true;
        Info.ExpireDuration = Msg.IsEmpty() ? 4.0f : 6.0f;
        Info.bUseLargeFont = Msg.IsEmpty();
        Info.Image = FEditorStyle::GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification( Info );
    }
    return 0;
}

int32 UPMUBrushBuilder::Vertexv(FVector V)
{
    int32 Result = Vertices.Num();
    new(Vertices)FVector(V);

    return Result;
}

int32 UPMUBrushBuilder::Vertex3f(float X, float Y, float Z)
{
    int32 Result = Vertices.Num();
    new(Vertices)FVector(X,Y,Z);
    return Result;
}

void UPMUBrushBuilder::Poly3i(int32 Direction, int32 i, int32 j, int32 k, FName ItemName, bool bIsTwoSidedNonSolid )
{
    new(Polys)FBuilderPoly;
    Polys.Last().Direction=Direction;
    Polys.Last().ItemName=ItemName;
    new(Polys.Last().VertexIndices)int32(i);
    new(Polys.Last().VertexIndices)int32(j);
    new(Polys.Last().VertexIndices)int32(k);
    Polys.Last().PolyFlags = PF_DefaultFlags | (bIsTwoSidedNonSolid ? (PF_TwoSided|PF_NotSolid) : 0);
}

void UPMUBrushBuilder::Poly4i(int32 Direction, int32 i, int32 j, int32 k, int32 l, FName ItemName, bool bIsTwoSidedNonSolid )
{
    new(Polys)FBuilderPoly;
    Polys.Last().Direction=Direction;
    Polys.Last().ItemName=ItemName;
    new(Polys.Last().VertexIndices)int32(i);
    new(Polys.Last().VertexIndices)int32(j);
    new(Polys.Last().VertexIndices)int32(k);
    new(Polys.Last().VertexIndices)int32(l);
    Polys.Last().PolyFlags = PF_DefaultFlags | (bIsTwoSidedNonSolid ? (PF_TwoSided|PF_NotSolid) : 0);
}

void UPMUBrushBuilder::PolyBegin(int32 Direction, FName ItemName)
{
    new(Polys)FBuilderPoly;
    Polys.Last().ItemName=ItemName;
    Polys.Last().Direction = Direction;
    Polys.Last().PolyFlags = PF_DefaultFlags;
}

void UPMUBrushBuilder::Polyi(int32 i)
{
    new(Polys.Last().VertexIndices)int32(i);
}

void UPMUBrushBuilder::PolyEnd()
{
}

bool UPMUBrushBuilder::Build( UWorld* InWorld, ABrush* InBrush )
{
    return false;
}

void UPMUBrushBuilder::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    if (!GIsTransacting)
    {
        // Rebuild brush on property change
        ABrush* Brush = Cast<ABrush>(GetOuter());
        UWorld* BrushWorld = Brush ? Brush->GetWorld() : nullptr;
        if (BrushWorld)
        {
            Brush->bInManipulation = PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive;
            Build(BrushWorld, Brush);
        }
    }
}

/*-----------------------------------------------------------------------------
    UPMUCubeBrushBuilder.
-----------------------------------------------------------------------------*/

UPMUCubeBrushBuilder::UPMUCubeBrushBuilder(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Structure to hold one-time initialization
    struct FConstructorStatics
    {
        FName NAME_Cube;
        FConstructorStatics()
            : NAME_Cube(TEXT("Cube"))
        {
        }
    };
    static FConstructorStatics ConstructorStatics;

    X = 200.0f;
    Y = 200.0f;
    Z = 200.0;
    WallThickness = 10.0f;
    GroupName = ConstructorStatics.NAME_Cube;
    Hollow = false;
    Tessellated = false;
    BitmapFilename = TEXT("Btn_Box");
    ToolTip = TEXT("BrushBuilderName_Cube");
}

void UPMUCubeBrushBuilder::BuildCube( int32 Direction, float dx, float dy, float dz, bool _tessellated )
{
    int32 n = GetVertexCount();

    for( int32 i=-1; i<2; i+=2 )
        for( int32 j=-1; j<2; j+=2 )
            for( int32 k=-1; k<2; k+=2 )
                Vertex3f( i*dx/2, j*dy/2, k*dz/2 );

    // If the user wants a Tessellated cube, create the sides out of tris instead of quads.
    if( _tessellated )
    {
        Poly3i(Direction,n+0,n+1,n+3);
        Poly3i(Direction,n+3,n+2,n+0);
        Poly3i(Direction,n+2,n+3,n+7);
        Poly3i(Direction,n+7,n+6,n+2);
        Poly3i(Direction,n+6,n+7,n+5);
        Poly3i(Direction,n+5,n+4,n+6);
        Poly3i(Direction,n+4,n+5,n+1);
        Poly3i(Direction,n+1,n+0,n+4);
        Poly3i(Direction,n+3,n+1,n+5);
        Poly3i(Direction,n+5,n+7,n+3);
        Poly3i(Direction,n+0,n+2,n+6);
        Poly3i(Direction,n+6,n+4,n+0);
    }
    else
    {
        Poly4i(Direction,n+0,n+1,n+3,n+2);
        Poly4i(Direction,n+2,n+3,n+7,n+6);
        Poly4i(Direction,n+6,n+7,n+5,n+4);
        Poly4i(Direction,n+4,n+5,n+1,n+0);
        Poly4i(Direction,n+3,n+1,n+5,n+7);
        Poly4i(Direction,n+0,n+2,n+6,n+4);
    }
}

void UPMUCubeBrushBuilder::BuildTestCube( int32 Direction, FVector o, float dx, float dy, float dz, bool _tessellated )
{
    int32 n = GetVertexCount();

    for( int32 i=-1; i<2; i+=2 )
        for( int32 j=-1; j<2; j+=2 )
            for( int32 k=-1; k<2; k+=2 )
                Vertex3f( o.X+i*dx/2, o.Y+j*dy/2, o.Z+k*dz/2 );

    // If the user wants a Tessellated cube, create the sides out of tris instead of quads.
    if( _tessellated )
    {
        Poly3i(Direction,n+0,n+1,n+3);
        Poly3i(Direction,n+3,n+2,n+0);
        Poly3i(Direction,n+2,n+3,n+7);
        Poly3i(Direction,n+7,n+6,n+2);
        Poly3i(Direction,n+6,n+7,n+5);
        Poly3i(Direction,n+5,n+4,n+6);
        Poly3i(Direction,n+4,n+5,n+1);
        Poly3i(Direction,n+1,n+0,n+4);
        Poly3i(Direction,n+3,n+1,n+5);
        Poly3i(Direction,n+5,n+7,n+3);
        Poly3i(Direction,n+0,n+2,n+6);
        Poly3i(Direction,n+6,n+4,n+0);
    }
    else
    {
        Poly4i(Direction,n+0,n+1,n+3,n+2);
        Poly4i(Direction,n+2,n+3,n+7,n+6);
        Poly4i(Direction,n+6,n+7,n+5,n+4);
        Poly4i(Direction,n+4,n+5,n+1,n+0);
        Poly4i(Direction,n+3,n+1,n+5,n+7);
        Poly4i(Direction,n+0,n+2,n+6,n+4);
    }
}

void UPMUCubeBrushBuilder::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    if (PropertyChangedEvent.Property)
    {
        static FName Name_X(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, X));
        static FName Name_Y(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, Y));
        static FName Name_Z(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, Z));
        static FName Name_WallThickness(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, WallThickness));

        const float ThicknessEpsilon = 0.1f;

        if (Hollow && PropertyChangedEvent.Property->GetFName() == Name_X && X <= WallThickness)
        {
            X = WallThickness + ThicknessEpsilon;
        }

        if (Hollow && PropertyChangedEvent.Property->GetFName() == Name_Y && Y <= WallThickness)
        {
            Y = WallThickness + ThicknessEpsilon;
        }

        if (Hollow && PropertyChangedEvent.Property->GetFName() == Name_Z && Z <= WallThickness)
        {
            Z = WallThickness + ThicknessEpsilon;
        }

        if (Hollow && PropertyChangedEvent.Property->GetFName() == Name_WallThickness && WallThickness >= FMath::Min3(X, Y, Z))
        {
            WallThickness = FMath::Max(0.0f, FMath::Min3(X, Y, Z) - ThicknessEpsilon);
        }

        static FName Name_Hollow(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, Hollow));
        static FName Name_Tesselated(GET_MEMBER_NAME_CHECKED(UPMUCubeBrushBuilder, Tessellated));

        if (PropertyChangedEvent.Property->GetFName() == Name_Hollow && Hollow && Tessellated)
        {
            Hollow = false;
        }

        if (PropertyChangedEvent.Property->GetFName() == Name_Tesselated && Hollow && Tessellated)
        {
            Tessellated = false;
        }
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UPMUCubeBrushBuilder::Build(UWorld* InWorld, ABrush* InBrush)
{
    if( Z<=0 || Y<=0 || X<=0 )
        return BadParameters(LOCTEXT("CubeInvalidDimensions", "Invalid cube dimensions"));
    if( Hollow && (Z<=WallThickness || Y<=WallThickness || X<=WallThickness) )
        return BadParameters(LOCTEXT("CubeInvalidWallthickness", "Invalid cube wall thickness"));
    if( Hollow && Tessellated )
        return BadParameters(LOCTEXT("TessellatedIncompatibleWithHollow", "The 'Tessellated' option can't be specified with the 'Hollow' option."));

    //BeginBrush( false, GroupName );
    BeginBrush( true, GroupName );
    //float OffsetLength = Offset.X;
    //BuildTestCube( +1, FVector( OffsetLength, 0.f, 0.f), X, Y, Z, Tessellated );
    //BuildTestCube( +1, FVector(-OffsetLength, 0.f, 0.f), X, Y, Z, Tessellated );
    //BuildTestCube( +1, FVector(0.f,  OffsetLength, 0.f), X, Y, Z, Tessellated );
    //BuildTestCube( +1, FVector(0.f, -OffsetLength, 0.f), X, Y, Z, Tessellated );
    BuildCube( +1, X, Y, Z, Tessellated );
    if( Hollow )
        BuildCube( -1, X-WallThickness, Y-WallThickness, Z-WallThickness, Tessellated );
    return EndBrush( InWorld, InBrush );
}

#undef LOCTEXT_NAMESPACE
