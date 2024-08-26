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

#pragma once

#include "NativeGameplayTags.h"
#include "Components/ActorComponent.h"

#include "AdhocObjectiveComponent.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Adhoc_Objective);

UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class ADHOCPLUGIN_API UAdhocObjectiveComponent : public UActorComponent
{
    GENERATED_BODY()

    DECLARE_MULTICAST_DELEGATE(FOnFactionIndexChangedDelegate);

public:
    FOnFactionIndexChangedDelegate OnFactionIndexChangedDelegate;

private:
    /** During game mode initialization, an index will be assigned to this objective. The index will be unique within the region (map/level).
     * @see UAdhocGameModeComponent::InitObjectiveStates */
    UPROPERTY(Replicated)
    int32 ObjectiveIndex = -1;

    /** Friendly (human-readable) name of the objective. */
    UPROPERTY(Category="Adhoc Objective", EditAnywhere)
    FString FriendlyName = TEXT("Objective");

    /** Faction which initially holds this objective. */
    UPROPERTY(Category="Adhoc Objective", EditInstanceOnly)
    int32 InitialFactionIndex = -1;

    /** Faction which currently holds this objective. */
    UPROPERTY(Replicated, ReplicatedUsing = OnRep_FactionIndex)
    int32 FactionIndex = -1;

    /** A faction must own one of the linked objectives before it can take this objective. */
    UPROPERTY(Category="Adhoc Objective", EditInstanceOnly)
    TSet<const AActor*> LinkedObjectives;

    /** During PostInitializeComponents, this will be set to the area volume in which the objective resides.
     * Should the area volume(s) ever be adjusted at runtime, this will be need to be set again if necessary. */
    UPROPERTY()
    class AAdhocAreaVolume* AreaVolume;

public:
    FORCEINLINE int32 GetObjectiveIndex() const { return ObjectiveIndex; }
    FORCEINLINE const FString& GetFriendlyName() const { return FriendlyName; }
    FORCEINLINE int32 GetInitialFactionIndex() const { return InitialFactionIndex; }
    FORCEINLINE int32 GetFactionIndex() const { return FactionIndex; }
    FORCEINLINE const TSet<const AActor*>& GetLinkedObjectives() const { return LinkedObjectives; }

    FORCEINLINE void SetObjectiveIndex(const int32 NewObjectiveIndex) { ObjectiveIndex = NewObjectiveIndex; }

    FORCEINLINE AActor* GetObjective() const { return GetOwner(); }

private:
    explicit UAdhocObjectiveComponent(const FObjectInitializer& ObjectInitializer);

    virtual void InitializeComponent() override;

public:
    void SetFactionIndex(const int32 NewFactionIndex);

private:
    UFUNCTION()
    void OnRep_FactionIndex(int32 OldFactionIndex) const;

#if WITH_EDITOR
    /** Overridden so we can auto add/remove back-links to any linked objectives we add/remove. */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
