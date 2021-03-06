// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ILauncherProfileManager.h: Declares the ILauncherProfileManager interface.
=============================================================================*/

#pragma once


/**
 * Type definition for shared pointers to instances of ILauncherProfileManager.
 */
typedef TSharedPtr<class ILauncherProfileManager> ILauncherProfileManagerPtr;

/**
 * Type definition for shared references to instances of ILauncherProfileManager.
 */
typedef TSharedRef<class ILauncherProfileManager> ILauncherProfileManagerRef;


/**
 * Declares a delegate to be invoked when a device group was added to a profile manager.
 *
 * The first parameter is the profile that was added.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileManagerDeviceGroupAdded, const ILauncherDeviceGroupRef&);

/**
 * Declares a delegate to be invoked when a device group was removed from a profile manager.
 *
 * The first parameter is the profile that was removed.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileManagerDeviceGroupRemoved, const ILauncherDeviceGroupRef&);

/**
 * Declares a delegate to be invoked when a launcher profile was added to a profile manager.
 *
 * The first parameter is the profile that was added.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileManagerProfileAdded, const ILauncherProfileRef&);

/**
 * Declares a delegate to be invoked when a launcher profile was removed from a profile manager.
 *
 * The first parameter is the profile that was removed.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileManagerProfileRemoved, const ILauncherProfileRef&);


/**
 * Interface for launcher profile managers.
 */
class ILauncherProfileManager
{
public:

	/**
	 * Adds the given device group.
	 *
	 * @param DeviceGroup - The group to add.
	 */
	virtual void AddDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup ) = 0;

	/*
	 * Create a new device group and maintains a reference for its future usage.
	 *
	 * @return The device group created.
	 */
	virtual ILauncherDeviceGroupRef AddNewDeviceGroup( ) = 0;

	/**
	 * Gets the collection of device groups.
	 *
	 * @return A read-only collection of device groups.
	 */
	virtual const TArray<ILauncherDeviceGroupPtr>& GetAllDeviceGroups( ) const = 0;

	/**
	 * Gets the device group with the specified identifier.
	 *
	 * @param GroupId - The unique identifier of the group to get.
	 *
	 * @return A shared pointer to the group, or NULL if the group was not found.
	 */
	virtual ILauncherDeviceGroupPtr GetDeviceGroup( const FGuid& GroupId ) const = 0;

	/** 
	 * Deletes the specified device group.
	 *
	 *( @param DeviceGroup - The group to remove.
	 */
	virtual void RemoveDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup ) = 0;


public:

	/**
	 * Creates a new profile.
	 *
	 * @return The new profile created.
	 */
	virtual ILauncherProfileRef AddNewProfile( ) = 0;

	/**
	 * Adds the given profile to the list of managed profiles.
	 *
	 * If a profile with the same identifier already exists in the profile
	 * collection, it will be deleted before the given profile is added.
	 *
	 * @param Profile - The profile to add.
	 */
	virtual void AddProfile( const ILauncherProfileRef& Profile ) = 0;

	/**
	 * Gets the profile with the specified name.
	 *
	 * @param ProfileName - The name of the profile to get.
	 *
	 * @return The profile, or NULL if the profile doesn't exist.
	 *
	 * @see GetProfile
	 */
	virtual ILauncherProfilePtr FindProfile( const FString& ProfileName ) = 0;

	/**
	 * Gets the collection of profiles.
	 *
	 * @return A read-only collection of profiles.
	 *
	 * @see GetSelectedProfile
	 */
	virtual const TArray<ILauncherProfilePtr>& GetAllProfiles( ) const = 0;

	/**
	 * Gets the profile with the specified identifier.
	 *
	 * @param ProfileId - The identifier of the profile to get.
	 *
	 * @return The profile, or NULL if the profile doesn't exist.
	 *
	 * @see FindProfile
	 */
	virtual ILauncherProfilePtr GetProfile( const FGuid& ProfileId ) const = 0;

	/**
	 * Attempts to load a profile from the specified archive.
	 *
	 * The loaded profile is NOT automatically added to the profile manager.
	 * Use AddProfile() to add it to the collection.
	 *
	 * @param Archive - The archive to load from.
	 *
	 * @return The loaded profile, or NULL if loading failed.
	 *
	 * @see AddProfile
	 * @see SaveProfile
	 */
	virtual ILauncherProfilePtr LoadProfile( FArchive& Archive ) = 0;

	/**
	 * Deletes the given profile.
	 *
	 * @param Profile - The profile to delete.
	 */
	virtual void RemoveProfile( const ILauncherProfileRef& Profile ) = 0;

	/**
	 * Saves the given profile to the specified archive.
	 *
	 * @param Profile - The profile to save.
	 * @param Archive - The archive to save to.
	 *
	 * @see LoadProfile
	 */
	virtual void SaveProfile( const ILauncherProfileRef& Profile, FArchive& Archive ) = 0;


public:

	/**
	 * Loads all device groups and launcher profiles from disk.
	 *
	 * When this function is called, it will discard any in-memory changes to device groups
	 * and launcher profiles that are not yet persisted to disk. Settings are also loaded
	 * automatically when a profile manager is first created.
	 *
	 * @see SaveSettings
	 */
	virtual void LoadSettings( ) = 0;

	/**
	 * Persists all device groups, launcher profiles and other settings to disk.\
	 *
	 * @see LoadSettings
	 */
	virtual void SaveSettings( ) = 0;


public:

	/**
	 * Returns a delegate that is invoked when a device group was added.
	 *
	 * @return The delegate.
	 */
	virtual FOnLauncherProfileManagerDeviceGroupAdded& OnDeviceGroupAdded( ) = 0;

	/**
	 * Returns a delegate that is invoked when a device group was removed.
	 *
	 * @return The delegate.
	 */
	virtual FOnLauncherProfileManagerDeviceGroupRemoved& OnDeviceGroupRemoved( ) = 0;

	/**
	 * Returns a delegate that is invoked when a profile was added.
	 *
	 * @return The delegate.
	 */
	virtual FOnLauncherProfileManagerProfileAdded& OnProfileAdded( ) = 0;

	/**
	 * Returns a delegate that is invoked when a profile was removed.
	 *
	 * @return The delegate.
	 */
	virtual FOnLauncherProfileManagerProfileRemoved& OnProfileRemoved( ) = 0;


public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ILauncherProfileManager( ) { }
};
