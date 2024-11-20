// Copyright Dennis Andersson. All Rights Reserved.

#include "ActorProjectile.h"
#include "ProjectileConfig.h"

// Engine
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"

AActorProjectile::AActorProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// projectile movement will handle primary ticking
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore); // don't overlap other projectiles
	CollisionComponent->InitSphereRadius(1.0f);
	CollisionComponent->SetCanEverAffectNavigation(false);
	SetRootComponent(CollisionComponent);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(GetRootComponent());
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->SetCanEverAffectNavigation(false);
	ProjectileMovement->bShouldBounce = false;
}

void AActorProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other,
		class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
		FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// destroy on impact
	Destroy();
}

void AActorProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if ENABLE_DRAW_DEBUG
	DrawDebugLine(GetWorld(), GetActorLocation(), PrevLocation, FColor::Red, false, 1.0f);
#endif // ENABLE_DRAW_DEBUG

	PrevLocation = GetActorLocation();
}

void AActorProjectile::InitFromConfig(UProjectileConfig* Config)
{
	SetActorTickEnabled(Config->bDebugDraw);
	PrevLocation = GetActorLocation();

	ProjectileMovement->InitialSpeed = Config->InitialSpeed;
	ProjectileMovement->MaxSpeed = Config->InitialSpeed;
	ProjectileMovement->bRotationFollowsVelocity = Config->bRotationFollowsVelocity;

	SetLifeSpan(Config->MaxLifetime);
}
