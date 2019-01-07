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

#include "Line/PMUMeshPolyLine.h"

void FPMUMeshPolyLine::SetGeometry(const TArray<FVector2D>& Points)
{
    //this.widthCallback = c;

    const int32 PointCount = Points.Num();
    const float PointCountInv = 1.f / PointCount;

    Positions.Empty(PointCount * 2);
    Counters.Empty(PointCount * 2);

    for (int32 i=0; i<PointCount; ++i)
    {
        const FVector2D& v(Points[i]);
        const float c = i * PointCountInv;

        Positions.Emplace(v);
        Positions.Emplace(v);

        Counters.Emplace(c);
        Counters.Emplace(c);
    }

    ConstructGeometry();
}

void FPMUMeshPolyLine::ConstructGeometry()
{
    // Construct vertex shape information

    //var l = this.positions.length / 6;
    const int32 n = Positions.Num();
    const int32 l = n / 2;

    Sides.Empty(n);
    Widths.Empty(n);
    UVs.Empty(n);

    for (int32 i=0; i<l; ++i)
    {
        Sides.Emplace(1);
        Sides.Emplace(-1);
    }

    for (int32 i=0; i<l; ++i)
    {
        //if( this.widthCallback ) w = this.widthCallback( j / ( l -1 ) );
        //else w = 1;
        float w = 1.f;
        Widths.Emplace(w);
        Widths.Emplace(w);
    }

    for (int32 i=0; i<l; ++i)
    {
        const float fi = (float)i;
        UVs.Emplace(fi / (l - 1), 0.f);
        UVs.Emplace(fi / (l - 1), 1.f);
    }

    // Construct vertex relation information

    Previous.Empty(n);
    Next.Empty(n);

    {
        FVector2D v = Positions[0].Equals(Positions[n-2]) ? Positions[n-4] : Positions[0];
        Previous.Emplace(v);
        Previous.Emplace(v);
    }

    for (int32 i=0; i<(l-1); ++i)
    {
        const int32 idx = i*2;
        Previous.Emplace(Positions[idx  ]);
        Previous.Emplace(Positions[idx+1]);
    }

    for (int32 i=1; i<l; ++i)
    {
        const int32 idx = i*2;
        Next.Emplace(Positions[idx  ]);
        Next.Emplace(Positions[idx+1]);
    }

    {
        FVector2D v = Positions[n-2].Equals(Positions[0]) ? Positions[2] : Positions[n-2];
        Previous.Emplace(v);
        Previous.Emplace(v);
    }

    // Construct vertex indices

    Indices.Empty((l-1)*6);

    for (int32 i=0; i<(l-1); ++i)
    {
        int32 n = i * 2;

        Indices.Emplace(n  );
        Indices.Emplace(n+1);
        Indices.Emplace(n+2);

        Indices.Emplace(n+2);
        Indices.Emplace(n+1);
        Indices.Emplace(n+3);
    }

    /*
    if (!this.attributes) {
        this.attributes = {
            position: new THREE.BufferAttribute( new Float32Array( this.positions ), 3 ),
            previous: new THREE.BufferAttribute( new Float32Array( this.previous ), 3 ),
            next: new THREE.BufferAttribute( new Float32Array( this.next ), 3 ),
            side: new THREE.BufferAttribute( new Float32Array( this.side ), 1 ),
            width: new THREE.BufferAttribute( new Float32Array( this.width ), 1 ),
            uv: new THREE.BufferAttribute( new Float32Array( this.uvs ), 2 ),
            index: new THREE.BufferAttribute( new Uint16Array( this.indices_array ), 1 ),
            counters: new THREE.BufferAttribute( new Float32Array( this.counters ), 1 )
        }
    } else {
        this.attributes.position.copyArray(new Float32Array(this.positions));
        this.attributes.position.needsUpdate = true;
        this.attributes.previous.copyArray(new Float32Array(this.previous));
        this.attributes.previous.needsUpdate = true;
        this.attributes.next.copyArray(new Float32Array(this.next));
        this.attributes.next.needsUpdate = true;
        this.attributes.side.copyArray(new Float32Array(this.side));
        this.attributes.side.needsUpdate = true;
        this.attributes.width.copyArray(new Float32Array(this.width));
        this.attributes.width.needsUpdate = true;
        this.attributes.uv.copyArray(new Float32Array(this.uvs));
        this.attributes.uv.needsUpdate = true;
        this.attributes.index.copyArray(new Uint16Array(this.indices_array));
        this.attributes.index.needsUpdate = true;
    }

    this.geometry.addAttribute( 'position', this.attributes.position );
    this.geometry.addAttribute( 'previous', this.attributes.previous );
    this.geometry.addAttribute( 'next', this.attributes.next );
    this.geometry.addAttribute( 'side', this.attributes.side );
    this.geometry.addAttribute( 'width', this.attributes.width );
    this.geometry.addAttribute( 'uv', this.attributes.uv );
    this.geometry.addAttribute( 'counters', this.attributes.counters );

    this.geometry.setIndex( this.attributes.index );
    */
}
