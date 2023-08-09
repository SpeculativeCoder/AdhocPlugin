// Copyright (c) 2022-2023 SpeculativeCoder (https://github.com/SpeculativeCoder)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "AdhocGameEngineSubsystem.h"

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "AdhocGameModeComponent.h"
#include "AdhocGameStateComponent.h"
#include "GameFramework/PlayerState.h"
#include "Player/AdhocPlayerStateComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocGameEngineSubsystem, Log, All)

void UAdhocGameEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("Initialize"));

	FWorldDelegates::OnPostWorldCreation.AddUObject(this, &UAdhocGameEngineSubsystem::OnPostWorldCreation);
	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UAdhocGameEngineSubsystem::OnPreWorldInitialization);
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UAdhocGameEngineSubsystem::OnPostWorldInitialization);
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UAdhocGameEngineSubsystem::OnWorldInitializedActors);

	FGameModeEvents::OnGameModeInitializedEvent().AddUObject(this, &UAdhocGameEngineSubsystem::OnGameModeInitialized);
	FGameModeEvents::OnGameModePreLoginEvent().AddUObject(this, &UAdhocGameEngineSubsystem::OnGameModePreLogin);
	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UAdhocGameEngineSubsystem::OnGameModePostLogin);
}

void UAdhocGameEngineSubsystem::OnPostWorldCreation(UWorld* World)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPostWorldCreation: World=%s"), *World->GetName());

	World->OnWorldBeginPlay.AddUObject(this, &UAdhocGameEngineSubsystem::OnWorldBeginPlay, World);

	//World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UAdhocGameEngineSubsystem::OnActorSpawned));
}

void UAdhocGameEngineSubsystem::OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPreWorldInitialization: World=%s"), *World->GetName());
}

void UAdhocGameEngineSubsystem::OnPostWorldInitialization(UWorld* World, UWorld::InitializationValues InitializationValues)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPostWorldInitialization: World=%s"), *World->GetName());
}

void UAdhocGameEngineSubsystem::OnWorldInitializedActors(const UWorld::FActorsInitializedParams& ActorsInitializedParams)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnWorldInitializedActors: World=%s"), *ActorsInitializedParams.World->GetName());
}

void UAdhocGameEngineSubsystem::OnWorldBeginPlay(UWorld* World)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnWorldBeginPlay: World=%s"), *World->GetName());
}

void UAdhocGameEngineSubsystem::OnActorSpawned(AActor *Actor)
{
	check(0);

	UE_LOG(LogAdhocGameEngineSubsystem, VeryVerbose, TEXT("OnActorSpawned: Actor=%s"), *Actor->GetName());
}

void UAdhocGameEngineSubsystem::OnGameModeInitialized(AGameModeBase* GameMode)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModeInitialized: GameMode=%s"), *GameMode->GetName());

	UAdhocGameModeComponent* AdhocGameMode = NewObject<UAdhocGameModeComponent>(GameMode, UAdhocGameModeComponent::StaticClass());
	AdhocGameMode->RegisterComponent();
}

void UAdhocGameEngineSubsystem::OnGameModePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueNetId, FString& ErrorMessage)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModePreLogin: GameMode=%s UniqueNetId=%s ErrorMessage=%s"), *GameMode->GetName(), *UniqueNetId.ToString(), *ErrorMessage);
}

void UAdhocGameEngineSubsystem::OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* PlayerController)
{
	UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModePostLogin: GameMode=%s PlayerController=%s"), *GameMode->GetName(), *PlayerController->GetName());

	UAdhocGameModeComponent* AdhocGameMode = CastChecked<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
	AdhocGameMode->PostLogin(PlayerController);
}
