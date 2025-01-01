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

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Controller.h"

#include "AdhocControllerComponent.generated.h"

UCLASS(Abstract, Transient)
class ADHOCPLUGIN_API UAdhocControllerComponent : public UActorComponent
{
    GENERATED_BODY()

    DECLARE_MULTICAST_DELEGATE(FOnFriendlyNameChangedDelegate);
    DECLARE_MULTICAST_DELEGATE(FOnFactionIndexChangedDelegate);

public:
    FOnFriendlyNameChangedDelegate OnFriendlyNameChangedDelegate;
    FOnFactionIndexChangedDelegate OnFactionIndexChangedDelegate;

protected:
    int64 UserID = -1;

    /** Human readable name of the controller. This is what will appear on screen for the controller. */
    UPROPERTY(Replicated, ReplicatedUsing = OnRep_FriendlyName)
    FString FriendlyName;

    /** Index of faction for the controller (first faction has index 0, next faction has index 1 and so on, -1 means no faction). */
    UPROPERTY(Replicated, ReplicatedUsing = OnRep_FactionIndex)
    int32 FactionIndex = -1;

    /** When set, will spawn at this location / rotation.
     * Typically this is set when the user has travelled from an area on another server to an area on this server
     * and we wish to immediately spawn them at the location they were previously at. */
    TOptional<FTransform> ImmediateSpawnTransform;

public:
    FORCEINLINE int64 GetUserID() const { return UserID; }
    FORCEINLINE const FString& GetFriendlyName() const { return FriendlyName; }
    FORCEINLINE int32 GetFactionIndex() const { return FactionIndex; }
    FORCEINLINE TOptional<FTransform> GetImmediateSpawnTransform() const { return ImmediateSpawnTransform; }

    FORCEINLINE void SetUserID(const int64 NewUserID) { UserID = NewUserID; }
    FORCEINLINE void SetImmediateSpawnTransform(const TOptional<FTransform>& NewImmediateSpawnTransform) { ImmediateSpawnTransform = NewImmediateSpawnTransform; }
    FORCEINLINE void ClearImmediateSpawnTransform() { ImmediateSpawnTransform = TOptional<FTransform>(); }

    virtual bool IsHuman() const PURE_VIRTUAL(UAdhocControllerComponent::IsHuman, return false;);

    FORCEINLINE AController* GetController() const { return Cast<AController>(GetOwner()); }

protected:
    explicit UAdhocControllerComponent(const FObjectInitializer& ObjectInitializer);

    virtual void InitializeComponent() override;

public:
    class UAdhocGameModeComponent* GetAdhocGameModeComponent() const;

    void SetFriendlyName(const FString& NewFriendlyName);
    void SetFactionIndex(const int64 NewFactionIndex);

protected:
    UFUNCTION()
    void OnRep_FriendlyName(const FString& OldFriendlyName) const;
    UFUNCTION()
    void OnRep_FactionIndex(int32 OldFactionIndex) const;

    /** When possessing a pawn, we will initialize the friendly name and faction index on the pawn
     * (this will allow the pawn to show its faction via color etc. and maybe put the name of the pawn in a nameplate etc.) */
    void OnNewPawn(APawn* Pawn) const;
};
