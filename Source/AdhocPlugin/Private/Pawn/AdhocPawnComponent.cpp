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

#include "Pawn/AdhocPawnComponent.h"
#include "Player/AdhocPlayerControllerComponent.h"
#include "Net/UnrealNetwork.h"

UAdhocPawnComponent::UAdhocPawnComponent(const FObjectInitializer& ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
	SetNetAddressable();
}

void UAdhocPawnComponent::InitializeComponent()
{
	Super::InitializeComponent();

	const APawn* Pawn = GetPawn();
	check(Pawn);

	if (Pawn->HasAuthority())
	{
		UUID = FGuid::NewGuid();
	}
}

void UAdhocPawnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAdhocPawnComponent, FriendlyName);
	DOREPLIFETIME(UAdhocPawnComponent, FactionIndex);
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

void UAdhocPawnComponent::OnRep_FactionIndex(int32 OldFactionIndex) const
{
	OnFactionIndexChangedDelegate.Broadcast();
}
