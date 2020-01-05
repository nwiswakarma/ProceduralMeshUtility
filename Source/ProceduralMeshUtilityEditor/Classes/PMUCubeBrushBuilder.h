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
#include "PMUBrushBuilder.h"
#include "PMUCubeBrushBuilder.generated.h"

class ABrush;

UCLASS(autoexpandcategories=BrushSettings, EditInlineNew, meta=(DisplayName="PMU Box"))
class UPMUCubeBrushBuilder : public UPMUBrushBuilder
{
public:
	GENERATED_BODY()

public:
	UPMUCubeBrushBuilder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The size of the cube in the X dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float X;

	/** The size of the cube in the Y dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float Y;

	/** The size of the cube in the Z dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float Z;

	/** The size of the cube in the Z dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings)
	FVector Offset;

	/** The thickness of the cube wall when hollow */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(EditCondition="Hollow"))
	float WallThickness;

	UPROPERTY()
	FName GroupName;

	/** Whether this is a hollow or solid cube */
	UPROPERTY(EditAnywhere, Category=BrushSettings)
	uint32 Hollow:1;

	/** Whether extra internal faces should be generated for each cube face */
	UPROPERTY(EditAnywhere, Category=BrushSettings)
	uint32 Tessellated:1;


	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UBrushBuilder Interface
	virtual bool Build( UWorld* InWorld, ABrush* InBrush = NULL ) override;
	//~ End UBrushBuilder Interface

	// @todo document
	virtual void BuildCube( int32 Direction, float dx, float dy, float dz, bool _tessellated );

	virtual void BuildTestCube( int32 Direction, FVector o, float dx, float dy, float dz, bool _tessellated );
};
