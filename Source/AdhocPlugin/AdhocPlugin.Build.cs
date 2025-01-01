// Copyright (c) 2022-2025 SpeculativeCoder (https://github.com/SpeculativeCoder)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

using UnrealBuildTool;
using Tools.DotNETCommon;

public class AdhocPlugin : ModuleRules
{
    public AdhocPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; //PCHUsageMode.NoPCHs;

        //PrivatePCHHeaderFile = "Private/AdhocPluginPrivatePCH.h";
        //MinFilesUsingPrecompiledHeaderOverride = 1;
        //bUsePrecompiled = false;
        bUseUnity = false;

        // check if AdhocPluginExtra is available
        DirectoryReference Plugins = new DirectoryReference(PluginDirectory);
        DirectoryReference AdhocPluginExtra = DirectoryReference.Combine(Plugins, "Source", "AdhocPlugin", "AdhocPluginExtra");
        //Log.TraceInformation("AdhocPluginExtra={0}", AdhocPluginExtra);

        bool AdhocPluginExtraExists = DirectoryReference.Exists(AdhocPluginExtra);
        //Log.TraceInformation("AdhocPluginExtraExists={0}", AdhocPluginExtraExists);

        PublicDefinitions.Add("WITH_ADHOC_PLUGIN_EXTRA=" + (AdhocPluginExtraExists ? "1" : "0"));

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "AIModule",
                "Slate",
                "SlateCore",
                "Stomp",
                "WebSockets",
                "Http",
                "Json",
                "GameplayTags"
            }
        );
    }
}
