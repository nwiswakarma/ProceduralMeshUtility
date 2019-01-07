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
#include "RHI/PMURWBuffer.h"

class FPMUPrefixSumScan
{
public:

#if 1
    const static int32 BLOCK_SIZE  = 128;
    const static int32 BLOCK_SIZE2 = 256;
#else
    const static int32 BLOCK_SIZE  = 16; //2;
    const static int32 BLOCK_SIZE2 = 32; //4;
#endif

    FORCEINLINE static int32 GetBlockOffsetForSize(int32 ElementCount)
    {
        return FPlatformMath::RoundUpToPowerOfTwo(FMath::DivideAndRoundUp(ElementCount, BLOCK_SIZE2));
    }

    template<uint32 ScanVectorSize> static const TCHAR* GetScanVectorSizeName()
    {
        switch (ScanVectorSize)
        {
            case 1: return TEXT("uint");
            case 2: return TEXT("uint2");
            case 4: return TEXT("uint4");
            default:
                return TEXT("uint");
        }
    }

    template<uint32 ScanVectorSize> static bool IsValidScanVectorSize()
    {
        switch (ScanVectorSize)
        {
            case 1: return true;
            case 2: return true;
            case 4: return true;
        }

        return false;
    }

    template<uint32 ScanVectorSize>
    static int32 ExclusiveScan(
        FShaderResourceViewRHIParamRef SrcDataSRV,
        int32 DataStride,
        int32 ElementCount,
        FPMURWBufferStructured& ScanResult,
        FPMURWBufferStructured& SumBuffer,
        uint32 AdditionalOutputUsage = 0
        );
};
