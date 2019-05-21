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

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class ProceduralMeshUtility: ModuleRules
    {
        public ProceduralMeshUtility(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            // Get the engine path. Ends with "Engine/"
            //string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);

            //PublicDefinitions.Add("PMU_LINE_SIMPLIFY_VIS_CHECK_HEAP_CONSISTENCY");
            PublicDefinitions.Add("PMU_LINE_SIMPLIFY_VIS_CHECK_INSTANCE_MEMORY");

            PrivateIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Private/Line/Simplify/ThirdParty/Leaflet"),
                    Path.Combine(ModuleDirectory, "Private/Line/Simplify/ThirdParty/Visvalingam")
                } );

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "RHI",
                    "RenderCore",
                    "Renderer"
                } );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Projects",
                    "GenericWorkerThread",
                    "RenderingUtilityLibrary"
                } );
        }
    }
}
