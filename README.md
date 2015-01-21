Notes from Oculus
=================
**DISCLAIMER: THIS IS AN EXPERIMENTAL BRANCH!**

This branch introduces significant changes in OculusRift plugin and in the way how VR camera is controlled by the HMD.

This is 4.7-preview with re-factored plugin and improved (I hope) camera control. I haven't added new Blueprint functions yet (like raw accel data or player's metrics, still in work). I am going to document all the changes (documentation is long due anyway)...

So, briefly. I've added PlayerController.bFollowHmd property, which is by default set to 'true' (for compatibility reasons). When it is set in 'true' then the behavior is the same as it was before: HMD drives PlayerController. You can either set it to 'false' or just don't bother, it will be set to false automatically, once you use PlayerCameraManager or CameraComponent approach.

The main approach you guys care the most (I think) is to enable HMD via CameraComponent. This class now has two additional boolean properties - bFollowHmdOrientation and bFollowHmdPosition. In addition, the scale of the CameraComponent now will be a scale of the position change / ipd. The bigger scale is set, the smaller world is perceived. Camera's origin is located in-between eyes, i.e. IPD will be applied at the camera's origin, half of IPD in positive Y-axis direction and another half in negative Y-axis direction (in camera coordinate system).

Besides CameraComponent, the PlayerCameraManager also has boolean properties bFollowHmdOrientation and bFollowHmdPosition. Thus, any camera attached to this PlayerCameraManager via SetViewTarget method will follow the HMD orientation and/or position.

The method GetOrientationAndPosition should be used to set the camera's orientation / position manually via blueprint. Now it has 3 additional optional parameters:
boolean bUseOrienationForPlayerCamera = false: indicates that this orientation will be used to change camera orientation
boolean bUsePositionForPlayerCamera = false: indicates that this orientation will be used to change camera position
Vector PositionScale = Vector(1, 1, 1): scale to be applied to position (similar to CameraComponent.Scale); used only if bUsePositionForPlayerCamera == true.
The boolean params are necessary to calculate (or not) orientation/position corrections on render thread to reduce latency (especially, for position).

Also added GetRawSensorData method to HeadMountedDisplay (blueprint & C++). It returns the following:

* FVector Accelerometer;	// Acceleration reading in m/s^2.
* FVector Gyro;			// Rotation rate in rad/s.
* FVector Magnetometer;   // Magnetic field in Gauss.
* float Temperature;		// Temperature of the sensor in degrees Celsius.
* float TimeInSeconds;	// Time when the reported IMU reading took place, in seconds.

A couple more BP/C++ functions were added. Two of them GetScreenPercentage/SetScreenPercentage - the analog of 'hmd sp xxx' console command.
The third one - GetUserProfile, it returns a structure with the following members:
C++:

`	struct UserProfile`
`	{`
`		FString Name;`
`		FString Gender;`
`		float PlayerHeight;				// Height of the player, in meters`
`		float EyeHeight;				// Height of the player's eyes, in meters`
`		float IPD;						// Interpupillary distance, in meters`
`		FVector2D EyeToNeckDistance;	// Eye-to-neck distance, X - horizontal, Y - vertical, in meters`
`		TMap<FString, FString> ExtraFields; // extra fields in name / value pairs.`
`	};`

Blueprint struct is similar, however, instead of TMap I had to use TArray of FString pairs. The name of BP struct is FHmdUserProfile.

If you have any suggestions, there is ongoing discussion [here](https://forums.oculus.com/viewtopic.php?p=233266#p233266).

More details to follow, stay tuned!...

Unreal Engine
=============

Welcome to the Unreal Engine source code! 

From this repository you can build the Unreal Editor for Windows and Mac, compile Unreal Engine games for Android, iOS, Playstation 4, Xbox One, HTML5 and Linux,
and build tools like Unreal Lightmass and Unreal Frontend. Modify them in any way you can imagine, and share your changes with others! 

We have a heap of documentation available for the engine on the web. If you're looking for the answer to something, you may want to start here: 

* [Unreal Engine Programming Guide](https://docs.unrealengine.com/latest/INT/Programming/index.html)
* [Unreal Engine API Reference](https://docs.unrealengine.com/latest/INT/API/index.html)
* [Engine source and GitHub on the Unreal Engine forums](https://forums.unrealengine.com/forumdisplay.php?1-Development-Discussion)

If you need more, just ask! A lot of Epic developers hang out on the [forums](https://forums.unrealengine.com/) or [AnswerHub](https://answers.unrealengine.com/), 
and we're proud to be part of a well-meaning, friendly and welcoming community of thousands. 


Branches
--------

We publish source for the engine in three rolling branches:

The **[release branch](https://github.com/EpicGames/UnrealEngine/tree/release)** is extensively tested by our QA team and makes a great starting point for learning the engine or
making your own games. We work hard to make releases stable and reliable, and aim to publish new releases every 1-2 months.

The **[promoted branch](https://github.com/EpicGames/UnrealEngine/tree/promoted)** is updated with builds for our artists and designers to use. We try to update it daily 
(though we often catch things that prevent us from doing so) and it's a good balance between getting the latest cool stuff and knowing most things work.

The **[master branch](https://github.com/EpicGames/UnrealEngine/tree/master)** tracks [live changes](https://github.com/EpicGames/UnrealEngine/commits/master) by our engine team. 
This is the cutting edge and may be buggy - it may not even compile. Battle-hardened developers eager to work lock-step with us on the latest and greatest should head here.

Other short-lived branches may pop-up from time to time as we stabilize new releases or hotfixes.

Getting up and running
----------------------

The steps below will take you through cloning your own private fork, then compiling and running the editor yourself:

### Windows

1. Install **[GitHub for Windows](https://windows.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
   To use Git from the command line, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

   If you'd prefer not to use Git, you can get the source with the 'Download ZIP' button on the right. The built-in Windows zip utility will mark the contents of zip files 
   downloaded from the Internet as unsafe to execute, so right-click the zip file and select 'Properties...' and 'Unblock' before decompressing it. Third-party zip utilities don't normally do this.

1. Install **Visual Studio 2013**. 
   All desktop editions of Visual Studio 2013 can build UE4, including [Visual Studio Community 2013](http://www.visualstudio.com/products/visual-studio-community-vs), which is available for free.
   Be sure to include the MFC libraries as part of the install (it's included by default), which we need for ATL support.
  
1. Open your source folder in Explorer and run **Setup.bat**. 
   This will download binary content for the engine, as well as installing prerequisites and setting up Unreal file associations. 
   On Windows 8, a warning from SmartScreen may appear.  Click "More info", then "Run anyway" to continue.
   
   A clean download of the engine binaries is currently around 2.5gb, which may take some time to complete.
   Subsequent checkouts only require incremental downloads and will be much quicker.
 
1. Run **GenerateProjectFiles.bat** to create project files for the engine. It should take less than a minute to complete.  

1. Load the project into Visual Studio by double-clicking on the **UE4.sln** file. Set your solution configuration to **Development Editor** and your solution
   platform to **Win64**, then right click on the **UE4** target and select **Build**. It may take anywhere between 10 and 40 minutes to finish compiling, depending on your system specs.

1. After compiling finishes, you can load the editor from Visual Studio by setting your startup project to **UE4** and pressing **F5** to debug.




### Mac
   
1. Install **[GitHub for Mac](https://mac.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
   To use Git from the Terminal, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.
   If you'd rather not use Git, use the 'Download ZIP' button on the right to get the source directly.

1. Install the latest version of [Xcode](https://itunes.apple.com/us/app/xcode/id497799835).

1. Open your source folder in Finder and double-click on **Setup.command** to download binary content for the engine. You can close the Terminal window afterwards.

   If you downloaded the source as a .zip file, you may see a warning about it being from an unidentified developer (because .zip files on GitHub aren't digitally signed).
   To work around it, right-click on Setup.command, select Open, then click the Open button.

1. In the same folder, double-click **GenerateProjectFiles.command**.  It should take less than a minute to complete.  

1. Load the project into Xcode by double-clicking on the **UE4.xcodeproj** file. Select the **UE4Editor - Mac** for **My Mac** target in the title bar,
   then select the 'Product > Build' menu item. Compiling may take anywhere between 15 and 40 minutes, depending on your system specs.
   
1. After compiling finishes, select the 'Product > Run' menu item to load the editor.




### Linux

1. [Set up Git](https://help.github.com/articles/set-up-git/) and [fork our repository](https://help.github.com/articles/fork-a-repo/).
   If you'd prefer not to use Git, use the 'Download ZIP' button on the right to get the source as a zip file.

1. Open your source folder and run **Setup.sh** to download binary content for the engine.

1. Both cross-compiling and native builds are supported. 

   **Cross-compiling** is handy when you are a Windows (Mac support planned too) developer who wants to package your game for Linux with minimal hassle, and it requires a [cross-compiler toolchain](http://cdn.unrealengine.com/qfe/v4_clang-3.5.0_ld-2.24_glibc-2.12.2.zip) to be installed (see the [Linux cross-compiling page on the wiki](https://wiki.unrealengine.com/Compiling_For_Linux)).

   **Native compilation** is discussed in [a separate README](Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://wiki.unrealengine.com/Building_On_Linux). Downloading the dependencies has now been automated, so you will only need to clone the repo and run **GenerateProjectFiles.sh** (provided that you have OAUTH_TOKEN set to your personal access token, see the above README for details).




### Additional target platforms

**Android** support will be downloaded by the setup script if you have the Android NDK installed. See the [Android getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/Android/GettingStarted/).

**iOS** programming requires a Mac. Instructions are in the [iOS getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html).

**HTML5** support will be downloaded by the setup script if you have Emscripten installed. Please see the [HTML5 getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/HTML5/GettingStarted/index.html).

**Playstation 4** or **XboxOne** development require additional files that can only be provided after your registered developer status is confirmed by Sony or Microsoft. See [the announcement blog post](https://www.unrealengine.com/blog/playstation-4-and-xbox-one-now-supported) for more information.



Additional Notes
----------------

Visual Studio 2012 is supported by the Windows toolchain, though Visual Studio 2013 is recommended.

The first time you start the editor from a fresh source build, you may experience long load times. 
The engine is optimizing content for your platform to the _derived data cache_, and it should only happen once.

Your private forks of the Unreal Engine code are associated with your GitHub account permissions.
If you unsubscribe or switch GitHub user names, you'll need to re-fork and upload your changes from a local copy. 
