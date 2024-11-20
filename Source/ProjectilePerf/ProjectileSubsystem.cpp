// Copyright Dennis Andersson. All Rights Reserved.

#include "ProjectileSubsystem.h"
#include "ProjectileConfig.h"

// max number of projectiles. hard limited by 16-bits
#define MAX_PROJECTILE_HANDLES (16384)
// max number of unique chunks. hard limited by 8-bits
#define MAX_CHUNK_COUNT (256)
// max number of projectiles that fit inside a single chunk. hard limited by 8-bits
#define MAX_CHUNK_PROJECTILE_COUNT (256)
// number of 32-bit values needed to create a bitmap with 1 bit for each MAX_CHUNK_PROJECTILE_COUNT
#define MAX_CHUNK_PROJECTILE_BITMAP32_COUNT ((MAX_CHUNK_PROJECTILE_COUNT + 31) / 32)

static_assert(MAX_PROJECTILE_HANDLES <= TNumericLimits<uint16>::Max() + 1);
static_assert(MAX_CHUNK_COUNT <= TNumericLimits<uint8>::Max() + 1);
static_assert(MAX_CHUNK_PROJECTILE_COUNT <= TNumericLimits<uint8>::Max() + 1);

// maximum frame delta allowed before we need to substep
#define MAX_PROJECTILE_TIMESTEP (1.0f / 20.0f)
// maximum number of substep iterations before continuing
#define MAX_PROJECTILE_SUBSTEP (4)

static_assert(MAX_PROJECTILE_SUBSTEP > 0);
static_assert(MAX_PROJECTILE_TIMESTEP > 0.0f);

void FProjectileTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Subsystem->Tick(DeltaTime);
}

struct FProjectileHandleLookup
{
	uint8 Chunk; // index of the chunk in the manager
	uint8 Index; // index inside the chunk
};

static_assert(sizeof(FProjectileHandleLookup) == sizeof(FHandleLookup));

FORCEINLINE FProjectileHandleLookup UnpackHandleLookup(FHandleLookup* Lookup)
{
	// FProjectileHandleLookup and FHandleLookup are bitwise the same in this implementation
	union 
	{
		FHandleLookup A;
		FProjectileHandleLookup B;
	} Cast = { .A = *Lookup };

	return Cast.B;
}

FORCEINLINE FHandleLookup PackHandleLookup(FProjectileHandleLookup* Lookup)
{
	// FProjectileHandleLookup and FHandleLookup are bitwise the same in this implementation
	union 
	{
		FHandleLookup A;
		FProjectileHandleLookup B;
	} Cast = { .B = *Lookup };

	return Cast.A;
}

struct BumpAllocator
{
	uint32 Pos;

	template<typename T>
	uint32 Bump(uint32 Count = 1)
	{
		uint32 AlignedPos = Align(Pos, alignof(T));
		uint32 Bytes = sizeof(T) * Count;
		Pos += Bytes;
		return AlignedPos;
	}
};

static FProjectileChunk CreateProjectileChunk(UProjectileConfig* Config)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateProjectileChunk);

	FProjectileChunk Chunk = {};
	Chunk.Config = Config;
	Chunk.Count = 0;

	// figure out memory size requirements and the offsets to each array inside of that block
	BumpAllocator Alloc = { 0 };
	uint32 StatesOffset = Alloc.Bump<FProjectileChunk>(MAX_CHUNK_PROJECTILE_COUNT);
	uint32 HandlesOffset = Alloc.Bump<FProjectileHandle>(MAX_CHUNK_PROJECTILE_COUNT);

	// allocate a single memory block to fit everything
	uint32 DataAlignment = (uint32)FPlatformMemory::GetConstants().PageSize;
	uint8* DataPtr = (uint8*)FMemory::MallocZeroed(Alloc.Pos, DataAlignment);
	Chunk.DataPtr = DataPtr;

	// assign the pointers
	Chunk.States = (FProjectileState*)(DataPtr + StatesOffset);
	Chunk.Handles = (FProjectileHandle*)(DataPtr + HandlesOffset);

	return Chunk;
}

static void DestroyProjectileChunk(FProjectileChunk* Chunk)
{
	FMemory::Free(Chunk->DataPtr);
	*Chunk = {};
}

UProjectileSubsystem::UProjectileSubsystem()
	: Super()
{
	PrimaryTickFunction.Subsystem = this;
	PrimaryTickFunction.TickGroup = TG_PrePhysics;
	PrimaryTickFunction.bCanEverTick = true;
	PrimaryTickFunction.bStartWithTickEnabled = true;
}

int32 UProjectileSubsystem::GetOrCreateChunk(UProjectileConfig* Config)
{
	// find an existing chunk with enough space
	for (int32 I = 0; I < Chunks.Num(); ++I)
	{
		FProjectileChunk& It = Chunks[I];
		if (It.Config == Config && It.Count < MAX_CHUNK_PROJECTILE_COUNT)
		{
			return I;
		}
	}

	// or create a new chunk if we couldn't find a suitable one
	FProjectileChunk Tmp = CreateProjectileChunk(Config);
	int32 Index = Chunks.Add(MoveTemp(Tmp));
	return Index;
}

FProjectileHandle UProjectileSubsystem::CreateProjectile(UProjectileConfig* Config,
	const FVector& Location, const FRotator& Rotation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UProjectileSubsystem::CreateProjectile);

	// creating a projectile is as simple as grabbing a new handle,
	// finding the chunk the projectile needs to go into,
	// incrementing the chunk counter, which also gives us the index in that chunk,
	// which finally gives us the lookup data.
	// then just get the state and initialize the projectile data.
	// done.
	
	FProjectileHandle Handle = HandleTable.Claim();
	FHandleLookup* LookupPtr = HandleTable.Get(Handle);
	FProjectileHandleLookup Lookup = UnpackHandleLookup(LookupPtr);

	// find an existing chunk with enough space
	// or create a new chunk if we couldn't find a suitable one
	int32 ChunkIndex = GetOrCreateChunk(Config);
	
	FProjectileChunk *Chunk = &Chunks[ChunkIndex];

	// increment the counter to get the index
	uint32 IndexInChunk = Chunk->Count++;

	// initialize the projectile
	FProjectileState* State = Chunk->States + IndexInChunk;
	*State = {};

	State->Position = Location;
	State->Rotation = FRotator3f(Rotation);
	State->Velocity = FRotator3f(Rotation).Vector() * Config->InitialSpeed;
	State->Lifetime = 0.0f;

	// update the handle lookup data
	Lookup.Chunk = (uint8)ChunkIndex;
	Lookup.Index = (uint8)IndexInChunk;
	*LookupPtr = PackHandleLookup(&Lookup);

	Chunk->Handles[IndexInChunk] = Handle;

	return Handle;
}

void UProjectileSubsystem::DestroyProjectile(FProjectileHandle Handle)
{
	if (HandleTable.IsValid(Handle))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UProjectileSubsystem::DestroyProjectile);

		// when destroying a projectile, we don't want "gaps" in the array.
		// destroy a projectile by just swapping the last projectile in the array
		// and decrement the counter
		//
		// |P1|P2|P3|P4|
		// ------------> count: 4
		//
		// destroying P2 creates a "gap" in the array
		// |P1|  |P3|P4|
		// ------------> count: 4
		// 
		// so we move P4 into that slot instead to fill it and decrement the counter
		// |P1|P4|P3|  |
		// --------> count: 3
		//
		// this requires us to update the handle lookup for P4 to point into index 2
		//
		// TODO(dennis): in theory we would also want a compaction step to keep each chunk compact
		// we might have two chunks with the exact same config with a low count in each
		// this should be packed to fill one of the chunks, basically move from one chunk to another

		FHandleLookup* ThisLookupPtr = HandleTable.Get(Handle);
		FProjectileHandleLookup ThisLookup = UnpackHandleLookup(ThisLookupPtr);
		FProjectileChunk *Chunk = &Chunks[ThisLookup.Chunk];

		check(!Chunk->bInsideTick);

		// decrement the projectile counter
		uint32 LastProjIndex = --Chunk->Count;
		// get the last handle + lookup in this chunk that we are going to swap with
		FProjectileHandle LastHandle = Chunk->Handles[LastProjIndex];
		FHandleLookup* LastLookupPtr = HandleTable.Get(LastHandle);
		// move the lookup so the last index handle now points to the destroyed projectile slot
		*LastLookupPtr = *ThisLookupPtr;

		FProjectileState* ThisState = Chunk->States + ThisLookup.Index;
		FProjectileState* LastState = Chunk->States + LastProjIndex;
		// move the projectile state from the last index to the destroyed index
		*ThisState = *LastState;

		FProjectileHandle* ThisHandlePtr = Chunk->Handles + ThisLookup.Index;
		FProjectileHandle* LastHandlePtr = Chunk->Handles + LastProjIndex;
		// finally move the handles array in the chunk
		*ThisHandlePtr = *LastHandlePtr;

		HandleTable.Release(Handle);
	}
}

FProjectileState* UProjectileSubsystem::GetProjectileState(FProjectileHandle Handle)
{
	if (HandleTable.IsValid(Handle))
	{
		FProjectileHandleLookup Lookup = UnpackHandleLookup(HandleTable.Get(Handle));
		FProjectileChunk* Chunk = &Chunks[Lookup.Chunk];
		FProjectileState* State = Chunk->States + Lookup.Index;
		return State;
	}
	else
	{
		// if the handle is invalid, we still return a valid pointer to something
		// so that the caller can continue as normal without doing any error checking
		// if the wish to actually make sure that the projectile is valid, then they should use
		// UProjectileSubsystem::IsProjectileValid
		static FProjectileState GStubState;
		return &GStubState;
	}
}

bool UProjectileSubsystem::IsProjectileValid(FProjectileHandle Handle) const
{
	return HandleTable.IsValid(Handle);
}

void UProjectileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UProjectileSubsystem::OnWorldCleanup);

	HandleTable.Init(MAX_PROJECTILE_HANDLES); 
}

void UProjectileSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
	OnWorldCleanupHandle = {};

	// cleanup all chunks
	for (FProjectileChunk& It : Chunks)
	{
		DestroyProjectileChunk(&It);
	}

	Super::Deinitialize();
}

void UProjectileSubsystem::OnWorldBeginPlay(UWorld& InWorld) 
{
	if (InWorld.IsGameWorld())
	{
		PrimaryTickFunction.RegisterTickFunction(InWorld.PersistentLevel);
	}
}

void UProjectileSubsystem::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	PrimaryTickFunction.UnRegisterTickFunction();
}

static FVector3f CalcProjectileVelocity(float DeltaTime, FVector3f V0, float MaxSpeed, float GravityZ)
{
	// v = v0 + a*t
	FVector3f Acc = FVector3f(0.0f, 0.0f, GravityZ);
	FVector3f V1 = V0 + (Acc * DeltaTime);
	if (MaxSpeed > 0.0f)
	{
		V1 = V1.GetClampedToMaxSize(MaxSpeed);
	}

	return V1;
}

void UProjectileSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UProjectileSubsystem::Tick);

	UWorld* World = GetWorld();
	check(World);

	float GravityZ = World->GetGravityZ();

	// number of update iterations we need to run
	uint32 SubstepCount = (uint32)FMath::CeilToInt(DeltaTime / MAX_PROJECTILE_TIMESTEP);
	SubstepCount = FMath::Min<uint32>(SubstepCount, MAX_PROJECTILE_SUBSTEP);

	uint32 ChunkCount = (uint32)Chunks.Num();
	for (uint32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TickProjectileChunk);

		FProjectileChunk* Chunk = &Chunks[ChunkIndex];
		UProjectileConfig* Config = Chunk->Config;
		
		// projectile that should be destroyed this frame
		// it's not safe to destroy projectiles while we are updating them so it needs to be deferred
		// we also don't want to trash the cpu cache with destruction
		static FProjectileHandle DestroyHandles[MAX_CHUNK_PROJECTILE_COUNT];
		uint32 DestroyHandlesCount = 0;

		// input & output to the second stage
		static FVector3f MoveDeltas[MAX_CHUNK_PROJECTILE_COUNT];
		static FHitResult Hits[MAX_CHUNK_PROJECTILE_COUNT];

		FCollisionQueryParams QueryParams(TEXT("Projectile"), false, NULL);
		QueryParams.bReturnFaceIndex = false;
		
		for (uint32 Step = 0; Step < SubstepCount; ++Step)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Substep);

			// this prevents dangerous functions like Destroy Projectile from being called while
			// we are updating the projectiles
			Chunk->bInsideTick = true;

			float StepDt = DeltaTime / (float)SubstepCount;
			uint32 ProjCount = Chunk->Count;

			// projectile are updated in 3 stages:
			// 1. compute the MoveDelta to use for the sweep stage
			// 2. perform hit sweeps
			// 3. handle hit result and compute final velocity

			// calculate MoveDelta
			for (uint32 I = 0; I < ProjCount; ++I)
			{
				FProjectileState* State = Chunk->States + I;

				// Velocity Verlet integration (http://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)
				// The addition of p0 is done outside this method, we are just computing the delta.
				// p = p0 + v0*t + 1/2*a*t^2
				
				// v = v0 + a*t
				FVector3f Vel = CalcProjectileVelocity(StepDt, State->Velocity, Config->InitialSpeed, GravityZ);
				// p = v0*t + 1/2*a*t^2
				FVector3f Delta = (State->Velocity * StepDt) + (Vel - State->Velocity) * (0.5f * StepDt);

				FRotator3f Rot = State->Rotation;
				// branches using 'Config' is going to be 100% predictable
				// since every single projectile in this Chunk use the exact same one
				if (Config->bRotationFollowsVelocity)
				{
					// NOTE(dennis): this is actually  expensive
					// 2x arctanf
					// 1x sqrtf
					Rot = Vel.Rotation();
				}
				
				State->Rotation = Rot;
				MoveDeltas[I] = Delta;
			}

			for (uint32 I = 0; I < ProjCount; ++I)
			{
				FProjectileState* State = Chunk->States + I;

				FVector Start = State->Position;
				FVector End = Start + FVector(MoveDeltas[I]);

				// NOTE(dennis): use a Shape cast if you need larger projectiles
				World->LineTraceSingleByChannel(Hits[I], Start, End, ECC_WorldDynamic, QueryParams);
			}

			// finalize velocity and handle hits
			for (uint32 I = 0; I < ProjCount; ++I)
			{
				FProjectileState* State = Chunk->States + I;
				FVector3f MoveDelta = MoveDeltas[I];
				FHitResult Hit = Hits[I];
				
				bool bMarkForKill = false;
				
				// add p0 to the verlet integration from the first update
				// p = p0 + v0*t + 1/2*a*t^2
				FVector P0 = State->Position;
				FVector P1 = P0 + FVector(MoveDelta * Hit.Time);

				// v = v0 + a*t
				FVector3f V0 = State->Velocity;
				// take the hit time into account when calculating velocity
				float VelTime = StepDt * Hit.Time;
				FVector3f V1 = CalcProjectileVelocity(VelTime, V0, Config->InitialSpeed, GravityZ);

				bool bHitSomething = Hit.bBlockingHit || Hit.bStartPenetrating;
				if (bHitSomething)
				{
					bMarkForKill = true;

					// TODO(dennis): here you handle projectile impact events
				}

#if ENABLE_DRAW_DEBUG
				if (Config->bDebugDraw)
				{
					DrawDebugLine(World, P0, P1, FColor::Green, false, 1.0f);
				}
#endif // ENABLE_DRAW_DEBUG

				State->Position = P1;
				State->Velocity = V1;

				State->Lifetime += StepDt;
				// destroy projectile when lifetime exceeded
				if (Config->MaxLifetime != 0.0f && State->Lifetime >= Config->MaxLifetime)
				{
					bMarkForKill = true;
				}

				if (bMarkForKill)
				{
					FProjectileHandle Handle = Chunk->Handles[I];
					DestroyHandles[DestroyHandlesCount++] = Handle;
				}
			}

			// after we are done with the updating we can destroy the projectiles
			Chunk->bInsideTick = false;

			for (uint32 I = 0; I < DestroyHandlesCount; ++I)
			{
				FProjectileHandle Handle = DestroyHandles[I];
				DestroyProjectile(Handle);
			}

			DestroyHandlesCount = 0;
		}
	}
}
