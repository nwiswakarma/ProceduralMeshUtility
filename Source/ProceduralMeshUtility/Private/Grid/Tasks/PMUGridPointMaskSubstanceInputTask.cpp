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

#include "Grid/Tasks/PMUGridPointMaskSubstanceInputTask.h"
#include "PMUUtilityLibrary.h"

#include "AGGTypes.h"
#include "AGGTypedContext.h"
#include "AGGRenderer.h"

#ifdef PMU_SUBSTANCE_ENABLED
#include "SubstanceImageInput.h"
#include "SubstanceUtility.h"
#endif

void UPMUGridPointMaskSubstanceInputTask::ExecuteTask_Impl(FGridData& GD)
{
#ifdef PMU_SUBSTANCE_ENABLED
    USubstanceImageInput* SubstanceImage = Cast<USubstanceImageInput>(SubstanceImageInput);

    check(Points.Num() >= 3);
    check(IsValid(SubstanceImage));

    const FIntPoint& Dim(GD.Dimension);
    const int32 MapSize = GD.CalcSize();

    FAGGBufferBGRA32 Buffer;
    FAGGRendererScanlineBGRA Renderer;
    FAGGPathController Path;

    Buffer.Init(Dim.X, Dim.Y, 0, true);
    Renderer.Attach(Buffer.GetAGGBuffer());

    const int32 BufferSize = Buffer.GetBufferSize();

    check((MapSize * sizeof(FColor)) == BufferSize);

    // Draw vector paths

    Path.Clear();
    Path.MoveTo(Points[0].X, Points[0].Y);
    for (int32 i=1; i<Points.Num(); ++i)
    {
        const FVector2D& V(Points[i]);
        Path.LineTo(V.X, V.Y);
    }
    Path.ClosePolygon();

    Renderer.Render(Path.GetAGGPath(), MaskColor, EAGGScanline::SL_Bin);

    // Construct Substance Image Input

    SubstanceImage->CompressionLevelRGB = 0;
    SubstanceImage->CompressionLevelAlpha = 0;
    SubstanceImage->NumComponents = 1;
    SubstanceImage->SizeX = GD.Dimension.X;
    SubstanceImage->SizeY = GD.Dimension.Y;
        
    FByteBulkData& ImageRGB(SubstanceImage->ImageRGB);

    ImageRGB.RemoveBulkData();
    ImageRGB.Lock(LOCK_READ_WRITE);

    uint8* ImagePtr = reinterpret_cast<uint8*>(ImageRGB.Realloc(BufferSize));
    Buffer.CopyToUnsafe(ImagePtr);

    ImageRGB.Unlock();
#endif
}

bool UPMUGridPointMaskSubstanceInputTask::SetupTask(FGridData& GridData)
{
#ifdef PMU_SUBSTANCE_ENABLED
    USubstanceImageInput* SubstanceImage = Cast<USubstanceImageInput>(SubstanceImageInput);
    return GridData.CalcSize() > 0 && Points.Num() >= 3 && IsValid(SubstanceImage);
#else
    return false;
#endif
}
