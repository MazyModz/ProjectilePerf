// Copyright Dennis Andersson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectileHandle.h"
#include "ProjectileSpawner.generated.h"

class UProjectileConfig;
class AActorProjectile;
class UBoxComponent;

/**
 * Actor that spawns projectiles for performance testing
 * It will keep spawning projectiles when they are destroyed to maintain a consistent count
 */
UCLASS()
class PROJECTILEPERF_API AProjectileSpawner : public AActor
{
	GENERATED_BODY()
	
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UBoxComponent* BoundsVisualizerComponent;
#endif // WITH_EDITORONLY_DATA

	/** Defines where projectiles are spawned*/
	UPROPERTY(EditInstanceOnly)
	FVector SpawnBounds;

	/** What kind of projectiles to spawn  */
	UPROPERTY(EditInstanceOnly)
	UProjectileConfig* Config;

	/** Number of projectiles to spawn */
	UPROPERTY(EditInstanceOnly, Meta=(UIMin="0", ClampMin="0"))
	int32 SpawnCount;

	/** Spawn the actor based projectiles (slow) */
	UPROPERTY(EditInstanceOnly)
	uint32 bSpawnActorProjectiles:1;

	/** Spawn the non actor projectiles (fast) */
	UPROPERTY(EditInstanceOnly)
	uint32 bSpawnActorlessProjectiles:1;

	UPROPERTY(Transient)
	TArray<AActorProjectile*> ActorProjectiles;

	TArray<FProjectileHandle> ActorlessProjectiles;

	AProjectileSpawner(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ~ begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// ~ end UObject interface

	// ~ begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// ~ end AActor interface
};
