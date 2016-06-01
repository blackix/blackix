This is an early drop of a single-pass forward renderer for UE 4.11
=============

There are a good number of incomplete, unsupported and untested rendering features when using this renderer.
In particular, as it is a forward renderer, normal screen-space effects or techniques that utilize a gbuffer are not supported (SSAO, SSR, the fancy subsurface shaders, decals, etc).  Shadowing, lighting and reflections are also limited in various ways.  We expect to make some improvements, particularly around shadows, and would like to get your feedback on what you feel is necessary for your project.

The current set well-supported features include:
- (HQ) lightmaps
- stationary sky light
- single, stationary directional light with limited CSM (4 cascades maximum)
- small number of stationary or movable point lights, using stationary (distance field) shadowing or no shadowing
- exponential height fog
- single sphere or box reflection capture

There are some new features:
- MSAA (2, 4 samples are currently supported)
- wide resolve filters (several settings are supported)
- Transparency can be lit identically to opaque surfaces.  This can be enabled by using the "per pixel" translucency lighting mode in the material.
- Translucent objects will otherwise be lit via as they are in deferred using the translucent lighting volume, except for the contribution of the main directional light, which will be evaluated the same as opaque (optional, see CLUSTERED_SUPPORTS_TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT)
- Alpha-to-coverage support (both for opaque and masked materials, the logic for masked tries to work around the limitations of UE shader creation, see the shader).  Enabled in the translucent settings of the material.
- Translucent depth prepass: allows rendering only the front-most faces of transparent objects, to avoid sorting issues.  In the translucent settings of the material.
- Geometric Antialiasing: ad-hoc method to reduce shimmering on highly reflective objects, can be enabled per-material, uses global scale/bias set in the scene settings.
- new IndirectLightingCache mode "point from volume" which averages the lighting samples over the bounds of the object, can give less flickering than the normal "point" mode.
- "Fully Rough" like in mobile builds, allows for additional optimizations (useful for distance geometry)
- "Cheap Shading" is new and enables a faster lighting model as well as skips shadow sampling.  This can be useful for lit particles.
- Some changes were made to allow setting per-object stencil ref.
- Modifications to HierarchicalInstancedStaticMesh that make the LOD behaviors behave much more closely to the non-instanced behavior.  We have used this extensively to reduce the number of draw calls in our levels.

And some particular limitations, beyond the lack of gbuffer dependent effects:
- Maximum of 8 simultaneously visible lights in a view (this can be easily lifted to 16 or 32). the scene may have many lights total, this is just a limit on per-frame visibility
- Currently only movable objects will render into the CSM.  movable objects will be shadowed using static shadowing.  You can preview this using a similar visualization mode to the irradiance samples (see "Visualize Volume Shadow Samples")

This build also includes support for monoscopic backgrounds / layered rendering.
Backgrounds have a (fixable, if necessary) restriction where there won't be any dynamic lighting applied, just directional lighting.  We plan to fix this.

To enable the monoscopic background, find the set of renderable objects you wish to comprise the background, and switch them to the SDPG_Background DepthPriorityGroup.  All objects in those groups will render once, and then the result will be copied to the background of both eyes (adjusting for their slightly different viewing frusta).
You will also need to enable the feature by setting r.BackgroundLayerEnabled=1
You can modify the background's resolution and resolve width independently of the scene to save additional overhead:
see r.BackgroundLayerSP (50-150 is 50%->150% of the main scene resolution)
and r.BackgroundLayerWideResolve (-1 => use default, otherwise see r.WideCustomResolve)
Beware using depth fading in the background layer, it is currently unset (and will be the depth from the previous foreground frame in most cases).

There are a number of compile-time changes to supported rendering features in a few locations:
* ClusteredBasePassRendering.h (more details are in the source)
  - CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
  - CLUSTERED_SUPPORTS_TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT
  - CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS
  - CLUSTERED_USE_BOX_REFLECTION_CAPTURE
* ClusteredShadingCommon.usf
  - CLUSTERED_SUPPORT_DIRECTIONAL_LIGHTS 
  - CLUSTERED_SUPPORT_SPOT_LIGHTS
  - CLUSTERED_SUPPORT_LEGACY_ATTENUATION
  - CLUSTERED_USE_PARALLAX_CORRECTION


A number of additional refinements and features are planned, in addition to further performance optimizations.
We do plan to try to support Temporal Antialiasing, and/or some form of normal-map filtering to reduce shimmering, though we suggest you use the "Texture Compositing" feature to modify roughness values from a normal map, this works very well for us (see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/Textures/Composite/index.html)

Console variables that control the renderer.  These can be toggled at runtime.

; Enable/Disable the renderer
r.UseClusteredForward=1

; Set # of MSAA samples (2,4)
r.MobileMSAA=4

; Ranges from 0 (normal HW resolve with box filter) to 1,2,3 each of which has slightly wider filtering (12,16,20 samples)
r.WideCustomResolve=1

; Z-prepass
; 1=not masked, "only if large" (not sure if this logic is implemented)
; 2=all opaque (including mask)
; 3=auto select (usually ==1)
r.EarlyZPass=3

; If movable objects are in the zpass
r.EarlyZPassMovable=0

; Determines how objects are sorted
; 0 = "auto"
; 1 = none
; 2 = per policy (ok with zpass, less state changing??)
; 3 = per policy per mesh
;r.ForwardBasePassSort=0

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
making your own games. We work hard to make releases stable and reliable, and aim to publish new releases every few months.
>**Important**: The Release branch has unintentionally been overwritten with code from the 4.11 Previews, and is *not* currently the latest and stable release. Please use Tags to grab the latest 4.10 release for our most stable version. This message will be removed when this matter has been resolved. 

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

1. Install **Visual Studio 2015**. 
   All desktop editions of Visual Studio 2015 can build UE4, including [Visual Studio Community 2015](http://www.visualstudio.com/products/visual-studio-community-vs), which is free for small teams and individual developers.
   Be sure to include C++ support as part of the install, which is disabled by default.
  
1. Open your source folder in Explorer and run **Setup.bat**. 
   This will download binary content for the engine, as well as installing prerequisites and setting up Unreal file associations. 
   On Windows 8, a warning from SmartScreen may appear.  Click "More info", then "Run anyway" to continue.
   
   A clean download of the engine binaries is currently 3-4gb, which may take some time to complete.
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

1. Load the project into Xcode by double-clicking on the **UE4.xcworkspace** file. Select the **UE4** for **My Mac** target in the title bar,
   then select the 'Product > Build' menu item. Compiling may take anywhere between 15 and 40 minutes, depending on your system specs.
   
1. After compiling finishes, select the 'Product > Run' menu item to load the editor.




### Linux

1. [Set up Git](https://help.github.com/articles/set-up-git/) and [fork our repository](https://help.github.com/articles/fork-a-repo/).
   If you'd prefer not to use Git, use the 'Download ZIP' button on the right to get the source as a zip file.

1. Open your source folder and run **Setup.sh** to download binary content for the engine.

1. Both cross-compiling and native builds are supported. 

   **Cross-compiling** is handy when you are a Windows (Mac support planned too) developer who wants to package your game for Linux with minimal hassle, and it requires a [cross-compiler toolchain](http://cdn.unrealengine.com/qfe/v4_clang-3.5.0_ld-2.24_glibc-2.12.2.zip) to be installed (see the [Linux cross-compiling page on the wiki](https://wiki.unrealengine.com/Compiling_For_Linux)).

   **Native compilation** is discussed in [a separate README](Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://wiki.unrealengine.com/Building_On_Linux). 




### Additional target platforms

**Android** support will be downloaded by the setup script if you have the Android NDK installed. See the [Android getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/Android/GettingStarted/).

**iOS** programming requires a Mac. Instructions are in the [iOS getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html).

**HTML5** support will be downloaded by the setup script if you have Emscripten installed. Please see the [HTML5 getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/HTML5/GettingStarted/index.html).

**Playstation 4** or **XboxOne** development require additional files that can only be provided after your registered developer status is confirmed by Sony or Microsoft. See [the announcement blog post](https://www.unrealengine.com/blog/playstation-4-and-xbox-one-now-supported) for more information.


Licensing and Contributions
---------------------------

Your access to and use of Unreal Engine on GitHub is governed by the [Unreal Engine End User License Agreement](https://www.unrealengine.com/eula). If you don't agree to those terms, as amended from time to time, you are not permitted to access or use Unreal Engine.

We welcome any contributions to Unreal Engine development through [pull requests](https://help.github.com/articles/using-pull-requests/) on GitHub. Most of our active development is in the **master** branch, so we prefer to take pull requests there (particularly for new features). We try to make sure that all new code adheres to the [Epic coding standards](https://docs.unrealengine.com/latest/INT/Programming/Development/CodingStandard/).  All contributions are governed by the terms of the EULA.


Additional Notes
----------------

The first time you start the editor from a fresh source build, you may experience long load times. 
The engine is optimizing content for your platform to the _derived data cache_, and it should only happen once.

Your private forks of the Unreal Engine code are associated with your GitHub account permissions.
If you unsubscribe or switch GitHub user names, you'll need to re-fork and upload your changes from a local copy. 
