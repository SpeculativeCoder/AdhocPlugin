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

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Objective/AdhocObjectiveState.h"
#include "Faction/AdhocFactionState.h"
#include "Area/AdhocAreaState.h"
#include "Server/AdhocServerState.h"
#include "Structure/AdhocStructureState.h"
#include "AdhocGameStateComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocGameStateComponent : public UActorComponent
{
	GENERATED_BODY()

	/** The unique ID of this server. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	int64 ServerID = 1;

	/** The region this server has been assigned. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	int64 RegionID = 1;

	/** The areas (as indexes) this server has been assigned to manage.
	 * The server will only allow objectives to be taken if they are within an active area. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	TArray<int32> ActiveAreaIndexes;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	TArray<FAdhocFactionState> Factions;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<FAdhocAreaState> Areas;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	TArray<FAdhocObjectiveState> Objectives;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<FAdhocServerState> Servers;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TMap<FGuid, struct FAdhocStructureState> Structures;

public:
	FORCEINLINE int32 GetServerID() const { return ServerID; }
	FORCEINLINE int32 GetRegionID() const { return RegionID; }
	FORCEINLINE const TArray<int32>& GetActiveAreaIndexes() const { return ActiveAreaIndexes; }

	FORCEINLINE void SetServerID(const int64 NewServerID) { ServerID = NewServerID; }
	FORCEINLINE void SetRegionID(const int64 NewRegionID) { RegionID = NewRegionID; }
	FORCEINLINE void SetActiveAreaIndexes(const TArray<int32>& NewActiveAreaIndexes) { ActiveAreaIndexes = NewActiveAreaIndexes; }

	FORCEINLINE int32 GetNumFactions() const { return Factions.Num(); }
	FORCEINLINE FAdhocFactionState& GetFaction(const int32 FactionIndex) { return Factions[FactionIndex]; }

#if WITH_ADHOC_PLUGIN_EXTRA
	FORCEINLINE TMap<FGuid, FAdhocStructureState>& GetStructures() { return Structures; }
#endif

	void SetFactions(const TArray<FAdhocFactionState>& NewFactions);
	void SetAreas(const TArray<FAdhocAreaState>& NewAreas);
	void SetObjectives(const TArray<FAdhocObjectiveState>& NewObjectives);
	void SetServers(const TArray<FAdhocServerState>& NewServers);

	FAdhocFactionState* FindFactionByID(int64 FactionID);
	FAdhocAreaState* FindAreaByID(int64 AreaID);
	FAdhocAreaState* FindAreaByIndex(int32 AreaIndex);
	FAdhocAreaState* FindAreaByRegionIDAndIndex(int64 InRegionID, int32 Index);

	FAdhocObjectiveState* FindObjectiveByID(int64 ObjectiveID);
	FAdhocObjectiveState* FindObjectiveByIndex(int32 ObjectiveIndex);
	FAdhocObjectiveState* FindObjectiveByRegionIDAndIndex(int64 InRegionID, int32 Index);

	FAdhocServerState* FindServerByID(int64 InServerID);
	FAdhocServerState* FindServerByAreaID(const int64 AreaID);
	FAdhocServerState* FindOrInsertServerByID(int64 InServerID);

	/** Get a color which represents the given faction, or gray if not a valid faction. */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FColor GetFactionColorSafe(int32 FactionIndex) const;

	/** An objective can be taken if it is active and linked to an objective already taken by the faction OR if the faction has taken no objectives yet. */
	bool IsObjectiveActiveAndTakeableByFaction(int32 ObjectiveIndex, int32 FactionIndex);

	// bool IsFriendlyActiveObjectiveIndexTakeableByFactionIndex(int32 ObjectiveIndex, int32 FactionIndex);
	int32 GetNumActiveObjectivesByFactionIndex(int32 FactionIndex) const;

	bool IsObjectiveLinkedToFriendlyObjective(int32 ObjectiveIndex, int32 FactionIndex);
	bool IsObjectiveLinkedToFriendlyObjective(const FAdhocObjectiveState* ObjectiveState, int32 FactionIndex);

	bool IsObjectiveLinkedToEnemyObjective(int32 ObjectiveIndex, int32 FactionIndex);
	bool IsObjectiveLinkedToEnemyObjective(const FAdhocObjectiveState* ObjectiveState, int32 FactionIndex);
};
