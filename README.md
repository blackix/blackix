Oculus integration with UE 4.5
==============================================
This is the source code page for the **Unreal Engine with Latest Oculus Support on GitHub**.
Current integration supports Rift DK2 and backward compatible with all previous Rifts and prototypes.
Timewarp technique support has been added into this integration. The new technique called Timewarp intends 
to reduce motion-to-photon latency. This technique re-projects the scene to a more recently measured orientation during the distortion 
rendering phase. Implemented for both DX11 and OpenGL, on Windows and Mac (we are working on adding Linux support too).

Full list of Oculus-related console commands is included in UE4-Oculus.txt file.

Note, you need to download pre-requisite .zip and uncompress it into UnrealEngine directory, similarly how you do it with Epic's .zip files:

[Link1](http://static.oculusvr.com/sdk-downloads/ovr_ue4_4.5.1_github_prereq_0.4.4.zip)

Before running UE4 with this integration, you must install Oculus Run-Time & Drivers. The installer downloaded from www.oculus.com (go to "Developer", register/login, "Downloads", "Oculus Runtime").
Make sure you have installed the latest Run-Time.
For Windows: run the executable and follow the instructions.
For Mac: double click on downloaded file and run the installer. Follow the instructions.

If the headset is properly connected (do not use HDMI splitters), it should be automatically detected. In the "Extended Desktop to HMD" mode, switching to fullscreen should render on the HMD only. 
In the "Direct HMD access from Apps" mode the Rift won't be detected by the system as a separate display; instead, it will be activated only when you switch to "fullscreen" (Alt-Enter). Note, in this mode
UE4 won't switch to fullscreen, it will render the scene in window (and in Rift).
It will be automatically de-activated once you switch back to non-stereo mode. 
Only one HMD can be connected at a time.

If stereo mode doesn't work then check the log-file (search for 'OCULUS' and 'LogHMD' sub-strings). Make sure the Run-Time & Drivers are installed and the service (OVRService.exe) is running. Check the Oculus User Guide for further details (located in Engine/Extras/Oculus directory).

See UE4-Oculus.txt for the new console commands.

And now, back to Epic's notes.

Welcome to the UE 4.5 source code!
==================================

This is the source code page for the **Unreal Engine on GitHub**.  With the UE4 source code, you can modify the
engine and tools in any way imaginable and share your changes with others!

You can build the editor for Windows and Mac, and compile games for Android, iOS, Playstation 4, Xbox
One, HTML5 and Linux.  Source code for all tools is included as well, such as Unreal Lightmass and Unreal Frontend.

Before continuing, check out this [wiki page](https://wiki.unrealengine.com/GitHub_Setup) about getting started
with the engine code.  We also have a [programming guide](https://docs.unrealengine.com/latest/INT/Programming/index.html) and
full [API documentation](https://docs.unrealengine.com/latest/INT/API/index.html).

We also have a forum section where you can discuss [engine source and GitHub](https://forums.unrealengine.com/forumdisplay.php?1-Development-Discussion).
Have fun - we can't wait to see what you create!

Source releases
---------------

This branch contains source code for the **4.5 release**.  You'll need to download dependency files per the instructions below in order to build and run the engine.

We're also publishing bleeding edge changes from our engine team in the [master branch](https://github.com/EpicGames/UnrealEngine/tree/master) on GitHub.  Here you can 
see [live commits](https://github.com/EpicGames/UnrealEngine/commits/master) from Epic programmers along with integrated code submissions from the community.  This branch is 
unstable and may not even compile, though we periodically tag [preview releases](https://github.com/EpicGames/UnrealEngine/releases/tag/latest-preview) which
receive basic testing and have matching dependencies attached.

We recommend you work with a versioned release such as this one.  The master branch contains unstable and possibly untested code,
but it should be a great reference for new developments, or for spot merging bug fixes.  Use it at your own risk.  




Getting up and running
----------------------

Here is the fun part!  This is a quick start guide to getting up and running with the source.  The steps below will take you through cloning your own private fork, then compiling and 
running the editor yourself on Windows or Mac.  Oh, and you might want to watch our [short tutorial video](http://youtu.be/usjlNHPn-jo)
first.  Okay, here we go!

1. We recommend using Git in order to participate in the community, but you can **download the source** as a zip file if you prefer. See instructions for 
   [setting up Git](http://help.github.com/articles/set-up-git), then [fork our repository](https://help.github.com/articles/fork-a-repo), clone it to your local 
   machine, and switch to the 4.5 branch:
   
```
git checkout 4.5
```	
   
1. You should now have an _UnrealEngine_ folder on your computer.  All of the source and dependencies will go into this folder.  The folder name might 
   have a branch suffix (such as _UnrealEngine-4.5_), but that's fine.

1. Download the **required dependencies**:
   [Required_1of2.zip](https://github.com/EpicGames/UnrealEngine/releases/download/4.5.0-release/Required_1of2.zip) and
   [Required_2of2.zip](https://github.com/EpicGames/UnrealEngine/releases/download/4.5.0-release/Required_2of2.zip).

1. Unzip the dependencies into the _UnrealEngine_ folder alongside the source.  Be careful to make sure the folders are merged together 
   correctly.  On Mac, we recommend **Option + dragging** the unzipped files into the _UnrealEngine_ folder, then selecting **Keep Newer** if prompted.

1. Okay, platform stuff comes next.  Depending on whether you are on Windows or Mac, follow one of the sections below:


### Windows

1. Be sure to have [Visual Studio 2013](http://www.microsoft.com/en-us/download/details.aspx?id=40787) installed.  You can use any 
   desktop version of Visual Studio 2013, including the free version:  [Visual Studio 2013 Express for Windows Desktop](http://www.microsoft.com/en-us/download/details.aspx?id=40787).
   See the "Additional Notes" below for using Visual Studio 2012.

1. Make sure you have [June 2010 DirectX runtime](http://www.microsoft.com/en-us/download/details.aspx?id=8109) installed.  You don't need the SDK, just the runtime.

1. You'll need project files in order to compile.  In the _UnrealEngine_ folder, double-click on **GenerateProjectFiles.bat**.  It should take less than a minute to complete.  On Windows 8, a warning from SmartScreen may appear.  Click "More info", then "Run anyway" to continue.

1. Load the project into Visual Studio by double-clicking on the **UE4.sln** file.

1. It's time to **compile the editor**!  In Visual Studio, make sure your solution configuration is set to **Development Editor**, and your solution 
   platform is set to **Win64**.  Right click on the **UE4** target and select **Build**.  It will take between 15 and 40 minutes to finish compiling,
   depending on your system specs.

1. After compiling finishes, you can **load the editor** from Visual Studio by setting your startup project to **UE4** and pressing **F5** to debug.

1. One last thing.  You'll want to setup your Windows shell so that you can interact with .uproject files.  Find the file named **UnrealVersionSelector-Win64-Shippping.exe** in 
   the _UnrealEngine/Engine/Binaries/Win64/_ folder and run it.  Now, you'll be able to double-click .uproject files to load the project, or right click them to quickly update Visual Studio files.         



### Mac

1. Be sure to have [Xcode 5.1](https://itunes.apple.com/us/app/xcode/id497799835) installed.

1. You'll need project files in order to compile.  In the _UnrealEngine_ folder, double-click on **GenerateProjectFiles.command**.  It should take less than a minute to complete.  You can close the Terminal window afterwards.  If you downloaded the source in .zip format, you may see a warning about an unidentified developer.  This is because because the .zip files on GitHub are not digitally signed.  To work around this, right-click on GenerateProjectFiles.command, select Open, then click the Open button if you are sure you want to open it.

1. Load the project into Xcode by double-clicking on the **UE4.xcodeproj** file.

1. It's time to **compile the editor**!  In Xcode, build the **UE4Editor - Mac** target for **My Mac** by selecting it in the target drop down
   in the top left, then using **Product** -> **Build For** -> **Running**.  It will take between 15 and 40 minutes, depending on your system specs.

1. After compiling finishes, you can **load the editor** from Xcode using the **Product** -> **Run** menu command!




### Linux

1. Both cross-compiling and native builds are supported. 

2. Cross-compiling is handy when you are a Windows (Mac support planned too) developer who wants to package your game for Linux with minimal hassle, and it requires a [cross-compiler toolchain](http://cdn.unrealengine.com/qfe/v4_clang-3.5.0_ld-2.24_glibc-2.12.2.zip) to be installed (see the [Linux cross-compiling page on the wiki](https://wiki.unrealengine.com/Compiling_For_Linux)). Note that you will also need [optional dependencies](https://github.com/EpicGames/UnrealEngine/releases/download/4.5.0-release/Optional.zip) to be unzipped into your _UnrealEngine_ folder.

2. Native compilation is discussed in [a separate README](https://github.com/EpicGames/UnrealEngine/blob/4.5/Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://wiki.unrealengine.com/Building_On_Linux). Downloading the dependencies has now been automated, so you will only need to clone the repo and run **GenerateProjectFiles.sh** (provided that you have OAUTH_TOKEN set to your personal access token, see the above README for details).




### Additional target platforms

**Android** development currently works best from a PC. See the [Android getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/Android/GettingStarted/).

**iOS** programming requires a Mac. Instructions are in the [iOS getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html).

**HTML5** is supported using Emscripten, and requires the [optional dependencies](https://github.com/EpicGames/UnrealEngine/releases/download/4.5.0-release/Optional.zip) to be unzipped into your _UnrealEngine_ folder. Please see the [HTML5 getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/HTML5/GettingStarted/index.html).

**Playstation 4** or **XboxOne** development require additional files that can only be provided after your registered developer status is confirmed by Sony or Microsoft. See [the announcement blog post](https://www.unrealengine.com/blog/playstation-4-and-xbox-one-now-supported) for more information.



Additional Notes
----------------

Visual Studio 2013 and Xcode 5.1 are strongly recommended for development.

Legacy support for Visual Studio 2012 can be enabled by unzipping the [optional dependencies](https://github.com/EpicGames/UnrealEngine/releases/download/4.5.0-release/Optional.zip) into your UnrealEngine folder.

The first time you start the editor from a fresh source build, you may experience long load times.  This only happens on the first 
run as the engine optimizes content for the platform and _fills the derived data cache_.

Your private forks of the Unreal Engine code are associated with your GitHub account permissions.  Just remember
that if you unsubscribe or switch GitHub user names, you'll need to re-fork and upload your changes from a local copy. 