USE [CrashReporterUE4]
GO
/****** Object:  Table [dbo].[Buggs]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Buggs](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Status] [varchar](64) NULL,
	[TTPID] [varchar](32) NULL,
	[Pattern] [varchar](800) NOT NULL,
	[NumberOfCrashes] [int] NULL,
	[NumberOfUsers] [int] NULL,
	[TimeOfFirstCrash] [datetime] NULL,
	[TimeOfLastCrash] [datetime] NULL,
	[FixedChangeList] [varchar](64) NULL,
	[Description] [varchar](512) NULL,
 CONSTRAINT [PK_Buggs] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'Id'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Set to one of ''Unset'', ''Reviewed'', ''New'', ''Coder'', ''Tester''' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'Status'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'A string description of the associated TTP.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'TTPID'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The callstack pattern, stored as function ids delimited with ''+''.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'Pattern'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The number of associated crashes.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'NumberOfCrashes'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The number of affected users.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'NumberOfUsers'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The UTC of the first found instance of the crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'TimeOfFirstCrash'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The UTC of the most recent instance of this crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'TimeOfLastCrash'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The changelist this group of crashes was reported fixed in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'FixedChangeList'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'A user description of the crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs', @level2type=N'COLUMN',@level2name=N'Description'
GO
/****** Object:  Table [dbo].[UserGroups]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[UserGroups](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Name] [varchar](64) NOT NULL,
 CONSTRAINT [PK_UserGroups] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'UserGroups', @level2type=N'COLUMN',@level2name=N'Id'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the user group.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'UserGroups', @level2type=N'COLUMN',@level2name=N'Name'
GO
/****** Object:  Table [dbo].[FunctionCalls]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[FunctionCalls](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Call] [varchar](max) NULL,
 CONSTRAINT [PK_FunctionCalls] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'FunctionCalls', @level2type=N'COLUMN',@level2name=N'Id'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The full text of the function name.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'FunctionCalls', @level2type=N'COLUMN',@level2name=N'Call'
GO
/****** Object:  Table [dbo].[Users]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Users](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[UserName] [varchar](64) NOT NULL,
	[UserGroupId] [int] NOT NULL,
 CONSTRAINT [PK_Users] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Users', @level2type=N'COLUMN',@level2name=N'Id'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of a user.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Users', @level2type=N'COLUMN',@level2name=N'UserName'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the usergroup this user belongs to.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Users', @level2type=N'COLUMN',@level2name=N'UserGroupId'
GO
/****** Object:  Table [dbo].[PIIMapping]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[PIIMapping](
	[ID] [int] IDENTITY(1,1) NOT NULL,
	[UserNameId] [int] NOT NULL,
	[MachineGUID] [varchar](64) NOT NULL,
	[MachineName] [varchar](64) NOT NULL,
 CONSTRAINT [PK_PIIMapping] PRIMARY KEY CLUSTERED 
(
	[ID] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'PIIMapping', @level2type=N'COLUMN',@level2name=N'ID'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the user for this machine guid.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'PIIMapping', @level2type=N'COLUMN',@level2name=N'UserNameId'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The guid of the machine to be mapped.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'PIIMapping', @level2type=N'COLUMN',@level2name=N'MachineGUID'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the machine for the given guid.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'PIIMapping', @level2type=N'COLUMN',@level2name=N'MachineName'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'A mapping of the anonymous WER machine id to a user name and machine name' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'PIIMapping'
GO
/****** Object:  Table [dbo].[Crashes]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Crashes](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Branch] [varchar](32) NULL,
	[GameName] [varchar](64) NULL,
	[CrashType] [smallint] NULL,
	[Status] [varchar](64) NULL,
	[TTPID] [varchar](32) NULL,
	[FixedChangeList] [varchar](64) NULL,
	[TimeOfCrash] [datetime] NULL,
	[ChangeListVersion] [varchar](32) NULL,
	[PlatformName] [varchar](32) NULL,
	[EngineMode] [varchar](32) NULL,
	[Description] [varchar](512) NULL,
	[RawCallStack] [varchar](max) NULL,
	[SourceContext] [varchar](max) NULL,
	[Pattern] [varchar](800) NULL,
	[CommandLine] [varchar](512) NULL,
	[ComputerName] [varchar](64) NULL,
	[LanguageExt] [varchar](32) NULL,
	[Module] [varchar](128) NULL,
	[BuildVersion] [varchar](64) NULL,
	[BaseDir] [varchar](256) NULL,
	[UserNameId] [int] NOT NULL,
	[HasLogFile] [bit] NULL,
	[HasMiniDumpFile] [bit] NULL,
	[HasVideoFile] [bit] NULL,
	[HasDiagnosticsFile] [bit] NULL,
	[HasMetaData] [bit] NULL,
 CONSTRAINT [PK_Crashes] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The unique key.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Id'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the branch this crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Branch'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the game this crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'GameName'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The type of crash. 1. Crash, 2. Assert. 3. Ensure.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'CrashType'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Set to one of ''Unset'', ''Reviewed'', ''New'', ''Coder'', ''Tester''' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Status'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'A string description of the associated TTP.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'TTPID'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The changelist this crash was reported fixed in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'FixedChangeList'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The UTC of when the crash occurred.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'TimeOfCrash'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The changelist of the build the crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'ChangeListVersion'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the platform. This is the same as the parent folder of the executable.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'PlatformName'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Set to ''Editor'', ''Game'', ''Server'', or ''Commandlet'' depending on how the application was running when the crash occurred.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'EngineMode'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'A user description of the crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Description'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The entire callstack as an unformatted text blob.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'RawCallStack'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Context lines from the source file the crash was detected in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'SourceContext'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The callstack pattern, stored as function ids delimited with ''+''.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Pattern'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The command line used when the application crashed.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'CommandLine'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the computer the crash occurred in. This is either the anonymous machine guid from WER, or the actual machine name mapped via RegisterPII.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'ComputerName'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The language code of the machine the crash occurred on.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'LanguageExt'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The name of the module the crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'Module'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The version of the application the crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'BuildVersion'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The base directory of the application the crash occurred in.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'BaseDir'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the user name.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'UserNameId'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Whether the report has a log file.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'HasLogFile'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Whether the report has a minidump.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'HasMiniDumpFile'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Whether the report has a video file.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'HasVideoFile'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Whether the report has  a diagnostics file.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'HasDiagnosticsFile'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'Whether the report has the WER meta data file.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes', @level2type=N'COLUMN',@level2name=N'HasMetaData'
GO
/****** Object:  Table [dbo].[Buggs_Users]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE TABLE [dbo].[Buggs_Users](
	[BuggId] [int] NOT NULL,
	[UserNameId] [int] NOT NULL
) ON [PRIMARY]
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the id associated with the user.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs_Users', @level2type=N'COLUMN',@level2name=N'BuggId'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the user name associated with this Bugg.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs_Users', @level2type=N'COLUMN',@level2name=N'UserNameId'
GO
/****** Object:  Table [dbo].[Buggs_Crashes]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE TABLE [dbo].[Buggs_Crashes](
	[BuggId] [int] NOT NULL,
	[CrashId] [int] NOT NULL,
 CONSTRAINT [PK_Buggs_Crashes] PRIMARY KEY CLUSTERED 
(
	[BuggId] ASC,
	[CrashId] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the Bugg associated with the crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs_Crashes', @level2type=N'COLUMN',@level2name=N'BuggId'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the crash associated with the Bugg.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Buggs_Crashes', @level2type=N'COLUMN',@level2name=N'CrashId'
GO
/****** Object:  Table [dbo].[Crashes_FunctionCalls]    Script Date: 06/24/2013 14:28:57 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE TABLE [dbo].[Crashes_FunctionCalls](
	[CrashId] [int] NOT NULL,
	[FunctionCallId] [int] NOT NULL,
 CONSTRAINT [PK_Crashes_FunctionCalls] PRIMARY KEY CLUSTERED 
(
	[CrashId] ASC,
	[FunctionCallId] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the crash associated with this function.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes_FunctionCalls', @level2type=N'COLUMN',@level2name=N'CrashId'
GO
EXEC sys.sp_addextendedproperty @name=N'MS_Description', @value=N'The id of the function associated with this crash.' , @level0type=N'SCHEMA',@level0name=N'dbo', @level1type=N'TABLE',@level1name=N'Crashes_FunctionCalls', @level2type=N'COLUMN',@level2name=N'FunctionCallId'
GO
/****** Object:  StoredProcedure [dbo].[UpdateCrashesByPattern]    Script Date: 06/24/2013 14:29:00 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE PROCEDURE [dbo].[UpdateCrashesByPattern]
AS
BEGIN
	-- SET NOCOUNT ON added to prevent extra result sets from
	-- interfering with SELECT statements.
	SET NOCOUNT ON;
	
	--Create Buggs
	MERGE Buggs AS Bugg
	USING 
	(
		SELECT 	TTPID,
			  	Pattern,
			  	NumberOfCrashes,
			  	NumberOfUsers,
			  	TimeOfFirstCrash,
			  	TimeOfLastCrash,
			  	Status,
			  	FixedChangeList
		FROM
		(
			SELECT COUNT(1) AS NumberOfCrashes
				  , MAX(TimeOfCrash) AS TimeOfLastCrash
				  , MIN(TimeOfCrash) AS TimeOfFirstCrash
				  , COUNT(DISTINCT UserNameId) AS NumberOfUsers
				  , COUNT(DISTINCT GameName) AS GameNameCount
				  , MAX(Status) AS Status
				  , MAX(TTPID) AS TTPID
				  , MAX(FixedChangeList) AS FixedChangeList
				  , Pattern
			  FROM [dbo].[Crashes]
			  WHERE
				Pattern IS NOT NULL AND Pattern NOT LIKE ''
			  GROUP BY Pattern
		  ) AS CrashSet
		 WHERE CrashSet.NumberOfCrashes > 1
	) AS Crash 
	ON (Bugg.Pattern = Crash.Pattern)
	WHEN NOT MATCHED BY TARGET
		THEN INSERT 
		(
			TTPID,
			Pattern,
			NumberOfCrashes,
			NumberOfUsers,
			TimeOfFirstCrash,
			TimeOfLastCrash,
			Status,
			FixedChangeList
		) 
		VALUES
		(
			Crash.TTPID,
			Crash.Pattern,
			Crash.NumberOfCrashes,
			Crash.NumberOfUsers,
			Crash.TimeOfFirstCrash,
			Crash.TimeOfLastCrash,
			Crash.Status,
			Crash.FixedChangeList
		) 
	WHEN MATCHED 
		THEN UPDATE SET 
			Bugg.NumberOfCrashes = Crash.NumberOfCrashes, 
			Bugg.TimeOfLastCrash = Crash.TimeOfLastCrash, 
			Bugg.NumberOfUsers = Crash.NumberOfUsers

	OUTPUT $action, Inserted.*, Deleted.*;
		
	/****** Join Buggs and Crashes  ******/
	MERGE dbo.Buggs_Crashes BuggCrash
	USING 
	( 
		SELECT Bugg.Id as BuggId, Crash.Id as CrashId
		FROM [dbo].[Crashes] Crash
		JOIN [dbo].[Buggs] Bugg on (Bugg.Pattern = Crash.Pattern)
		GROUP BY Bugg.Id, Crash.Id
	 ) AS Crash
	 ON BuggCrash.BuggId = Crash.BuggId AND BuggCrash.CrashId = Crash.CrashId
	 WHEN NOT MATCHED BY TARGET
		THEN INSERT 
			(BuggId, CrashId)
		VALUES 
			(Crash.BuggId, Crash.CrashId)	
	 WHEN MATCHED
		THEN UPDATE SET
			BuggCrash.BuggId = Crash.BuggId, 
			BuggCrash.CrashId = Crash.CrashId
			
	OUTPUT $action, Inserted.*, Deleted.*;
	
	/****** Join Buggs_Users and Crashes  ******/
	MERGE dbo.Buggs_Users AS BuggUser
	USING
	(
	  SELECT Bugg.Id as BuggId, Crash.UserNameId
	  FROM [dbo].[Crashes] Crash
	  JOIN [dbo].[Buggs] Bugg on (Bugg.Pattern = Crash.Pattern)
	  GROUP BY Bugg.Id, Crash.UserNameId
	) AS Crash
	ON BuggUser.BuggId = Crash.BuggId AND BuggUser.UserNameId = Crash.UserNameId
	 WHEN NOT MATCHED BY TARGET
		THEN INSERT 
			(BuggId, UserNameId) 
		VALUES 
			(Crash.BuggId, Crash.UserNameId)	
	 WHEN MATCHED
		THEN UPDATE SET
			BuggUser.BuggId = Crash.BuggId, 
			BuggUser.UserNameId = Crash.UserNameId
			
	OUTPUT $action, Inserted.*, Deleted.*;
 
END
GO
/****** Object:  Default [DF_Buggs_Status]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs] ADD  CONSTRAINT [DF_Buggs_Status]  DEFAULT ('New') FOR [Status]
GO
/****** Object:  Default [DF_Buggs_TTPID]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs] ADD  CONSTRAINT [DF_Buggs_TTPID]  DEFAULT ('') FOR [TTPID]
GO
/****** Object:  Default [DF_Buggs_FixedChangeList]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs] ADD  CONSTRAINT [DF_Buggs_FixedChangeList]  DEFAULT ('') FOR [FixedChangeList]
GO
/****** Object:  Default [DF_Buggs_Description]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs] ADD  CONSTRAINT [DF_Buggs_Description]  DEFAULT ('') FOR [Description]
GO
/****** Object:  Default [DF_Buggs_Users_UserNameId]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs_Users] ADD  CONSTRAINT [DF_Buggs_Users_UserNameId]  DEFAULT ((0)) FOR [UserNameId]
GO
/****** Object:  Default [DF_Crashes_Branch]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_Branch]  DEFAULT ('UE4') FOR [Branch]
GO
/****** Object:  Default [DF_Crashes_GameName]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_GameName]  DEFAULT ('') FOR [GameName]
GO
/****** Object:  Default [DF_Crashes_CrashType]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_CrashType]  DEFAULT ((1)) FOR [CrashType]
GO
/****** Object:  Default [DF_Crashes_Status]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_Status]  DEFAULT ('New') FOR [Status]
GO
/****** Object:  Default [DF_Crashes_TTPID]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_TTPID]  DEFAULT ('') FOR [TTPID]
GO
/****** Object:  Default [DF_Crashes_FixedChangeList]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_FixedChangeList]  DEFAULT ('') FOR [FixedChangeList]
GO
/****** Object:  Default [DF_Crashes_ChangeListVersion]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_ChangeListVersion]  DEFAULT ('') FOR [ChangeListVersion]
GO
/****** Object:  Default [DF_Crashes_PlatformName]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_PlatformName]  DEFAULT ('') FOR [PlatformName]
GO
/****** Object:  Default [DF_Crashes_EngineMode]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_EngineMode]  DEFAULT ('Game') FOR [EngineMode]
GO
/****** Object:  Default [DF_Crashes_Description]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_Description]  DEFAULT ('') FOR [Description]
GO
/****** Object:  Default [DF_Crashes_RawCallStack]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_RawCallStack]  DEFAULT ('') FOR [RawCallStack]
GO
/****** Object:  Default [DF_Crashes_SourceContext]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_SourceContext]  DEFAULT ('') FOR [SourceContext]
GO
/****** Object:  Default [DF_Crashes_Pattern]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_Pattern]  DEFAULT ('') FOR [Pattern]
GO
/****** Object:  Default [DF_Crashes_CommandLine]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_CommandLine]  DEFAULT ('') FOR [CommandLine]
GO
/****** Object:  Default [DF_Crashes_ComputerName]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_ComputerName]  DEFAULT ('') FOR [ComputerName]
GO
/****** Object:  Default [DF_Crashes_LanguageExt]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_LanguageExt]  DEFAULT ('') FOR [LanguageExt]
GO
/****** Object:  Default [DF_Crashes_Module]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_Module]  DEFAULT ('') FOR [Module]
GO
/****** Object:  Default [DF_Crashes_BuildVersion]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_BuildVersion]  DEFAULT ('') FOR [BuildVersion]
GO
/****** Object:  Default [DF_Crashes_BaseDir]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_BaseDir]  DEFAULT ('') FOR [BaseDir]
GO
/****** Object:  Default [DF_Crashes_HasLogFile]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_HasLogFile]  DEFAULT ((0)) FOR [HasLogFile]
GO
/****** Object:  Default [DF_Crashes_HasMiniDumpFile]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_HasMiniDumpFile]  DEFAULT ((0)) FOR [HasMiniDumpFile]
GO
/****** Object:  Default [DF_Crashes_HasVideoFile]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_HasVideoFile]  DEFAULT ((0)) FOR [HasVideoFile]
GO
/****** Object:  Default [DF_Crashes_HasDiagnosticsFile]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_HasDiagnosticsFile]  DEFAULT ((0)) FOR [HasDiagnosticsFile]
GO
/****** Object:  Default [DF_Crashes_HasMetaData]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes] ADD  CONSTRAINT [DF_Crashes_HasMetaData]  DEFAULT ((0)) FOR [HasMetaData]
GO
/****** Object:  Default [DF_PIIMapping_UserNameId]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[PIIMapping] ADD  CONSTRAINT [DF_PIIMapping_UserNameId]  DEFAULT ((2)) FOR [UserNameId]
GO
/****** Object:  Default [DF_Users_UserGroupId]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Users] ADD  CONSTRAINT [DF_Users_UserGroupId]  DEFAULT ((1)) FOR [UserGroupId]
GO
/****** Object:  ForeignKey [FK_Buggs_Crashes_Buggs]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs_Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Crashes_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Crashes] CHECK CONSTRAINT [FK_Buggs_Crashes_Buggs]
GO
/****** Object:  ForeignKey [FK_Buggs_Crashes_Crashes]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs_Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Crashes_Crashes] FOREIGN KEY([CrashId])
REFERENCES [dbo].[Crashes] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Crashes] CHECK CONSTRAINT [FK_Buggs_Crashes_Crashes]
GO
/****** Object:  ForeignKey [FK_Buggs_Users_Buggs]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs_Users]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Users_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Users] CHECK CONSTRAINT [FK_Buggs_Users_Buggs]
GO
/****** Object:  ForeignKey [FK_Buggs_Users_Users]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Buggs_Users]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Users_Users] FOREIGN KEY([UserNameId])
REFERENCES [dbo].[Users] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Users] CHECK CONSTRAINT [FK_Buggs_Users_Users]
GO
/****** Object:  ForeignKey [FK_Crashes_Users]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_Users] FOREIGN KEY([UserNameId])
REFERENCES [dbo].[Users] ([Id])
GO
ALTER TABLE [dbo].[Crashes] CHECK CONSTRAINT [FK_Crashes_Users]
GO
/****** Object:  ForeignKey [FK_Crashes_FunctionCalls_Crashes]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes_FunctionCalls]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_FunctionCalls_Crashes] FOREIGN KEY([CrashId])
REFERENCES [dbo].[Crashes] ([Id])
GO
ALTER TABLE [dbo].[Crashes_FunctionCalls] CHECK CONSTRAINT [FK_Crashes_FunctionCalls_Crashes]
GO
/****** Object:  ForeignKey [FK_Crashes_FunctionCalls_FunctionCalls]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Crashes_FunctionCalls]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_FunctionCalls_FunctionCalls] FOREIGN KEY([FunctionCallId])
REFERENCES [dbo].[FunctionCalls] ([Id])
GO
ALTER TABLE [dbo].[Crashes_FunctionCalls] CHECK CONSTRAINT [FK_Crashes_FunctionCalls_FunctionCalls]
GO
/****** Object:  ForeignKey [FK_PIIMapping_Users]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[PIIMapping]  WITH CHECK ADD  CONSTRAINT [FK_PIIMapping_Users] FOREIGN KEY([UserNameId])
REFERENCES [dbo].[Users] ([Id])
GO
ALTER TABLE [dbo].[PIIMapping] CHECK CONSTRAINT [FK_PIIMapping_Users]
GO
/****** Object:  ForeignKey [FK_Users_UserGroups]    Script Date: 06/24/2013 14:28:57 ******/
ALTER TABLE [dbo].[Users]  WITH CHECK ADD  CONSTRAINT [FK_Users_UserGroups] FOREIGN KEY([UserGroupId])
REFERENCES [dbo].[UserGroups] ([Id])
GO
ALTER TABLE [dbo].[Users] CHECK CONSTRAINT [FK_Users_UserGroups]
GO
