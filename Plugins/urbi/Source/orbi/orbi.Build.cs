
using UnrealBuildTool;

public class orbi : ModuleRules
{
	public orbi(ReadOnlyTargetRules Target) : base(Target)
	{
	    var rootPath = Target.ProjectFile.Directory.FullName;
	    var sep = "\\";
	    var linux = Target.Platform == UnrealTargetPlatform.Linux;
	    if (linux)
	        sep = "/";
	    var urbiSdkPath = rootPath 
	      + sep + "Plugins" 
	      + sep + "urbi" 
	      + sep + "Source"
	      + sep + "orbi"
	      + sep + "urbi-sdk-" + Target.Platform.ToString();
		System.Console.WriteLine("*** " + urbiSdkPath);
		PrecompileForTargets = PrecompileTargetsType.Any;
        bPrecompile = true;
	    PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		//CppStandard = CppStandardVersion.Cpp17;
		bEnableUndefinedIdentifierWarnings = false;
		//bUseRTTI = true;
		bEnableExceptions = true;
		//PrivateDefinitions.Add("-g");
		//WarningLevel.Warning
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				urbiSdkPath + sep + "include",

			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EnhancedInput",
            });
        if (Target.Type == TargetType.Editor)
            PrivateDependencyModuleNames.Add( "UnrealEd");
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		

		/*PublicDelayLoadDLLs.Add("boost_filesystem-vc143-mt-x64-1_80.dll");
        PublicDelayLoadDLLs.Add("boost_thread-vc143-mt-x64-1_80.dll");
		PublicDelayLoadDLLs.Add("port.dll");
		PublicDelayLoadDLLs.Add("sched.dll");
        PublicDelayLoadDLLs.Add("urbi.dll");
        PublicDelayLoadDLLs.Add("uobject.dll");*/
        if (linux) PublicAdditionalLibraries.Add("uobject-full");
        else PublicAdditionalLibraries.Add("uobject-full.lib");
		//PublicAdditionalLibraries.Add("sched");
		//PublicAdditionalLibraries.Add("port");
        //PublicAdditionalLibraries.Add("serialize");
        //PublicAdditionalLibraries.Add("urbi");
        //PublicSystemLibraryPaths.Add("/bury/transient/boost-1.80.0-static/lib");
        //PublicAdditionalLibraries.Add("boost_filesystem");
        //PublicAdditionalLibraries.Add("boost_thread");
        //PublicAdditionalLibraries.Add("libboost_filesystem-vc143-mt-x64-1_80.lib");
        //PublicAdditionalLibraries.Add("libboost_thread-vc143-mt-x64-1_80.lib");
        //PublicAdditionalLibraries.Add("/bury/transient/urbi/lib/gostai/engine/libuobject.so");
        PublicSystemLibraryPaths.Add(urbiSdkPath + sep + "lib");
		//PublicSystemLibraryPaths.Add(urbiSdkPath + "/lib/gostai/engine");
       // PublicSystemLibraryPaths.Add("c:/boost_1_80_0/lib64-msvc-14.3");
	}
}
