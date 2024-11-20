// Copyright Dennis Andersson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileConfig.generated.h"

class UNiagaraSystem;

/**
 * Configuration for a projectile type
 */
UCLASS()
class PROJECTILEPERF_API UProjectileConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, Meta=(UIMin="1.0", ClampMin="1.0", ForceUnits="cm/s"))
	float InitialSpeed = 2000.f;

	UPROPERTY(EditAnywhere, Meta=(UIMin="0.0", ClampMin="0.0", ForceUnits="s"))
	float MaxLifetime = 10.f;

	UPROPERTY(EditAnywhere)
	uint8 bRotationFollowsVelocity:1 = true;

	UPROPERTY(EditAnywhere)
	uint8 bDebugDraw:1 = false;
};
