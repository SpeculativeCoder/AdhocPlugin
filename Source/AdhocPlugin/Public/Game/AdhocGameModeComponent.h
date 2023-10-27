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

#include "Faction/AdhocFactionState.h"
#include "Objective/AdhocObjectiveState.h"
#include "CoreMinimal.h"
#include "AdhocEmission.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Controller.h"
#include "Interfaces/IHttpRequest.h"

#include "AdhocGameModeComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocGameModeComponent : public UActorComponent
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnObjectiveTakenEventDelegate, FAdhocObjectiveState& OutObjective, FAdhocFactionState& Faction);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUserDefeatedUserEventDelegate, APlayerController* PlayerController, APlayerController* DefeatedPlayerController);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUserDefeatedBotEventDelegate, APlayerController* PlayerController, AController* DefeatedBotController);

public:
	FOnObjectiveTakenEventDelegate OnObjectiveTakenEventDelegate;
	FOnUserDefeatedUserEventDelegate OnUserDefeatedUserEventDelegate;
	FOnUserDefeatedBotEventDelegate OnUserDefeatedBotEventDelegate;

private:
	UPROPERTY()
	class AGameModeBase* GameMode;

	UPROPERTY()
	class AGameStateBase* GameState;

	UPROPERTY()
	class UAdhocGameStateComponent* AdhocGameState;

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	FString PrivateIP = TEXT("127.0.0.1");	 // non-public IP of the server within its hosting service / cluster etc.
	FString ManagerHost = TEXT("127.0.0.1"); // host which is managing this Unreal server (we will talk to and maintain web socket connection to this)
	TArray<FString> ManagerHosts;			 // IPs of other managers which we can fall back to

	FString BasicAuthUsername = TEXT("server");
	FString BasicAuthPassword;
	const FString DefaultPlayInEditorBasicAuthPassword = TEXT("TODO: local testing password");

	const FString BasicAuthHeaderName = TEXT("Authorization");
	FString BasicAuthHeaderValue; // a combination of the basic auth username and password

	class FHttpModule* Http;
	class FWebSocketsModule* WebSockets;
	class FStompModule* Stomp;

	TSharedPtr<class IStompClient> StompClient;

	FTimerHandle TimerHandle_ServerPawns;
	FTimerHandle TimerHandle_RecentEmissions;

#if WITH_ADHOC_PLUGIN_EXTRA
	/** Recent emissions (e.g. explosions) are cached here to be submitted as an event for all others to see. */
	TArray<FAdhocEmission> RecentEmissions;
#endif
#endif

	UAdhocGameModeComponent();

	/** Handle command line options, start up modules, and set up some initial factions and control points in the game state (until we get the full info from web server). */
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitFactionStates() const;
	void InitAreaStates() const;
	void InitServerStates() const;
	void InitObjectiveStates() const;
#if WITH_ADHOC_PLUGIN_EXTRA
	void InitStructureStates();
#endif

public:
	/** Perform a login with the web server and retrieve information about the player e.g. which faction they are etc.. */
	void PostLogin(const APlayerController* PlayerController);

	void ObjectiveTaken(struct FAdhocObjectiveState& OutObjective, struct FAdhocFactionState& Faction) const;
private:
	/** Called when an ObjectiveTaken event occurs. Updates the objective state and associated actor(s) in game and broadcast a message. */
	void OnObjectiveTakenEvent(FAdhocObjectiveState& OutObjective, FAdhocFactionState& Faction) const;

public:
	void CreateStructure(const class APawn* PlayerPawn, const FGuid& UUID, const FString& Type, const FVector& Location, const FRotator& Rotation, const FVector& Scale) const;
private:
#if WITH_ADHOC_PLUGIN_EXTRA
	/** Called when a StructureCreated event occurs. */
	void OnStructureCreatedEvent(const struct FAdhocStructureState& Structure) const;
	void UpdateOrCreateStructureActor(const FAdhocStructureState& Structure) const;
#endif

public:
	/** Trigger an event appropriate for when a player killed another player. */
	void UserDefeatedUser(class APlayerController* PlayerController, class APlayerController* DefeatedPlayerController) const;
	/** Trigger an event appropriate for when a player killed a bot. */
	void UserDefeatedBot(class APlayerController* PlayerController, class AController* DefeatedBotController) const;
private:
	/** Called when a DefeatedUser event occurs. Broadcasts a message to all users in the server. */
	void OnUserDefeatedUserEvent(class APlayerController* PlayerController, class APlayerController* DefeatedPlayerController) const;
	/** Called when a DefeatedBot event occurs. */
	void OnUserDefeatedBotEvent(class APlayerController* PlayerController, class AController* DefeatedBotController) const;

public:
	/** Called when player enters an area volume and may cause the player to connect to different server managing that area. */
	void PlayerEnterArea(APlayerController* PlayerController, int32 AreaIndex) const;

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
private:
	void ShutdownIfNotInEditor() const;

	void OnStompConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString);
	void OnStompClosed(const FString& Reason) const;
	void OnStompConnectionError(const FString& Error) const;
	void OnStompError(const FString& Error) const;
	void OnStompRequestCompleted(bool bSuccess, const FString& Error) const;
	void OnStompSubscriptionEvent(const class IStompMessage& Message);

	void RetrieveFactions();
	void RetrieveServers();
	void SubmitAreas();
	void SubmitObjectives();
#if WITH_ADHOC_PLUGIN_EXTRA
	void SubmitStructures();
#endif

	void OnFactionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const;
	void OnServersResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const;
	void OnAreasResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnObjectivesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
#if WITH_ADHOC_PLUGIN_EXTRA
	void OnStructuresResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const;
#endif

#if WITH_ADHOC_PLUGIN_EXTRA
	static void ExtractStructureFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FAdhocStructureState &OutStructure);
#endif

	void ServerStarted() const;
	/** Called when a ServerUpdated event occurs. May set the areas this server has been assigned to manage. */
	void OnServerUpdatedEvent(int32 EventServerID, int32 EventRegionID, const FString& EventStatus, const FString& EventPrivateIP, const FString& EventPublicIP,
		int32 EventPublicWebSocketPort, const TArray<int64>& EventAreaIDs, const TArray<int32>& EventAreaIndexes) const;

	void SetActiveAreas(int32 RegionID, const TArray<int32>& AreaIndexes) const;

	/** Called when a WorldUpdated event occurs. */
	void OnWorldUpdatedEvent(int64 WorldWorldID, int64 WorldVersion, const TArray<FString>& WorldManagerHosts);

	void SubmitUserJoin(class UAdhocPlayerControllerComponent* AdhocPlayerController);
	void SubmitUserRegister(class UAdhocPlayerControllerComponent* AdhocPlayerController);
	/** When details of the user are received (from either user join or register) - update the player state (e.g. to set faction). */
	void OnUserJoinResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, UAdhocPlayerControllerComponent* AdhocPlayerController, bool bKickOnFailure);

	void SubmitNavigate(class UAdhocPlayerControllerComponent* AdhocPlayerController, int32 AreaID) const;
	void OnNavigateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, UAdhocPlayerControllerComponent* AdhocPlayerController) const;

	/** Regularly send a server pawns event (includes pawn names, locations etc.). */
	void OnTimer_ServerPawns() const;

#if WITH_ADHOC_PLUGIN_EXTRA
public:
	void AddEmission(const FAdhocEmission& Emission);
private:
	/** Send emissions (e.g. explosions) event if any recent emissions. */
	void OnTimer_RecentEmissions();

	static void ExtractEmissionFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, FAdhocEmission &OutEmission);

	void OnEmissionsEvent(const TArray<FAdhocEmission>& Emissions) const;
#endif

#endif
};
