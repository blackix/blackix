// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;

namespace AutomationTool
{

	public partial class CommandUtils
	{
		/// <summary>
		/// True if UBT has deleted junk files
		/// </summary>
		private static bool bJunkDeleted = false;

		/// <summary>
		/// Runs UBT with the specified commandline. Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">Environment to use.</param>
		/// <param name="CommandLine">Commandline to pass on to UBT.</param>
		/// <param name="LogName">Optional logfile name.</param>
        public static void RunUBT(CommandEnvironment Env, string UBTExecutable, string CommandLine, string LogName = null, Dictionary<string, string> EnvVars = null)
		{
			if (!FileExists(UBTExecutable))
			{
				throw new AutomationException("Unable to find UBT executable: " + UBTExecutable);
			}

			if (GlobalCommandLine.Rocket)
			{
				CommandLine += " -rocket";
			}
			if (GlobalCommandLine.VS2015)
			{
				CommandLine += " -2015";
			}
			if (GlobalCommandLine.VS2013)
			{
				CommandLine += " -2013";
			}
			if (GlobalCommandLine.VS2012)
			{
				CommandLine += " -2012";
			}
			if (!IsBuildMachine && UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac)
			{
				CommandLine += " -nocreatestub";
			}
			CommandLine += " -NoHotReloadFromIDE";
			if (bJunkDeleted || GlobalCommandLine.IgnoreJunk)
			{
				// UBT has already deleted junk files, make sure it doesn't do it again
				CommandLine += " -ignorejunk";
			}
			else
			{
				// UBT will delete junk on first run
				bJunkDeleted = true;
			}

			CommandUtils.RunAndLog(Env, UBTExecutable, CommandLine, LogName, EnvVars: EnvVars);
		}

		/// <summary>
		/// Builds a UBT Commandline.
		/// </summary>
		/// <param name="Project">Unreal project to build (optional)</param>
		/// <param name="Target">Target to build.</param>
		/// <param name="Platform">Platform to build for.</param>
		/// <param name="Config">Configuration to build.</param>
		/// <param name="AdditionalArgs">Additional arguments to pass on to UBT.</param>
		public static string UBTCommandline(string Project, string Target, string Platform, string Config, string AdditionalArgs = "")
		{
			string CmdLine;
			if (String.IsNullOrEmpty(Project))
			{
				CmdLine = String.Format("{0} {1} {2} {3}", Target, Platform, Config, AdditionalArgs);
			}
			else
			{
				CmdLine = String.Format("{0} {1} {2} -Project={3} {4}", Target, Platform, Config, CommandUtils.MakePathSafeToUseWithCommandLine(Project), AdditionalArgs);
			}
			return CmdLine;
		}

		/// <summary>
		/// Builds a target using UBT.  Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">BuildEnvironment to use.</param>
		/// <param name="Project">Unreal project to build (optional)</param>
		/// <param name="Target">Target to build.</param>
		/// <param name="Platform">Platform to build for.</param>
		/// <param name="Config">Configuration to build.</param>
		/// <param name="AdditionalArgs">Additional arguments to pass on to UBT.</param>
		/// <param name="LogName">Optional logifle name.</param>
		public static void RunUBT(CommandEnvironment Env, string UBTExecutable, string Project, string Target, string Platform, string Config, string AdditionalArgs = "", string LogName = null, Dictionary<string, string> EnvVars = null)
		{
			RunUBT(Env, UBTExecutable, UBTCommandline(Project, Target, Platform, Config, AdditionalArgs), LogName, EnvVars);
		}

	}

}