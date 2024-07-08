// Copyright (c) 2022-2024 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#include "AI/AdhocAIControllerComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "Game/AdhocGameModeComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocAIControllerComponent, Log, All)

UAdhocAIControllerComponent::UAdhocAIControllerComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    FriendlyName = TEXT("Bot");

    bWantsInitializeComponent = true;
}

void UAdhocAIControllerComponent::InitializeComponent()
{
    Super::InitializeComponent();

    const AAIController* AIController = GetAIController();
    check(AIController);

    UE_LOG(LogAdhocAIControllerComponent, Verbose, TEXT("InitializeComponent: AIController=%s"), *AIController->GetName());

    // TODO: if no faction set by game mode, pick a random faction?
}

void UAdhocAIControllerComponent::BeginPlay()
{
    Super::BeginPlay();

    const AAIController* AIController = GetAIController();
    check(AIController);

    UE_LOG(LogAdhocAIControllerComponent, Verbose, TEXT("BeginPlay: AIController=%s"), *AIController->GetName());

    const UWorld* World = GetWorld();
    check(World);
    UAdhocGameModeComponent* AdhocGameModeComponent = GetAdhocGameModeComponent();
    check(AdhocGameModeComponent);

    // execute join on next tick to give game logic time to do any controller setup it needs
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(AdhocGameModeComponent, &UAdhocGameModeComponent::BotJoin, AIController));
}

void UAdhocAIControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    const AAIController* AIController = GetAIController();
    check(AIController);

    UE_LOG(LogAdhocAIControllerComponent, Verbose, TEXT("EndPlay: AIController=%s"), *AIController->GetName());

    UAdhocGameModeComponent* AdhocGameModeComponent = GetAdhocGameModeComponent();
    check(AdhocGameModeComponent);

    AdhocGameModeComponent->BotLeave(AIController);
}
