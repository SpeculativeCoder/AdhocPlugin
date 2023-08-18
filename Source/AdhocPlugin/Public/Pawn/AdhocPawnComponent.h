﻿// Copyright (c) 2022-2023 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AdhocPawnComponent.generated.h"

UCLASS()
class ADHOCPLUGIN_API UAdhocPawnComponent : public UActorComponent
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE(FOnFriendlyNameChangedDelegate);
	DECLARE_MULTICAST_DELEGATE(FOnPawnFactionIndexChangedDelegate);

public:
	FOnFriendlyNameChangedDelegate OnFriendlyNameChangedDelegate;
	FOnPawnFactionIndexChangedDelegate OnFactionIndexChangedDelegate;

private:
	// TODO: is it worth replicating this to the client?
	/** UUID for the pawn. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	FGuid UUID;

	/** Human readable name of the pawn. This is what will appear on screen for the pawn. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated, ReplicatedUsing = OnRep_FriendlyName)
	FString FriendlyName;

	/** Index of faction for the pawn (first faction has index 0, next faction has index 1 and so on, -1 means no faction). */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated, ReplicatedUsing = OnRep_FactionIndex)
	int32 FactionIndex = -1;

public:
	FORCEINLINE class APawn* GetPawn() const { return GetOwner<APawn>(); }

	FORCEINLINE const FGuid& GetUUID() const { return UUID; }
	FORCEINLINE const FString& GetFriendlyName() const { return FriendlyName; }
	FORCEINLINE int32 GetFactionIndex() const { return FactionIndex; }

private:
	explicit UAdhocPawnComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeComponent() override;

public:
	void SetFriendlyName(const FString& NewFriendlyName);
	void SetFactionIndex(const int32 NewFactionIndex);

private:
	UFUNCTION()
	void OnRep_FriendlyName(const FString& OldFriendlyName) const;
	UFUNCTION()
	void OnRep_FactionIndex(int32 OldFactionIndex) const;
};
