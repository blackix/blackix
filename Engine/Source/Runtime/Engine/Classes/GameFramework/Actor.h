// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentInstanceDataCache.h"
#include "Components/SceneComponent.h"
#include "EngineDefines.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "RenderCommandFence.h"
#include "TimerManager.h"

struct FHitResult;
class AActor;
class FTimerManager; 

#include "Actor.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogActor, Log, Warning);
 

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams( FTakeAnyDamageSignature, float, Damage, const class UDamageType*, DamageType, class AController*, InstigatedBy, AActor*, DamageCauser );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams( FTakePointDamageSignature, float, Damage, class AController*, InstigatedBy, FVector, HitLocation, class UPrimitiveComponent*, FHitComponent, FName, BoneName, FVector, ShotFromDirection, const class UDamageType*, DamageType, AActor*, DamageCauser );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorBeginOverlapSignature, AActor*, OtherActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorEndOverlapSignature, AActor*, OtherActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams( FActorHitSignature, AActor*, SelfActor, AActor*, OtherActor, FVector, NormalImpulse, const FHitResult&, Hit );

DECLARE_DYNAMIC_MULTICAST_DELEGATE( FActorBeginCursorOverSignature );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FActorEndCursorOverSignature );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FActorOnClickedSignature );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FActorOnReleasedSignature );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorOnInputTouchBeginSignature, ETouchIndex::Type, FingerIndex );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorOnInputTouchEndSignature, ETouchIndex::Type, FingerIndex );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorBeginTouchOverSignature, ETouchIndex::Type, FingerIndex );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorEndTouchOverSignature, ETouchIndex::Type, FingerIndex );

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FActorDestroyedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActorEndPlaySignature, EEndPlayReason::Type, EndPlayReason);

DECLARE_DELEGATE_FourParams(FMakeNoiseDelegate, AActor*, float, class APawn*, const FVector&);

#if !UE_BUILD_SHIPPING
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnProcessEvent, AActor*, UFunction*, void*);
#endif

DECLARE_CYCLE_STAT_EXTERN(TEXT("GetComponentsTime"),STAT_GetComponentsTime,STATGROUP_Engine,ENGINE_API);

/**
 * Actor is the base class for an Object that can be placed or spawned in a level.
 * Actors may contain a collection of ActorComponents, which can be used to control how actors move, how they are rendered, etc.
 * The other main function of an Actor is the replication of properties and function calls across the network during play.
 * 
 * @see https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/Actors/
 * @see UActorComponent
 */
UCLASS(BlueprintType, Blueprintable, config=Engine, meta=(ShortTooltip="An Actor is an object that can be placed or spawned in the world."))
class ENGINE_API AActor : public UObject
{
	/**
	* The functions of interest to initialization order for an Actor is roughly as follows:
	* PostLoad/PostActorCreated - Do any setup of the actor required for construction. PostLoad for serialized actors, PostActorCreated for spawned.  
	* AActor::OnConstruction - The construction of the actor, this is where Blueprint actors have their components created and blueprint variables are initialized
	* AActor::PreInitializeComponents - Called before InitializeComponent is called on the actor's components
	* UActorComponent::InitializeComponent - Each component in the actor's components array gets an initialize call (if bWantsInitializeComponent is true for that component)
	* AActor::PostInitializeComponents - Called after the actor's components have been initialized
	* AActor::BeginPlay - Called when the level is started
	*/

	GENERATED_BODY()
public:

	/**
	 * Default constructor for AActor
	 */
	AActor();

	/**
	 * Constructor for AActor that takes an ObjectInitializer
	 */
	AActor(const FObjectInitializer& ObjectInitializer);

private:
	/** Called from the constructor to initialize the class to its default settings */
	void InitializeDefaults();

public:
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Primary Actor tick function, which calls TickActor().
	 * Tick functions can be configured to control whether ticking is enabled, at what time during a frame the update occurs, and to set up tick dependencies.
	 * @see https://docs.unrealengine.com/latest/INT/API/Runtime/Engine/Engine/FTickFunction/
	 * @see AddTickPrerequisiteActor(), AddTickPrerequisiteComponent()
	 */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	struct FActorTickFunction PrimaryActorTick;

	/** Allow each actor to run at a different time speed. The DeltaTime for a frame is multiplied by the global TimeDilation (in WorldSettings) and this CustomTimeDilation for this actor's tick.  */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category="Misc")
	float CustomTimeDilation;	

public:
	/**
	 * Allows us to only see this Actor in the Editor, and not in the actual game.
	 * @see SetActorHiddenInGame()
	 */
	UPROPERTY(EditAnywhere, Category=Rendering, BlueprintReadOnly, replicated, meta=(DisplayName = "Actor Hidden In Game"))
	uint32 bHidden:1;

	/** If true, when the actor is spawned it will be sent to the client but receive no further replication updates from the server afterwards. */
	UPROPERTY()
	uint32 bNetTemporary:1;

	/** If true, this actor was loaded directly from the map, and for networking purposes can be addressed by its full path name */
	UPROPERTY()
	uint32 bNetStartup:1;

	/** If ture, this actor is only relevant to its owner. If this flag is changed during play, all non-owner channels would need to be explicitly closed. */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadOnly)
	uint32 bOnlyRelevantToOwner:1;

	/** Always relevant for network (overrides bOnlyRelevantToOwner). */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	uint32 bAlwaysRelevant:1;    

	/**
	 * If true, replicate movement/location related properties.
	 * Actor must also be set to replicate.
	 * @see SetReplicates()
	 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Replication/
	 */
	UPROPERTY(Replicated, Category=Replication, EditDefaultsOnly)
	uint32 bReplicateMovement:1;    

	/**
	 * If true, this actor is no longer replicated to new clients, and is "torn off" (becomes a ROLE_Authority) on clients to which it was being replicated.
	 * @see TornOff()
	 */
	UPROPERTY(replicated)
	uint32 bTearOff:1;    

	/**
	 * Whether we have already exchanged Role/RemoteRole on the client, as when removing then re-adding a streaming level.
	 * Causes all initialization to be performed again even though the actor may not have actually been reloaded.
	 */
	UPROPERTY(transient)
	uint32 bExchangedRoles:1;

	/** Is this actor still pending a full net update due to clients that weren't able to replicate the actor at the time of LastNetUpdateTime */
	UPROPERTY(transient)
	uint32 bPendingNetUpdate:1;

	/** This actor will be loaded on network clients during map load */
	UPROPERTY(Category=Replication, EditDefaultsOnly)
	uint32 bNetLoadOnClient:1;

	/** If actor has valid Owner, call Owner's IsNetRelevantFor and GetNetPriority */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	uint32 bNetUseOwnerRelevancy:1;

	/** If true, all input on the stack below this actor will not be considered */
	UPROPERTY(EditDefaultsOnly, Category=Input)
	uint32 bBlockInput:1;

	/** True if this actor is currently running user construction script (used to defer component registration) */
	uint32 bRunningUserConstructionScript:1;

private:
	/** Whether FinishSpawning has been called for this Actor.  If it has not, the Actor is in a mal-formed state */
	uint32 bHasFinishedSpawning:1;

	/**
	 * Enables any collision on this actor.
	 * @see SetActorEnableCollision(), GetActorEnableCollision()
	 */
	UPROPERTY()
	uint32 bActorEnableCollision:1;

protected:
	/**
	 * If true, this actor will replicate to remote machines
	 * @see SetReplicates()
	 */
	UPROPERTY(Category = Replication, EditDefaultsOnly, BlueprintReadOnly)
	uint32 bReplicates:1;

	/** This function should only be used in the constructor of classes that need to set the RemoteRole for backwards compatibility purposes */
	void SetRemoteRoleForBackwardsCompat(const ENetRole InRemoteRole) { RemoteRole = InRemoteRole; }

	/**
	 * Does this actor have an owner responsible for replication? (APlayerController typically)
	 *
	 * @return true if this actor can call RPCs or false if no such owner chain exists
	 */
	virtual bool HasNetOwner() const;

private:
	/**
	 * Describes how much control the remote machine has over the actor.
	 */
	UPROPERTY(Replicated, transient)
	TEnumAsByte<enum ENetRole> RemoteRole;

	/**
	 * Owner of this Actor, used primarily for replication (bNetUseOwnerRelevancy & bOnlyRelevantToOwner) and visibility (PrimitiveComponent bOwnerNoSee and bOnlyOwnerSee)
	 * @see SetOwner(), GetOwner()
	 */
	UPROPERTY(replicated)
	AActor* Owner;

public:

	/**
	 * Set whether this actor replicates to network clients. When this actor is spawned on the server it will be sent to clients as well.
	 * Properties flagged for replication will update on clients if they change on the server.
	 * Internally changes the RemoteRole property and handles the cases where the actor needs to be added to the network actor list.
	 * @param bInReplicates Whether this Actor replicates to network clients.
	 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Replication/
	 */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	void SetReplicates(bool bInReplicates);

	/** Sets whether or not this Actor is an autonomous proxy, which is an actor on a network client that is controlled by a user on that client. */
	void SetAutonomousProxy(bool bInAutonomousProxy);
	
	/** Copies RemoteRole from another Actor and adds this actor to the list of network actors if necessary. */
	void CopyRemoteRoleFrom(const AActor* CopyFromActor);

	/** Returns how much control the remote machine has over this actor. */
	ENetRole GetRemoteRole() const;

	/** Used for replication of our RootComponent's position and velocity */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedMovement)
	struct FRepMovement ReplicatedMovement;

	/** Used for replicating attachment of this actor's RootComponent to another actor. */
	UPROPERTY(replicatedUsing=OnRep_AttachmentReplication)
	struct FRepAttachment AttachmentReplication;

	/** Called on client when updated AttachmentReplication value is received for this actor. */
	UFUNCTION()
	virtual void OnRep_AttachmentReplication();

	/** Describes how much control the local machine has over the actor. */
	UPROPERTY(Replicated)
	TEnumAsByte<enum ENetRole> Role;

	/** Dormancy setting for actor to take itself off of the replication list without being destroyed on clients. */
	TEnumAsByte<enum ENetDormancy> NetDormancy;

	/** Automatically registers this actor to receive input from a player. */
	UPROPERTY(EditAnywhere, Category=Input)
	TEnumAsByte<EAutoReceiveInput::Type> AutoReceiveInput;

	/** The priority of this input component when pushed in to the stack. */
	UPROPERTY(EditAnywhere, Category=Input)
	int32 InputPriority;

	/** Component that handles input for this actor, if input is enabled. */
	UPROPERTY()
	class UInputComponent* InputComponent;

	UPROPERTY()
	TEnumAsByte<enum EInputConsumeOptions> InputConsumeOption_DEPRECATED;

	/** Square of the max distance from the client's viewpoint that this actor is relevant and will be replicated. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=Replication)
	float NetCullDistanceSquared;   

	/** Internal - used by UWorld::ServerTickClients() */
	UPROPERTY(transient)
	int32 NetTag;

	/** Next time this actor will be considered for replication, set by SetNetUpdateTime() */
	UPROPERTY()
	float NetUpdateTime;

	/** How often (per second) this actor will be considered for replication, used to determine NetUpdateTime */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	float NetUpdateFrequency;

	/** Priority for this actor when checking for replication in a low bandwidth or saturated situation, higher priority means it is more likely to replicate */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	float NetPriority;

	/** Last time this actor was updated for replication via NetUpdateTime
	 * @warning: internal net driver time, not related to WorldSettings.TimeSeconds */
	UPROPERTY(transient)
	float LastNetUpdateTime;

	/** Used to specify the net driver to replicate on (NAME_None || NAME_GameNetDriver is the default net driver) */
	UPROPERTY()
	FName NetDriverName;

	/** Method that allows an actor to replicate subobjects on its actor channel */
	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags);

	/** Called on the actor when a new subobject is dynamically created via replication */
	virtual void OnSubobjectCreatedFromReplication(UObject *NewSubobject);

	/** Called on the actor when a new subobject is dynamically created via replication */
	virtual void OnSubobjectDestroyFromReplication(UObject *NewSubobject);

	/** Called on the actor right before replication occurs */
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker );

	/** If true then destroy self when "finished", meaning all relevant components report that they are done and no timelines or timers are in flight. */
	UPROPERTY(BlueprintReadWrite, Category=Actor)
	uint32 bAutoDestroyWhenFinished:1;

	/**
	 * Whether this actor can take damage. Must be true for damage events (e.g. ReceiveDamage()) to be called.
	 * @see https://www.unrealengine.com/blog/damage-in-ue4
	 * @see TakeDamage(), ReceiveDamage()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category=Actor)
	uint32 bCanBeDamaged:1;

	/**
	 * Set when actor is about to be deleted.
	 * @see IsPendingKillPending()
	 */
	UPROPERTY(Transient, DuplicateTransient)
	uint32 bPendingKillPending:1;    

	/** This actor collides with the world when placing in the editor or when spawned, even if RootComponent collision is disabled */
	UPROPERTY()
	uint32 bCollideWhenPlacing:1;    

	/** If true, this actor should search for an owned camera component to view through when used as a view target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Actor, AdvancedDisplay)
	uint32 bFindCameraComponentWhenViewTarget:1;

	/** Pawn responsible for damage caused by this actor. */
	UPROPERTY(BlueprintReadWrite, replicatedUsing=OnRep_Instigator, meta=(ExposeOnSpawn=true), Category=Actor)
	class APawn* Instigator;

	/** Called on clients when Instigator is replicated. */
	UFUNCTION()
	virtual void OnRep_Instigator();

	/** The time this actor was created, relative to World->GetTimeSeconds().
	 * @see UWorld::GetTimeSeconds()
	 */
	float CreationTime;

	/** Array of Actors whose Owner is this actor */
	UPROPERTY(transient)
	TArray<AActor*> Children;
	
	// Animation update rate control.
protected:
	/** Unique Tag assigned to spread updates of SkinnedMeshes over time. */
	UPROPERTY(Transient)
	uint32 AnimUpdateRateShiftTag;
public:
	/** Frame counter to call AnimUpdateRateTick() just once per frame. */
	UPROPERTY(Transient)
	uint32 AnimUpdateRateFrameCount;

	/** Get a unique ID to share with all SkinnedMeshComponents in this actor. */
	uint32 GetAnimUpdateRateShiftTag();

protected:
	/**
	 * Collision primitive that defines the transform (location, rotation, scale) of this Actor.
	 */
	UPROPERTY()
	class USceneComponent* RootComponent;

	/** The matinee actors that control this actor. */
	UPROPERTY(transient)
	TArray<class AMatineeActor*> ControllingMatineeActors;

	/** How long this Actor lives before dying, 0=forever. Note this is the INITIAL value and should not be modified once play has begun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Actor)
	float InitialLifeSpan; 

private:

	/** Handle for efficient management of LifeSpanExpired timer */
	FTimerHandle TimerHandle_LifeSpanExpired;

protected:

	/**
	 * If false, the Blueprint ReceiveTick() event will be disabled on dedicated servers.
	 * @see AllowReceiveTickEventOnDedicatedServer()
	 */
	UPROPERTY()
	uint32 bAllowReceiveTickEventOnDedicatedServer:1;

public:

	/** Return the value of bAllowReceiveTickEventOnDedicatedServer, indicating whether the Blueprint ReceiveTick() event will occur on dedicated servers. */
	FORCEINLINE bool AllowReceiveTickEventOnDedicatedServer() const { return bAllowReceiveTickEventOnDedicatedServer; }

	/** Layer's the actor belongs to.  This is outside of the editoronly data to allow hiding of LD-specified layers at runtime for profiling. */
	UPROPERTY()
	TArray< FName > Layers;

#if WITH_EDITORONLY_DATA
protected:

	UPROPERTY()
	uint32 bActorLabelEditable:1;    // Is the actor label editable by the user?

private:
	/**
	 * The friendly name for this actor, displayed in the editor.  You should always use AActor::GetActorLabel() to access the actual label to display,
	 * and call AActor::SetActorLabel() or AActor::SetActorLabelUnique() to change the label.  Never set the label directly.
	 */
	UPROPERTY()
	FString ActorLabel;

	/** The folder path of this actor in the world (empty=root, / separated)*/
	UPROPERTY()
	FName FolderPath;

public:
	/** Whether this actor is hidden within the editor viewport. */
	UPROPERTY()
	uint32 bHiddenEd:1;

protected:
	/** Whether the actor can be manipulated by editor operations. */
	UPROPERTY()
	uint32 bEditable:1;

	/** Whether this actor should be listed in the scene outliner. */
	UPROPERTY()
	uint32 bListedInSceneOutliner:1;

public:
	/** Whether this actor is hidden by the layer browser. */
	UPROPERTY()
	uint32 bHiddenEdLayer:1;

private:
	/** Whether this actor is temporarily hidden within the editor; used for show/hide/etc functionality w/o dirtying the actor. */
	UPROPERTY(transient)
	uint32 bHiddenEdTemporary:1;

public:

	/** Whether this actor is hidden by the level browser. */
	UPROPERTY(transient)
	uint32 bHiddenEdLevel:1;

	/** If true, prevents the actor from being moved in the editor viewport. */
	UPROPERTY()
	uint32 bLockLocation:1;

	/** The group this actor is a part of. */
	UPROPERTY(transient)
	AActor* GroupActor;

	/** The scale to apply to any billboard components in editor builds (happens in any WITH_EDITOR build, including non-cooked games). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rendering, meta=(DisplayName="Editor Billboard Scale"))
	float SpriteScale;

	/** Returns how many lights are uncached for this actor. */
	int32 GetNumUncachedLights();
#endif // WITH_EDITORONLY_DATA

	/** The Actor that owns the UChildActorComponent that owns this Actor. */
	UPROPERTY()
	TWeakObjectPtr<AActor> ParentComponentActor;	

public:

	/** 
	 *	Indicates that PreInitializeComponents/PostInitializeComponents have been called on this Actor 
	 *	Prevents re-initializing of actors spawned during level startup
	 */
	uint32 bActorInitialized:1;

	/** Indicates the actor was pulled through a seamless travel.  */
	UPROPERTY()
	uint32 bActorSeamlessTraveled:1;

	/** Whether this actor should no be affected by world origin shifting. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Actor)
	uint32 bIgnoresOriginShifting:1;
	
	/** Array of tags that can be used for grouping and categorizing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Actor)
	TArray<FName> Tags;

	/** Bitflag to represent which views this actor is hidden in, via per-view layer visibility. */
	UPROPERTY(transient)
	uint64 HiddenEditorViews;

	//==============================================================================================
	// Delegates
	
	/** Called when the actor is damaged in any way. */
	UPROPERTY(BlueprintAssignable, Category="Game|Damage")
	FTakeAnyDamageSignature OnTakeAnyDamage;

	/** Called when the actor is damaged by point damage. */
	UPROPERTY(BlueprintAssignable, Category="Game|Damage")
	FTakePointDamageSignature OnTakePointDamage;
	
	/** 
	 *	Called when another actor begins to overlap this actor, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorBeginOverlapSignature OnActorBeginOverlap;

	/** 
	 *	Called when another actor steps overlapping this actor. 
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorEndOverlapSignature OnActorEndOverlap;

	/** Called when the mouse cursor is moved over this actor if mouse over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorBeginCursorOverSignature OnBeginCursorOver;

	/** Called when the mouse cursor is moved off this actor if mouse over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorEndCursorOverSignature OnEndCursorOver;

	/** Called when the left mouse button is clicked while the mouse is over this actor and click events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorOnClickedSignature OnClicked;

	/** Called when the left mouse button is released while the mouse is over this actor and click events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorOnReleasedSignature OnReleased;

	/** Called when a touch input is received over this actor when touch events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorOnInputTouchBeginSignature OnInputTouchBegin;
		
	/** Called when a touch input is received over this component when touch events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorOnInputTouchEndSignature OnInputTouchEnd;

	/** Called when a finger is moved over this actor when touch over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorBeginTouchOverSignature OnInputTouchEnter;

	/** Called when a finger is moved off this actor when touch over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorEndTouchOverSignature OnInputTouchLeave;

	/** 
	 *	Called when this Actor hits (or is hit by) something solid. This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 *	For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *	@note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorHitSignature OnActorHit;

	/** 
	 * Pushes this actor on to the stack of input being handled by a PlayerController.
	 * @param PlayerController The PlayerController whose input events we want to receive.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void EnableInput(class APlayerController* PlayerController);

	/** 
	 * Removes this actor from the stack of input being handled by a PlayerController.
	 * @param PlayerController The PlayerController whose input events we no longer want to receive. If null, this actor will stop receiving input from all PlayerControllers.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DisableInput(class APlayerController* PlayerController);

	/** Gets the value of the input axis if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", BlueprintProtected = "true", HidePin="InputAxisName"))
	float GetInputAxisValue(const FName InputAxisName) const;

	/** Gets the value of the input axis key if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", BlueprintProtected = "true", HidePin="InputAxisKey"))
	float GetInputAxisKeyValue(const FKey InputAxisKey) const;

	/** Gets the value of the input axis key if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", BlueprintProtected = "true", HidePin="InputAxisKey"))
	FVector GetInputVectorAxisValue(const FKey InputAxisKey) const;

	/** Returns the instigator for this actor, or NULL if there is none. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintProtected = "true"), Category="Game|Damage")
	APawn* GetInstigator() const;

	/**
	 * Get the instigator, cast as a specific class.
	 * @return The instigator for this weapon if it is the specified type, NULL otherwise.
	 */
	template <class T>
	T* GetInstigator() const { return Cast<T>(Instigator); };

	/** Returns the instigator's controller for this actor, or NULL if there is none. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintProtected = "true"), Category="Game|Damage")
	AController* GetInstigatorController() const;


	//=============================================================================
	// General functions.

	/**
	 * Get the actor-to-world transform.
	 * @return The transform that transforms from actor space to world space.
	 */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetActorTransform"), Category="Utilities|Transformation")
	FTransform GetTransform() const;

	/** Get the local-to-world transform of the RootComponent. Identical to GetTransform(). */
	FTransform ActorToWorld() const;

	/** Returns the location of the RootComponent of this Actor */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetActorLocation", Keywords="position"), Category="Utilities|Transformation")
	FVector K2_GetActorLocation() const;

	/** 
	 *	Move the Actor to the specified location.
	 *	@param NewLocation	The new location to move the Actor to.
	 *	@param bSweep		Should we sweep to the destination location, stopping short of the target if blocked by something.
	 *  @param SweepHitResult	The hit result from the move if swept.
	 *	@return	Whether the location was successfully set (if not swept), or whether movement occurred at all (if swept).
	 */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "SetActorLocation", Keywords="position"), Category="Utilities|Transformation")
	bool K2_SetActorLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult);

	/** Returns rotation of the RootComponent of this Actor. */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetActorRotation"), Category="Utilities|Transformation")
	FRotator K2_GetActorRotation() const;

	/** Get the forward (X) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorForwardVector() const;

	/** Get the up (Z) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorUpVector() const;

	/** Get the right (Y) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorRightVector() const;

	/**
	 * Returns the bounding box of all components that make up this Actor.
	 * @param	bOnlyCollidingComponents	If true, will only return the bounding box for components with collision enabled.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(FriendlyName = "GetActorBounds"))
	void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent) const;

	/** Returns the RootComponent of this Actor */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetRootComponent"), Category="Utilities|Transformation")
	class USceneComponent* K2_GetRootComponent() const;

	/** Returns velocity (in cm/s (Unreal Units/second) of the rootcomponent if it is either using physics or has an associated MovementComponent */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	virtual FVector GetVelocity() const;

	/** 
	 * Move the actor instantly to the specified location. 
	 * 
	 * @param NewLocation		The new location to teleport the Actor to.
	 * @param bSweep			Whether to sweep to the destination location, triggering overlaps along the way and stopping at the first blocking hit.
	 * @param OutSweepHitResult The hit result from the move if swept.
	 * @return	Whether the location was successfully set if not swept, or whether movement occurred if swept.
	 */
	bool SetActorLocation(const FVector& NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);

	/** 
	 * Set the Actor's rotation instantly to the specified rotation.
	 * 
	 * @param	NewRotation	The new rotation for the Actor.
	 * @return	Whether the rotation was successfully set.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	bool SetActorRotation(FRotator NewRotation);

	/** 
	 * Move the actor instantly to the specified location and rotation.
	 * 
	 * @param NewLocation			The new location to teleport the Actor to.
	 * @param NewRotation			The new rotation for the Actor.
	 * @param bSweep				Whether to sweep to the destination location, triggering overlaps along the way and stopping at the first blocking hit.
	 * @param SweepHitResult		The hit result from the move if swept.
	 * @return	Whether the rotation was successfully set.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="SetActorLocationAndRotation"))
	bool K2_SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult);
	
	/** 
	 * Move the actor instantly to the specified location and rotation.
	 * 
	 * @param NewLocation			The new location to teleport the Actor to.
	 * @param NewRotation			The new rotation for the Actor.
	 * @param bSweep				Whether to sweep to the destination location, triggering overlaps along the way and stopping at the first blocking hit.
	 * @param OutSweepHitResult	The hit result from the move if swept.
	 * @return	Whether the rotation was successfully set.
	 */
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);

	/** Set the Actor's world-space scale. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetActorScale3D(FVector NewScale3D);

	/** Returns the Actor's world-space scale. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Orientation")
	FVector GetActorScale3D() const;

	/** Returns the distance from this Actor to OtherActor. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetDistanceTo(const AActor* OtherActor) const;

	/** Returns the distance from this Actor to OtherActor, ignoring Z. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetHorizontalDistanceTo(const AActor* OtherActor) const;

	/** Returns the distance from this Actor to OtherActor, ignoring XY. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetVerticalDistanceTo(const AActor* OtherActor) const;

	/** Returns the dot product from this Actor to OtherActor. Returns -2.0 on failure. Returns 0.0 for coincidental actors. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetDotProductTo(const AActor* OtherActor) const;

	/** Returns the dot product from this Actor to OtherActor, ignoring Z. Returns -2.0 on failure. Returns 0.0 for coincidental actors. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetHorizontalDotProductTo(const AActor* OtherActor) const;

	/**
	 * Adds a delta to the location of this actor in world space.
	 * 
	 * @param  DeltaLocation		The change in location.
	 * @param  bSweep				Whether to sweep to the destination location, triggering overlaps along the way and stopping at the first blocking hit.
	 * @param  SweepHitResult		The hit result from the move if swept.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorWorldOffset", Keywords="location position"))
	void K2_AddActorWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult);

	/**
	 * Adds a delta to the location of this actor in world space.
	 * 
	 * @param  DeltaLocation		The change in location.
	 * @param  bSweep				Whether to sweep to the destination location, triggering overlaps along the way and stopping at the first blocking hit.
	 * @param  SweepHitResult		The hit result from the move if swept.
	 */
	void AddActorWorldOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/**
	 * Adds a delta to the rotation of this actor in world space.
	 * 
	 * @param  DeltaRotation		The change in rotation.
	 * @param  bSweep				Whether to sweep to the target rotation (not currently supported).
	 * @param  SweepHitResult		The hit result from the move if swept.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorWorldRotation", AdvancedDisplay="bSweep,SweepHitResult"))
	void K2_AddActorWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult);
	void AddActorWorldRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/** Adds a delta to the transform of this actor in world space. Scale is unchanged. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorWorldTransform"))
	void K2_AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult);
	void AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/** 
	 * Set the Actors transform to the specified one.
	 * @param bSweep		Whether to sweep to the destination location, stopping short of the target if blocked by something.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="SetActorTransform"))
	bool K2_SetActorTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult);
	bool SetActorTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/** Adds a delta to the location of this component in its local reference frame */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorLocalOffset", Keywords="location position"))
	void K2_AddActorLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult);
	void AddActorLocalOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/** Adds a delta to the rotation of this component in its local reference frame */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorLocalRotation", AdvancedDisplay="bSweep,SweepHitResult"))
	void K2_AddActorLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult);
	void AddActorLocalRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/** Adds a delta to the transform of this component in its local reference frame */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="AddActorLocalTransform"))
	void K2_AddActorLocalTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult);
	void AddActorLocalTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);


	/**
	 * Set the actor's RootComponent to the specified relative location
	 * @param NewRelativeLocation	New relative location to set the actor's RootComponent to
	 * @param bSweep				Should we sweep to the destination location. If true, will stop short of the target if blocked by something
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="SetActorRelativeLocation"))
	void K2_SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep, FHitResult& SweepHitResult);
	void SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);

	/**
	 * Set the actor's RootComponent to the specified relative rotation
	 * @param NewRelativeRotation		New relative rotation to set the actor's RootComponent to
	 * @param bSweep					Should we sweep to the destination rotation. If true, will stop short of the target if blocked by something
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="SetActorRelativeRotation", AdvancedDisplay="bSweep,SweepHitResult"))
	void K2_SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep, FHitResult& SweepHitResult);
	void SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);

	/**
	 * Set the actor's RootComponent to the specified relative transform
	 * @param NewRelativeTransform		New relative transform to set the actor's RootComponent to
	 * @param bSweep					Should we sweep to the destination transform. If true, will stop short of the target if blocked by something
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(FriendlyName="SetActorRelativeTransform"))
	void K2_SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep, FHitResult& SweepHitResult);
	void SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr);

	/**
	 * Set the actor's RootComponent to the specified relative scale 3d
	 * @param NewRelativeScale	New scale to set the actor's RootComponent to
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetActorRelativeScale3D(FVector NewRelativeScale);

	/**
	 * Return the actor's relative scale 3d
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Orientation")
	FVector GetActorRelativeScale3D() const;

	/**
	 *	Sets the actor to be hidden in the game
	 *	@param	bNewHidden	Whether or not to hide the actor and all its components
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=( FriendlyName = "Set Actor Hidden In Game", Keywords = "Visible Hidden Show Hide" ))
	virtual void SetActorHiddenInGame(bool bNewHidden);

	/** Allows enabling/disabling collision for the whole actor */
	UFUNCTION(BlueprintCallable, Category="Collision")
	void SetActorEnableCollision(bool bNewActorEnableCollision);

	/** Get current state of collision for the whole actor */
	UFUNCTION(BlueprintPure, Category="Collision")
	bool GetActorEnableCollision();

	/** Destroy the actor */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "Delete", FriendlyName = "DestroyActor"))
	virtual void K2_DestroyActor();

	/** Returns whether this actor has network authority */
	UFUNCTION(BlueprintCallable, Category="Networking")
	bool HasAuthority() const;

	/** 
	 * Create a new component given a template name. Template is found in the owning Blueprint.
	 * Automatic attachment causes the first component created to become the root component, and all subsequent components
	 * will be attached to the root component.  In manual mode, it is up to the user to attach or set as root.
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", DefaultToSelf="ComponentTemplateContext", HidePin="ComponentTemplateContext"))
	class UActorComponent* AddComponent(FName TemplateName, bool bManualAttachment, const FTransform& RelativeTransform, const UObject* ComponentTemplateContext);

	/** DEPRECATED - Use Component::DestroyComponent */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use Component.DestroyComponent instead", BlueprintProtected = "true", FriendlyName = "DestroyComponent"))
	void K2_DestroyComponent(UActorComponent* Component);

	/** 
	 *  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered. 
	 *   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	 */
	void AttachRootComponentTo(class USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = false);

	/**
	*  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	*   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	*/

	UFUNCTION(BlueprintCallable, meta = (FriendlyName = "AttachActorToComponent", AttachLocationType = "KeepRelativeOffset"), Category = "Utilities|Transformation")
	void K2_AttachRootComponentTo(class USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = true);

	/**
	 * Attaches the RootComponent of this Actor to the RootComponent of the supplied actor, optionally at a named socket.
	 * @param InParentActor				Actor to attach this actor's RootComponent to
	 * @param InSocketName				Socket name to attach to, if any
	 * @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	 */
	void AttachRootComponentToActor(AActor* InParentActor, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = false);

	/**
	*  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	*   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	*/

	UFUNCTION(BlueprintCallable, meta = (FriendlyName = "AttachActorToActor", AttachLocationType = "KeepRelativeOffset"), Category = "Utilities|Transformation")
	void K2_AttachRootComponentToActor(AActor* InParentActor, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = true);

	/** 
	 *  Snap the RootComponent of this Actor to the supplied Actor's root component, optionally at a named socket. It is not valid to call this on components that are not Registered. 
	 *  If InSocketName == NAME_None, it will attach to origin of the InParentActor. 
	 */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use AttachRootComponentTo with EAttachLocation::SnapToTarget option instead", FriendlyName = "SnapActorTo"), Category="Utilities|Transformation")
	void SnapRootComponentTo(AActor* InParentActor, FName InSocketName = NAME_None);

	/** 
	 *  Detaches the RootComponent of this Actor from any SceneComponent it is currently attached to. 
	 *   @param bMaintainWorldTransform	If true, update the relative location/rotation of this component to keep its world position the same
	 */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "DetachActorFromActor"), Category="Utilities|Transformation")
	void DetachRootComponentFromParent(bool bMaintainWorldPosition = true);

	/** 
	 *	Detaches all SceneComponents in this Actor from the supplied parent SceneComponent. 
	 *	@param InParentComponent		SceneComponent to detach this actor's components from
	 *	@param bMaintainWorldTransform	If true, update the relative location/rotation of this component to keep its world position the same
	 */
	void DetachSceneComponentsFromParent(class USceneComponent* InParentComponent, bool bMaintainWorldPosition = true);

	//==============================================================================
	// Tags

	/** See if this actor contains the supplied tag */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool ActorHasTag(FName Tag) const;


	//==============================================================================
	// Misc Blueprint support

	/** 
	 * Get CustomTimeDilation - this can be used for input control or speed control for slomo.
	 * We don't want to scale input globally because input can be used for UI, which do not care for TimeDilation.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Time")
	float GetActorTimeDilation() const;

	DEPRECATED(4.5, "Actor::SetTickPrerequisite() will be removed, use AddTickPrerequisiteActor().")
	void SetTickPrerequisite(AActor* PrerequisiteActor);

	/** Make this actor tick after PrerequisiteActor. This only applies to this actor's tick function; dependencies for owned components must be set up separately if desired. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Make this actor tick after PrerequisiteComponent. This only applies to this actor's tick function; dependencies for owned components must be set up separately if desired. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Remove tick dependency on PrerequisiteActor. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Remove tick dependency on PrerequisiteComponent. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Sets whether this actor can tick when paused. */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetTickableWhenPaused(bool bTickableWhenPaused);

	/** Allocate a MID for a given parent material. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PrimitiveComponent.CreateAndSetMaterialInstanceDynamic instead.", BlueprintProtected = "true"), Category="Rendering|Material")
	class UMaterialInstanceDynamic* MakeMIDForMaterial(class UMaterialInterface* Parent);

	//=============================================================================
	// Sound functions.
	
	DEPRECATED(4.0, "Actor::PlaySoundOnActor will be removed. Use UGameplayStatics::PlaySoundAttached instead.")
	void PlaySoundOnActor(class USoundCue* InSoundCue, float VolumeMultiplier=1.f, float PitchMultiplier=1.f);

	DEPRECATED(4.0, "Actor::PlaySoundOnActor will be removed. Use UGameplayStatics::PlaySoundAtLocation instead.")
	void PlaySoundAtLocation(class USoundCue* InSoundCue, FVector SoundLocation, float VolumeMultiplier=1.f, float PitchMultiplier=1.f);

	//=============================================================================
	// AI functions.
	
	/**
	 * Trigger a noise caused by a given Pawn, at a given location.
	 * Note that the NoiseInstigator Pawn MUST have a PawnNoiseEmitterComponent for the noise to be detected by a PawnSensingComponent.
	 * Senders of MakeNoise should have an Instigator if they are not pawns, or pass a NoiseInstigator.
	 *
	 * @param Loudness - is the relative loudness of this noise (range 0.0 to 1.0).  Directly affects the hearing range specified by the SensingComponent's HearingThreshold.
	 * @param NoiseInstigator - Pawn responsible for this noise.  Uses the actor's Instigator if NoiseInstigator=NULL
	 * @param NoiseLocation - Position of noise source.  If zero vector, use the actor's location.
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI", meta=(BlueprintProtected = "true"))
	void MakeNoise(float Loudness=1.f, APawn* NoiseInstigator=NULL, FVector NoiseLocation=FVector::ZeroVector);

	//=============================================================================
	// Blueprint
	
	/** Event when play begins for this actor. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "BeginPlay"))
	virtual void ReceiveBeginPlay();

	/** Event when play begins for this actor. */
	virtual void BeginPlay();

	/** Event when this actor takes ANY damage */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(FriendlyName = "AnyDamage"), Category="Damage")
	virtual void ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
	
	/** 
	 * Event when this actor takes RADIAL damage 
	 * @todo Pass it the full array of hits instead of just one?
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(FriendlyName = "RadialDamage"), Category="Damage")
	virtual void ReceiveRadialDamage(float DamageReceived, const class UDamageType* DamageType, FVector Origin, const struct FHitResult& HitInfo, class AController* InstigatedBy, AActor* DamageCauser);

	/** Event when this actor takes POINT damage */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(FriendlyName = "PointDamage"), Category="Damage")
	virtual void ReceivePointDamage(float Damage, const class UDamageType* DamageType, FVector HitLocation, FVector HitNormal, class UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, class AController* InstigatedBy, AActor* DamageCauser);

	/** Event called every frame */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "Tick"))
	virtual void ReceiveTick(float DeltaSeconds);

	/** 
	 *	Event when this actor overlaps another actor, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorBeginOverlap"), Category="Collision")
	virtual void ReceiveActorBeginOverlap(AActor* OtherActor);

	/** 
	 *	Event when an actor no longer overlaps another actor, and they have separated. 
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorEndOverlap"), Category="Collision")
	virtual void ReceiveActorEndOverlap(AActor* OtherActor);

	/** Event when this actor has the mouse moved over it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorBeginCursorOver"), Category="Mouse Input")
	virtual void ReceiveActorBeginCursorOver();

	/** Event when this actor has the mouse moved off of it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorEndCursorOver"), Category="Mouse Input")
	virtual void ReceiveActorEndCursorOver();

	/** Event when this actor is clicked by the mouse when using the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorOnClicked"), Category="Mouse Input")
	virtual void ReceiveActorOnClicked();

	/** Event when this actor is under the mouse when left mouse button is released while using the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "ActorOnReleased"), Category="Mouse Input")
	virtual void ReceiveActorOnReleased();

	/** Event when this actor is touched when click events are enabled. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "BeginInputTouch"), Category="Touch Input")
	virtual void ReceiveActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex);

	/** Event when this actor is under the finger when untouched when click events are enabled. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "EndInputTouch"), Category="Touch Input")
	virtual void ReceiveActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex);

	/** Event when this actor has a finger moved over it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "TouchEnter"), Category="Touch Input")
	virtual void ReceiveActorOnInputTouchEnter(const ETouchIndex::Type FingerIndex);

	/** Event when this actor has a finger moved off of it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "TouchLeave"), Category="Touch Input")
	virtual void ReceiveActorOnInputTouchLeave(const ETouchIndex::Type FingerIndex);

	/** Event when keys/touches/tilt/etc happen */
	UFUNCTION(BlueprintImplementableEvent, meta=(DeprecatedFunction))
	virtual void ReceiveInput(const FString& InputName, float Value, FVector VectorValue, bool bStarted, bool bEnded);

	/** 
	 * Returns list of actors this actor is overlapping (any component overlapping any component). Does not return itself.
	 * @param OverlappingActors		[out] Returned list of overlapping actors
	 * @param ClassFilter			[optional] If set, only returns actors of this class or subclasses
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingActors(TArray<AActor*>& OverlappingActors, UClass* ClassFilter=NULL) const;

	/** Returns list of components this actor is overlapping. */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingComponents(TArray<UPrimitiveComponent*>& OverlappingComponents) const;

	/** 
	 *	Event when this actor bumps into a blocking object, or blocks another actor that bumps into it. This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 *	For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *	@note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName = "Hit"), Category="Collision")
	virtual void ReceiveHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);

	/** Set the lifespan of this actor. When it expires the object will be destroyed. If requested lifespan is 0, the timer is cleared and the actor will not be destroyed. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "delete destroy"))
	virtual void SetLifeSpan( float InLifespan );

	/** Get the remaining lifespan of this actor. If zero is returned the actor lives forever. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "delete destroy"))
	virtual float GetLifeSpan() const;

	/**
	 * Construction script, the place to spawn components and do other setup.
	 * @note Name used in CreateBlueprint function
	 * @param	Location	The location.
	 * @param	Rotation	The rotation.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintInternalUseOnly = "true", FriendlyName = "Construction Script"))
	virtual void UserConstructionScript();

	/**
	 * Destroy this actor. Returns true if destroyed, false if indestructible.
	 * Destruction is latent. It occurs at the end of the tick.
	 * @param	bNetForce				[opt] Ignored unless called during play.  Default is false.
	 * @param	bShouldModifyLevel		[opt] If true, Modify() the level before removing the actor.  Default is true.	
	 * returns	the state of the RF_PendingKill flag
	 */
	bool Destroy(bool bNetForce = false, bool bShouldModifyLevel = true );

	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "delete", FriendlyName = "Destroyed"))
	virtual void ReceiveDestroyed();

	/** Event triggered when the actor is destroyed. */
	UPROPERTY(BlueprintAssignable, Category="Game")
	FActorDestroyedSignature OnDestroyed;


	/** Event to notify blueprints this actor is about to be deleted. */
	UFUNCTION(BlueprintImplementableEvent, meta=(Keywords = "delete", FriendlyName = "End Play"))
	virtual void ReceiveEndPlay(EEndPlayReason::Type EndPlayReason);

	/** Event triggered when the actor is being removed from a level. */
	UPROPERTY(BlueprintAssignable, Category="Game")
	FActorEndPlaySignature OnEndPlay;
	
	// Begin UObject Interface
	virtual bool CheckDefaultSubobjectsInternal() override;
	virtual void PostInitProperties() override;
	virtual bool Modify( bool bAlwaysMarkDirty=true ) override;
	virtual void ProcessEvent( UFunction* Function, void* Parameters ) override;
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack ) override;
	virtual bool CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack ) override;
	virtual void PostLoad() override;
	virtual void PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph ) override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual bool Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	struct FActorRootComponentReconstructionData
	{
		// Struct to store info about attached actors
		struct FAttachedActorInfo
		{
			TWeakObjectPtr<AActor> Actor;
			FName SocketName;
			FTransform RelativeTransform;
		};

		// The RootComponent's transform
		FTransform Transform;

		// The Actor the RootComponent is attached to
		FAttachedActorInfo AttachedParentInfo;

		// Actors that are attached to this RootComponent
		TArray<FAttachedActorInfo> AttachedToInfo;
	};

	class FActorTransactionAnnotation : public ITransactionObjectAnnotation
	{
	public:
		FActorTransactionAnnotation(const AActor* Actor);

		bool HasInstanceData() const;

		FComponentInstanceDataCache ComponentInstanceData;

		// Root component reconstruction data
		bool bRootComponentDataCached;
		FActorRootComponentReconstructionData RootComponentData;
	};

	/* Cached pointer to the transaction annotation data from PostEditUndo to be used in the next RerunConstructionScript */
	TSharedPtr<FActorTransactionAnnotation> CurrentTransactionAnnotation;

	virtual TSharedPtr<ITransactionObjectAnnotation> GetTransactionAnnotation() const override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;

	/** @return true if the component is allowed to re-register its components when modified.  False for CDOs or PIE instances. */
	bool ReregisterComponentsWhenModified() const;
#endif // WITH_EDITOR
	// End UObject Interface

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished);
#endif // WITH_EDITOR

	//-----------------------------------------------------------------------------------------------
	// PROPERTY REPLICATION

	/** fills ReplicatedMovement property */
	virtual void GatherCurrentMovement();

	/**
	 * See if this actor is owned by TestOwner.
	 */
	inline bool IsOwnedBy( const AActor* TestOwner ) const
	{
		for( const AActor* Arg=this; Arg; Arg=Arg->Owner )
		{
			if( Arg == TestOwner )
				return true;
		}
		return false;
	}

	/**
	 * Returns location of the RootComponent 
	 * this is a template for no other reason than to delay compilation until USceneComponent is defined
	 */ 
	template<class T>
	static FORCEINLINE FVector GetActorLocation(const T* RootComponent)
	{
		FVector Result(0.f);
		if( RootComponent != NULL )
		{
			Result = RootComponent->GetComponentLocation();
		}
		return Result;
	}

	/**
	 * Returns rotation of the RootComponent 
	 * this is a template for no other reason than to delay compilation until USceneComponent is defined
	 */ 
	template<class T>
	static FORCEINLINE FRotator GetActorRotation(T* RootComponent)
	{
		FRotator Result(0,0,0);
		if( RootComponent != NULL )
		{
			Result = RootComponent->GetComponentRotation();
		}
		return Result;
	}

	/**
	 * Returns scale of the RootComponent 
	 * this is a template for no other reason than to delay compilation until USceneComponent is defined
	 */ 
	template<class T>
	static FORCEINLINE FVector GetActorScale(T* RootComponent)
	{
		FVector Result(1,1,1);
		if( RootComponent != NULL )
		{
			Result = RootComponent->GetComponentScale();
		}
		return Result;
	}

	/**
	 * Returns quaternion of the RootComponent
	 * this is a template for no other reason than to delay compilation until USceneComponent is defined
	 */ 
	template<class T>
	static FORCEINLINE FQuat GetActorQuat(T* RootComponent)
	{
		FQuat Result(ForceInit);
		if( RootComponent != NULL )
		{
			Result = RootComponent->GetComponentQuat();
		}
		return Result;
	}

	/** Returns this actor's root component. */
	FORCEINLINE class USceneComponent* GetRootComponent() const { return RootComponent; }

	/** Returns this actor's root component cast to a primitive component */
	DEPRECATED(4.5, "Use GetRootComponent() and cast manually if needed")
	class UPrimitiveComponent* GetRootPrimitiveComponent() const;

	/**
	 * Sets root component to be the specified component.  NewRootComponent's owner should be this actor.
	 * @return true if successful
	 */
	bool SetRootComponent(class USceneComponent* NewRootComponent);

	/** Returns the location of the RootComponent of this Actor*/ 
	FORCEINLINE FVector GetActorLocation() const
	{
		return GetActorLocation(RootComponent);
	}

	/** Returns the rotation of the RootComponent of this Actor */
	FORCEINLINE FRotator GetActorRotation() const
	{
		return GetActorRotation(RootComponent);
	}

	/** Returns the scale of the RootComponent of this Actor */
	FORCEINLINE FVector GetActorScale() const
	{
		return GetActorScale(RootComponent);
	}

	/** Returns the quaternion of the RootComponent of this Actor */
	FORCEINLINE FQuat GetActorQuat() const
	{
		return GetActorQuat(RootComponent);
	}

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

	/** 
	 * Called by owning level to shift an actor location and all relevant data structures by specified delta
	 *  
	 * @param InWorldOffset	 Offset vector to shift actor location
	 * @param bWorldShift	 Whether this call is part of whole world shifting
	 */
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** 
	 * Indicates whether this actor should participate in level bounds calculations
	 */
	virtual bool IsLevelBoundsRelevant() const { return true; }

#if WITH_EDITOR
	// Editor specific

	/** @todo: Remove this flag once it is decided that additive interactive scaling is what we want */
	static bool bUsePercentageBasedScaling;

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to translate the actor's location.
	 */
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to modify the actor's rotation.
	 */
	virtual void EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to modify the actor's draw scale.
	 */
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/** Called by MirrorActors to perform a mirroring operation on the actor */
	virtual void EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation);

	/**
	 * Simple accessor to check if the actor is hidden upon editor startup
	 * @return	true if the actor is hidden upon editor startup; false if it is not
	 */
	bool IsHiddenEdAtStartup() const
	{
		return bHiddenEd;
	}

	// Returns true if this actor is hidden in the editor viewports.
	bool IsHiddenEd() const;

	/**
	 * Sets whether or not this actor is hidden in the editor for the duration of the current editor session
	 *
	 * @param bIsHidden	True if the actor is hidden
	 */
	virtual void SetIsTemporarilyHiddenInEditor( bool bIsHidden );

	/**
	 * @return Whether or not this actor is hidden in the editor for the duration of the current editor session
	 */
	bool IsTemporarilyHiddenInEditor() const { return bHiddenEdTemporary; }

	/** @return	Returns true if this actor is allowed to be displayed, selected and manipulated by the editor. */
	bool IsEditable() const;

	/** @return	Returns true if this actor should be shown in the scene outliner */
	bool IsListedInSceneOutliner() const;

	/** @return	Returns true if this actor is allowed to be attached to the given actor */
	virtual bool EditorCanAttachTo(const AActor* InParent, FText& OutReason) const;

	// Called before editor copy, true allow export
	virtual bool ShouldExport() { return true; }

	// Called before editor paste, true allow import
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) { return true; }

	/** Called by InputKey when an unhandled key is pressed with a selected actor */
	virtual void EditorKeyPressed(FKey Key, EInputEvent Event) {}

	/** Called by ReplaceSelectedActors to allow a new actor to copy properties from an old actor when it is replaced */
	virtual void EditorReplacedActor(AActor* OldActor) {}

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForErrors();

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForDeprecated();

	/**
	 * Returns this actor's current label.  Actor labels are only available in development builds.
	 * @return	The label text
	 */
	const FString& GetActorLabel() const;

	/**
	 * Assigns a new label to this actor.  Actor labels are only available in development builds.
	 * @param	NewActorLabel	The new label string to assign to the actor.  If empty, the actor will have a default label.
	 */
	void SetActorLabel( const FString& NewActorLabel );

	/** Advanced - clear the actor label. */
	void ClearActorLabel();

	/**
	 * Returns if this actor's current label is editable.  Actor labels are only available in development builds.
	 * @return	The editable status of the actor's label
	 */
	bool IsActorLabelEditable() const;

	/**
	 * Returns this actor's folder path. Actor folder paths are only available in development builds.
	 * @return	The folder path
	 */
	const FName& GetFolderPath() const;

	/**
	 * Assigns a new folder to this actor. Actor folder paths are only available in development builds.
	 * @param	NewFolderPath	The new folder to assign to the actor.
	 */
	void SetFolderPath(const FName& NewFolderPath);

	/**
	 * Used by the "Sync to Content Browser" right-click menu option in the editor.
	 * @param	Objects	Array to add content object references to.
	 * @return	Whether the object references content (all overrides of this function should return true)
	 */
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const;

	/** Returns NumUncachedStaticLightingInteractions for this actor */
	const int32 GetNumUncachedStaticLightingInteractions() const;

#endif		// WITH_EDITOR

	/**
	* @param ViewPos		Position of the viewer
	* @param ViewDir		Vector direction of viewer
	* @param Viewer			PlayerController owned by the client for whom net priority is being determined
	* @param InChannel		Channel on which this actor is being replicated.
	* @param Time			Time since actor was last replicated
	* @param bLowBandwidth True if low bandwith of viewer
	* @return				Priority of this actor for replication
	 */
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, class APlayerController* Viewer, UActorChannel* InChannel, float Time, bool bLowBandwidth);

	virtual bool GetNetDormancy(const FVector& ViewPos, const FVector& ViewDir, class APlayerController* Viewer, UActorChannel* InChannel, float Time, bool bLowBandwidth);

	/** 
	 * Allows for a specific response from the actor when the actor channel is opened (client side)
	 * @param InBunch Bunch received at time of open
	 * @param Connection the connection associated with this actor
	 */
	virtual void OnActorChannelOpen(class FInBunch& InBunch, class UNetConnection* Connection) {};

	/**
	 * SerializeNewActor has just been called on the actor before network replication (server side)
	 * @param OutBunch Bunch containing serialized contents of actor prior to replication
	 */
	virtual void OnSerializeNewActor(class FOutBunch& OutBunch) {};

	/** 
	 * Handles cleaning up the associated Actor when killing the connection 
	 * @param Connection the connection associated with this actor
	 */
	virtual void OnNetCleanup(class UNetConnection* Connection) {};

	/** Swaps Role and RemoteRole if client */
	void ExchangeNetRoles(bool bRemoteOwner);

	/**
	 * When called, will call the virtual call chain to register all of the tick functions for both the actor and optionally all components
	 * Do not override this function or make it virtual
	 * @param bRegister - true to register, false, to unregister
	 * @param bDoComponents - true to also apply the change to all components
	 */
	void RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents);

	/** 
	 * Set this actor's tick functions to be enabled or disabled. Only has an effect if the function is registered
	 * This only modifies the tick function on actor itself
	 * @param	bEnabled - Rather it should be enabled or not
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetActorTickEnabled(bool bEnabled);

	/**  Returns whether this actor has tick enabled or not	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool IsActorTickEnabled() const;

	/**
	 *	ticks the actor
	 *	@param	DeltaTime			The time slice of this tick
	 *	@param	TickType			The type of tick that is happening
	 *	@param	ThisTickFunction	The tick function that is firing, useful for getting the completion handle
	 */
	virtual void TickActor( float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction );

	/**
	 * Called when an actor is done spawning into the world (from UWorld::SpawnActor).
	 * For actors with a root component, the location and rotation will have already been set.
	 * Takes place after any construction scripts have been called
	 */
	virtual void PostActorCreated();

	/** Called when the lifespan of an actor expires (if he has one). */
	virtual void LifeSpanExpired();

	// Always called immediately before properties are received from the remote.
	virtual void PreNetReceive() override;
	
	// Always called immediately after properties are received from the remote.
	virtual void PostNetReceive() override;

	/** IsNameStableForNetworking means an object can be referred to its path name (relative to outer) over the network */
	virtual bool IsNameStableForNetworking() const override;

	/** IsSupportedForNetworking means an object can be referenced over the network */
	virtual bool IsSupportedForNetworking() const override;

	/** Returns a list of sub-objects that have stable names for networking */
	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*> &ObjList) override;

	// Always called immediately after spawning and reading in replicated properties
	virtual void PostNetInit();

	/** ReplicatedMovement struct replication event */
	UFUNCTION()
	virtual void OnRep_ReplicatedMovement();

	/** Update and smooth location, not called for simulated physics! */
	DEPRECATED(4.4, "PostNetReceiveLocation() has been replaced by PostNetReceiveLocationAndRotation().")
	virtual void PostNetReceiveLocation() {}

	/** Update location and rotation from ReplicatedMovement. Not called for simulated physics! */
	virtual void PostNetReceiveLocationAndRotation();

	/** Update velocity - typically from ReplicatedMovement, not called for simulated physics! */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity);

	/** Update and smooth simulated physic state, replaces PostNetReceiveLocation() and PostNetReceiveVelocity() */
	virtual void PostNetReceivePhysicState();

	/** Set the owner of this Actor, used primarily for network replication. */
	void SetOwner( AActor* NewOwner );

	/**
	 * Get the owner of this Actor, used primarily for network replication.
	 * @return Actor that owns this Actor
	 */
	UFUNCTION(BlueprintCallable, Category=Actor)
	AActor* GetOwner() const;

	/**
	 * This will check to see if the Actor is still in the world.  It will check things like
	 * the KillZ, outside world bounds, etc. and handle the situation.
	 */
	virtual bool CheckStillInWorld();

	//--------------------------------------------------------------------------------------
	// Actor overlap tracking
	
	/**
	 * Dispatch all EndOverlap for all of the Actor's PrimitiveComponents. 
	 * Generally used when removing the Actor from the world.
	 */
	void ClearComponentOverlaps();

	/** 
	 * Queries world and updates overlap detection state for this actor.
	 * @param bDoNotifies		True to dispatch being/end overlap notifications when these events occur.
	 */
	void UpdateOverlaps(bool bDoNotifies=true);
	
	/** 
	 * Check to see if current Actor is overlapping specified Actor
	 * @param Other the Actor to test for
	 * Returns true if any component of this actor is overlapping any component of Other. 
	 */
	bool IsOverlappingActor(const AActor* Other) const;

	/** Returns whether a MatineeActor is currently controlling this Actor */
	bool IsMatineeControlled() const;

	/** See if the root component has ModifyFrequency of MF_Static */
	bool IsRootComponentStatic() const;

	/** See if the root component has Mobility of EComponentMobility::Stationary */
	bool IsRootComponentStationary() const;

	/** See if the root component has Mobility of EComponentMobility::Movable */
	bool IsRootComponentMovable() const;

	//--------------------------------------------------------------------------------------
	// Actor ticking

	/** accessor for the value of bCanEverTick */
	FORCEINLINE bool CanEverTick() const { return PrimaryActorTick.bCanEverTick; }

	/** 
	 *	Function called every frame on this Actor. Override this function to implement custom logic to be executed every frame.
	 *	Note that Tick is disabled by default, and you will need to check PrimaryActorTick.bCanEverTick is set to true to enable it.
	 *
	 *	@param	DeltaSeconds	Game time elapsed since last call to Tick
	 */
	virtual void Tick( float DeltaSeconds );

	/** If true, actor is ticked even if TickType==LEVELTICK_ViewportsOnly	 */
	virtual bool ShouldTickIfViewportsOnly() const;

	//--------------------------------------------------------------------------------------
	// Actor relevancy determination

	/** 
	  * @param RealViewer - is the PlayerController associated with the client for which network relevancy is being checked
	  * @param Viewer - is the Actor being used as the point of view for the PlayerController
	  * @param SrcLocation - is the viewing location
	  *
	  * @return bool - true if this actor is network relevant to the client associated with RealViewer 
	  */
	virtual bool IsNetRelevantFor(const APlayerController* RealViewer, const AActor* Viewer, const FVector& SrcLocation) const;

	/**
	 * Check if this actor is the owner when doing relevancy checks for actors marked bOnlyRelevantToOwner
	 *
	 * @param ReplicatedActor - the actor we're doing a relevancy test on
	 * @param ActorOwner - the owner of ReplicatedActor
	 * @param ConnectionActor - the controller of the connection that we're doing relevancy checks for
	 *
	 * @return bool - true if this actor should be considered the owner
	 */
	virtual bool IsRelevancyOwnerFor(AActor* ReplicatedActor, AActor* ActorOwner, AActor* ConnectionActor);

	/** Called after the actor is spawned in the world.  Responsible for setting up actor for play. */
	void PostSpawnInitialize(FVector const& SpawnLocation, FRotator const& SpawnRotation, AActor* InOwner, APawn* InInstigator, bool bRemoteOwned, bool bNoFail, bool bDeferConstruction);

	/** Called to finish the spawning process, generally in the case of deferred spawning */
	void FinishSpawning(const FTransform& Transform, bool bIsDefaultTransform = false);

private:
	/** Called after the actor has run its construction. Responsible for finishing the actor spawn process. */
	void PostActorConstruction();

public:
	/** Called immediately before gameplay begins. */
	virtual void PreInitializeComponents();

	// Allow actors to initialize themselves on the C++ side
	virtual void PostInitializeComponents();

	/**
	 * Adds a controlling matinee actor for use during matinee playback
	 * @param InMatineeActor	The matinee actor which controls this actor
	 * @todo UE4 would be nice to move this out of Actor to MatineeInterface, but it needs variable, and I can't declare variable in interface
	 *	do we still need this?
	 */
	void AddControllingMatineeActor( AMatineeActor& InMatineeActor );

	/**
	 * Removes a controlling matinee actor
	 * @param InMatineeActor	The matinee actor which currently controls this actor
	 */
	void RemoveControllingMatineeActor( AMatineeActor& InMatineeActor );

	/** Dispatches ReceiveHit virtual and OnComponentHit delegate */
	void DispatchPhysicsCollisionHit(const struct FRigidBodyCollisionInfo& MyInfo, const struct FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData);
	
	/** @return the owning UPlayer (if any) of this actor. This will be a local player, a net connection, or NULL. */
	virtual class UPlayer* GetNetOwningPlayer();

	/**
	 * Get the owning connection used for communicating between client/server 
	 * @return NetConnection to the client or server for this actor
	 */
	virtual class UNetConnection* GetNetConnection();

	/**
	 * Gets the net mode for this actor, indicating whether it is a client or server (including standalone/not networked).
	 */
	ENetMode GetNetMode() const;

	class UNetDriver * GetNetDriver() const;

	/** Puts actor in dormant networking state */
	void SetNetDormancy(ENetDormancy NewDormancy);

	/** Forces dormant actor to replicate but doesn't change NetDormancy state (i.e., they will go dormant again if left dormant) */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category="Networking")
	void FlushNetDormancy();

	/** Ensure that all the components in the Components array are registered */
	virtual void RegisterAllComponents();

	/** Called after all the components in the Components array are registered */
	virtual void PostRegisterAllComponents();

	/** Returns true if Actor has a registered root component */
	bool HasValidRootComponent();

	/** Unregister all currently registered components */
	virtual void UnregisterAllComponents();

	/** Called after all currently registered components are cleared */
	virtual void PostUnregisterAllComponents() {}

	/** Will reregister all components on this actor. Does a lot of work - should only really be used in editor, generally use UpdateComponentTransforms or MarkComponentsRenderStateDirty. */
	virtual void ReregisterAllComponents();

	/**
	 * Incrementally registers components associated with this actor
	 *
	 * @param NumComponentsToRegister  Number of components to register in this run, 0 for all
	 * @return true when all components were registered for this actor
	 */
	bool IncrementalRegisterComponents(int32 NumComponentsToRegister);

	/** Flags all component's render state as dirty	 */
	void MarkComponentsRenderStateDirty();

	/** Update all components transforms */
	void UpdateComponentTransforms();

	/** Iterate over components array and call InitializeComponent */
	void InitializeComponents();

	/** Iterate over components array and call UninitializeComponent */
	void UninitializeComponents();

	/** Debug rendering to visualize the component tree for this actor. */
	void DrawDebugComponents(FColor const& BaseColor=FColor::White) const;

	virtual void MarkComponentsAsPendingKill();

	/** Returns true if this actor has begun the destruction process.
	 *  This is set to true in UWorld::DestroyActor, after the network connection has been closed but before any other shutdown has been performed.
	 *	@return true if this actor has begun destruction, or if this actor has been destroyed already.
	 **/
	inline bool IsPendingKillPending() const
	{
		return bPendingKillPending || IsPendingKill();
	}

	/** Invalidate lighting cache with default options. */
	void InvalidateLightingCache()
	{
		InvalidateLightingCacheDetailed(false);
	}

	/** Invalidates anything produced by the last lighting build. */
	virtual void InvalidateLightingCacheDetailed(bool bTranslationOnly);

	/**
	  * Used for adding actors to levels or teleporting them to a new location.
	  * The result of this function is independent of the actor's current location and rotation.
	  * If the actor doesn't fit exactly at the location specified, tries to slightly move it out of walls and such if bNoCheck is false.
	  *
	  * @param DestLocation The target destination point
	  * @param DestRotation The target rotation at the destination
	  * @param bIsATest is true if this is a test movement, which shouldn't cause any notifications (used by AI pathfinding, for example)
	  * @param bNoCheck is true if we should skip checking for encroachment in the world or other actors
	  * @return true if the actor has been successfully moved, or false if it couldn't fit.
	  */
	virtual bool TeleportTo( const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest=false, bool bNoCheck=false );

	/**
	 * Teleport this actor to a new location. If the actor doesn't fit exactly at the location specified, tries to slightly move it out of walls and such.
	 *
	 * @param DestLocation The target destination point
	 * @param DestRotation The target rotation at the destination
	 * @return true if the actor has been successfully moved, or false if it couldn't fit.
	 */
	UFUNCTION(BlueprintCallable, meta=( FriendlyName="Teleport", Keywords = "Move Position" ), Category="Utilities|Transformation")
	bool K2_TeleportTo( FVector DestLocation, FRotator DestRotation );

	/** Called from TeleportTo() when teleport succeeds */
	virtual void TeleportSucceeded(bool bIsATest) {}

	/**
	 *  Trace a ray against the Components of this Actor and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 *  @return TRUE if a blocking hit is found
	 */
	bool ActorLineTraceSingle(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params);

	/** 
	 * returns Distance to closest Body Instance surface. 
	 * Checks against all components of this Actor having valid collision and blocking TraceChannel.
	 *
	 * @param Point						World 3D vector
	 * @param TraceChannel				The 'channel' used to determine which components to consider.
	 * @param ClosestPointOnCollision	Point on the surface of collision closest to Point
	 * @param OutPrimitiveComponent		PrimitiveComponent ClosestPointOnCollision is on.
	 * 
	 * @return		Success if returns > 0.f, if returns 0.f, it is either not convex or inside of the point
	 *				If returns < 0.f, this Actor does not have any primitive with collision
	 */
	float ActorGetDistanceToCollision(const FVector& Point, ECollisionChannel TraceChannel, FVector& ClosestPointOnCollision, UPrimitiveComponent** OutPrimitiveComponent = NULL) const;

	/**
	 * Returns true if this actor is contained by TestLevel.
	 * @todo seamless: update once Actor->Outer != Level
	 */
	bool IsInLevel(const class ULevel *TestLevel) const;

	/** Return the ULevel that this Actor is part of. */
	ULevel* GetLevel() const;

	/**	Do anything needed to clear out cross level references; Called from ULevel::PreSave	 */
	virtual void ClearCrossLevelReferences();
	
	/** Non-virtual function to evaluate which portions of the EndPlay process should be dispatched for each actor */
	void RouteEndPlay(const EEndPlayReason::Type EndPlayReason);

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/** iterates up the Base chain to see whether or not this Actor is based on the given Actor
	 * @param Other the Actor to test for
	 * @return true if this Actor is based on Other Actor
	 */
	virtual bool IsBasedOnActor(const AActor* Other) const;
	

	/** iterates up the Base chain to see whether or not this Actor is attached to the given Actor
	* @param Other the Actor to test for
	* @return true if this Actor is attached on Other Actor
	*/
	virtual bool IsAttachedTo( const AActor* Other ) const;

	/** Get the extent used when placing this actor in the editor, used for 'pulling back' hit. */
	FVector GetPlacementExtent() const;

	// Blueprint 

#if WITH_EDITOR
	/** Find all FRandomStream structs in this ACtor and generate new random seeds for them. */
	void SeedAllRandomStreams();
#endif // WITH_EDITOR

	/** Reset private properties to defaults, and all FRandomStream structs in this Actor, so they will start their sequence of random numbers again. */
	void ResetPropertiesForConstruction();

	/** Rerun construction scripts, destroying all autogenerated components; will attempt to preserve the root component location. */
	virtual void RerunConstructionScripts();

	/** 
	 * Debug helper to show the component hierarchy of this actor.
	 * @param Info	Optional String to display at top of info
	 */
	void DebugShowComponentHierarchy( const TCHAR* Info, bool bShowPosition  = true);
	
	/** 
	 * Debug helper for showing the component hierarchy of one component
	 * @param Info	Optional String to display at top of info
	 */
	void DebugShowOneComponentHierarchy( USceneComponent* SceneComp, int32& NestLevel, bool bShowPosition );

	/**
	 * Run any construction script for this Actor. Will call OnConstruction.
	 * @param	Transform			The transform to construct the actor at.
	 * @param	InstanceDataCache	Optional cache of state to apply to newly created components (e.g. precomputed lighting)
	 * @param	bIsDefaultTransform	Whether or not the given transform is a "default" transform, in which case it can be overridden by template defaults
	 */
	void ExecuteConstruction(const FTransform& Transform, const class FComponentInstanceDataCache* InstanceDataCache, bool bIsDefaultTransform = false);

	/**
	 * Called when an instance of this class is placed (in editor) or spawned.
	 * @param	Transform			The transform the actor was constructed at.
	 */
	virtual void OnConstruction(const FTransform& Transform) {}

	/**
	 * Helper function to register the specified component, and add it to the serialized components array
	 * @param	Component	Component to be finalized
	 */
	void FinishAndRegisterComponent(UActorComponent* Component);

	/**  Util to create a component based on a template	 */
	UActorComponent* CreateComponentFromTemplate(UActorComponent* Template, const FString& InName = FString() );

	/** Destroys the constructed components. */
	void DestroyConstructedComponents();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	private:
	// this is the old name of the tick function. We just want to avoid mistakes with an attempt to override this
	virtual void Tick( float DeltaTime, enum ELevelTick TickType ) final
	{
		check(0);
	}
#endif

protected:
	/**
	 * Virtual call chain to register all tick functions for the actor class hierarchy
	 * @param bRegister - true to register, false, to unregister
	 */
	virtual void RegisterActorTickFunctions(bool bRegister);

	/** Runs UserConstructionScript, delays component registration until it's complete. */
	void ProcessUserConstructionScript();

	/**
	* Checks components for validity, implemented in AActor
	*/
	bool CheckActorComponents();

public:

	/** Walk up the attachment chain from RootComponent until we encounter a different actor, and return it. If we are not attached to a component in a different actor, returns NULL */
	virtual AActor* GetAttachParentActor() const;

	/** Walk up the attachment chain from RootComponent until we encounter a different actor, and return the socket name in the component. If we are not attached to a component in a different actor, returns NAME_None */
	virtual FName GetAttachParentSocketName() const;

	/** Find all Actors which are attached directly to a component in this actor */
	virtual void GetAttachedActors(TArray<AActor*>& OutActors) const;

	/**
	 * Sets the ticking group for this actor.
	 * @param NewTickGroup the new value to assign
	 */
	void SetTickGroup(ETickingGroup NewTickGroup);

	/** Called once this actor has been deleted */
	virtual void Destroyed();

	/** Call ReceiveHit, as well as delegates on Actor and Component */
	void DispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit);

	/** called when the actor falls out of the world 'safely' (below KillZ and such) */
	virtual void FellOutOfWorld(const class UDamageType& dmgType);

	/** called when the Actor is outside the hard limit on world bounds */
	virtual void OutsideWorldBounds();

	/** 
	 *	Returns the bounding box of all components in this Actor.
	 *	@param bNonColliding Indicates that you want to include non-colliding components in the bounding box
	 */
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false) const;

	/* Get half-height/radius of a big axis-aligned cylinder around this actors registered colliding components, or all registered components if bNonColliding is false. */
	virtual void GetComponentsBoundingCylinder(float& CollisionRadius, float& CollisionHalfHeight, bool bNonColliding = false) const;

	/**
	 * Get axis-aligned cylinder around this actor, used for simple collision checks (ie Pawns reaching a destination).
	 * If IsRootComponentCollisionRegistered() returns true, just returns its bounding cylinder, otherwise falls back to GetComponentsBoundingCylinder.
	 */
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const;

	/** @returns the radius of the collision cylinder from GetSimpleCollisionCylinder(). */
	float GetSimpleCollisionRadius() const;

	/** @returns the half height of the collision cylinder from GetSimpleCollisionCylinder(). */
	float GetSimpleCollisionHalfHeight() const;

	/** @returns collision extents vector for this Actor, based on GetSimpleCollisionCylinder(). */
	FVector GetSimpleCollisionCylinderExtent() const;

	/** @returns true if the root component is registered and has collision enabled.  */
	virtual bool IsRootComponentCollisionRegistered() const;

	/**
	 * Networking - called on client when actor is torn off (bTearOff==true), meaning it's no longer replicated to clients.
	 * @see bTearOff
	 */
	virtual void TornOff();

	//=============================================================================
	// Collision functions.
 
	/** 
	 * Get Collision Response to the Channel that entered for all components
	 * It returns Max of state - i.e. if Component A overlaps, but if Component B blocks, it will return block as response
	 * if Component A ignores, but if Component B overlaps, it will return overlap
	 *
	 * @param Channel - The channel to change the response of
	 */
	virtual ECollisionResponse GetComponentsCollisionResponseToChannel(ECollisionChannel Channel) const;

	//=============================================================================
	// Physics

	/** Stop all simulation from all components in this actor */
	void DisableComponentsSimulatePhysics();

public:
	/** @return WorldSettings for the World the actor is in
	 - if you'd like to know UWorld this placed actor (not dynamic spawned actor) belong to, use GetTypedOuter<UWorld>() **/
	class AWorldSettings* GetWorldSettings() const;

	/**
	 * Return true if the given Pawn can be "based" on this actor (ie walk on it).
	 * @param Pawn - The pawn that wants to be based on this actor
	 */
	virtual bool CanBeBaseForCharacter(class APawn* Pawn) const;

	/** Apply damage to this actor.
	 * @see https://www.unrealengine.com/blog/damage-in-ue4
	 * @param DamageAmount		How much damage to apply
	 * @param DamageEvent		Data package that fully describes the damage received.
	 * @param EventInstigator	The Controller responsible for the damage.
	 * @param DamageCauser		The Actor that directly caused the damage (e.g. the projectile that exploded, the rock that landed on you)
	 * @return					The amount of damage actually applied.
	 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser);

protected:
	virtual float InternalTakeRadialDamage(float Damage, struct FRadialDamageEvent const& RadialDamageEvent, class AController* EventInstigator, AActor* DamageCauser);
	virtual float InternalTakePointDamage(float Damage, struct FPointDamageEvent const& RadialDamageEvent, class AController* EventInstigator, AActor* DamageCauser);
public:

	/* Called when this actor becomes the given PlayerController's ViewTarget. Triggers the Blueprint event K2_OnBecomeViewTarget. */
	virtual void BecomeViewTarget( class APlayerController* PC );

	/* Called when this actor is no longer the given PlayerController's ViewTarget. Also triggers the Blueprint event K2_OnEndViewTarget. */
	virtual void EndViewTarget( class APlayerController* PC );

	/** Event called when this Actor becomes the view target for the given PlayerController. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName="OnBecomeViewTarget", Keywords="Activate Camera"), Category=Actor)
	virtual void K2_OnBecomeViewTarget( class APlayerController* PC );

	/** Event called when this Actor is no longer the view target for the given PlayerController. */
	UFUNCTION(BlueprintImplementableEvent, meta=(FriendlyName="OnEndViewTarget", Keywords="Deactivate Camera"), Category=Actor)
	virtual void K2_OnEndViewTarget( class APlayerController* PC );

	/**
	 *	Calculate camera view point, when viewing this actor.
	 *
	 * @param	DeltaTime	Delta time seconds since last update
	 * @param	OutResult	Camera configuration
	 */
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult);

	// Returns the human readable string representation of an object.
	virtual FString GetHumanReadableName() const;

	/** Reset actor to initial state - used when restarting level without reloading. */
	virtual void Reset();

	/** Returns the most recent time any of this actor's components were rendered */
	virtual float GetLastRenderTime() const;

	/** Forces this actor to be net relevant if it is not already by default	 */
	virtual void ForceNetRelevant();

	/** Updates NetUpdateTime to the new value for future net relevancy checks */
	void SetNetUpdateTime(float NewUpdateTime);

	/** Force actor to be updated to clients */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category="Networking")
	virtual void ForceNetUpdate();

	/**
	 *	Calls PrestreamTextures() for all the actor's meshcomponents.
	 *	@param Seconds - Number of seconds to force all mip-levels to be resident
	 *	@param bEnableStreaming	- Whether to start (true) or stop (false) streaming
	 *	@param CinematicTextureGroups - Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	virtual void PrestreamTextures( float Seconds, bool bEnableStreaming, int32 CinematicTextureGroups = 0 );

	/**
	 * returns the point of view of the actor.
	 * note that this doesn't mean the camera, but the 'eyes' of the actor.
	 * For example, for a Pawn, this would define the eye height location,
	 * and view rotation (which is different from the pawn rotation which has a zeroed pitch component).
	 * A camera first person view will typically use this view point. Most traces (weapon, AI) will be done from this view point.
	 *
	 * @param	OutLocation - location of view point
	 * @param	OutRotation - view rotation of actor.
	 */
	virtual void GetActorEyesViewPoint( FVector& OutLocation, FRotator& OutRotation ) const;

	/**
	 * @param RequestedBy - the Actor requesting the target location
	 * @return the optimal location to fire weapons at this actor
	 */
	virtual FVector GetTargetLocation(AActor* RequestedBy = NULL) const;

	/** 
	  * Hook to allow actors to render HUD overlays for themselves.  Called from AHUD::DrawActorOverlays(). 
	  * @param PC is the PlayerController on whose view this overlay is rendered
	  * @param Canvas is the Canvas on which to draw the overlay
	  * @param CameraPosition Position of Camera
	  * @param CameraDir direction camera is pointing in.
	  */
	virtual void PostRenderFor(class APlayerController* PC, class UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

	/** whether this Actor is in the persistent level, i.e. not a sublevel */
	bool IsInPersistentLevel(bool bIncludeLevelStreamingPersistent = false) const;

	/** Getter for the cached world pointer */
	virtual UWorld* GetWorld() const override;

	/** Get the timer instance from the actors world */
	class FTimerManager& GetWorldTimerManager() const;

	/** Gets the GameInstance that ultimately contains this actor. */
	class UGameInstance* GetGameInstance() const;

	/** Returns true if this is a replicated actor that was placed in the map */
	bool IsNetStartupActor() const;

	/** Searches components array and returns first encountered component of the specified class. */
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const;
	
	/** Script exposed version of FindComponentByClass */
	UFUNCTION()
	virtual UActorComponent* GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass);

	/* Gets all the components that inherit from the given class.
		Currently returns an array of UActorComponent which must be cast to the correct type. */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (ComponentClass = "ActorComponent"), meta=(DeterminesOutputType="ComponentClass"))
	TArray<UActorComponent*> GetComponentsByClass(TSubclassOf<UActorComponent> ComponentClass) const;

	/* Gets all the components that inherit from the given class with a given tag. */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (ComponentClass = "ActorComponent"), meta = (DeterminesOutputType = "ComponentClass"))
	TArray<UActorComponent*> GetComponentsByTag(TSubclassOf<UActorComponent> ComponentClass, FName Tag) const;

	/** Templatized version for syntactic nicety. */
	template<class T>
	T* FindComponentByClass() const
	{
		static_assert(CanConvertPointerFromTo<T, UActorComponent>::Result, "'T' template parameter to FindComponentByClass must be derived from ActorComponent");

		return (T*)FindComponentByClass(T::StaticClass());
	}

	/**
	 * Get all components derived from class 'T' and fill in the OutComponents array with the result.
	 * It's recommended to use TArrays with a TInlineAllocator to potentially avoid memory allocation costs.
	 * TInlineComponentArray is defined to make this easier, for example:
	 * {
	 * 	   TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
	 *     Actor->GetComponents(PrimComponents);
	 * }
	 */
	template<class T, class AllocatorType>
	void GetComponents(TArray<T*, AllocatorType>& OutComponents) const
	{
		static_assert(CanConvertPointerFromTo<T, UActorComponent>::Result, "'T' template parameter to GetComponents must be derived from ActorComponent");

		SCOPE_CYCLE_COUNTER(STAT_GetComponentsTime);

		OutComponents.Reset(OwnedComponents.Num());

		for (UActorComponent* OwnedComponent : OwnedComponents)
		{
			T* Component = Cast<T>(OwnedComponent);
			if (Component)
			{
				OutComponents.Add(Component);
			}
		}
	}

	/** UActorComponent specialization of GetComponents() to avoid unnecessary casts. */
	template<class AllocatorType>
	void GetComponents(TArray<UActorComponent*, AllocatorType>& OutComponents) const
	{
		SCOPE_CYCLE_COUNTER(STAT_GetComponentsTime);

		OutComponents.Reset(OwnedComponents.Num());

		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component)
			{
				OutComponents.Add(Component);
			}
		}
	}

	// Get a direct reference to the Components array rather than a copy
	// with the null pointers removed
	const TArray<UActorComponent*>& GetComponents() const
	{
		return OwnedComponents;
	}

	/** Puts a component in to the OwnedComponents array of the Actor.
	 *  The Component must be owned by the Actor or else it will assert
	 *  In general this should not need to be called directly by anything other than UActorComponent functions
	 */
	void AddOwnedComponent(UActorComponent* Component);

	/** Removes a component from the OwnedComponents array of the Actor.
	 *  In general this should not need to be called directly by anything other than UActorComponent functions
	 */
	void RemoveOwnedComponent(UActorComponent* Component);

#if DO_CHECK
	// Utility function for validating that a component is correctly in its Owner's OwnedComponents array
	bool OwnsComponent(UActorComponent* Component) const;
#endif

	/** Force the Actor to clear and rebuild its OwnedComponents array by evaluating all children (recursively) and locating components
	 *  In general this should not need to be called directly, but can sometimes be necessary as part of undo/redo code paths.
	 */
	void ResetOwnedComponents();

	/** Called when the replicated state of a component changes to update the Actor's cached ReplicatedComponents array
	 */
	void UpdateReplicatedComponent(UActorComponent* Component);

	/** Completely synchronizes the replicated components array so that it contains exactly the number of replicated components currently owned
	 */
	void UpdateAllReplicatedComponents();

	/** Returns a constant reference to the replicated components array
	 */
	const TArray<UActorComponent*>& GetReplicatedComponents() const;

private:
	/**
	 * All ActorComponents owned by this Actor.
	 * @see GetComponents()
	 */
	TArray<UActorComponent*> OwnedComponents;

	/** List of replicated components. */
	TArray<UActorComponent*> ReplicatedComponents;

public:

	/** Array of ActorComponents that are created by blueprints and serialized per-instance. */
	UPROPERTY(TextExportTransient, NonTransactional)
	TArray<UActorComponent*> BlueprintCreatedComponents;

private:
	/** Array of ActorComponents that have been added by the user on a per-instance basis. */
	UPROPERTY(Instanced)
	TArray<UActorComponent*> InstanceComponents;

public:

	/** Adds a component to the instance components array */
	void AddInstanceComponent(UActorComponent* Component);

	/** Removes a component from the instance components array */
	void RemoveInstanceComponent(UActorComponent* Component);

	/** Clears the instance components array */
	void ClearInstanceComponents(bool bDestroyComponents);

	/** Returns the instance components array */
	const TArray<UActorComponent*>& GetInstanceComponents() const;

public:
	//=============================================================================
	// Navigation related functions
	// 

	/** Check if owned component should be relevant for navigation
	 *  Allows implementing master switch to disable e.g. collision export in projectiles
	 */
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const { return true; }

	//=============================================================================
	// Debugging functions
public:
	/**
	 * Draw important Actor variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when the ShowDebug exec is used
	 *
	 * @param Canvas - Canvas to draw on
	 * @param DebugDisplay - Contains information about what debug data to display
	 * @param YL - Height of the current font
	 * @param YPos - Y position on Canvas. YPos += YL, gives position to draw text for next debug line.
	 */
	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	/** Retrieves actor's name used for logging, or string "NULL" if Actor == NULL */
	static FString GetDebugName(const AActor* Actor) { return Actor ? Actor->GetName() : TEXT("NULL"); }

#if ENABLE_VISUAL_LOG
	/** 
	 *	Hook for Actors to supply visual logger with additional data.
	 *	It's guaranteed that Snapshot != NULL
	 */
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const {}

private:
	friend class FVisualLog;
#endif // ENABLE_VISUAL_LOG

	//* Sets the friendly actor label and name */
	void SetActorLabelInternal( const FString& NewActorLabelDirty, bool bMakeGloballyUniqueFName );

	static FMakeNoiseDelegate MakeNoiseDelegate;

public:
#if !UE_BUILD_SHIPPING
	/** Delegate for globally hooking ProccessEvent calls - used by a non-public testing plugin */
	static FOnProcessEvent ProcessEventDelegate;
#endif

	static void MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation);
	static void SetMakeNoiseDelegate(const FMakeNoiseDelegate& NewDelegate);

	/** A fence to track when the primitive is detached from the scene in the rendering thread. */
	FRenderCommandFence DetachFence;

// DEPRECATED FUNCTIONS

	/** Get the class of this Actor. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, FriendlyName = "GetActorClass"), Category="Class")
	class UClass* GetActorClass() const;
};


//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE float AActor::GetSimpleCollisionRadius() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return Radius;
}

FORCEINLINE float AActor::GetSimpleCollisionHalfHeight() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return HalfHeight;
}

FORCEINLINE FVector AActor::GetSimpleCollisionCylinderExtent() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return FVector(Radius, Radius, HalfHeight);
}

//////////////////////////////////////////////////////////////////////////
// Macro to hide common Transform functions in native code for classes where they don't make sense.
// Note that this doesn't prevent access through function calls from parent classes (ie an AActor*), but
// does prevent use in the class that hides them and any derived child classes.

#define HIDE_ACTOR_TRANSFORM_FUNCTIONS() private: \
	FTransform GetTransform() const { return Super::GetTransform(); } \
	FVector GetActorLocation() const { return Super::GetActorLocation(); } \
	FRotator GetActorRotation() const { return Super::GetActorRotation(); } \
	FQuat GetActorQuat() const { return Super::GetActorQuat(); } \
	FVector GetActorScale() const { return Super::GetActorScale(); } \
	bool SetActorLocation(const FVector& NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr) { return Super::SetActorLocation(NewLocation, bSweep, OutSweepHitResult); } \
	bool SetActorRotation(FRotator NewRotation) { return Super::SetActorRotation(NewRotation); } \
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr) { return Super::SetActorLocationAndRotation(NewLocation, NewRotation, bSweep, OutSweepHitResult); } \
	virtual bool TeleportTo( const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck ) override { return Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck); } \
	virtual FVector GetVelocity() const override { return Super::GetVelocity(); } \
	float GetHorizontalDistanceTo(AActor* OtherActor)  { return Super::GetHorizontalDistanceTo(OtherActor); } \
	float GetVerticalDistanceTo(AActor* OtherActor)  { return Super::GetVerticalDistanceTo(OtherActor); } \
	float GetDotProductTo(AActor* OtherActor) { return Super::GetDotProductTo(OtherActor); } \
	float GetHorizontalDotProductTo(AActor* OtherActor) { return Super::GetHorizontalDotProductTo(OtherActor); } \
	float GetDistanceTo(AActor* OtherActor) { return Super::GetDistanceTo(OtherActor); }




