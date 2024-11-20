// Copyright Dennis Andersson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectileSubsystem.generated.h"

class UProjectileConfig;

USTRUCT()
struct FProjectileTickFunction : public FTickFunction
{
	GENERATED_BODY()

	UProjectileSubsystem* Subsystem;

	// ~ begin FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	// ~ end FTickFunction interface
};

template<>
struct TStructOpsTypeTraits<FProjectileTickFunction>
	: public TStructOpsTypeTraitsBase2<FProjectileTickFunction>
{
	enum { WithCopy = false };
};

/** Per-projectile data which is updated for each simulation step */
struct PROJECTILEPERF_API alignas(PLATFORM_CACHE_LINE_SIZE) FProjectileState
{
	FVector				Position;		// world position
	FRotator3f			Rotation;		// world rotation, float32 is enough
	FVector3f			Velocity;		// world velocity, float32 is enough
	float				Lifetime;		// time this projectile has been alive
	// 12 bytes of padding available here
};

/** Main data container for projectiles */
struct FProjectileChunk
{
	UProjectileConfig*	Config;			// shared config all these projectiles use
	uint8*				DataPtr;		// pointer to the allocated memory block
	FProjectileState*	States;			// per-projectile state
	FProjectileHandle*	Handles;		// index to the handle for each projectile in the chunk
	uint32				Count;			// number of projectiles in this chunk
	bool				bInsideTick;	// this chunk is being updated (not safe to add/remove projectiles)
};

/**
 * Creates/Destroys/Updates Highly efficient "Actorless" Projectiles
 */
UCLASS()
class PROJECTILEPERF_API UProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:

	TArray<FProjectileChunk>	Chunks;			// the main data storage for projectiles
	FHandleTable				HandleTable;	// projectile handle data for external access

	FProjectileTickFunction		PrimaryTickFunction;
	FDelegateHandle				OnWorldCleanupHandle;

	UProjectileSubsystem();

	static UProjectileSubsystem* Get(const UWorld* World)
	{
		return UWorld::GetSubsystem<UProjectileSubsystem>(World);
	}

	/** spawns a new projectile to be simulated */
	FProjectileHandle CreateProjectile(UProjectileConfig* Config, const FVector& Location, const FRotator& Rotation);
	/** destroys the projectile. cannot be called inside this subsystem Tick */
	void DestroyProjectile(FProjectileHandle Handle);
	/** gets the internal simulation state for a projectile. will return a stub if the
		handle is valid so the code works. use IsProjectileValid for actual error handling */
	FProjectileState* GetProjectileState(FProjectileHandle Handle);
	/** test if the handle refers to a valid projectile */
	bool IsProjectileValid(FProjectileHandle Handle) const;

	// ~ begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~ end USubsystem interface

	// ~ begin UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	// ~ end UWorldSubsystem interface

	int32 GetOrCreateChunk(UProjectileConfig* Config);
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void Tick(float DeltaTime);
};
