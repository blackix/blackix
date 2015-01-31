// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Runtime.InteropServices;

namespace UnrealBuildTool
{
	// This enum has to be compatible with the one defined in the
	// UE4\Engine\Source\Runtime\Core\Public\Modules\ModuleManager.h
	// to keep communication between UHT, UBT and Editor compiling
	// processes valid.
	public enum ECompilationResult
	{
		/** All targets were up to date, used only with -canskiplink */
		UpToDate = -2,
		/** Build was canceled, this is used on the engine side only */
		Canceled = -1,
		/** Compilation succeeded */
		Succeeded = 0,
		/** Compilation failed because generated code changed which was not supported */
		FailedDueToHeaderChange = 1,
		/** Compilation failed due to compilation errors */
		OtherCompilationError = 2,
		/** The process has most likely crashed. This is what UE returns in case of an assert */
		CrashOrAssert = 3,
		/** Compilation is not supported in the current build */
		Unsupported,
		/** Unknown error */
		Unknown
	}
	public static class CompilationResultExtensions
	{
		public static bool Succeeded(this ECompilationResult Result)
		{
			return Result == ECompilationResult.Succeeded || Result == ECompilationResult.UpToDate;
		}
	}

	/** Information about a module that needs to be passed to UnrealHeaderTool for code generation */
	[Serializable]
	public class UHTModuleInfo
	{
		/** Module name */
		public string ModuleName;

		/** Module base directory */
		public string ModuleDirectory;

		/** Module type */
		public string ModuleType;

		/** Public UObject headers found in the Classes directory (legacy) */
		public List<FileItem> PublicUObjectClassesHeaders;

		/** Public headers with UObjects */
		public List<FileItem> PublicUObjectHeaders;

		/** Private headers with UObjects */
		public List<FileItem> PrivateUObjectHeaders;

		/** Module PCH absolute path */
		public string PCH;

		/** Base (i.e. extensionless) path+filename of the .generated files */
		public string GeneratedCPPFilenameBase;

		public override string ToString()
		{
			return ModuleName;
		}
	}

	public struct UHTManifest
	{
		public struct Module
		{
			public string       Name;
			public string		ModuleType;
			public string       BaseDirectory;
			public string       IncludeBase;     // The include path which all UHT-generated includes should be relative to
			public string       OutputDirectory;
			public List<string> ClassesHeaders;
			public List<string> PublicHeaders;
			public List<string> PrivateHeaders;
			public string       PCH;
			public string       GeneratedCPPFilenameBase;
			public bool         SaveExportedHeaders;

			public override string ToString()
			{
				return Name;
			}
		}

		public UHTManifest(UEBuildTarget Target, string InRootLocalPath, string InRootBuildPath, IEnumerable<UHTModuleInfo> ModuleInfo)
		{
			IsGameTarget  = TargetRules.IsGameType(Target.TargetType);
			RootLocalPath = InRootLocalPath;
			RootBuildPath = InRootBuildPath;
			TargetName    = Target.GetTargetName();

			Modules = ModuleInfo.Select(Info => new Module{
				Name                     = Info.ModuleName,
				ModuleType				 = Info.ModuleType,
				BaseDirectory            = Info.ModuleDirectory,
				IncludeBase              = Info.ModuleDirectory,
				OutputDirectory          = Path.GetDirectoryName( Info.GeneratedCPPFilenameBase ),
				ClassesHeaders           = Info.PublicUObjectClassesHeaders.Select((Header) => Header.AbsolutePath).ToList(),
				PublicHeaders            = Info.PublicUObjectHeaders       .Select((Header) => Header.AbsolutePath).ToList(),
				PrivateHeaders           = Info.PrivateUObjectHeaders      .Select((Header) => Header.AbsolutePath).ToList(),
				PCH                      = Info.PCH,
				GeneratedCPPFilenameBase = Info.GeneratedCPPFilenameBase,
				//@todo.Rocket: This assumes Engine/Source is a 'safe' folder name to check for
				SaveExportedHeaders = !UnrealBuildTool.RunningRocket() || !Info.ModuleDirectory.Contains("Engine\\Source\\")

			}).ToList();
		}

		public bool         IsGameTarget;     // True if the current target is a game target
		public string       RootLocalPath;    // The engine path on the local machine
		public string       RootBuildPath;    // The engine path on the build machine, if different (e.g. Mac/iOS builds)
		public string       TargetName;       // Name of the target currently being compiled
		public List<Module> Modules;
	}


	/**
	 * This handles all running of the UnrealHeaderTool
	 */
	public class ExternalExecution
	{


		static ExternalExecution()
		{
		}

		/// <summary>
		/// Gets UnrealHeaderTool.exe path. Does not care if UnrealheaderTool was build as a monolithic exe or not.
		/// </summary>
		static string GetHeaderToolPath()
		{
			UnrealTargetPlatform Platform = BuildHostPlatform.Current.Platform;
			string ExeExtension = UEBuildPlatform.GetBuildPlatform(Platform).GetBinaryExtension(UEBuildBinaryType.Executable);
			string HeaderToolExeName = "UnrealHeaderTool";
			string HeaderToolPath = Path.Combine("..", "Binaries", Platform.ToString(), HeaderToolExeName + ExeExtension);
			return HeaderToolPath;
		}

		class VersionedBinary
		{
			public VersionedBinary(string InFilename, int InVersion)
			{
				Filename = InFilename;
				Version = InVersion;
			}
			public string Filename;
			public int Version;
		}

		/// <summary>
		/// Finds all UnrealHeaderTool plugins in the plugin directory
		/// </summary>
		static void RecursivelyCollectHeaderToolPlugins(string RootPath, string Pattern, string Platform, List<VersionedBinary> PluginBinaries)
		{
			var SubDirectories = Directory.GetDirectories(RootPath);
			foreach (var Dir in SubDirectories)
			{
				if (Dir.IndexOf("Intermediate", StringComparison.InvariantCultureIgnoreCase) < 0)
				{
					RecursivelyCollectHeaderToolPlugins(Dir, Pattern, Platform, PluginBinaries);
					if (Dir.EndsWith("Binaries", StringComparison.InvariantCultureIgnoreCase))
					{
						// No need to search the other folders
						break;
					}
				}
			}
			var Binaries = Directory.GetFiles(RootPath, Pattern);
			foreach (var Binary in Binaries)
			{
				if (Binary.Contains(Platform))
				{
					PluginBinaries.Add(new VersionedBinary(Binary, BuildHostPlatform.Current.GetDllApiVersion(Binary)));
				}
			}
		}

		/// <summary>
		/// Gets all UnrealHeaderTool binaries (including DLLs if it was not build monolithically)
		/// </summary>
		static VersionedBinary[] GetHeaderToolBinaries()
		{
			var Binaries = new List<VersionedBinary>();
			var HeaderToolExe = GetHeaderToolPath();
			if (File.Exists(HeaderToolExe))
			{
				Binaries.Add(new VersionedBinary(HeaderToolExe, -1));

				var HeaderToolLocation = Path.GetDirectoryName(HeaderToolExe);
				var Platform = BuildHostPlatform.Current.Platform;
				var DLLExtension = UEBuildPlatform.GetBuildPlatform(Platform).GetBinaryExtension(UEBuildBinaryType.DynamicLinkLibrary);
				var DLLSearchPattern = "UnrealHeaderTool-*" + DLLExtension;
				var HeaderToolDLLs = Directory.GetFiles(HeaderToolLocation, DLLSearchPattern, SearchOption.TopDirectoryOnly);


				foreach (var Binary in HeaderToolDLLs)
				{
					Binaries.Add(new VersionedBinary(Binary, BuildHostPlatform.Current.GetDllApiVersion(Binary)));
				}

				var PluginDirectory = Path.Combine("..", "Plugins");
				RecursivelyCollectHeaderToolPlugins(PluginDirectory, DLLSearchPattern, Platform.ToString(), Binaries);
			}
			return Binaries.ToArray();
		}

		/// <summary>
		/// Gets the latest write time of any of the UnrealHeaderTool binaries (including DLLs and Plugins) or DateTime.MaxValue if UnrealHeaderTool does not exist
		/// </summary>
		/// <returns>
		/// Latest timestamp of UHT binaries or DateTime.MaxValue if UnrealHeaderTool is out of date and needs to be rebuilt.
		/// </returns>
		static DateTime CheckIfUnrealHeaderToolIsUpToDate()
		{
			var LatestWriteTime = DateTime.MinValue;
			int? MinVersion = null;
			using (var TimestampTimer = new ScopedTimer("GetHeaderToolTimestamp"))
			{
				var HeaderToolBinaries = GetHeaderToolBinaries();				
				// Find the latest write time for all UnrealHeaderTool binaries
				foreach (var Binary in HeaderToolBinaries)
				{
					var BinaryInfo = new FileInfo(Binary.Filename);
					if (BinaryInfo.Exists)
					{
						// Latest write time
						if (BinaryInfo.LastWriteTime > LatestWriteTime)
						{
							LatestWriteTime = BinaryInfo.LastWriteTime;
						}
						// Minimum version
						if (Binary.Version > -1)
						{
							MinVersion = MinVersion.HasValue ? Math.Min(MinVersion.Value, Binary.Version) : Binary.Version;
						}
					}
				}
				if (MinVersion.HasValue)
				{
					// If we were able to retrieve the minimal API version, go through all binaries one more time
					// and delete all binaries that do not match the minimum version (which for local builds would be 0, but it will
					// also detect bad or partial syncs)
					foreach (var Binary in HeaderToolBinaries)
					{
						if (Binary.Version > -1)
						{
							if (Binary.Version != MinVersion.Value)
							{
								// Bad sync
								File.Delete(Binary.Filename);
								LatestWriteTime = DateTime.MaxValue;
								Log.TraceWarning("Detected mismatched version in UHT binary {0} (API Version {1}, expected: {2})", Path.GetFileName(Binary.Filename), Binary.Version, MinVersion.Value);
							}
						}
					}
				}
			}
			// If UHT doesn't exist or is out of date/mismatched, force regenerate.
			return LatestWriteTime > DateTime.MinValue ? LatestWriteTime : DateTime.MaxValue;
		}



		/// <summary>
		/// Gets the timestamp of CoreUObject.generated.cpp file.
		/// </summary>
		/// <returns>Last write time of CoreUObject.generated.cpp or DateTime.MaxValue if it doesn't exist.</returns>
		private static DateTime GetCoreGeneratedTimestamp(string ModuleName, string ModuleGeneratedCodeDirectory)
		{			
			DateTime Timestamp;
			if( UnrealBuildTool.RunningRocket() )
			{
				// In Rocket, we don't check the timestamps on engine headers.  Default to a very old date.
				Timestamp = DateTime.MinValue;
			}
			else
			{
				string CoreGeneratedFilename = Path.Combine(ModuleGeneratedCodeDirectory, ModuleName + ".generated.cpp");
				if (File.Exists(CoreGeneratedFilename))
				{
					Timestamp = new FileInfo(CoreGeneratedFilename).LastWriteTime;
				}
				else
				{
					// Doesn't exist, so use a 'newer that everything' date to force rebuild headers.
					Timestamp = DateTime.MaxValue; 
				}
			}

			return Timestamp;
		}

		/**
		 * Checks the class header files and determines if generated UObject code files are out of date in comparison.
		 * @param UObjectModules	Modules that we generate headers for
		 * 
		 * @return					True if the code files are out of date
		 * */
		private static bool AreGeneratedCodeFilesOutOfDate(List<UHTModuleInfo> UObjectModules)
		{
			bool bIsOutOfDate = false;

			// Get UnrealHeaderTool timestamp. If it's newer than generated headers, they need to be rebuilt too.
			var HeaderToolTimestamp = CheckIfUnrealHeaderToolIsUpToDate();

			// Get CoreUObject.generated.cpp timestamp.  If the source files are older than the CoreUObject generated code, we'll
			// need to regenerate code for the module
			DateTime? CoreGeneratedTimestamp = null;
			{ 
				// Find the CoreUObject module
				foreach( var Module in UObjectModules )
				{
					if( Module.ModuleName.Equals( "CoreUObject", StringComparison.InvariantCultureIgnoreCase ) )
					{
						CoreGeneratedTimestamp = GetCoreGeneratedTimestamp(Module.ModuleName, Path.GetDirectoryName( Module.GeneratedCPPFilenameBase ));
						break;
					}
				}
				if( CoreGeneratedTimestamp == null )
				{
					throw new BuildException( "Could not find CoreUObject in list of all UObjectModules" );
				}
			}


			foreach( var Module in UObjectModules )
			{
				// In Rocket, we skip checking timestamps for modules that don't exist within the project's directory
				if (UnrealBuildTool.RunningRocket())
				{
					// @todo Rocket: This could be done in a better way I'm sure
					if (!Utils.IsFileUnderDirectory( Module.ModuleDirectory, UnrealBuildTool.GetUProjectPath() ))
					{
						// Engine or engine plugin module - Rocket does not regenerate them so don't compare their timestamps
						continue;
					}
				}

				// Make sure we have an existing folder for generated code.  If not, then we definitely need to generate code!
				var GeneratedCodeDirectory = Path.GetDirectoryName( Module.GeneratedCPPFilenameBase );
				var TestDirectory = (FileSystemInfo)new DirectoryInfo(GeneratedCodeDirectory);
				if( TestDirectory.Exists )
				{
					// Grab our special "Timestamp" file that we saved after the last set of headers were generated.  This file
					// actually contains the list of source files which contained UObjects, so that we can compare to see if any
					// UObject source files were deleted (or no longer contain UObjects), which means we need to run UHT even
					// if no other source files were outdated
					string TimestampFile = Path.Combine( GeneratedCodeDirectory, @"Timestamp" );
					var SavedTimestampFileInfo = (FileSystemInfo)new FileInfo(TimestampFile);
					if (SavedTimestampFileInfo.Exists)
					{
						// Make sure the last UHT run completed after UnrealHeaderTool.exe was compiled last, and after the CoreUObject headers were touched last.
						var SavedTimestamp = SavedTimestampFileInfo.LastWriteTime;
						if( SavedTimestamp.CompareTo(HeaderToolTimestamp) > 0 &&
							SavedTimestamp.CompareTo(CoreGeneratedTimestamp) > 0 )
						{
							// Iterate over our UObjects headers and figure out if any of them have changed
							var AllUObjectHeaders = new List<FileItem>();
							AllUObjectHeaders.AddRange( Module.PublicUObjectClassesHeaders );
							AllUObjectHeaders.AddRange( Module.PublicUObjectHeaders );
							AllUObjectHeaders.AddRange( Module.PrivateUObjectHeaders );

							// Load up the old timestamp file and check to see if anything has changed
							{
								var UObjectFilesFromPreviousRun = File.ReadAllLines( TimestampFile );
								if( AllUObjectHeaders.Count == UObjectFilesFromPreviousRun.Length )
								{
									for( int FileIndex = 0; FileIndex < AllUObjectHeaders.Count; ++FileIndex )
									{
										if( !UObjectFilesFromPreviousRun[ FileIndex ].Equals( AllUObjectHeaders[ FileIndex ].AbsolutePath, StringComparison.InvariantCultureIgnoreCase ) )
										{
											bIsOutOfDate = true;
											Log.TraceVerbose( "UnrealHeaderTool needs to run because the set of UObject source files in module {0} has changed", Module.ModuleName );
											break;
										}
									}
								}
 								else
								{
									bIsOutOfDate = true;
									Log.TraceVerbose( "UnrealHeaderTool needs to run because there are a different number of UObject source files in module {0}", Module.ModuleName );
								}
							}

							foreach( var HeaderFile in AllUObjectHeaders )
							{
								var HeaderFileTimestamp = HeaderFile.Info.LastWriteTime;

								// Has the source header changed since we last generated headers successfully?
								if( SavedTimestamp.CompareTo( HeaderFileTimestamp ) < 0 )
								{
									Log.TraceVerbose( "UnrealHeaderTool needs to run because SavedTimestamp is older than HeaderFileTimestamp (" + HeaderFile.AbsolutePath + ") for module {0}", Module.ModuleName );
									bIsOutOfDate = true;
									break;
								}
							}
						}
						else
						{
							// Generated code is older UnrealHeaderTool.exe or CoreUObject headers.  Out of date!
							Log.TraceVerbose( "UnrealHeaderTool needs to run because UnrealHeaderTool.exe or CoreUObject headers are newer than SavedTimestamp for module {0}", Module.ModuleName );
							bIsOutOfDate = true;
						}
					}
					else
					{
						// Timestamp file was missing (possibly deleted/cleaned), so headers are out of date
						Log.TraceVerbose( "UnrealHeaderTool needs to run because UHT Timestamp file did not exist for module {0}", Module.ModuleName );
						bIsOutOfDate = true;
					}
				}
				else
				{
					// Generated code directory is missing entirely!
					Log.TraceVerbose( "UnrealHeaderTool needs to run because no generated code directory was found for module {0}", Module.ModuleName );
					bIsOutOfDate = true;
				}

				// If even one module is out of date, we're done!  UHT does them all in one fell swoop.;
				if( bIsOutOfDate )
				{
					break;
				}
			}

			return bIsOutOfDate;
		}

		/** Updates the intermediate include directory timestamps of all the passed in UObject modules */
		private static void UpdateDirectoryTimestamps(List<UHTModuleInfo> UObjectModules)
		{
			foreach( var Module in UObjectModules )
			{
				string GeneratedCodeDirectory = Path.GetDirectoryName( Module.GeneratedCPPFilenameBase );
				var GeneratedCodeDirectoryInfo = new DirectoryInfo( GeneratedCodeDirectory );

				try
				{
					if (GeneratedCodeDirectoryInfo.Exists)
					{
						if (UnrealBuildTool.RunningRocket())
						{
							// If it is an Engine folder and we are building a rocket project do NOT update the timestamp!
							// @todo Rocket: This contains check is hacky/fragile
							string FullGeneratedCodeDirectory = GeneratedCodeDirectoryInfo.FullName;
							FullGeneratedCodeDirectory = FullGeneratedCodeDirectory.Replace("\\", "/");
							if (FullGeneratedCodeDirectory.Contains("Engine/Intermediate/Build"))
							{
								continue;
							}

							// Skip checking timestamps for engine plugin intermediate headers in Rocket
							PluginInfo Info = Plugins.GetPluginInfoForModule( Module.ModuleName );
							if( Info != null )
							{
								if( Info.LoadedFrom == PluginInfo.LoadedFromType.Engine )
								{
									continue;
								}
							}
						}

						// Touch the include directory since we have technically 'generated' the headers
						// However, the headers might not be touched at all since that would cause the compiler to recompile everything
						// We can't alter the directory timestamp directly, because this may throw exceptions when the directory is
						// open in visual studio or windows explorer, so instead we create a blank file that will change the timestamp for us
						string TimestampFile = GeneratedCodeDirectoryInfo.FullName + Path.DirectorySeparatorChar + @"Timestamp";

						if( !GeneratedCodeDirectoryInfo.Exists )
						{
							GeneratedCodeDirectoryInfo.Create();
						}

						// Save all of the UObject files to a timestamp file.  We'll load these on the next run to see if any new
						// files with UObject classes were deleted, so that we'll know to run UHT even if the timestamps of all
						// of the other source files were unchanged
						{
							var AllUObjectFiles = new List<string>();
							AllUObjectFiles.AddRange( Module.PublicUObjectClassesHeaders.ConvertAll( Item => Item.AbsolutePath ) );
							AllUObjectFiles.AddRange( Module.PublicUObjectHeaders.ConvertAll( Item => Item.AbsolutePath ) );
							AllUObjectFiles.AddRange( Module.PrivateUObjectHeaders.ConvertAll( Item => Item.AbsolutePath ) );
							ResponseFile.Create( TimestampFile, AllUObjectFiles );
						}
					}
				}
				catch (Exception Exception)
				{
					throw new BuildException(Exception, "Couldn't touch header directories: " + Exception.Message);
				}
			}
		}

		/** Run an external exe (and capture the output), given the exe path and the commandline. */
		public static int RunExternalExecutable(string ExePath, string Commandline)
		{
			var ExeInfo = new ProcessStartInfo(ExePath, Commandline);
			Log.TraceVerbose( "RunExternalExecutable {0} {1}", ExePath, Commandline );
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (var GameProcess = Process.Start(ExeInfo))
			{
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += PrintProcessOutputAsync;
				GameProcess.WaitForExit();

				return GameProcess.ExitCode;
			}
		}

		/** Simple function to pipe output asynchronously */
		private static void PrintProcessOutputAsync(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if( !String.IsNullOrEmpty( Event.Data ) )
			{
				Log.TraceInformation( Event.Data );
			}
		}



		/**
		 * Builds and runs the header tool and touches the header directories.
		 * Performs any early outs if headers need no changes, given the UObject modules, tool path, game name, and configuration
		 */
		public static bool ExecuteHeaderToolIfNecessary( UEBuildTarget Target, CPPEnvironment GlobalCompileEnvironment, List<UHTModuleInfo> UObjectModules, string ModuleInfoFileName, ref ECompilationResult UHTResult )
		{
			using (ProgressWriter Progress = new ProgressWriter("Generating code...", false))
			{
				// We never want to try to execute the header tool when we're already trying to build it!
				var bIsBuildingUHT = Target.GetTargetName().Equals( "UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase );

				var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);
				var CppPlatform = BuildPlatform.GetCPPTargetPlatform(Target.Platform);
				var ToolChain = UEToolChain.GetPlatformToolChain(CppPlatform);
				var RootLocalPath  = Path.GetFullPath(ProjectFileGenerator.RootRelativePath);


				// ensure the headers are up to date
				bool bUHTNeedsToRun = (UEBuildConfiguration.bForceHeaderGeneration == true || AreGeneratedCodeFilesOutOfDate(UObjectModules));
				if( bUHTNeedsToRun || UnrealBuildTool.IsGatheringBuild )
				{
					// Since code files are definitely out of date, we'll now finish computing information about the UObject modules for UHT.  We
					// want to save this work until we know that UHT actually needs to be run to speed up best-case iteration times.
					if( UnrealBuildTool.IsGatheringBuild )		// In assembler-only mode, PCH info is loaded from our UBTMakefile!
					{
						foreach( var UHTModuleInfo in UObjectModules )
						{
							// Only cache the PCH name if we don't already have one.  When running in 'gather only' mode, this will have already been cached
							if( string.IsNullOrEmpty( UHTModuleInfo.PCH ) )
							{
								UHTModuleInfo.PCH = "";

								// We need to figure out which PCH header this module is including, so that UHT can inject an include statement for it into any .cpp files it is synthesizing
								var DependencyModuleCPP = (UEBuildModuleCPP)Target.GetModuleByName( UHTModuleInfo.ModuleName );
								var ModuleCompileEnvironment = DependencyModuleCPP.CreateModuleCompileEnvironment(GlobalCompileEnvironment);
								DependencyModuleCPP.CachePCHUsageForModuleSourceFiles(ModuleCompileEnvironment);
								if (DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile != null)
								{
									UHTModuleInfo.PCH = DependencyModuleCPP.ProcessedDependencies.UniquePCHHeaderFile.AbsolutePath;
								}
							}
						}
					}
				}

				// @todo fastubt: @todo ubtmake: Optimization: Ideally we could avoid having to generate this data in the case where UHT doesn't even need to run!  Can't we use the existing copy?  (see below use of Manifest)
				UHTManifest Manifest = new UHTManifest(Target, RootLocalPath, ToolChain.ConvertPath(RootLocalPath + '\\'), UObjectModules);

				if( !bIsBuildingUHT && bUHTNeedsToRun )
				{
					// Always build UnrealHeaderTool if header regeneration is required, unless we're running within a Rocket ecosystem or hot-reloading
					if (UnrealBuildTool.RunningRocket() == false && 
						UEBuildConfiguration.bDoNotBuildUHT == false &&
						UEBuildConfiguration.bHotReloadFromIDE == false &&
						!( !UnrealBuildTool.IsGatheringBuild && UnrealBuildTool.IsAssemblingBuild ) )	// If running in "assembler only" mode, we assume UHT is already up to date for much faster iteration!
					{
						// If it is out of date or not there it will be built.
						// If it is there and up to date, it will add 0.8 seconds to the build time.
						Log.TraceInformation("Building UnrealHeaderTool...");

						var UBTArguments = new StringBuilder();

						UBTArguments.Append( "UnrealHeaderTool" );

						// Which desktop platform do we need to compile UHT for?
						UBTArguments.Append(" " + BuildHostPlatform.Current.Platform.ToString());
						// NOTE: We force Development configuration for UHT so that it runs quickly, even when compiling debug
						UBTArguments.Append( " " + UnrealTargetConfiguration.Development.ToString() );

						// NOTE: We disable mutex when launching UBT from within UBT to compile UHT
						UBTArguments.Append( " -NoMutex" );

						if (UnrealBuildTool.CommandLineContains("-noxge"))
						{
							UBTArguments.Append(" -noxge");
						}
						
						if ( RunExternalExecutable( UnrealBuildTool.GetUBTPath(), UBTArguments.ToString() ) != 0 )
						{ 
							return false;
						}
					}

					Progress.Write(1, 3);

					var ActualTargetName = String.IsNullOrEmpty( Target.GetTargetName() ) ? "UE4" : Target.GetTargetName();
					Log.TraceInformation( "Parsing headers for {0}", ActualTargetName );

					string HeaderToolPath = GetHeaderToolPath();
					if (!File.Exists(HeaderToolPath))
					{
						throw new BuildException( "Unable to generate headers because UnrealHeaderTool binary was not found ({0}).", Path.GetFullPath( HeaderToolPath ) );
					}

					// Disable extensions when serializing to remove the $type fields
					Directory.CreateDirectory(Path.GetDirectoryName(ModuleInfoFileName));
					System.IO.File.WriteAllText(ModuleInfoFileName, fastJSON.JSON.Instance.ToJSON(Manifest, new fastJSON.JSONParameters{ UseExtensions = false }));

					string CmdLine = (UnrealBuildTool.HasUProjectFile()) ? "\"" + UnrealBuildTool.GetUProjectFile() + "\"" : Target.GetTargetName();
					CmdLine += " \"" + ModuleInfoFileName + "\" -LogCmds=\"loginit warning, logexit warning, logdatabase error\"";
					if (UnrealBuildTool.RunningRocket())
					{
						CmdLine += " -rocket -installed";
					}

					if (UEBuildConfiguration.bFailIfGeneratedCodeChanges)
					{
						CmdLine += " -FailIfGeneratedCodeChanges";
					}

					Stopwatch s = new Stopwatch();
					s.Start();
					UHTResult = (ECompilationResult) RunExternalExecutable(ExternalExecution.GetHeaderToolPath(), CmdLine);
					s.Stop();

					if (UHTResult != ECompilationResult.Succeeded)
					{
						Log.TraceInformation("Error: Failed to generate code for {0} - error code: {2} ({1})", ActualTargetName, (int) UHTResult, UHTResult.ToString());
						return false;
					}

					Log.TraceInformation( "Reflection code generated for {0}", ActualTargetName );
					if( BuildConfiguration.bPrintPerformanceInfo )
					{
						Log.TraceInformation( "UnrealHeaderTool took {1}", ActualTargetName, (double)s.ElapsedMilliseconds/1000.0 );
					}

					// Now that UHT has successfully finished generating code, we need to update all cached FileItems in case their last write time has changed.
					// Otherwise UBT might not detect changes UHT made.
					DateTime StartTime = DateTime.UtcNow;
					FileItem.ResetInfos();
					double ResetDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
					Log.TraceVerbose("FileItem.ResetInfos() duration: {0}s", ResetDuration);
				}
				else
				{
					Log.TraceVerbose( "Generated code is up to date." );
				}

				Progress.Write(2, 3);

				// There will never be generated code if we're building UHT, so this should never be called.
				if (!bIsBuildingUHT)
				{
					// Allow generated code to be sync'd to remote machines if needed. This needs to be done even if UHT did not run because
					// generated headers include other generated headers using absolute paths which in case of building remotely are already
					// the remote machine absolute paths. Because of that parsing headers will not result in finding all includes properly.
					// @todo ubtmake: Need to figure out what this does in the assembler case, and whether we need to run it
					ToolChain.PostCodeGeneration(Manifest);
				}

				// touch the directories
				UpdateDirectoryTimestamps(UObjectModules);

				Progress.Write(3, 3);
			}
			return true;
		}
	}
}
