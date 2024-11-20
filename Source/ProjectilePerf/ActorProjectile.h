// Copyright Dennis Andersson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActorProjectile.generated.h"

class UProjectileMovementComponent;
class UNiagaraComponent;
class USphereComponent;
class UProjectileConfig;

/** An "Actor" Based projectile in the typical Unreal way */
UCLASS(NotBlueprintable)
class PROJECTILEPERF_API AActorProjectile : public AActor
{
	GENERATED_BODY()
	
public:

	/** Used to detect projectile collision */
	UPROPERTY()
	USphereComponent* CollisionComponent;

	/** Updates the actor movement as a projectile */
	UPROPERTY()
	UProjectileMovementComponent* ProjectileMovement;

	/** Projectile rendering */
	//UPROPERTY()
	//UNiagaraComponent* NiagaraComponent;

	FVector PrevLocation = {};

	AActorProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ~ begin AActor interface
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other,
		class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
		FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	virtual void Tick(float DeltaTime) override;
	// ~ end AActor interface

	void InitFromConfig(UProjectileConfig* Config);
};
