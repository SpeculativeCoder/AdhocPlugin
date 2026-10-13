// Copyright (c) 2022-2026 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#include "Pawn/AdhocPawnComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Game/AdhocGameModeComponent.h"
#include "Game/AdhocGameStateComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "Net/UnrealNetwork.h"

UAdhocPawnComponent::UAdhocPawnComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bWantsInitializeComponent = true;
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
    SetNetAddressable();
}

void UAdhocPawnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAdhocPawnComponent, FriendlyName);
    DOREPLIFETIME(UAdhocPawnComponent, Description);
    DOREPLIFETIME(UAdhocPawnComponent, FactionIndex);
}

void UAdhocPawnComponent::InitializeComponent()
{
    Super::InitializeComponent();

    const APawn* Pawn = GetPawn();
    check(Pawn);
    if (!Pawn) { return; }

    if (Pawn->HasAuthority())
    {
        UUID = FGuid::NewGuid();
    }
}

void UAdhocPawnComponent::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    const APawn* Pawn = GetPawn();
    check(World);
    check(Pawn);
    if (!World || !Pawn) { return; }

    if (Pawn->HasAuthority())
    {
        World->GetTimerManager().SetTimer(TimerHandle_CheckNavigate, this, &UAdhocPawnComponent::OnTimer_CheckNavigate, 2, true, 2);
    }
}

void UAdhocPawnComponent::SetFriendlyName(const FString& NewFriendlyName)
{
    const bool bChanged = FriendlyName != NewFriendlyName;

    FriendlyName = NewFriendlyName;

    if (bChanged)
    {
        OnFriendlyNameChangedDelegate.Broadcast();
    }
}

void UAdhocPawnComponent::SetDescription(const FString& NewDescription)
{
    const bool bChanged = Description != NewDescription;

    Description = NewDescription;

    if (bChanged)
    {
        OnDescriptionChangedDelegate.Broadcast();
    }
}

void UAdhocPawnComponent::SetFactionIndex(const int32 NewFactionIndex)
{
    const bool bChanged = FactionIndex != NewFactionIndex;

    FactionIndex = NewFactionIndex;

    if (bChanged)
    {
        OnFactionIndexChangedDelegate.Broadcast();
    }
}

void UAdhocPawnComponent::OnRep_FriendlyName(const FString& OldFriendlyName) const
{
    OnFriendlyNameChangedDelegate.Broadcast();
}

void UAdhocPawnComponent::OnRep_Description(const FString& OldDescription) const
{
    OnDescriptionChangedDelegate.Broadcast();
}

void UAdhocPawnComponent::OnRep_FactionIndex(int32 OldFactionIndex) const
{
    OnFactionIndexChangedDelegate.Broadcast();
}

void UAdhocPawnComponent::OnTimer_CheckNavigate()
{
    // TODO: backoff and also area priority (to allow stacked areas)

    const UWorld* World = GetWorld();
    check(World);
    if (!World) { return; }

    const AGameStateBase* GameState = World->GetGameState();
    const AGameModeBase* GameMode = World->GetAuthGameMode();
    const APawn* Pawn = GetPawn();
    check(GameState);
    check(GameMode);
    check(Pawn);

    if (!GameState || !GameMode || !Pawn) { return; }

    const UAdhocGameStateComponent* AdhocGameState = Cast<UAdhocGameStateComponent>(GameState->GetComponentByClass(UAdhocGameStateComponent::StaticClass()));
    const UAdhocGameModeComponent* AdhocGameMode = Cast<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
    check(AdhocGameState);
    check(AdhocGameMode);
    if (!AdhocGameState || !AdhocGameMode) { return; }

    APlayerController* PlayerController = Pawn->GetController<APlayerController>();

    if (PlayerController)
    {
        int32 AreaIndex = -1; // TODO: find best
        for (const FAdhocAreaState& AreaState : AdhocGameState->GetAreas())
        {
            FBox Box(AreaState.Location - AreaState.Size * 0.5f, AreaState.Location + AreaState.Size * 0.5f);

            if (Box.IsInside(Pawn->GetActorLocation()))
            {
                AdhocGameMode->PlayerEnterArea(PlayerController, AreaState.Index);
                break;
            }
        }
    }
}
