// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectilePerfGameMode.h"
#include "ProjectilePerfCharacter.h"
#include "UObject/ConstructorHelpers.h"

AProjectilePerfGameMode::AProjectilePerfGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
