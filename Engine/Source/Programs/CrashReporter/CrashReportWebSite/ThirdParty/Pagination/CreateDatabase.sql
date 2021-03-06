USE [CrashReport-Dev]
GO
/****** Object:  Table [dbo].[Users]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Users](
	[UserName] [varchar](50) NOT NULL,
	[UserGroup] [varchar](50) NOT NULL,
	[UserGroupId] [int] NULL,
 CONSTRAINT [PK_Users] PRIMARY KEY CLUSTERED 
(
	[UserName] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
/****** Object:  Table [dbo].[Crashes]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Crashes](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Title] [nchar](20) NULL,
	[Summary] [varchar](140) NULL,
	[GameName] [varchar](15) NULL,
	[Status] [varchar](15) NULL,
	[TimeOfCrash] [datetime] NULL,
	[ChangeListVersion] [varchar](50) NULL,
	[PlatformName] [varchar](50) NULL,
	[EngineMode] [varchar](50) NULL,
	[Description] [varchar](300) NULL,
	[RawCallStack] [varchar](max) NULL,
	[Pattern] [varchar](800) NULL,
	[CommandLine] [varchar](512) NULL,
	[ComputerName] [varchar](50) NULL,
	[Selected] [bit] NULL,
	[FixedChangeList] [varchar](140) NULL,
	[LanguageExt] [varchar](20) NULL,
	[BuildVersion] [varchar](80) NULL,
	[BaseDir] [varchar](260) NULL,
	[Version] [int] NULL,
	[UserName] [varchar](50) NULL,
	[TTPID] [varchar](50) NULL,
	[AutoReporterID] [int] NULL,
 CONSTRAINT [PK_Crashes] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
/****** Object:  Table [dbo].[UserGroups]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[UserGroups](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[Name] [varchar](50) NOT NULL,
 CONSTRAINT [PK_UserGroups] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
/****** Object:  Table [dbo].[FunctionCalls]    Script Date: 10/01/2010 14:51:34 ******/
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
/****** Object:  Table [dbo].[Crashes_FunctionCalls]    Script Date: 10/01/2010 14:51:34 ******/
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
/****** Object:  Table [dbo].[Buggs]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Buggs](
	[Id] [int] IDENTITY(1,1) NOT NULL,
	[TTPID] [varchar](50) NULL,
	[Title] [varchar](120) NULL,
	[Summary] [varchar](250) NULL,
	[Priority] [int] NULL,
	[Pattern] [varchar](250) NULL,
	[Type] [varchar](50) NULL,
	[NumberOfCrashes] [int] NULL,
	[NumberOfUsers] [int] NULL,
	[TimeOfFirstCrash] [datetime] NULL,
	[TimeOfLastCrash] [datetime] NULL,
	[Status] [varchar](50) NULL,
	[FixedChangeList] [varchar](50) NULL,
	[Description] [text] NULL,
	[ReproSteps] [text] NULL,
 CONSTRAINT [PK_Buggs] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY] TEXTIMAGE_ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
/****** Object:  Table [dbo].[Notes]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE TABLE [dbo].[Notes](
	[Id] [int] NOT NULL,
	[Tag] [nchar](10) NULL,
	[Text] [nchar](10) NULL,
	[CrashId] [int] NULL,
	[BuggId] [int] NULL,
 CONSTRAINT [PK_Notes_1] PRIMARY KEY CLUSTERED 
(
	[Id] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
/****** Object:  Table [dbo].[Buggs_Crashes]    Script Date: 10/01/2010 14:51:34 ******/
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
/****** Object:  Table [dbo].[Buggs_Users]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
SET ANSI_PADDING ON
GO
CREATE TABLE [dbo].[Buggs_Users](
	[BuggId] [int] NOT NULL,
	[UserName] [varchar](50) NOT NULL,
 CONSTRAINT [PK_Buggs_Users] PRIMARY KEY CLUSTERED 
(
	[BuggId] ASC,
	[UserName] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
SET ANSI_PADDING OFF
GO
/****** Object:  Table [dbo].[Buggs_UserGroups]    Script Date: 10/01/2010 14:51:34 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
CREATE TABLE [dbo].[Buggs_UserGroups](
	[BuggId] [int] NOT NULL,
	[UserGroupId] [int] NOT NULL,
 CONSTRAINT [PK_Buggs_UserGroups] PRIMARY KEY CLUSTERED 
(
	[BuggId] ASC,
	[UserGroupId] ASC
)WITH (PAD_INDEX  = OFF, STATISTICS_NORECOMPUTE  = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS  = ON, ALLOW_PAGE_LOCKS  = ON) ON [PRIMARY]
) ON [PRIMARY]
GO
/****** Object:  ForeignKey [FK_Buggs_Crashes_Buggs]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Crashes_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Crashes] CHECK CONSTRAINT [FK_Buggs_Crashes_Buggs]
GO
/****** Object:  ForeignKey [FK_Buggs_Crashes_Crashes]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Crashes_Crashes] FOREIGN KEY([CrashId])
REFERENCES [dbo].[Crashes] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Crashes] CHECK CONSTRAINT [FK_Buggs_Crashes_Crashes]
GO
/****** Object:  ForeignKey [FK_Buggs_UserGroups_Buggs]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_UserGroups]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_UserGroups_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Buggs_UserGroups] CHECK CONSTRAINT [FK_Buggs_UserGroups_Buggs]
GO
/****** Object:  ForeignKey [FK_Buggs_UserGroups_UserGroups]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_UserGroups]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_UserGroups_UserGroups] FOREIGN KEY([UserGroupId])
REFERENCES [dbo].[UserGroups] ([Id])
GO
ALTER TABLE [dbo].[Buggs_UserGroups] CHECK CONSTRAINT [FK_Buggs_UserGroups_UserGroups]
GO
/****** Object:  ForeignKey [FK_Buggs_Users_Buggs]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_Users]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Users_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Buggs_Users] CHECK CONSTRAINT [FK_Buggs_Users_Buggs]
GO
/****** Object:  ForeignKey [FK_Buggs_Users_Users]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Buggs_Users]  WITH CHECK ADD  CONSTRAINT [FK_Buggs_Users_Users] FOREIGN KEY([UserName])
REFERENCES [dbo].[Users] ([UserName])
GO
ALTER TABLE [dbo].[Buggs_Users] CHECK CONSTRAINT [FK_Buggs_Users_Users]
GO
/****** Object:  ForeignKey [FK_Crashes_Users]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Crashes]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_Users] FOREIGN KEY([UserName])
REFERENCES [dbo].[Users] ([UserName])
GO
ALTER TABLE [dbo].[Crashes] CHECK CONSTRAINT [FK_Crashes_Users]
GO
/****** Object:  ForeignKey [FK_Crashes_FunctionCalls_Crashes]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Crashes_FunctionCalls]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_FunctionCalls_Crashes] FOREIGN KEY([CrashId])
REFERENCES [dbo].[Crashes] ([Id])
GO
ALTER TABLE [dbo].[Crashes_FunctionCalls] CHECK CONSTRAINT [FK_Crashes_FunctionCalls_Crashes]
GO
/****** Object:  ForeignKey [FK_Crashes_FunctionCalls_FunctionCalls]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Crashes_FunctionCalls]  WITH CHECK ADD  CONSTRAINT [FK_Crashes_FunctionCalls_FunctionCalls] FOREIGN KEY([FunctionCallId])
REFERENCES [dbo].[FunctionCalls] ([Id])
GO
ALTER TABLE [dbo].[Crashes_FunctionCalls] CHECK CONSTRAINT [FK_Crashes_FunctionCalls_FunctionCalls]
GO
/****** Object:  ForeignKey [FK_Notes_Buggs]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Notes]  WITH CHECK ADD  CONSTRAINT [FK_Notes_Buggs] FOREIGN KEY([BuggId])
REFERENCES [dbo].[Buggs] ([Id])
GO
ALTER TABLE [dbo].[Notes] CHECK CONSTRAINT [FK_Notes_Buggs]
GO
/****** Object:  ForeignKey [FK_Notes_Crashes]    Script Date: 10/01/2010 14:51:34 ******/
ALTER TABLE [dbo].[Notes]  WITH CHECK ADD  CONSTRAINT [FK_Notes_Crashes] FOREIGN KEY([CrashId])
REFERENCES [dbo].[Crashes] ([Id])
GO
ALTER TABLE [dbo].[Notes] CHECK CONSTRAINT [FK_Notes_Crashes]
GO

INSERT INTO [dbo].[UserGroups]
           ([Name])
     VALUES
           ('General')
GO
INSERT INTO [dbo].[UserGroups]
           ([Name])
     VALUES
           ('Tester')
GO

INSERT INTO [dbo].[UserGroups]
           ([Name])
     VALUES
           ('Coder')
GO
INSERT INTO [dbo].[UserGroups]
           ([Name])
     VALUES
           ('Automated')
GO
INSERT INTO [dbo].[UserGroups]
           ([Name])
     VALUES
           ('Undefined')
GO


