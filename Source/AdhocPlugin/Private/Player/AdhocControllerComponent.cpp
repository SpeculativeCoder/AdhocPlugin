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

#include "Player/AdhocControllerComponent.h"

#include "Net/UnrealNetwork.h"
#include "Pawn/AdhocPawnComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocControllerComponent, Log, All)

UAdhocControllerComponent::UAdhocControllerComponent(const FObjectInitializer& ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
	SetNetAddressable();
}

void UAdhocControllerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAdhocControllerComponent, FriendlyName);
	DOREPLIFETIME(UAdhocControllerComponent, FactionIndex);
}

void UAdhocControllerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AController* Controller = GetController();
	check(Controller);

	UE_LOG(LogAdhocControllerComponent, Verbose, TEXT("InitializeComponent: Controller=%s"), *Controller->GetName());

	Controller->GetOnNewPawnNotifier().AddUObject(this, &UAdhocControllerComponent::OnNewPawn);
}

void UAdhocControllerComponent::SetFriendlyName(const FString& NewFriendlyName)
{
	const bool bChanged = FriendlyName != NewFriendlyName;

	FriendlyName = NewFriendlyName;

	if (bChanged)
	{
		OnFriendlyNameChangedDelegate.Broadcast();
	}
}

void UAdhocControllerComponent::SetFactionIndex(const int64 NewFactionIndex)
{
	const bool bChanged = FactionIndex != NewFactionIndex;

	FactionIndex = NewFactionIndex;

	if (bChanged)
	{
		OnFactionIndexChangedDelegate.Broadcast();
	}
}

void UAdhocControllerComponent::OnNewPawn(APawn* Pawn) const
{
	const AController* Controller = GetController();
	check(Controller);

	UE_LOG(LogAdhocControllerComponent, VeryVerbose, TEXT("OnNewPawn: Controller=%s Pawn=%s NetNode=%d"), *Controller->GetName(), Pawn ? *Pawn->GetName() : TEXT("nullptr"), Controller->GetLocalRole());

	if (Pawn && Controller->HasAuthority())
	{
		UAdhocPawnComponent* AdhocPawn = Cast<UAdhocPawnComponent>(Pawn->GetComponentByClass(UAdhocPawnComponent::StaticClass()));
		check(AdhocPawn);

		if (AdhocPawn->GetFriendlyName().IsEmpty())
		{
			if (!FriendlyName.IsEmpty())
			{
				UE_LOG(LogTemp, VeryVerbose, TEXT("OnNewPawn: Setting FriendlyName=%s"), *FriendlyName);
				AdhocPawn->SetFriendlyName(FriendlyName);
			}
			// else
			// {
			// 	const APlayerState* PlayerState = Pawn->GetPlayerState();
			// 	if (PlayerState)
			// 	{
			// 		UE_LOG(LogTemp, VeryVerbose, TEXT("OnNewPawn: Setting (from PlayerState) FriendlyName=%s"), *PlayerState->GetPlayerName());
			// 		AdhocPawn->SetFriendlyName(PlayerState->GetPlayerName());
			// 	}
			// }
		}

		if (AdhocPawn->GetFactionIndex() == -1)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("OnNewPawn: Setting FactionIndex=%d"), FactionIndex);
			AdhocPawn->SetFactionIndex(FactionIndex);
		}
	}

	// TODO: clear out anything when unpossess a pawn?
}

void UAdhocControllerComponent::OnRep_FriendlyName(const FString& OldFriendlyName) const
{
	OnFriendlyNameChangedDelegate.Broadcast();
}

void UAdhocControllerComponent::OnRep_FactionIndex(int32 OldFactionIndex) const
{
	OnFactionIndexChangedDelegate.Broadcast();
}
