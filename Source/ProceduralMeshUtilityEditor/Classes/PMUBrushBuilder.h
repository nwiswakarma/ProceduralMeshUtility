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
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/BrushBuilder.h"
#include "PMUBrushBuilder.generated.h"

class ABrush;

UCLASS(abstract)
class UPMUBrushBuilder : public UBrushBuilder
{
public:
	GENERATED_BODY()
public:

	UPMUBrushBuilder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UBrushBuilder interface */
	virtual void BeginBrush( bool InMergeCoplanars, FName InLayer ) override;
	virtual bool EndBrush( UWorld* InWorld, ABrush* InBrush ) override;
	virtual int32 GetVertexCount() const override;
	virtual FVector GetVertex( int32 i ) const override;
	virtual int32 GetPolyCount() const override;
	virtual bool BadParameters( const FText& msg ) override;
	virtual int32 Vertexv( FVector v ) override;
	virtual int32 Vertex3f( float X, float Y, float Z ) override;
	virtual void Poly3i( int32 Direction, int32 i, int32 j, int32 k, FName ItemName = NAME_None, bool bIsTwoSidedNonSolid = false ) override;
	virtual void Poly4i( int32 Direction, int32 i, int32 j, int32 k, int32 l, FName ItemName = NAME_None, bool bIsTwoSidedNonSolid = false ) override;
	virtual void PolyBegin( int32 Direction, FName ItemName = NAME_None ) override;
	virtual void Polyi( int32 i ) override;
	virtual void PolyEnd() override;
	virtual bool Build( UWorld* InWorld, ABrush* InBrush = NULL ) override;

	/** UObject interface */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;

    void ValidateBrush(UModel* Brush, bool ForceValidate, bool DoStatusUpdate);
};
