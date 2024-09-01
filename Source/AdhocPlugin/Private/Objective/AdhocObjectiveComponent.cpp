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

#include "Objective/AdhocObjectiveComponent.h"

#include "EngineUtils.h"
#include "Area/AdhocAreaComponent.h"
#include "Area/AdhocAreaVolume.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

/** Tag to indicate to Adhoc that an actor is an objective. */
UE_DEFINE_GAMEPLAY_TAG(Adhoc_Objective, "Adhoc_Objective");

DEFINE_LOG_CATEGORY_STATIC(LogAdhocObjectiveComponent, Log, All)

UAdhocObjectiveComponent::UAdhocObjectiveComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bWantsInitializeComponent = true;

    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
    SetNetAddressable();
}

void UAdhocObjectiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAdhocObjectiveComponent, ObjectiveIndex);
    DOREPLIFETIME(UAdhocObjectiveComponent, FactionIndex);
}

void UAdhocObjectiveComponent::InitializeComponent()
{
    Super::InitializeComponent();

    AActor* Objective = GetObjective();
    check(Objective);

    UE_LOG(LogAdhocObjectiveComponent, Verbose, TEXT("InitializeComponent: Objective=%s"), *Objective->GetName());

    if (!Objective->ActorHasTag(TEXT("Adhoc_Objective")))
    {
        UE_LOG(LogAdhocObjectiveComponent, Log, TEXT("Adding Adhoc_Objective tag to objective %s"), *GetFriendlyName());
        Objective->Tags.Add("Adhoc_Objective");
    }

    for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
    {
        AActor* Actor = *ActorIter;

        UAdhocAreaComponent* PossibleAdhocArea = Cast<UAdhocAreaComponent>(Actor->GetComponentByClass(UAdhocAreaComponent::StaticClass()));
        if (!PossibleAdhocArea)
        {
            continue;
        }

        if (Actor->GetComponentsBoundingBox().Intersect(GetObjective()->GetComponentsBoundingBox()))
        {
            UE_LOG(LogAdhocObjectiveComponent, Verbose, TEXT("Objective %s is within area %s"), *FriendlyName, *PossibleAdhocArea->GetFriendlyName());
            this->AdhocArea = PossibleAdhocArea;
            break;
        }
    }

    if (!AdhocArea)
    {
        UE_LOG(LogAdhocObjectiveComponent, Warning, TEXT("Objective %s is not within an area!"), *FriendlyName);
    }
}

void UAdhocObjectiveComponent::SetFactionIndex(const int32 NewFactionIndex)
{
    const bool bChanged = FactionIndex != NewFactionIndex;

    FactionIndex = NewFactionIndex;

    if (bChanged)
    {
        OnFactionIndexChangedDelegate.Broadcast();
    }
}

void UAdhocObjectiveComponent::OnRep_FactionIndex(int32 OldFactionIndex) const
{
    OnFactionIndexChangedDelegate.Broadcast();
}

#if WITH_EDITOR
void UAdhocObjectiveComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // UE_LOG(LogTemp, Log, TEXT("Objective %s property %s change type %d"),
    //	*GetFriendlyName(), *PropertyChangedEvent.GetPropertyName().ToString(), PropertyChangedEvent.ChangeType);

    if (PropertyChangedEvent.GetPropertyName() == TEXT("LinkedObjectives"))
    {
        // ensure any added outgoing link also has a back-link
        if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
        {
            for (const AActor* LinkedObjective : LinkedObjectives)
            {
                UAdhocObjectiveComponent* LinkedAdhocObjective = Cast<UAdhocObjectiveComponent>(LinkedObjective->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
                check(LinkedAdhocObjective);
                UE_LOG(LogAdhocObjectiveComponent, VeryVerbose, TEXT("Objective %s is linked to %s"), *FriendlyName, *LinkedAdhocObjective->GetFriendlyName());

                if (!LinkedAdhocObjective->LinkedObjectives.Contains(GetObjective()))
                {
                    UE_LOG(LogAdhocObjectiveComponent, Log, TEXT("Also adding back-link from objective %s to %s"), *LinkedAdhocObjective->GetFriendlyName(), *FriendlyName);
                    LinkedAdhocObjective->LinkedObjectives.Add(GetObjective());
                }
            }
        }
        // ensure any removed outgoing link no longer has a back-link
        else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
        {
            for (TActorIterator<AActor> OtherActorIter(GetWorld()); OtherActorIter; ++OtherActorIter)
            {
                const AActor* OtherActor = *OtherActorIter;

                UAdhocObjectiveComponent* OtherAdhocObjective = Cast<UAdhocObjectiveComponent>(OtherActor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
                if (!OtherAdhocObjective)
                {
                    continue;
                }

                if (OtherAdhocObjective->LinkedObjectives.Contains(GetObjective()) && !LinkedObjectives.Contains(OtherActor))
                {
                    UE_LOG(LogAdhocObjectiveComponent, Log, TEXT("Also removing back-link from objective %s to %s"), *OtherAdhocObjective->GetFriendlyName(), *FriendlyName);
                    OtherAdhocObjective->LinkedObjectives.Remove(GetObjective());
                }
            }
        }
    }
}
#endif
