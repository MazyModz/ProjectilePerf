// Copyright Dennis Andersson. All Rights Reserved.

#include "ProjectileSpawner.h"
#include "ProjectileConfig.h"
#include "ActorProjectile.h"
#include "ProjectileSubsystem.h"

// Engine
#include "Components/BoxComponent.h"

AProjectileSpawner::AProjectileSpawner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(SceneComponent);

#if WITH_EDITORONLY_DATA
	BoundsVisualizerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsVisualizerComponent"));
	BoundsVisualizerComponent->SetBoxExtent(SpawnBounds, false);
	BoundsVisualizerComponent->SetLineThickness(10.0f);
	BoundsVisualizerComponent->SetupAttachment(GetRootComponent());
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void AProjectileSpawner::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (BoundsVisualizerComponent)
	{
		BoundsVisualizerComponent->SetBoxExtent(SpawnBounds, false);
	}
}
#endif // WITH_EDITOR

void AProjectileSpawner::BeginPlay()
{
	Super::BeginPlay();

	// preallocate the projectiles array that we maintain
	ActorProjectiles.Reserve(SpawnCount);
	ActorlessProjectiles.Reserve(SpawnCount);
}

static FTransform GetProjectileSpawnTM(const FVector& SpawnBounds)
{
	FVector Location = {};
	Location.X = FMath::FRandRange(-FMath::Abs(SpawnBounds.X), FMath::Abs(SpawnBounds.X));
	Location.Y = FMath::FRandRange(-FMath::Abs(SpawnBounds.Y), FMath::Abs(SpawnBounds.Y));
	Location.Z = FMath::FRandRange(-FMath::Abs(SpawnBounds.Z), FMath::Abs(SpawnBounds.Z));

	FRotator Rotation = {};
	Rotation.Pitch = FMath::FRandRange(-180.f, 180.f);
	Rotation.Yaw = FMath::FRandRange(-180.f, 180.f);
	Rotation.Roll = FMath::FRandRange(-180.f, 180.f);

	FTransform SpawnTM(Rotation.Quaternion(), Location);
	return SpawnTM;
}

void AProjectileSpawner::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AProjectileSpawner::Tick);
	
	Super::Tick(DeltaTime);

	{
		// count number of destroyed projectiles that we need to respawn
		uint32 ProjCount = (uint32)ActorProjectiles.Num();
		uint32 InvalidProjCount = (uint32)SpawnCount - ProjCount;
		for (uint32 I = ProjCount; I-- > 0;)
		{
			AActorProjectile* Proj = ActorProjectiles[I];
			if (!IsValid(Proj))
			{
				++InvalidProjCount;
				ActorProjectiles.RemoveAtSwap(I, 1, false);
			}
		}

		for (uint32 I = 0; I < InvalidProjCount; ++I)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SpawnActorProjectile);

			FTransform SpawnTM = GetProjectileSpawnTM(SpawnBounds);

			AActorProjectile* Actor = GetWorld()->SpawnActorDeferred<AActorProjectile>(AActorProjectile::StaticClass(),
				SpawnTM, this, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			ActorProjectiles.Add(Actor);
			Actor->InitFromConfig(Config);
			Actor->FinishSpawning(SpawnTM);
		}
	}

	{
		UProjectileSubsystem* Subsystem = UProjectileSubsystem::Get(GetWorld());
		check(Subsystem);

		// count number of destroyed projectiles that we need to respawn
		uint32 ProjCount = (uint32)ActorlessProjectiles.Num();
		uint32 InvalidProjCount = (uint32)SpawnCount - ProjCount;
		for (uint32 I = ProjCount; I-- > 0;)
		{
			FProjectileHandle Proj = ActorlessProjectiles[I];
			if (!Subsystem->IsProjectileValid(Proj))
			{
				++InvalidProjCount;
				ActorlessProjectiles.RemoveAtSwap(I, 1, false);
			}
		}

		for (uint32 I = 0; I < InvalidProjCount; ++I)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SpawnActorlessProjectile);

			FTransform SpawnTM = GetProjectileSpawnTM(SpawnBounds);
			FProjectileHandle Handle = Subsystem->CreateProjectile(Config, SpawnTM.GetLocation(), SpawnTM.GetRotation().Rotator());
			ActorlessProjectiles.Add(Handle);
		}
	}
}
