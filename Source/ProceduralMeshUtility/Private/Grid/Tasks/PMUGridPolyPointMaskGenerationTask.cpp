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

#include "Grid/Tasks/PMUGridPolyPointMaskGenerationTask.h"
#include "PMUUtilityLibrary.h"

#include "AGGTypes.h"
#include "AGGTypedContext.h"
#include "AGGRenderer.h"

void UPMUGridPolyPointMaskGenerationTask::ExecuteTask_Impl(FGridData& GD)
{
    check(Points.Num() >= 3);
    check(GD.CalcSize() > 0);

    const FIntPoint& Dim(GD.Dimension);

    FAGGBufferG8 Buffer;
    FAGGRendererScanlineG8 Renderer;
    FAGGPathController Path;

    Buffer.Init(Dim.X, Dim.Y, 0, true);
    Renderer.Attach(Buffer.GetAGGBuffer());

    // Draw vector paths

    Path.Clear();

    Path.MoveTo(Points[0].X, Points[0].Y);
    for (int32 i=1; i<Points.Num(); ++i)
    {
        const FVector2D& V(Points[i]);
        Path.LineTo(V.X, V.Y);
    }
    Path.ClosePolygon();

    const FColor MaskColor(255,255,255,255);
    Renderer.Render(Path.GetAGGPath(), MaskColor, EAGGScanline::SL_Bin);

    // Copy buffer as point mask

    GD.PointMask = Buffer.GetByteBuffer();

    check(GD.HasValidPointMask());
}
