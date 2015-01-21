//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import java.io.File;

import java.util.Map;
import java.util.HashMap;
import java.util.ArrayList;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

import android.os.Vibrator;

import android.app.AlertDialog;
import android.app.Dialog;
import android.widget.EditText;
import android.text.InputType;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.IntentSender.SendIntentException;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import android.media.AudioManager;
import android.util.DisplayMetrics;

import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.WindowManager;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import android.media.AudioManager;

import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.GooglePlayServicesUtil;

import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdListener;

import com.google.android.gms.plus.Plus;

import java.net.URL;
import java.net.HttpURLConnection;

import com.epicgames.ue4.GooglePlayStoreHelper;
import com.epicgames.ue4.GooglePlayLicensing;


// TODO: use the resources from the UE4 lib project once we've got the packager up and running
//import com.epicgames.ue4.R;
import com.epicgames.ue4.JavaBuildSettings;

//Extending NativeActivity so that this Java class is instantiated
//from the beginning of the program.  This will allow the user
//to instantiate other Java libraries from here, that the user
//can then use the functions from C++
//NOTE -- This class is not necessary for the UnrealEngine C++ code
//  to startup, as this is handled through the base NativeActivity class.
//  This class's functionality is to provide a way to instantiate other
//  Java libraries at the startup of the program and store references 
//  to them in this class.

public class GameActivity extends NativeActivity
{
	public static Logger Log = new Logger("UE4");
	
	static GameActivity _activity;

	// Console
	AlertDialog consoleAlert;
	EditText consoleInputBox;
	ArrayList<String> consoleHistoryList;
	int consoleHistoryIndex;
	float consoleDistance;
	float consoleVelocity;

	// Virtual keyboard
	AlertDialog virtualKeyboardAlert;
	EditText virtualKeyboardInputBox;

	/** AssetManger reference - populated on start up and used when the OBB is packed into the APK */
	private AssetManager			AssetManagerReference;
	
	private GoogleApiClient googleClient;

	/** AdMob support */
	private PopupWindow adPopupWindow;
	private AdView adView;
	private boolean adInit = false;
	private LinearLayout adLayout;
	private LinearLayout activityLayout;
	private int adGravity = Gravity.TOP;

	/** true when the application has requested that an ad be displayed */
	private boolean adWantsToBeShown = false;

	/** true when an ad is available to be displayed */
	private boolean adIsAvailable = false;

	/** true when an ad request is in flight */
	private boolean adIsRequested = false;

	/** Request code to use when launching the Google Services resolution activity */
    private static final int GOOGLE_SERVICES_REQUEST_RESOLVE_ERROR = 1001;

	/** Unique tag for the error dialog fragment */
    private static final String DIALOG_ERROR = "dialog_error";

	/** Unique ID to identify Google Play Services error dialog */
	private static final int PLAY_SERVICES_DIALOG_ID = 1;

	/** Access singleton activity for game. **/
	public static GameActivity Get()
	{
		return _activity;
	}
	
	/**
	Get the SDK level of the OS we are running in.
	We do this instead of accessing the SDK_INT
	with JNI from C++ as the new ART runtime seems to have
	problems dynamically finding/loading static inner classes.
	*/
	public static final int ANDROID_BUILD_VERSION = android.os.Build.VERSION.SDK_INT;
	
	private StoreHelper IapStoreHelper;

	@Override
	public void onStart()
	{
		super.onStart();
		
		Log.debug("==================================> Inside onStart function in GameActivity");
	}

	public int getDeviceDefaultOrientation() 
	{

		// WindowManager windowManager =  (WindowManager) getSystemService(WINDOW_SERVICE);
		WindowManager windowManager =  getWindowManager();

		Configuration config = getResources().getConfiguration();

		int rotation = windowManager.getDefaultDisplay().getRotation();

		if ( ((rotation == android.view.Surface.ROTATION_0 || rotation == android.view.Surface.ROTATION_180) &&
				config.orientation == Configuration.ORIENTATION_LANDSCAPE)
			|| ((rotation == android.view.Surface.ROTATION_90 || rotation == android.view.Surface.ROTATION_270) &&    
				config.orientation == Configuration.ORIENTATION_PORTRAIT)) 
		{
			return Configuration.ORIENTATION_LANDSCAPE;
		}
		else 
		{
			return Configuration.ORIENTATION_PORTRAIT;
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		
		// Suppress java logs in Shipping builds
		if (nativeIsShippingBuild())
		{
			Logger.SuppressLogs();
		}

		_activity = this;
		
/*
		// Turn on and unlock screen.. Assumption is that this
		// will only really have an effect when for debug launching
		// as otherwise the screen is already unlocked.
		this.getWindow().addFlags(
			WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
//			WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
			WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
		// On some devices we can also unlock a key-locked screen by disabling the
		// keylock guard. To be safe we only do this on < Android 3.2. As the API
		// is deprecated from 3.2 onward.
		if (ANDROID_BUILD_VERSION < 13)
		{
			android.app.KeyguardManager keyman = (android.app.KeyguardManager)getSystemService(KEYGUARD_SERVICE);
			android.app.KeyguardManager.KeyguardLock keylock = keyman.newKeyguardLock("Unlock");
			keylock.disableKeyguard();
		}
*/

		// tell Android that we want volume controls to change the media volume, aka music
		setVolumeControlStream(AudioManager.STREAM_MUSIC);
		
		// is this a native landscape device (tablet, tv)?
		if ( getDeviceDefaultOrientation() == Configuration.ORIENTATION_LANDSCAPE )
		{
			boolean bForceLandscape = false;

			// check for a Google TV by checking system feature support
			if (getPackageManager().hasSystemFeature("com.google.android.tv"))
			{
				Log.debug( "Detected Google TV, will default to landscape" );
				bForceLandscape = true;
			} else

			// check NVidia devices
			if (android.os.Build.MANUFACTURER.equals("NVIDIA"))
			{
				// is it a Shield? (checking exact model)
				if (android.os.Build.MODEL.equals("SHIELD"))
				{
					Log.debug( "Detected NVidia Shield, will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Ouya
			if (android.os.Build.MANUFACTURER.equals("OUYA"))
			{
				// only one so far (ouya_1_1) but check prefix anyway
				if (android.os.Build.MODEL.toLowerCase().startsWith("ouya_"))
				{
					Log.debug( "Detected Ouya console (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Amazon devices
			if (android.os.Build.MANUFACTURER.equals("Amazon"))
			{
				// is it a Kindle Fire TV? (Fire TV FAQ says AFTB, but to check for AFT)
				if (android.os.Build.MODEL.startsWith("AFT"))
				{
					Log.debug( "Detected Kindle Fire TV (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			}

			// apply the force request if we found a device above
			if (bForceLandscape)
			{
				Log.debug( "Setting screen orientation to landscape because we have detected landscape device" );
				_activity.setRequestedOrientation( android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE );
			}
		}
		
		// Grab a reference to the asset manager
		AssetManagerReference = this.getAssets();

		// Get the preferred depth buffer size from AndroidManifest.xml
		int DepthBufferPreference = 0;
		try {
			ApplicationInfo ai = getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.DepthBufferPreference"))
			{
				DepthBufferPreference = bundle.getInt("com.epicgames.ue4.GameActivity.DepthBufferPreference");
				Log.debug( "Found DepthBufferPreference = " + DepthBufferPreference);
			} else {
				Log.debug( "Did not find DepthBufferPreference, using default.");
			}
		} catch (NameNotFoundException e) {
			Log.debug( "Failed to load meta-data: NameNotFound: " + e.getMessage());
		} catch (NullPointerException e) {
			Log.debug( "Failed to load meta-data: NullPointer: " + e.getMessage());
		}

		// tell the engine if this is a portrait app
		nativeSetGlobalActivity();
		nativeSetWindowInfo(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT, DepthBufferPreference);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		Log.debug( "Android version is " + android.os.Build.VERSION.RELEASE );
		Log.debug( "Android manufacturer is " + android.os.Build.MANUFACTURER );
		Log.debug( "Android model is " + android.os.Build.MODEL );
		Log.debug( "OS language is set to " + Language );

		nativeSetAndroidVersionInformation( android.os.Build.VERSION.RELEASE, android.os.Build.MANUFACTURER, android.os.Build.MODEL, Language );

		try
		{
			int Version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
			int PatchVersion = 0;
			nativeSetObbInfo(getApplicationContext().getPackageName(), Version, PatchVersion);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}
		
		// enable the physical volume controls to the game
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);

		AlertDialog.Builder builder;

		consoleInputBox = new EditText(this);
		consoleInputBox.setInputType(0x00080001); // TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_NO_SUGGESTIONS);
		consoleHistoryList = new ArrayList<String>();
		consoleHistoryIndex = 0;

		final ViewConfiguration vc = ViewConfiguration.get(this);
        DisplayMetrics dm = getResources().getDisplayMetrics();
        consoleDistance = vc.getScaledPagingTouchSlop() * dm.density;
        consoleVelocity = vc.getScaledMinimumFlingVelocity() / 1000.0f;

		consoleInputBox.setOnTouchListener(new OnTouchListener() {
			private long downTime;
			private float downX;

			public void swipeLeft() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex + 1 < consoleHistoryList.size()) {
					consoleInputBox.setText(consoleHistoryList.get(++consoleHistoryIndex));
				}
			}

			public void swipeRight() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex > 0) {
					consoleInputBox.setText(consoleHistoryList.get(--consoleHistoryIndex));
				}
			}

			public boolean onTouch(View v, MotionEvent event) {
				switch (event.getAction()) {
					case MotionEvent.ACTION_DOWN: {
						// remember down time and position
						downTime = System.currentTimeMillis();
						downX = event.getX();
						return true;
					}
					case MotionEvent.ACTION_UP: {
						long deltaTime = System.currentTimeMillis() - downTime;
						float delta = event.getX() - downX;
						float absDelta = Math.abs(delta);

						if (absDelta > consoleDistance && absDelta > deltaTime * consoleVelocity)
						{
							if (delta < 0)
								this.swipeLeft();
							else
								this.swipeRight();
							return true;
						}
						return false;
					}
				}
				return false;
			}
		});

		builder = new AlertDialog.Builder(this);
		builder.setTitle("Console Window - Enter Command")
		.setMessage("")
		.setView(consoleInputBox)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = consoleInputBox.getText().toString().trim();

				// remove it if already in history
				int index = consoleHistoryList.indexOf(message);
				if (index >= 0)
					consoleHistoryList.remove(index);

				// add it to the end
				consoleHistoryList.add(message);

				nativeConsoleCommand(message);
				consoleInputBox.setText(" ");
				dialog.dismiss();
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				consoleInputBox.setText(" ");
				dialog.dismiss();
			}
		});
		consoleAlert = builder.create();

		virtualKeyboardInputBox = new EditText(this);

		builder = new AlertDialog.Builder(this);
		builder.setTitle("")
		.setView(virtualKeyboardInputBox)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = virtualKeyboardInputBox.getText().toString();
				nativeVirtualKeyboardResult(true, message);
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				nativeVirtualKeyboardResult(false, " ");
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
			}
		});
		virtualKeyboardAlert = builder.create();

		GooglePlayLicensing.GoogleLicensing = new GooglePlayLicensing();
		GooglePlayLicensing.GoogleLicensing.Init(this, Log);

		// Now okay for event handler to be set up on native side
		nativeResumeMainInit();
		
		// Try to establish a connection to Google Play
		// AndroidThunkJava_GooglePlayConnect();

		Log.debug("==============> GameActive.onCreate complete!");
	}

	@Override
	public void onStop()
	{
		super.onStop();
	}

	// handle ad popup visibility and requests
	private void updateAdVisibility(boolean loadIfNeeded)
	{
		if (!adInit || (adPopupWindow == null))
		{
			return;
		}

		// request an ad if we don't have one available or requested, but would like one
		if (adWantsToBeShown && !adIsAvailable && !adIsRequested && loadIfNeeded)
		{
			AdRequest adRequest = new AdRequest.Builder().build();		// add test devices here
			_activity.adView.loadAd(adRequest);

			adIsRequested = true;
		}

		if (adIsAvailable && adWantsToBeShown)
		{
			if (adPopupWindow.isShowing())
			{
				return;
			}

			adPopupWindow.showAtLocation(activityLayout, adGravity, 0, 0);
			adPopupWindow.update();
		}
		else
		{
			if (!adPopupWindow.isShowing())
			{
				return;
			}

			adPopupWindow.dismiss();
			adPopupWindow.update();
		}
	}

	public void AndroidThunkJava_KeepScreenOn(boolean Enable)
	{
		if (Enable)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
		else
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
	}

	public void AndroidThunkJava_Vibrate(long Duration)
	{
		Vibrator vibrator = (Vibrator)getSystemService(VIBRATOR_SERVICE);
		if (Duration < 1)
		{
			vibrator.cancel();
		} else {
			vibrator.vibrate(Duration);
		}
	}

	// Called from event thread in NativeActivity	
	public void AndroidThunkJava_ShowConsoleWindow(String Formats)
	{
		if (consoleAlert.isShowing() == true)
		{
			Log.debug("Console already showing.");
			return;
		}

		// start at end of console history
		consoleHistoryIndex = consoleHistoryList.size();

		consoleAlert.setMessage("[Available texture formats: " + Formats + "]");
		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (consoleAlert.isShowing() == false)
				{
					Log.debug("Console not showing yet");
					consoleAlert.show(); 
				}
			}
		});
	}

	public void AndroidThunkJava_ShowVirtualKeyboardInput(int InputType, String Label, String Contents)
	{
		if (virtualKeyboardAlert.isShowing() == true)
		{
			Log.debug("Virtual keyboard already showing.");
			return;
		}

		// Set label and starting contents
		virtualKeyboardAlert.setTitle(Label);
		virtualKeyboardInputBox.setText(Contents);

		// configure for type of input
		virtualKeyboardInputBox.setInputType(InputType);

		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (virtualKeyboardAlert.isShowing() == false)
				{
					Log.debug("Virtual keyboard not showing yet");
					virtualKeyboardAlert.show(); 
				}
			}
		});
	}
	
	public void AndroidThunkJava_LaunchURL(String URL)
	{
		try
		{
			Intent BrowserIntent = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(URL));
			startActivity(BrowserIntent);
		}
		catch (Exception e)
		{
			Log.debug("LaunchURL failed with exception " + e.getMessage());
		}
	}

	public void AndroidThunkJava_ResetAchievements()
	{
		try
        {
			String email = Plus.AccountApi.getAccountName(googleClient);
			Log.debug("AndroidThunkJava_ResetAchievements: using email " + email);

            String accesstoken = GoogleAuthUtil.getToken(this, email, "oauth2:https://www.googleapis.com/auth/games");

			String ResetURL = "https://www.googleapis.com/games/v1management/achievements/reset?access_token=" + accesstoken;
			Log.debug("AndroidThunkJava_ResetAchievements: using URL " + ResetURL);

			URL url = new URL(ResetURL);
			HttpURLConnection urlConnection = (HttpURLConnection)url.openConnection();

			try
			{
				urlConnection.setRequestMethod("POST");
				int status = urlConnection.getResponseCode();
				Log.debug("AndroidThunkJava_ResetAchievements: HTTP response is " + status);
			}
			finally
			{
				urlConnection.disconnect();
			}
        }
        catch(Exception e)
        {
            Log.debug("AndroidThunkJava_ResetAchievements failed: " + e.getMessage());
        }
	}

	public void AndroidThunkJava_ShowAdBanner(String AdMobAdUnitID, boolean bShowOnBottonOfScreen)
	{
		Log.debug("In AndroidThunkJava_ShowAdBanner");
		Log.debug("AdID: " + AdMobAdUnitID);

		adGravity = bShowOnBottonOfScreen ? Gravity.BOTTOM : Gravity.TOP;

		if (adInit)
		{
			// already created, make it visible
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					if ((adPopupWindow == null) || adPopupWindow.isShowing())
					{
						return;
					}

					adWantsToBeShown = true;
					updateAdVisibility(true);
				}
			});

			return;
		}

		// init our AdMob window
		adView = new AdView(this);
		adView.setAdUnitId(AdMobAdUnitID);
		adView.setAdSize(AdSize.BANNER);

		if (adView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					adInit = true;

					final DisplayMetrics dm = getResources().getDisplayMetrics();
					final float scale = dm.density;
					adPopupWindow = new PopupWindow(_activity);
					adPopupWindow.setWidth((int)(320*scale));
					adPopupWindow.setHeight((int)(50*scale));
					adPopupWindow.setWindowLayoutMode(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
					adPopupWindow.setClippingEnabled(false);

					adLayout = new LinearLayout(_activity);
					activityLayout = new LinearLayout(_activity);

					final int padding = (int)(-5*scale);
					adLayout.setPadding(padding,padding,padding,padding);

					MarginLayoutParams params = new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);;

					params.setMargins(0,0,0,0);

					adLayout.setOrientation(LinearLayout.VERTICAL);
					adLayout.addView(adView, params);
					adPopupWindow.setContentView(adLayout);

					_activity.setContentView(activityLayout, params);

					// set up our ad callbacks
					_activity.adView.setAdListener(new AdListener()
					{
						 @Override
						public void onAdLoaded()
						{
							adIsAvailable = true;
							adIsRequested = false;

							updateAdVisibility(true);
						}

						 @Override
						public void onAdFailedToLoad(int errorCode)
						{
							adIsAvailable = false;
							adIsRequested = false;

							// don't immediately request a new ad on failure, wait until the next show
							updateAdVisibility(false);
						}
					});

					adWantsToBeShown = true;
					updateAdVisibility(true);
				}
			});
		}
	}

	public void AndroidThunkJava_HideAdBanner()
	{
		Log.debug("In AndroidThunkJava_HideAdBanner");

		if (!adInit)
		{
			return;
		}

		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				adWantsToBeShown = false;
				updateAdVisibility(true);
			}
		});
	}

	public void AndroidThunkJava_CloseAdBanner()
	{
		Log.debug("In AndroidThunkJava_CloseAdBanner");

		if (!adInit)
		{
			return;
		}

		// currently the same as hide.  should we do a full teardown?
		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				adWantsToBeShown = false;
				updateAdVisibility(true);
			}
		});
	}

	public AssetManager AndroidThunkJava_GetAssetManager()
	{
		if(AssetManagerReference == null)
		{
			Log.debug("No reference to asset manager found!");
		}

		return AssetManagerReference;
	}

	public static boolean isOBBInAPK()
	{
		return JavaBuildSettings.PackageType.AMAZON == JavaBuildSettings.PACKAGING;
	}

	public void AndroidThunkJava_Minimize()
	{
		Intent startMain = new Intent(Intent.ACTION_MAIN);
		startMain.addCategory(Intent.CATEGORY_HOME);
		startMain.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(startMain);
	}

	public void AndroidThunkJava_ForceQuit()
	{
		System.exit(0);
		// finish();
	}

	// call back into native code from the Java UI thread, initializing any available VR HMD modules
	public void AndroidThunkJava_InitHMDs()
	{
		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				nativeInitHMDs();
			}
		});
	}

	public static String AndroidThunkJava_GetFontDirectory()
	{
		// Parse and find the first known fonts directory on the device
		String[] fontdirs = { "/system/fonts", "/system/font", "/data/fonts" };

		String targetDir = null;

		for ( String fontdir : fontdirs )
        {
//			Log.debug(fontdir);
            File dir = new File( fontdir );

			if(dir.exists())
			{
				targetDir = fontdir;
				break;
			}
		}
		
		return targetDir + "/";
	}

	public boolean AndroidThunkJava_IsMusicActive()
	{
		AudioManager audioManager = (AudioManager)this.getSystemService(Context.AUDIO_SERVICE);
		return audioManager.isMusicActive();
	}
	
	// In app purchase functionality
	public void AndroidThunkJava_IapSetupService(String InProductKey)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapSetupService");
		IapStoreHelper = new GooglePlayStoreHelper(InProductKey, this, Log);
		if( IapStoreHelper == null )
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
	}
	

	private String[] CachedQueryProductIDs;
	private boolean[] CachedQueryConsumables;
	public boolean AndroidThunkJava_IapQueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapQueryInAppPurchases");
		CachedQueryProductIDs = ProductIDs;
		CachedQueryConsumables = bConsumable;

		boolean bTriggeredQuery = false;
		if( IapStoreHelper != null )
		{
			bTriggeredQuery = true;

			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					IapStoreHelper.QueryInAppPurchases(CachedQueryProductIDs, CachedQueryConsumables);
				}
			});
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredQuery;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		if( IapStoreHelper != null )
		{
			if(!IapStoreHelper.onActivityResult(requestCode, resultCode, data))
			{
				super.onActivityResult(requestCode, resultCode, data);
			}
			else
			{
				Log.debug("[JAVA] - Store Helper handled onActivityResult");
			}
		}
		else
		{
			super.onActivityResult(requestCode, resultCode, data);
		}
	}
	
	public boolean AndroidThunkJava_IapBeginPurchase(String ProductId, boolean bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapBeginPurchase");
		boolean bTriggeredPurchase = false;
		if( IapStoreHelper != null )
		{
			bTriggeredPurchase = IapStoreHelper.BeginPurchase(ProductId, bConsumable);
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredPurchase;
	}

	public boolean AndroidThunkJava_IapIsAllowedToMakePurchases()
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapIsAllowedToMakePurchases");
		boolean bIsAllowedToMakePurchase = false;
		if( IapStoreHelper != null )
		{
			bIsAllowedToMakePurchase = IapStoreHelper.IsAllowedToMakePurchases();
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bIsAllowedToMakePurchase;
	}

	public native boolean nativeIsShippingBuild();
	public native void nativeSetGlobalActivity();
	public native void nativeSetWindowInfo(boolean bIsPortrait, int DepthBufferPreference);
	public native void nativeSetObbInfo(String PackageName, int Version, int PatchVersion);
	public native void nativeSetAndroidVersionInformation( String AndroidVersion, String PhoneMake, String PhoneModel, String OSLanguage );

	public native void nativeConsoleCommand(String commandString);
	public native void nativeVirtualKeyboardResult(boolean update, String contents);

	public native void nativeInitHMDs();

	public native void nativeResumeMainInit();
	
	static
	{
		System.loadLibrary("gnustl_shared");
		System.loadLibrary("UE4");
	}
}

