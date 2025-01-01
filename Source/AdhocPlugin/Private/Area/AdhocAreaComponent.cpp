// Copyright (c) 2022-2025 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#include "Area/AdhocAreaComponent.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "Game/AdhocGameModeComponent.h"
#include "Game/AdhocGameStateComponent.h"
#include "GameFramework/GameStateBase.h"

/** Tag to indicate to Adhoc that an actor (typically a volume) is an initial area. */
UE_DEFINE_GAMEPLAY_TAG(Adhoc_Area, "Adhoc_Area");

DEFINE_LOG_CATEGORY_STATIC(LogAdhocAreaComponent, Log, All)

UAdhocAreaComponent::UAdhocAreaComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bWantsInitializeComponent = true;

    PrimaryComponentTick.bCanEverTick = false;
}

void UAdhocAreaComponent::InitializeComponent()
{
    Super::InitializeComponent();

    AActor* Area = GetArea();
    check(Area);

    UE_LOG(LogAdhocAreaComponent, Verbose, TEXT("InitializeComponent: Area=%s"), *Area->GetName());

    if (!Area->ActorHasTag(TEXT("Adhoc_Area")))
    {
        UE_LOG(LogAdhocAreaComponent, Log, TEXT("Adding Adhoc_Area tag to area %s"), *GetFriendlyName());
        Area->Tags.Add("Adhoc_Area");
    }
}

void UAdhocAreaComponent::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    check(World);

    // const AGameStateBase* GameState = World->GetGameState();
    // check(GameState);
    //
    // AdhocGameState = Cast<UAdhocGameStateComponent>(GameState->GetComponentByClass(UAdhocGameStateComponent::StaticClass()));

    AActor* Area = GetArea();
    if (Area->HasAuthority())
    {
        Area->OnActorBeginOverlap.AddDynamic(this, &UAdhocAreaComponent::OnAreaActorBeginOverlap);

        // World->GetTimerManager().SetTimer(TimerHandle_CheckOverlappingPawns, this, &UAdhocAreaComponent::OnTimer_CheckOverlappingPawns, 5, true, 5);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void UAdhocAreaComponent::OnAreaActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    AActor* Area = GetArea();
    check(Area->HasAuthority());
    check(OverlappedActor == Area);

    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Pawn->Controller);
    if (!PlayerController)
    {
        return;
    }

    const UWorld* World = GetWorld();
    check(World);

    const AGameModeBase* GameMode = World->GetAuthGameMode();
    check(GameMode);

    const UAdhocGameModeComponent* AdhocGameMode = Cast<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
    check(AdhocGameMode);

    AdhocGameMode->PlayerEnterArea(PlayerController, AreaIndex);
}

// void UAdhocAreaComponent::OnTimer_CheckOverlappingPawns() const
// {
//     if (!AdhocGameState->GetActiveAreaIndexes().Contains(AreaIndex))
//     {
//         return;
//     }
//
//     UWorld* World = GetWorld();
//     check(World);
// }
