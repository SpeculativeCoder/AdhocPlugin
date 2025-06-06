﻿// Copyright (c) 2022-2025 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#include "AdhocEngineSubsystem.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "AI/AdhocAIControllerComponent.h"
#include "Game/AdhocGameModeComponent.h"
#include "Game/AdhocGameStateComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "Objective/AdhocObjectiveComponent.h"
#include "Pawn/AdhocPawnComponent.h"
#include "Player/AdhocPlayerControllerComponent.h"
#include "Player/AdhocPlayerStateComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocGameEngineSubsystem, Log, All)

void UAdhocEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("Initialize"));

    FWorldDelegates::OnPostWorldCreation.AddUObject(this, &UAdhocEngineSubsystem::OnPostWorldCreation);
    FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UAdhocEngineSubsystem::OnPreWorldInitialization);
    FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UAdhocEngineSubsystem::OnPostWorldInitialization);
    FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UAdhocEngineSubsystem::OnWorldInitializedActors);

    FGameModeEvents::OnGameModeInitializedEvent().AddUObject(this, &UAdhocEngineSubsystem::OnGameModeInitialized);
    FGameModeEvents::OnGameModePreLoginEvent().AddUObject(this, &UAdhocEngineSubsystem::OnGameModePreLogin);
    FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UAdhocEngineSubsystem::OnGameModePostLogin);
    FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UAdhocEngineSubsystem::OnGameModeLogout);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnPostWorldCreation(UWorld* World) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPostWorldCreation: World=%s"), *World->GetName());

    World->OnWorldBeginPlay.AddUObject(this, &UAdhocEngineSubsystem::OnWorldBeginPlay, World);
    World->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateUObject(this, &UAdhocEngineSubsystem::OnActorPreSpawnInitialization));
    // World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UAdhocGameEngineSubsystem::OnActorSpawned));
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPreWorldInitialization: World=%s"), *World->GetName());
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnPostWorldInitialization(UWorld* World, UWorld::InitializationValues InitializationValues) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnPostWorldInitialization: World=%s"), *World->GetName());

    // for any existing actors, run the initialization we would have done
    for (TActorIterator<AActor> ActorIter(World); ActorIter; ++ActorIter)
    {
        OnActorPreSpawnInitialization(*ActorIter);
    }
}

void UAdhocEngineSubsystem::OnWorldInitializedActors(const UWorld::FActorsInitializedParams& ActorsInitializedParams) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnWorldInitializedActors: World=%s"), *ActorsInitializedParams.World->GetName());
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnWorldBeginPlay(UWorld* World) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnWorldBeginPlay: World=%s"), *World->GetName());
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeStatic
void UAdhocEngineSubsystem::OnActorPreSpawnInitialization(AActor* Actor) const
{
    // UE_LOG(LogAdhocGameEngineSubsystem, VeryVerbose, TEXT("OnActorPreSpawnInitialization: Actor=%s"), *Actor->GetName());

    if (Actor->IsA(APawn::StaticClass())
        && !Actor->IsA(ASpectatorPawn::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, VeryVerbose, TEXT("OnActorPreSpawnInitialization: Pawn=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocPawnComponent::StaticClass()))
        {
            UAdhocPawnComponent* AdhocPawnComponent = NewObject<UAdhocPawnComponent>(Actor, UAdhocPawnComponent::StaticClass(), UAdhocPawnComponent::StaticClass()->GetFName());

            // AdhocPawnComponent->RegisterComponent();
        }
    }
    else if (Actor->IsA(APlayerController::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: PlayerController=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocPlayerControllerComponent::StaticClass()))
        {
            UAdhocPlayerControllerComponent* AdhocPlayerControllerComponent = NewObject<UAdhocPlayerControllerComponent>(Actor, UAdhocPlayerControllerComponent::StaticClass(), UAdhocPlayerControllerComponent::StaticClass()->GetFName());

            // AdhocPlayerControllerComponent->RegisterComponent();
        }
    }
    else if (Actor->IsA(AAIController::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: BotController=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocAIControllerComponent::StaticClass()))
        {
            UAdhocAIControllerComponent* AdhocBotControllerComponent = NewObject<UAdhocAIControllerComponent>(Actor, UAdhocAIControllerComponent::StaticClass(), UAdhocAIControllerComponent::StaticClass()->GetFName());

            // AdhocBotControllerComponent->RegisterComponent();
        }
    }
    else if (Actor->IsA(APlayerState::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: PlayerState=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()))
        {
            UAdhocPlayerStateComponent* AdhocPlayerStateComponent = NewObject<UAdhocPlayerStateComponent>(Actor, UAdhocPlayerStateComponent::StaticClass(), UAdhocPlayerStateComponent::StaticClass()->GetFName());

            // AdhocPlayerStateComponent->RegisterComponent();
        }
    }
    else if (Actor->IsA(AGameStateBase::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: GameState=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocGameStateComponent::StaticClass()))
        {
            UAdhocGameStateComponent* AdhocGameStateComponent = NewObject<UAdhocGameStateComponent>(Actor, UAdhocGameStateComponent::StaticClass(), UAdhocGameStateComponent::StaticClass()->GetFName());

            // AdhocGameStateComponent->RegisterComponent();
        }
    }
    else if (Actor->ActorHasTag(TEXT("Adhoc_Area")))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: Area=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocAreaComponent::StaticClass()))
        {
            UAdhocAreaComponent* AdhocAreaComponent = NewObject<UAdhocAreaComponent>(Actor, UAdhocAreaComponent::StaticClass(), UAdhocAreaComponent::StaticClass()->GetFName());

            // AdhocAreaComponent->RegisterComponent();
        }
    }
    else if (Actor->ActorHasTag(TEXT("Adhoc_Objective")))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: Objective=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()))
        {
            UAdhocObjectiveComponent* AdhocObjectiveComponent = NewObject<UAdhocObjectiveComponent>(Actor, UAdhocObjectiveComponent::StaticClass(), UAdhocObjectiveComponent::StaticClass()->GetFName());

            // AdhocObjectiveComponent->RegisterComponent();
        }
    }
    else if (Actor->IsA(AGameModeBase::StaticClass()))
    {
        UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnActorPreSpawnInitialization: GameMode=%s"), *Actor->GetName());

        if (Actor->HasAuthority() && !Actor->GetComponentByClass(UAdhocGameModeComponent::StaticClass()))
        {
            UAdhocGameModeComponent* AdhocGameModeComponent = NewObject<UAdhocGameModeComponent>(Actor, UAdhocGameModeComponent::StaticClass(), UAdhocGameModeComponent::StaticClass()->GetFName());

            // AdhocGameModeComponent->RegisterComponent();
        }
    }
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnActorSpawned(AActor* Actor) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Warning, TEXT("OnActorSpawned: Actor=%s"), *Actor->GetName());

    check(0);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnGameModeInitialized(AGameModeBase* GameMode) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModeInitialized: GameMode=%s"), *GameMode->GetName());
}

// ReSharper disable twice CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnGameModePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueNetId, FString& ErrorMessage) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModePreLogin: GameMode=%s UniqueNetId=%s ErrorMessage=%s"), *GameMode->GetName(), *UniqueNetId.ToString(), *ErrorMessage);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* PlayerController) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModePostLogin: GameMode=%s PlayerController=%s"), *GameMode->GetName(), *PlayerController->GetName());

    UAdhocGameModeComponent* AdhocGameMode = Cast<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
    check(AdhocGameMode);

    AdhocGameMode->PostLogin(PlayerController);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocEngineSubsystem::OnGameModeLogout(AGameModeBase* GameMode, AController* Controller) const
{
    UE_LOG(LogAdhocGameEngineSubsystem, Verbose, TEXT("OnGameModeLogout: GameMode=%s Controller=%s"), *GameMode->GetName(), *Controller->GetName());

    UAdhocGameModeComponent* AdhocGameMode = Cast<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
    check(AdhocGameMode);

    AdhocGameMode->Logout(Controller);
}
