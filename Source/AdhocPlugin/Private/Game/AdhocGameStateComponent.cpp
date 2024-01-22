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

#include "Game/AdhocGameStateComponent.h"

#include "Net/UnrealNetwork.h"

UAdhocGameStateComponent::UAdhocGameStateComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
    SetNetAddressable();
}

void UAdhocGameStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAdhocGameStateComponent, ServerID);
    DOREPLIFETIME(UAdhocGameStateComponent, RegionID);
    DOREPLIFETIME(UAdhocGameStateComponent, ActiveAreaIndexes);
    DOREPLIFETIME(UAdhocGameStateComponent, Factions);
    DOREPLIFETIME(UAdhocGameStateComponent, Objectives);
}

void UAdhocGameStateComponent::SetFactions(const TArray<FAdhocFactionState>& NewFactions)
{
    Factions.Empty();
    Factions += NewFactions;
}

void UAdhocGameStateComponent::SetObjectives(const TArray<FAdhocObjectiveState>& NewObjectives)
{
    Objectives.Empty();
    Objectives += NewObjectives;
}

void UAdhocGameStateComponent::SetAreas(const TArray<FAdhocAreaState>& NewAreas)
{
    Areas.Empty();
    Areas += NewAreas;
}

void UAdhocGameStateComponent::SetServers(const TArray<FAdhocServerState>& NewServers)
{
    Servers.Empty();
    Servers += NewServers;
}

FAdhocFactionState* UAdhocGameStateComponent::FindFactionByID(const int64 FactionID)
{
    for (int i = 0; i < Factions.Num(); i++)
    {
        if (Factions[i].ID == FactionID)
        {
            return &Factions[i];
        }
    }
    return nullptr;
}

FAdhocAreaState* UAdhocGameStateComponent::FindAreaByID(const int64 AreaID)
{
    for (int i = 0; i < Areas.Num(); i++)
    {
        if (Areas[i].ID == AreaID)
        {
            return &Areas[i];
        }
    }
    return nullptr;
}

FAdhocAreaState* UAdhocGameStateComponent::FindAreaByIndex(const int32 AreaIndex)
{
    for (int i = 0; i < Areas.Num(); i++)
    {
        if (Areas[i].Index == AreaIndex)
        {
            return &Areas[i];
        }
    }
    return nullptr;
}

FAdhocAreaState* UAdhocGameStateComponent::FindAreaByRegionIDAndIndex(const int64 InRegionID, const int32 Index)
{
    for (int i = 0; i < Areas.Num(); i++)
    {
        if (Areas[i].RegionID == InRegionID && Areas[i].Index == Index)
        {
            return &Areas[i];
        }
    }
    return nullptr;
}

FAdhocObjectiveState* UAdhocGameStateComponent::FindObjectiveByID(const int64 ObjectiveID)
{
    for (int i = 0; i < Objectives.Num(); i++)
    {
        if (Objectives[i].ID == ObjectiveID)
        {
            return &Objectives[i];
        }
    }
    return nullptr;
}

FAdhocObjectiveState* UAdhocGameStateComponent::FindObjectiveByIndex(const int32 ObjectiveIndex)
{
    for (int i = 0; i < Objectives.Num(); i++)
    {
        if (Objectives[i].Index == ObjectiveIndex)
        {
            return &Objectives[i];
        }
    }
    return nullptr;
}

FAdhocObjectiveState* UAdhocGameStateComponent::FindObjectiveByRegionIDAndIndex(const int64 InRegionID, const int32 Index)
{
    for (int i = 0; i < Objectives.Num(); i++)
    {
        if (Objectives[i].RegionID == InRegionID && Objectives[i].Index == Index)
        {
            return &Objectives[i];
        }
    }
    return nullptr;
}

FAdhocServerState* UAdhocGameStateComponent::FindServerByID(const int64 InServerID)
{
    for (int i = 0; i < Servers.Num(); i++)
    {
        if (Servers[i].ID == ServerID)
        {
            return &Servers[i];
        }
    }
    return nullptr;
}

FAdhocServerState* UAdhocGameStateComponent::FindServerByAreaID(const int64 AreaID)
{
    for (int i = 0; i < Servers.Num(); i++)
    {
        if (Servers[i].AreaIDs.Contains(AreaID))
        {
            return &Servers[i];
        }
    }
    return nullptr;
}

FAdhocServerState* UAdhocGameStateComponent::FindOrInsertServerByID(const int64 InServerID)
{
    FAdhocServerState* Server = FindServerByID(InServerID);
    if (Server)
    {
        return Server;
    }

    FAdhocServerState NewServer;
    NewServer.ID = InServerID;
    return &Servers.Add_GetRef(NewServer);
}

FColor UAdhocGameStateComponent::GetFactionColorSafe(int32 FactionIndex) const
{
    if (FactionIndex >= 0 && FactionIndex < Factions.Num())
    {
        return Factions[FactionIndex].Color;

        // // TODO: proper color handling
        // if (Faction.Color.Equals(TEXT("blue")))
        // {
        // 	return FLinearColor(0.1, 0.2, 1);
        // }
        // else if (Faction.Color.Equals(TEXT("red")))
        // {
        // 	return FLinearColor(1, 0.1, 0.1);
        // }
        // else if (Faction.Color.Equals(TEXT("green")))
        // {
        // 	return FLinearColor(0.1, 1, 0.1);
        // }
        // else if (Faction.Color.Equals(TEXT("yellow")))
        // {
        // 	return FLinearColor(1, 1, 0.1);
        // }
        // else if (Faction.Color.Equals(TEXT("black")))
        // {
        // 	return FLinearColor(0, 0, 0);
        // }
        // else if (Faction.Color.Equals(TEXT("white")))
        // {
        // 	return FLinearColor(1, 1, 1);
        // }
        // else if (Faction.Color.Equals(TEXT("gray")))
        // {
        // 	return FLinearColor(0.6, 0.6, 0.6);
        // }
    }

    return FColor(127, 127, 127);
}

bool UAdhocGameStateComponent::IsObjectiveActiveAndTakeableByFaction(const int32 ObjectiveIndex, const int32 FactionIndex)
{
    const FAdhocObjectiveState* Objective = FindObjectiveByIndex(ObjectiveIndex);

    return Objective
        // must be active for this server
        && (Objective->AreaIndex == -1 || ActiveAreaIndexes.Contains(Objective->AreaIndex))
        // must not already be taken by this faction
        && Objective->FactionIndex != FactionIndex
        // and either not owned by any faction yet (so totally free to take)
        && (Objective->FactionIndex == -1
            // OR linked to an objective this faction already owns
            || IsObjectiveLinkedToFriendlyObjective(Objective, FactionIndex)
            // OR this faction has no active objectives at all so can take anything
            || GetNumActiveObjectivesByFactionIndex(FactionIndex) <= 0
            // OR objective has no links at all so can always be taken
            || Objective->LinkedObjectiveIndexes.Num() <= 0);
}

// bool UAdhocGameStateComponent::IsFriendlyActiveObjectiveIndexTakeableByFactionIndex(const int32 ObjectiveIndex, const int32 FactionIndex)
// {
// 	const FObjective* Objective = FindObjectiveByIndex(ObjectiveIndex);
//
// 	return Objective
// 		&& ActiveAreaIndexes.Contains(Objective->AreaIndex)
// 		&& Objective->FactionIndex == FactionIndex;
// }

int32 UAdhocGameStateComponent::GetNumActiveObjectivesByFactionIndex(const int32 FactionIndex) const
{
    int32 Count = 0;

    for (auto& Objective : Objectives)
    {
        if ((Objective.AreaIndex == -1 || ActiveAreaIndexes.Contains(Objective.AreaIndex)) && Objective.FactionIndex == FactionIndex)
        {
            Count++;
        }
    }

    return Count;
}

bool UAdhocGameStateComponent::IsObjectiveLinkedToFriendlyObjective(int32 ObjectiveIndex, int32 FactionIndex)
{
    const FAdhocObjectiveState* Objective = FindObjectiveByIndex(ObjectiveIndex);

    return Objective && IsObjectiveLinkedToFriendlyObjective(Objective, FactionIndex);
}

bool UAdhocGameStateComponent::IsObjectiveLinkedToFriendlyObjective(const FAdhocObjectiveState* ObjectiveState, int32 FactionIndex)
{
    for (auto& LinkedObjectiveIndex : ObjectiveState->LinkedObjectiveIndexes)
    {
        const FAdhocObjectiveState* LinkedObjective = FindObjectiveByIndex(LinkedObjectiveIndex);

        if (LinkedObjective && LinkedObjective->FactionIndex == FactionIndex)
        {
            return true;
        }
    }

    return false;
}

bool UAdhocGameStateComponent::IsObjectiveLinkedToEnemyObjective(int32 ObjectiveIndex, int32 FactionIndex)
{
    const FAdhocObjectiveState* Objective = FindObjectiveByIndex(ObjectiveIndex);

    return Objective && IsObjectiveLinkedToEnemyObjective(Objective, FactionIndex);
}

bool UAdhocGameStateComponent::IsObjectiveLinkedToEnemyObjective(const FAdhocObjectiveState* ObjectiveState, int32 FactionIndex)
{
    for (auto& LinkedObjectiveIndex : ObjectiveState->LinkedObjectiveIndexes)
    {
        const FAdhocObjectiveState* LinkedObjective = FindObjectiveByIndex(LinkedObjectiveIndex);
        if (LinkedObjective && LinkedObjective->FactionIndex != FactionIndex)
        {
            return true;
        }
    }

    return false;
}
