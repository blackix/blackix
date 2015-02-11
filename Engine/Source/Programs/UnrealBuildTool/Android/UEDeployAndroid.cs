﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool.Android
{
	public class UEDeployAndroid : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			string NDKPath = Environment.GetEnvironmentVariable("ANDROID_HOME");

			// we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				Log.TraceVerbose("        Unable to register Android deployment class because the ANDROID_HOME environment variable isn't set or points to something that doesn't exist");
			}
			else
			{
				UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.Android, this);
			}
		}

		/** Internal usage for GetApiLevel */
		private static List<string> PossibleApiLevels = null;

		/** Simple function to pipe output asynchronously */
		private static void ParseApiLevel(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				string Line = Event.Data;
				if (Line.StartsWith("id:"))
				{
					// the line should look like: id: 1 or "android-19"
					string[] Tokens = Line.Split("\"".ToCharArray());
					if (Tokens.Length >= 2)
					{
						PossibleApiLevels.Add(Tokens[1]);
					}
				}
			}
		}

		static private string CachedSDKLevel = null;
		private static string GetSdkApiLevel()
		{
			if (CachedSDKLevel == null)
			{
				// ask the .ini system for what version to use
				ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
				string SDKLevel;
				Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "SDKAPILevel", out SDKLevel);

				// if we want to use whatever version the ndk uses, then use that
				if (SDKLevel == "matchndk")
				{
					SDKLevel = AndroidToolChain.GetNdkApiLevel();
				}

				// run a command and capture output
				if (SDKLevel == "latest")
				{
					// we expect there to be one, so use the first one
					string AndroidCommandPath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/tools/android" + (Utils.IsRunningOnMono ? "" : ".bat"));

					var ExeInfo = new ProcessStartInfo(AndroidCommandPath, "list targets");
					ExeInfo.UseShellExecute = false;
					ExeInfo.RedirectStandardOutput = true;
					using (var GameProcess = Process.Start(ExeInfo))
					{
						PossibleApiLevels = new List<string>();
						GameProcess.BeginOutputReadLine();
						GameProcess.OutputDataReceived += ParseApiLevel;
						GameProcess.WaitForExit();
					}

					if (PossibleApiLevels != null && PossibleApiLevels.Count > 0)
					{
						SDKLevel = AndroidToolChain.GetLargestApiLevel(PossibleApiLevels.ToArray());
					}
					else
					{
						throw new BuildException("Can't make an APK without an API installed (see \"android.bat list targets\")");
					}
				}

				Console.WriteLine("Building Java with SDK API level '{0}'", SDKLevel);
				CachedSDKLevel = SDKLevel;
			}

			return CachedSDKLevel;
		}

		public static bool PackageDataInsideApk(bool bDisallowPackagingDataInApk, ConfigCacheIni Ini=null)
		{
			if (bDisallowPackagingDataInApk)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
			}

			// we check this a lot, so make it easy 
			bool bPackageDataInsideApk;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageDataInsideApk", out bPackageDataInsideApk);

			return bPackageDataInsideApk;
		}

		private static string GetAntPath()
		{
			// look up an ANT_HOME env var
			string AntHome = Environment.GetEnvironmentVariable("ANT_HOME");
			if (!string.IsNullOrEmpty(AntHome) && Directory.Exists(AntHome))
			{
				string AntPath = AntHome + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
				// use it if found
				if (File.Exists(AntPath))
				{
					return AntPath;
				}
			}

			// otherwise, look in the eclipse install for the ant plugin (matches the unzipped Android ADT from Google)
			string PluginDir = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/../eclipse/plugins");
			if (Directory.Exists(PluginDir))
			{
				string[] Plugins = Directory.GetDirectories(PluginDir, "org.apache.ant*");
				// use the first one with ant.bat
				if (Plugins.Length > 0)
				{
					foreach (string PluginName in Plugins)
					{
						string AntPath = PluginName + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
						// use it if found
						if (File.Exists(AntPath))
						{
							return AntPath;
						}
					}
				}
			}

			throw new BuildException("Unable to find ant.bat (via %ANT_HOME% or %ANDROID_HOME%/../eclipse/plugins/org.apache.ant*");
		}


		private static void CopyFileDirectory(string SourceDir, string DestDir, Dictionary<string,string> Replacements)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				// skip template files
				if (Path.GetExtension(Filename) == ".template")
				{
					continue;
				}

				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));
				if (File.Exists(DestFilename))
				{
					File.Delete(DestFilename);
				}

				// make the subdirectory if needed
				string DestSubdir = Path.GetDirectoryName(DestFilename);
				if (!Directory.Exists(DestSubdir))
				{
					Directory.CreateDirectory(DestSubdir);
				}

				// some files are handled specially
				string Ext = Path.GetExtension(Filename);
				if (Ext == ".xml")
				{
					string Contents = File.ReadAllText(Filename);

					// replace some varaibles
					foreach (var Pair in Replacements)
					{
						Contents = Contents.Replace(Pair.Key, Pair.Value);
					}

					// write out file
					File.WriteAllText(DestFilename, Contents);
				}
				else
				{
					File.Copy(Filename, DestFilename);

					// remove any read only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
			}
		}

		private static void DeleteDirectory(string InPath, string SubDirectoryToKeep="")
		{
			// skip the dir we want to
			if (String.Compare(Path.GetFileName(InPath), SubDirectoryToKeep, true) == 0)
			{
				return;
			}
			
			// delete all files in here
            string[] Files;
            try
            {
                Files = Directory.GetFiles(InPath);
            }
            catch (Exception)
            {
                // directory doesn't exist so all is good
                return;
            }
			foreach (string Filename in Files)
			{
				try
				{
					// remove any read only flags
					FileInfo FileInfo = new FileInfo(Filename);
					FileInfo.Attributes = FileInfo.Attributes & ~FileAttributes.ReadOnly;
					FileInfo.Delete();
				}
				catch (Exception)
				{
					Log.TraceInformation("Failed to delete all files in directory {0}. Continuing on...", InPath);
				}
			}

			string[] Dirs = Directory.GetDirectories(InPath, "*.*", SearchOption.TopDirectoryOnly);
			foreach (string Dir in Dirs)
			{
				DeleteDirectory(Dir, SubDirectoryToKeep);
				// try to delete the directory, but allow it to fail (due to SubDirectoryToKeep still existing)
				try
				{
					Directory.Delete(Dir);
				}
				catch (Exception)
				{
					// do nothing
				}
			}
		}

        public string GetUE4BuildFilePath(String EngineDirectory)
        {
            return Path.GetFullPath(Path.Combine(EngineDirectory, "Build/Android/Java"));
        }

		private static string GetNDKArch(string UE4Arch)
		{
			switch (UE4Arch)
			{
				case "-armv7": return "armeabi-v7a";
				case "-x86": return "x86";

				default: throw new BuildException("Unknown UE4 architecture {0}", UE4Arch);
			}
		}

		public static string GetUE4Arch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "armeabi-v7a": return "-armv7";
				case "x86":			return "-x86";

				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
			}
		}

		private static void CopySTL(string UE4BuildPath, string UE4Arch)
		{
			string Arch = GetNDKArch(UE4Arch);

			string GccVersion = "4.8";
			if (!Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.6";
			}

			// copy it in!
			string SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/") + GccVersion + "/libs/" + Arch + "/libgnustl_shared.so";
			string FinalSTLSOName = UE4BuildPath + "/libs/" + Arch + "/libgnustl_shared.so";
			Directory.CreateDirectory(Path.GetDirectoryName(FinalSTLSOName));
			File.Copy(SourceSTLSOName, FinalSTLSOName);
		}

		//@TODO: To enable the NVIDIA Gfx Debugger
		private static void CopyGfxDebugger(string UE4BuildPath, string UE4Arch)
		{
//			string Arch = GetNDKArch(UE4Arch);
//			Directory.CreateDirectory(UE4BuildPath + "/libs/" + Arch);
//			File.Copy("E:/Dev2/UE4-Clean/Engine/Source/ThirdParty/NVIDIA/TegraGfxDebugger/libNvPmApi.Core.so", UE4BuildPath + "/libs/" + Arch + "/libNvPmApi.Core.so");
//			File.Copy("E:/Dev2/UE4-Clean/Engine/Source/ThirdParty/NVIDIA/TegraGfxDebugger/libTegra_gfx_debugger.so", UE4BuildPath + "/libs/" + Arch + "/libTegra_gfx_debugger.so");
		}

		private static void RunCommandLineProgramAndThrowOnError(string WorkingDirectory, string Command, string Params, string OverrideDesc=null, bool bUseShellExecute=false)
		{
			if (OverrideDesc == null)
			{
				Log.TraceInformation("\nRunning: " + Command + " " + Params);
			}
			else if (OverrideDesc != "")
			{
				Log.TraceInformation(OverrideDesc);
				Log.TraceVerbose("\nRunning: " + Command + " " + Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			// android bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private void UpdateProjectProperties(string UE4BuildPath, string ProjectName)
		{
			Log.TraceInformation("\n===={0}====UPDATING BUILD CONFIGURATION FILES====================================================", DateTime.Now.ToString());

			// get all of the libs (from engine + project)
			string JavaLibsDir = Path.Combine(UE4BuildPath, "JavaLibs");
			string[] LibDirs = Directory.GetDirectories(JavaLibsDir);

			// get existing project.properties lines (if any)
			string ProjectPropertiesFile = Path.Combine(UE4BuildPath, "project.properties");
			string[] PropertiesLines = new string[] { };
			if (File.Exists(ProjectPropertiesFile))
			{
				PropertiesLines = File.ReadAllLines(ProjectPropertiesFile);
			}

			// figure out how many libraries were already listed (if there were more than this listed, then we need to start the file over, because we need to unreference a library)
			int NumOutstandingAlreadyReferencedLibs = 0;
			foreach (string Line in PropertiesLines)
			{
				if (Line.StartsWith("android.library.reference."))
				{
					NumOutstandingAlreadyReferencedLibs++;
				}
			}

			// now go through each one and verify they are listed in project properties, and if not, add them
			List<string> LibsToBeAdded = new List<string>();
			foreach (string LibDir in LibDirs)
			{
				// put it in terms of the subdirectory that would be in the project.properties
				string RelativePath = "JavaLibs/" + Path.GetFileName(LibDir);

				// now look for this in the existing file
				bool bWasReferencedAlready = false;
				foreach (string Line in PropertiesLines)
				{
					if (Line.StartsWith("android.library.reference.") && Line.EndsWith(RelativePath))
					{
						// this lib was already referenced, don't need to readd
						bWasReferencedAlready = true;
						break;
					}
				}

				if (bWasReferencedAlready)
				{
					// if it was, no further action needed, and count it off
					NumOutstandingAlreadyReferencedLibs--;
				}
				else
				{
					// otherwise, we need to add it to the project properties
					LibsToBeAdded.Add(RelativePath);
				}
			}

			// now at this point, if there are any outstanding already referenced libs, we have too many, so we have to start over
			if (NumOutstandingAlreadyReferencedLibs > 0)
			{
				// @todo android: If a user had a project.properties in the game, NEVER do this
				Log.TraceInformation("There were too many libs already referenced in project.properties, tossing it");
				File.Delete(ProjectPropertiesFile);

				LibsToBeAdded.Clear();
				foreach (string LibDir in LibDirs)
				{
					// put it in terms of the subdirectory that would be in the project.properties
					LibsToBeAdded.Add("JavaLibs/" + Path.GetFileName(LibDir));
				}
			}
			
			// now update the project for each library
			string AndroidCommandPath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/tools/android" + (Utils.IsRunningOnMono ? "" : ".bat"));
			string UpdateCommandLine = "--silent update project --subprojects --name " + ProjectName + " --path . --target " + GetSdkApiLevel();
			foreach (string Lib in LibsToBeAdded)
			{
				string LocalUpdateCommandLine = UpdateCommandLine + " --library " + Lib;

				// make sure each library has a build.xml - --subprojects doesn't create build.xml files, but it will create project.properties
				// and later code needs each lib to have a build.xml
				if (!File.Exists(Path.Combine(Lib, "build.xml")))
				{
					RunCommandLineProgramAndThrowOnError(UE4BuildPath, AndroidCommandPath, "--silent update lib-project --path " + Lib + " --target " + GetSdkApiLevel(), "");
				}
				RunCommandLineProgramAndThrowOnError(UE4BuildPath, AndroidCommandPath, LocalUpdateCommandLine, "Updating project.properties, local.properties, and build.xml...");
			}

		}


		private string GetAllBuildSettings(string BuildPath, bool bForDistribution, bool bMakeSeparateApks, bool bPackageDataInsideApk)
		{
			// make the settings string - this will be char by char compared against last time
			StringBuilder CurrentSettings = new StringBuilder();
			CurrentSettings.AppendLine(string.Format("NDKROOT={0}", Environment.GetEnvironmentVariable("NDKROOT")));
			CurrentSettings.AppendLine(string.Format("ANDROID_HOME={0}", Environment.GetEnvironmentVariable("ANDROID_HOME")));
			CurrentSettings.AppendLine(string.Format("ANT_HOME={0}", Environment.GetEnvironmentVariable("ANT_HOME")));
			CurrentSettings.AppendLine(string.Format("JAVA_HOME={0}", Environment.GetEnvironmentVariable("JAVA_HOME")));
			CurrentSettings.AppendLine(string.Format("SDKVersion={0}", GetSdkApiLevel()));
			CurrentSettings.AppendLine(string.Format("bForDistribution={0}", bForDistribution));
			CurrentSettings.AppendLine(string.Format("bMakeSeparateApks={0}", bMakeSeparateApks));
			CurrentSettings.AppendLine(string.Format("bPackageDataInsideApk={0}", bPackageDataInsideApk));

			// all AndroidRuntimeSettings ini settings in here
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
			ConfigCacheIni.IniSection Section = Ini.FindSection("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
			if (Section != null)
			{
				foreach (string Key in Section.Keys)
				{
					List<string> Values = Section[Key];
					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			Section = Ini.FindSection("/Script/AndroidPlatformEditor.AndroidSDKSettings");
			if (Section != null)
			{
				foreach (string Key in Section.Keys)
				{
					List<string> Values = Section[Key];
					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			string[] Arches = AndroidToolChain.GetAllArchitectures();
			foreach (string Arch in Arches)
			{
				CurrentSettings.AppendFormat("Arch={0}{1}", Arch, Environment.NewLine);
			}

			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();
			foreach (string GPUArch in GPUArchitectures)
			{
				CurrentSettings.AppendFormat("GPUArch={0}{1}", GPUArch, Environment.NewLine);
			}

			return CurrentSettings.ToString();
		}

		private bool CheckDependencies(string ProjectName, string ProjectDirectory, string UE4BuildFilesPath, string GameBuildFilesPath, string EngineDirectory, 
			string CookFlavor, string OutputPath, string UE4BuildPath, bool bMakeSeparateApks, bool bPackageDataInsideApk)
		{
			string[] Arches = AndroidToolChain.GetAllArchitectures();
			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();

			// check all input files (.so, java files, .ini files, etc)
			bool bAllInputsCurrent = true;
			foreach (string Arch in Arches)
			{
				foreach (string GPUArch in GPUArchitectures)
				{
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArch);
					// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
					string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

					// if we making multiple Apks, we need to put the architecture into the name
					if (bMakeSeparateApks)
					{
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArch);
					}

					// check to see if it's out of date before trying the slow make apk process (look at .so and all Engine and Project build files to be safe)
					List<String> InputFiles = new List<string>();
					InputFiles.Add(SourceSOName);
					InputFiles.AddRange(Directory.EnumerateFiles(UE4BuildFilesPath, "*.*", SearchOption.AllDirectories));
					if (Directory.Exists(GameBuildFilesPath))
					{
						InputFiles.AddRange(Directory.EnumerateFiles(GameBuildFilesPath, "*.*", SearchOption.AllDirectories));
					}

					// rebuild if .pak files exist for  in APK case
					if (bPackageDataInsideApk)
					{
						string PAKFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + "/" + ProjectName + "/Content/Paks";
						if (Directory.Exists(PAKFileLocation))
						{
							var PakFiles = Directory.EnumerateFiles(PAKFileLocation, "*.pak", SearchOption.TopDirectoryOnly);
							foreach (var Name in PakFiles)
							{
								InputFiles.Add(Name);
							}
						}
					}

					// look for any newer input file
					DateTime ApkTime = File.GetLastWriteTimeUtc(DestApkName);
					foreach (var InputFileName in InputFiles)
					{
						if (File.Exists(InputFileName))
						{
							// skip .log files
							if (Path.GetExtension(InputFileName) == ".log")
							{
								continue;
							}
							DateTime InputFileTime = File.GetLastWriteTimeUtc(InputFileName);
							if (InputFileTime.CompareTo(ApkTime) > 0)
							{
								bAllInputsCurrent = false;
								Log.TraceInformation("{0} is out of date due to newer input file {1}", DestApkName, InputFileName);
								break;
							}
						}
					}
				}
			}

			return bAllInputsCurrent;
		}

		private int ConvertDepthBufferIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "bits16":
					return 16;
				case "bits24":
					return 24;
				case "bits32":
					return 32;
				default:
					return 0;
			}
		}

		private string ConvertOrientationIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "portrait":
					return "portrait";
				case "reverseportrait":
					return "reversePortrait";
				case "sensorportrait":
					return "sensorPortrait";
				case "landscape":
					return "landscape";
				case "reversepandscape":
					return "reverseLandscape";
				case "sensorpandscape":
					return "sensorLandscape";
				case "sensor":
					return "sensor";
				case "fullsensor":
					return "fullSensor";
				default:
					return "landscape";
			}
		}

		private string GenerateManifest(string ProjectName, bool bIsForDistribution, bool bPackageDataInsideApk)
		{
			// ini file to get settings from
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
			string PackageName;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "PackageName", out PackageName);
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);
			string DepthBufferPreference;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "DepthBufferPreference", out DepthBufferPreference);
			int MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
			int StoreVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);
			string VersionDisplayName;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);
			string ExtraActivitySettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivitySettings", out ExtraActivitySettings);
			string ExtraApplicationSettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationSettings", out ExtraApplicationSettings);
			List<string> ExtraPermissions;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraPermissions", out ExtraPermissions);
			bool bPackageForGearVR;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageForGearVR", out bPackageForGearVR);

			// replace some variables
			PackageName = PackageName.Replace("[PROJECT]", ProjectName);

			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			Text.AppendLine("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"");
			Text.AppendLine(string.Format("          package=\"{0}\"", PackageName));
			Text.AppendLine(string.Format("          android:versionCode=\"{0}\"", StoreVersion));
			Text.AppendLine(string.Format("          android:versionName=\"{0}\">", VersionDisplayName));
			Text.AppendLine("");

			Text.AppendLine("\t<!-- Application Definition -->");
			Text.AppendLine("\t<application android:label=\"@string/app_name\"");
			Text.AppendLine("\t             android:icon=\"@drawable/icon\"");
			Text.AppendLine("\t             android:hasCode=\"true\">");
			Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.GameActivity\"");
			Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
			if (!bPackageForGearVR)
			{
				// normal application settings
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\"");
				Text.AppendLine("\t\t          android:configChanges=\"orientation|keyboardHidden\"");
			}
			else
			{
				// GearVR
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.Black.NoTitleBar.Fullscreen\"");
				Text.AppendLine("\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
				if (bIsForDistribution)
				{
					Text.AppendLine("\t\t          android:excludeFromRecents=\"true\"");
				}
			}
			Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
			Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
			Text.AppendLine(string.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
			Text.AppendLine("\t\t\t<meta-data android:name=\"android.app.lib_name\" android:value=\"UE4\"/>");
			Text.AppendLine("\t\t\t<intent-filter>");
			Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
			Text.AppendLine(string.Format("\t\t\t\t<category android:name=\"android.intent.category.{0}\" />", (bPackageForGearVR && bIsForDistribution) ? "INFO" : "LAUNCHER"));
			Text.AppendLine("\t\t\t</intent-filter>");
			if (!string.IsNullOrEmpty(ExtraActivitySettings))
			{
				ExtraActivitySettings = ExtraActivitySettings.Replace("\\n", "\n");
				foreach (string Line in ExtraActivitySettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			Text.AppendLine("\t\t</activity>");
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.DepthBufferPreference\" android:value=\"{0}\"/>", ConvertDepthBufferIniValue(DepthBufferPreference)));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bPackageDataInsideApk\" android:value=\"{0}\"/>", bPackageDataInsideApk ? "true" : "false"));
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.games.APP_ID\"");
			Text.AppendLine("\t\t           android:value=\"@string/app_id\" />");
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.version\"");
			Text.AppendLine("\t\t           android:value=\"@integer/google_play_services_version\" />");
			Text.AppendLine("\t\t<activity android:name=\"com.google.android.gms.ads.AdActivity\"");
			Text.AppendLine("\t\t          android:configChanges=\"keyboard|keyboardHidden|orientation|screenLayout|uiMode|screenSize|smallestScreenSize\"/>");
			if (bPackageForGearVR)
			{
				Text.AppendLine("\t\t<meta-data android:name=\"com.samsung.android.vr.application.mode\"");
				Text.AppendLine("\t\t           android:value=\"vr_only\" />");
				Text.AppendLine("\t\t<activity android:name=\"com.oculusvr.vrlib.PlatformActivity\"");
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.Black.NoTitleBar.Fullscreen\"");
				Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
				Text.AppendLine("\t\t          android:screenOrientation=\"landscape\"");
				Text.AppendLine("\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"/>");
			}
			if (!string.IsNullOrEmpty(ExtraApplicationSettings))
			{
				ExtraApplicationSettings = ExtraApplicationSettings.Replace("\\n", "\n");
				foreach (string Line in ExtraApplicationSettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}
			Text.AppendLine("\t</application>");

			Text.AppendLine("");
			Text.AppendLine("\t<!-- Requirements -->");
			// need just the number part of the sdk
			Text.AppendLine(string.Format("\t<uses-sdk android:minSdkVersion=\"{0}\"/>", MinSDKVersion));
			Text.AppendLine("\t<uses-feature android:glEsVersion=\"0x00020000\" android:required=\"true\" />");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.INTERNET\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.WAKE_LOCK\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.READ_PHONE_STATE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.CHECK_LICENSE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_WIFI_STATE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.MODIFY_AUDIO_SETTINGS\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"android.permission.GET_ACCOUNTS\"/>");
            Text.AppendLine("\t<uses-permission android:name=\"android.permission.VIBRATE\"/>");
			Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.BILLING\"/>");
//			Text.AppendLine("\t<uses-permission android:name=\"android.permission.DISABLE_KEYGUARD\"/>");
			if (bPackageForGearVR)
			{
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.CAMERA\"/>");
				Text.AppendLine("\t<uses-feature android:name=\"android.hardware.camera\"/>");
				Text.AppendLine("\t<uses-feature android:name=\"android.hardware.usb.host\"/>");
			}
			if (ExtraPermissions != null)
			{
				foreach (string Permission in ExtraPermissions)
				{
					Text.AppendLine(string.Format("\t<uses-permission android:name=\"{0}\"/>", Permission));
				}
			}
			Text.AppendLine("</manifest>");
			return Text.ToString();
		}

		private void MakeApk(string ProjectName, string ProjectDirectory, string OutputPath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bMakeSeparateApks, bool bIncrementalPackage, bool bDisallowPackagingDataInApk)
		{
			Log.TraceInformation("\n===={0}====PREPARING TO MAKE APK=================================================================", DateTime.Now.ToString());

			// cache some tools paths
			string NDKBuildPath = Environment.ExpandEnvironmentVariables("%NDKROOT%/ndk-build" + (Utils.IsRunningOnMono ? "" : ".cmd"));

			// set up some directory info
			string IntermediateAndroidPath = Path.Combine(ProjectDirectory, "Intermediate/Android/");
			string UE4BuildPath = Path.Combine(IntermediateAndroidPath, "APK");
			string UE4BuildFilesPath = GetUE4BuildFilePath(EngineDirectory);
			string GameBuildFilesPath = Path.Combine(ProjectDirectory, "Build/Android");
	
			string[] Arches = AndroidToolChain.GetAllArchitectures();
			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();
//			int NumArches = Arches.Length * GPUArchitectures.Length;

			// cache if we want data in the Apk
			bool bPackageDataInsideApk = PackageDataInsideApk(bDisallowPackagingDataInApk);

			// check to see if any "meta information" is newer than last time we build
			string CurrentBuildSettings = GetAllBuildSettings(UE4BuildPath, bForDistribution, bMakeSeparateApks, bPackageDataInsideApk);
			string BuildSettingsCacheFile = Path.Combine(UE4BuildPath, "UEBuildSettings.txt");

			// do we match previous build settings?
			bool bBuildSettingsMatch = false;

			string ManifestFile = Path.Combine(UE4BuildPath, "AndroidManifest.xml");
			string NewManifest = GenerateManifest(ProjectName, bForDistribution, bPackageDataInsideApk);
			string OldManifest = File.Exists(ManifestFile) ? File.ReadAllText(ManifestFile) : "";
			if (NewManifest == OldManifest) 
			{
				bBuildSettingsMatch = true;
			}
			else
			{
				Log.TraceInformation("AndroidManifest.xml that was generated is different than last build, forcing repackage.");
			}

			// if the manifest matches, look at other settings stored in a file
			if (bBuildSettingsMatch)
			{
				if (File.Exists(BuildSettingsCacheFile))
				{
					string PreviousBuildSettings = File.ReadAllText(BuildSettingsCacheFile);
					if (PreviousBuildSettings != CurrentBuildSettings)
					{
						bBuildSettingsMatch = false;
						Log.TraceInformation("Previous .apk file(s) were made with different build settings, forcing repackage.");
					}
				}
			}

			// only check input dependencies if the build settings already match
			if (bBuildSettingsMatch)
			{
				// check if so's are up to date against various inputs
				bool bAllInputsCurrent = CheckDependencies(ProjectName, ProjectDirectory, UE4BuildFilesPath, GameBuildFilesPath, 
					EngineDirectory, CookFlavor, OutputPath, UE4BuildPath, bMakeSeparateApks, bPackageDataInsideApk);

				if (bAllInputsCurrent)
				{
					Log.TraceInformation("Output .apk file(s) are up to date (dependencies and build settings are up to date)");
					return;
				}
			}


			// Once for all arches code:

			// make up a dictionary of strings to replace in the Manifest file
			Dictionary<string, string> Replacements = new Dictionary<string, string>();
			Replacements.Add("${EXECUTABLE_NAME}", ProjectName);

			// distribution apps can't be debuggable, so if it was set to true, set it to false:
			if (bForDistribution)
			{
				Replacements.Add("android:debuggable=\"true\"", "android:debuggable=\"false\"");
			}

			if (!bIncrementalPackage)
			{
				// Wipe the Intermediate/Build/APK directory first, except for dexedLibs, because Google Services takes FOREVER to predex, and it almost never changes
				// so allow the ANT checking to win here - if this grows a bit with extra libs, it's fine, it _should_ only pull in dexedLibs it needs
				Log.TraceInformation("Performing complete package - wiping {0}, except for predexedLibs", UE4BuildPath);
				DeleteDirectory(UE4BuildPath, "dexedLibs");
			}

			// If we are packaging for Amazon then we need to copy the  file to the correct location
			Log.TraceInformation("bPackageDataInsideApk = {0}", bPackageDataInsideApk);
			if (bPackageDataInsideApk)
			{
				string ObbFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".obb";
				Console.WriteLine("Obb location {0}", ObbFileLocation);
				string ObbFileDestination = UE4BuildPath + "/assets";
				Console.WriteLine("Obb destination location {0}", ObbFileDestination);
				if (File.Exists(ObbFileLocation))
				{
					Directory.CreateDirectory(UE4BuildPath);
					Directory.CreateDirectory(ObbFileDestination);
					Console.WriteLine("Obb file exists...");
					var DestFileName = Path.Combine(ObbFileDestination, "main.obb.png"); // Need a rename to turn off compression
					var SrcFileName = ObbFileLocation;
					if (!File.Exists(DestFileName) || File.GetLastWriteTimeUtc(DestFileName) < File.GetLastWriteTimeUtc(SrcFileName))
					{
						Console.WriteLine("Copying {0} to {1}", SrcFileName, DestFileName);
						File.Copy(SrcFileName, DestFileName);
					}
				}
			}

			//Copy build files to the intermediate folder in this order (later overrides earlier):
			//	- Shared Engine
			//  - Shared Engine NoRedist (for Epic secret files)
			//  - Game
			//  - Game NoRedist (for Epic secret files)
			CopyFileDirectory(UE4BuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);

			// in case the game had an AndroidManifest.xml file, we overwrite it now with the generated one
			File.WriteAllText(ManifestFile, NewManifest);

			// update metadata files (like project.properties, build.xml) if we are missing a build.xml or if we just overwrote project.properties with a bad version in it (from game/engine dir)
			UpdateProjectProperties(UE4BuildPath, ProjectName);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			// now make the apk(s)
			string FinalNdkBuildABICommand = "";
			for (int ArchIndex = 0; ArchIndex < Arches.Length; ArchIndex++)
			{
				string Arch = Arches[ArchIndex];
				for (int GPUArchIndex = 0; GPUArchIndex < GPUArchitectures.Length; GPUArchIndex++)
				{
					Log.TraceInformation("\n===={0}====PREPARING NATIVE CODE=================================================================", DateTime.Now.ToString());

					string GPUArchitecture = GPUArchitectures[GPUArchIndex];
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArchitecture);
					// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
					string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

					// if we making multiple Apks, we need to put the architecture into the name
					if (bMakeSeparateApks)
					{
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArchitecture);
					}

					// Copy the generated .so file from the binaries directory to the jni folder
					if (!File.Exists(SourceSOName))
					{
						throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
					}
					if (!Directory.Exists(UE4BuildPath + "/jni"))
					{
						throw new BuildException("Can't make an APK without the jni directory [{0}/jni]", UE4BuildFilesPath);
					}

					// Use ndk-build to do stuff and move the .so file to the lib folder (only if NDK is installed)
					string FinalSOName = "";
					if (File.Exists(NDKBuildPath))
					{
						string LibDir = UE4BuildPath + "/jni/" + GetNDKArch(Arch);
						Directory.CreateDirectory(LibDir);

						// copy the binary to the standard .so location
						FinalSOName = LibDir + "/libUE4.so";
						File.Copy(SourceSOName, FinalSOName, true);

						FinalNdkBuildABICommand += GetNDKArch(Arch) + " ";
					}
					else
					{
						// if no NDK, we don't need any of the debugger stuff, so we just copy the .so to where it will end up
						FinalSOName = UE4BuildPath + "/libs/" + GetNDKArch(Arch) + "/libUE4.so";
						Directory.CreateDirectory(Path.GetDirectoryName(FinalSOName));
						File.Copy(SourceSOName, FinalSOName);
					}

					// now do final stuff per apk (or after all .so's for a shared .apk)
					if (bMakeSeparateApks || (ArchIndex == Arches.Length - 1 && GPUArchIndex == GPUArchitectures.Length - 1))
					{
						// always delete libs up to this point so fat binaries and incremental builds work together (otherwise we might end up with multiple
						// so files in an apk that doesn't want them)
						// note that we don't want to delete all libs, just the ones we copied (
						foreach (string Lib in Directory.EnumerateFiles(UE4BuildPath + "/libs", "libUE4*.so", SearchOption.AllDirectories))
						{
							File.Delete(Lib);
						}

						// if we need to run ndk-build, do it now (if making a shared .apk, we need to wait until all .libs exist)
						if (!string.IsNullOrEmpty(FinalNdkBuildABICommand))
						{
							string CommandLine = "APP_ABI=\"" + FinalNdkBuildABICommand + "\"";
							if (!bForDistribution)
							{
								CommandLine += " NDK_DEBUG=1";
							}
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, NDKBuildPath, CommandLine, "Preparing native code for debugging...", true);

							// next loop we don't want to redo these ABIs
							FinalNdkBuildABICommand = "";
						}
	
						// after ndk-build is called, we can now copy in the stl .so (ndk-build deletes old files)
						// copy libgnustl_shared.so to library (use 4.8 if possible, otherwise 4.6)
						if (bMakeSeparateApks)
						{
							CopySTL(UE4BuildPath, Arch);
							CopyGfxDebugger(UE4BuildPath, Arch);
						}
						else
						{
							foreach (string InnerArch in Arches)
							{
								CopySTL(UE4BuildPath, InnerArch);
								CopyGfxDebugger(UE4BuildPath, InnerArch);
							}
						}


						// remove any read only flags
						FileInfo DestFileInfo = new FileInfo(FinalSOName);
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;

						Log.TraceInformation("\n===={0}====PERFORMING FINAL APK PACKAGE OPERATION================================================", DateTime.Now.ToString());

						string AntBuildType = "debug";
						string AntOutputSuffix = "-debug";
						if (bForDistribution)
						{
							// this will write out ant.properties with info needed to sign a distribution build
							PrepareToSignApk(UE4BuildPath);
							AntBuildType = "release";
							AntOutputSuffix = "-release";
						}

						// Use ant to build the .apk file
						if (Utils.IsRunningOnMono)
						{
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, "/bin/sh", "-c '\"" + GetAntPath() + "\" -quiet " + AntBuildType + "'", "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)");
						}
						else
						{
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, "cmd.exe", "/c \"" + GetAntPath() + "\" -quiet " + AntBuildType, "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)");
						}

						// make sure destination exists
						Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

						// now copy to the final location
						File.Copy(UE4BuildPath + "/bin/" + ProjectName + AntOutputSuffix + ".apk", DestApkName, true);
					}
				}
			}
		}

		private void PrepareToSignApk(string BuildPath)
		{
			// ini file to get settings from
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
			string KeyAlias, KeyStore, KeyStorePassword, KeyPassword;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

			if (string.IsNullOrEmpty(KeyAlias) || string.IsNullOrEmpty(KeyStore) || string.IsNullOrEmpty(KeyStorePassword))
			{
				throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Andriod tab of Project Settings");
			}

			string[] AntPropertiesLines = new string[4];
			AntPropertiesLines[0] = "key.store=" + KeyStore;
			AntPropertiesLines[1] = "key.alias=" + KeyAlias;
			AntPropertiesLines[2] = "key.store.password=" + KeyStorePassword;
			AntPropertiesLines[3] = "key.alias.password=" + ((string.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_") ? KeyStorePassword : KeyPassword);

			// now write out the properties
			File.WriteAllLines(Path.Combine(BuildPath, "ant.properties"), AntPropertiesLines);
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			// we need to strip architecture from any of the output paths
			string BaseSoName = AndroidToolChain.RemoveArchName(InTarget.OutputPaths[0]);

			// make an apk at the end of compiling, so that we can run without packaging (debugger, cook on the fly, etc)
			MakeApk(InTarget.AppName, InTarget.ProjectDirectory, BaseSoName, BuildConfiguration.RelativeEnginePath, bForDistribution: false, CookFlavor: "", 
				bMakeSeparateApks:ShouldMakeSeparateApks(), bIncrementalPackage:true, bDisallowPackagingDataInApk:false);

			// if we made any non-standard .apk files, the generated debugger settings may be wrong
			if (ShouldMakeSeparateApks() && (InTarget.OutputPaths.Length > 1 || !InTarget.OutputPaths[0].Contains("-armv7-es2")))
			{
				Console.WriteLine("================================================================================================================================");
				Console.WriteLine("Non-default apk(s) have been made: If you are debugging, you will need to manually select one to run in the debugger properties!");
				Console.WriteLine("================================================================================================================================");
			}
			return true;
		}

		public static bool ShouldMakeSeparateApks()
		{
			// @todo android fat binary: Currently, there isn't much utility in merging multiple .so's into a single .apk except for debugging,
			// but we can't properly handle multiple GPU architectures in a single .apk, so we are disabling the feature for now
			// The user will need to manually select the apk to run in their Visual Studio debugger settings (see Override APK in TADP, for instance)
			// If we change this, pay attention to <OverrideAPKPath> in AndroidProjectGenerator
			return true;
			
			// check to see if the project wants separate apks
// 			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
// 			bool bSeparateApks = false;
// 			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSplitIntoSeparateApks", out bSeparateApks);
// 
// 			return bSeparateApks;
		}

		public override bool PrepForUATPackageOrDeploy(string ProjectName, string ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			// note that we cannot allow the data packaged into the APK if we are doing something like Launch On that will not make an obb
			// file and instead pushes files directly via deploy
			MakeApk(ProjectName, ProjectDirectory, ExecutablePath, EngineDirectory, bForDistribution:bForDistribution, CookFlavor:CookFlavor, 
				bMakeSeparateApks:ShouldMakeSeparateApks(), bIncrementalPackage:false, bDisallowPackagingDataInApk:bIsDataDeploy);
			return true;
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Log.TraceInformation(Line.Data);
			}
		}
	}
}
