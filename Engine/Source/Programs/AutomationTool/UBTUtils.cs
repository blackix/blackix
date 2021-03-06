// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;

namespace AutomationTool
{

	public partial class CommandUtils
	{
		/// <summary>
		/// Runs UBT with the specified commandline. Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">Environment to use.</param>
		/// <param name="CommandLine">Commandline to pass on to UBT.</param>
		/// <param name="LogName">Optional logfile name.</param>
		public static void RunUBT(CommandEnvironment Env, string UBTExecutable, string CommandLine, string LogName = null)
		{
			if (!FileExists(UBTExecutable))
			{
				throw new AutomationException("Unable to find UBT executable: " + UBTExecutable);
			}

			if (GlobalCommandLine.Rocket)
			{
				CommandLine += " -rocket";
			}
			CommandUtils.RunAndLog(Env, UBTExecutable, CommandLine, LogName);
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
				CmdLine = String.Format("{0} {1} {2} -Project={3} {4}", Target, Platform, Config, Project, AdditionalArgs);
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
		public static void RunUBT(CommandEnvironment Env, string UBTExecutable, string Project, string Target, string Platform, string Config, string AdditionalArgs = "", string LogName = null)
		{
			RunUBT(Env, UBTExecutable, UBTCommandline(Project, Target, Platform, Config, AdditionalArgs), LogName);
		}

	}

}