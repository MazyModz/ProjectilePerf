// Copyright Dennis Andersson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Handle to a specific projectile */
struct PROJECTILEPERF_API FProjectileHandle
{
	uint16 Index;		// handle lookup index
	uint16 Version;		// version number for the handle
};

/** Where to find the projectile internal data inside the manager */
struct PROJECTILEPERF_API FHandleLookup
{
	uint16 Opaque;	// implementation specific bytes
};

/** Table of handles that can be used in a "weak pointer" way to access a piece of data */
struct PROJECTILEPERF_API FHandleTable
{
	TArray<FHandleLookup>	Lookup;		// opaque lookup data associated with a handle
	TArray<uint16>			Version;	// internal version number to compare against
	TArray<uint16>			FreeIndex;	// unused handle indices that we pop
	uint32					MaxCount;

	/** create the table */
	void Init(uint32 InMaxCount);

	/** get internal lookup data for the handle. doesn't test for valid */
	FHandleLookup* Get(FProjectileHandle Handle);

	/** allocates a new handle from the available pool*/
	FProjectileHandle Claim();
	/** frees an existing handle and returns it to the pool. doesn't test for valid */
	void Release(FProjectileHandle Handle);
	/** test if a handle is pointing to anything valid by comparing the version numbers */
	bool IsValid(FProjectileHandle Handle) const;
};

// ------------------------------------------------------

FORCEINLINE void FHandleTable::Init(uint32 InMaxCount) 
{
	MaxCount = InMaxCount;

	Lookup.Init({}, MaxCount);
	Version.Init(0, MaxCount);
	
	FreeIndex.Init(0, MaxCount);
	// we are going to pe popping off the free index array,
	// add in reverse order so that we pop in ascending order
	for (uint32 I = 0; I < MaxCount; ++I)
	{
		FreeIndex[I] = MaxCount - I - 1;
	}
}

FORCEINLINE FHandleLookup* FHandleTable::Get(FProjectileHandle Handle)
{
	return &Lookup[Handle.Index];
}

FORCEINLINE FProjectileHandle FHandleTable::Claim()
{
	FProjectileHandle Handle;
	Handle.Index = FreeIndex.Pop(false);
	Handle.Version = Version[Handle.Index];
	return Handle;
}

FORCEINLINE void FHandleTable::Release(FProjectileHandle Handle)
{
	// invalidates this handle by inc the version counter
	++Version[Handle.Index];
	// this index can be claimed again
	FreeIndex.Push(Handle.Index);
}

FORCEINLINE bool FHandleTable::IsValid(FProjectileHandle Handle) const
{
	uint16 TrueVersion = Version[Handle.Index];
	return (TrueVersion == Handle.Version);
}
