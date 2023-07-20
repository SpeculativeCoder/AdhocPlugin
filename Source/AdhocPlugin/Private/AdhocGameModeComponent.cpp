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

#include "AdhocGameModeComponent.h"

#include "AdhocGameStateComponent.h"
#include "EngineUtils.h"
#include "Area/AdhocAreaComponent.h"
#include "Faction/AdhocFactionState.h"
#include "Area/AdhocAreaVolume.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Server/AdhocServerState.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Online/WebSockets/Public/WebSocketsModule.h"
#include "Runtime/Online/Stomp/Public/IStompMessage.h"
#include "Runtime/Online/Stomp/Public/StompModule.h"
#include "Kismet/GameplayStatics.h"
#include "Objective/AdhocObjectiveInterface.h"
#include "Pawn/AdhocPawnComponent.h"
#include "Player/AdhocPlayerControllerInterface.h"
#include "Player/AdhocPlayerStateComponent.h"

UAdhocGameModeComponent::UAdhocGameModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	bWantsInitializeComponent = true;
}

void UAdhocGameModeComponent::InitializeComponent()
{
	Super::InitializeComponent();

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	BasicAuthPassword = FPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_BASIC_AUTH_PASSWORD"));
	if (BasicAuthPassword.IsEmpty())
	{
		const UWorld* World = GetWorld();
		check(World);

		if (!World->IsPlayInEditor())
		{
			UE_LOG(LogTemp, Error, TEXT("SERVER_BASIC_AUTH_PASSWORD environment variable not set - will shut down server"));
			// UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
			FPlatformMisc::RequestExitWithStatus(false, 1);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SERVER_BASIC_AUTH_PASSWORD environment variable not set - using default"));
			BasicAuthPassword = DefaultPlayInEditorBasicAuthPassword;
		}
	}

	const FString EncodedUsernameAndPassword = FBase64::Encode(FString::Printf(TEXT("%s:%s"), *BasicAuthUsername, *BasicAuthPassword));
	BasicAuthHeaderValue = FString::Printf(TEXT("Basic %s"), *EncodedUsernameAndPassword);

	FParse::Value(FCommandLine::Get(), TEXT("PrivateIP="), PrivateIP);
	FParse::Value(FCommandLine::Get(), TEXT("ManagerHost="), ManagerHost);

	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: InitializeComponent: PrivateIP=%s ManagerHost=%s"), *PrivateIP, *ManagerHost);

	FModuleManager& ModuleManager = FModuleManager::Get();

	if (!ModuleManager.IsModuleLoaded(TEXT("WebSockets")))
	{
		ModuleManager.LoadModule(TEXT("WebSockets"));
	}

	if (!ModuleManager.IsModuleLoaded(TEXT("Stomp")))
	{
		ModuleManager.LoadModule(TEXT("Stomp"));
	}

	Http = &FHttpModule::Get();
	WebSockets = &FWebSocketsModule::Get();
	Stomp = &FStompModule::Get();
#endif

	GameMode = GetOwner<AGameModeBase>();
	check(GameMode);

	GameState = GameMode->GetGameState<AGameStateBase>();
	check(GameState);

	AdhocGameState = CastChecked<UAdhocGameStateComponent>(GameState->GetComponentByClass(UAdhocGameStateComponent::StaticClass()));

	int64 ServerID = 1;
	int64 RegionID = 1;
	FParse::Value(FCommandLine::Get(), TEXT("ServerID="), ServerID);
	FParse::Value(FCommandLine::Get(), TEXT("RegionID="), RegionID);

	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: InitializeComponent: ServerID=%d RegionID=%d"), ServerID, RegionID);

	AdhocGameState->SetServerID(ServerID);
	AdhocGameState->SetRegionID(RegionID);

	// set up some default factions (will be overridden once we contact the manager server)
	TArray<FAdhocFactionState> Factions;
	Factions.SetNum(4);

	// Factions[0].ID = 1;
	Factions[0].Index = 0;
	Factions[0].Name = TEXT("Alpha");
	Factions[0].Color = FColor::FromHex(TEXT("#0088FF"));
	Factions[0].Score = 0;

	// Factions[1].ID = 2;
	Factions[1].Index = 1;
	Factions[1].Name = TEXT("Beta");
	Factions[1].Color = FColor::FromHex(TEXT("#FF2200"));
	Factions[1].Score = 0;

	// Factions[2].ID = 3;
	Factions[2].Index = 2;
	Factions[2].Name = TEXT("Gamma");
	Factions[2].Color = FColor::FromHex(TEXT("#FFFF00"));
	Factions[2].Score = 0;

	// Factions[3].ID = 4;
	Factions[3].Index = 3;
	Factions[3].Name = TEXT("Delta");
	Factions[3].Color = FColor::FromHex(TEXT("#8800FF"));
	Factions[3].Score = 0;

	AdhocGameState->SetFactions(Factions);

	// set up the areas in this region (manager server will tell us about the areas in other regions)
	int32 AreaIndex = 0;
	TArray<FAdhocAreaState> Areas;
	// TArray<int64> ActiveAreaIDs;
	TArray<int32> ActiveAreaIndexes;
	for (TActorIterator<AAdhocAreaVolume> AreaVolumeIter(GetWorld()); AreaVolumeIter; ++AreaVolumeIter)
	{
		AAdhocAreaVolume* AreaVolume = *AreaVolumeIter;
		// AreaVolume->SetAreaID(NextAreaIndex + 1);
		AreaVolume->GetAdhocArea()->SetAreaIndex(AreaIndex);

		FAdhocAreaState Area;
		// Area.ID = AreaVolume->GetAreaID();
		Area.RegionID = RegionID;
		Area.Index = AreaVolume->GetAdhocArea()->GetAreaIndex();
		Area.Name = AreaVolume->GetAdhocArea()->GetFriendlyName();
		Area.Location = AreaVolume->GetActorLocation();
		Area.Size = AreaVolume->GetActorScale() * 200;
		Area.ServerID = ServerID;
		Areas.Add(Area);

		// until we get told which areas should be active just assume they are all assigned to us?
		// ActiveAreaIDs.AddUnique(AreaVolume->GetAreaID());

		// TODO: HACK: just take first area for now
		if (ActiveAreaIndexes.Num() < 1)
		{
			ActiveAreaIndexes.AddUnique(AreaVolume->GetAdhocArea()->GetAreaIndex());
		}

		AreaIndex++;
	}

	// can uncomment two lines below to test auto area generation
	//Areas.Reset();
	//ActiveAreaIndexes.Reset();

	if (Areas.Num() <= 0)
	{
		FAdhocAreaState Area;
		// Area.ID = AreaVolume->GetAreaID();
		Area.RegionID = RegionID;
		Area.Index = 0;
		Area.Name = TEXT("A");
		Area.ServerID = ServerID;
		Area.Location = FVector::ZeroVector;
		// TODO: determine map size based on geometry
		Area.Size = FVector(10000, 10000, 10000);
		Areas.Add(Area);

		ActiveAreaIndexes.AddUnique(Area.Index);
	}

	AdhocGameState->SetAreas(Areas);
	// AdhocGameState->SetActiveAreaIDs(ActiveAreaIDs);
	AdhocGameState->SetActiveAreaIndexes(ActiveAreaIndexes);

	// assume this is the only server until we get the server(s) info
	TArray<FAdhocServerState> Servers;
	Servers.SetNum(1);

	Servers[0].ID = ServerID;
	Servers[0].RegionID = RegionID;
	// Servers[0].AreaIDs = ActiveAreaIDs;
	Servers[0].AreaIndexes = ActiveAreaIndexes;
	Servers[0].Name = TEXT("Server");
	Servers[0].Status = TEXT("STARTED");
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	Servers[0].PrivateIP = PrivateIP;
	Servers[0].PublicIP = PrivateIP;
	Servers[0].PublicWebSocketPort = 8889; // TODO: get from driver
#endif

	AdhocGameState->SetServers(Servers);

	// set up the known objectives
	int32 ObjectiveIndex = 0;
	TArray<FAdhocObjectiveState> Objectives;

	for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
	{
		IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
		if (!ObjectiveActor)
		{
			continue;
		}

		// ObjectiveActor->SetObjectiveID(NextObjectiveIndex + 1);
		ObjectiveActor->SetObjectiveIndex(ObjectiveIndex);

		FAdhocObjectiveState Objective;
		// Objective.ID = ObjectiveActor->GetObjectiveID();
		Objective.RegionID = RegionID;
		Objective.Index = ObjectiveActor->GetObjectiveIndex();
		Objective.Name = ObjectiveActor->GetFriendlyName();
		Objective.Location = (*ObjectiveActorIter)->GetActorLocation();
		Objective.InitialFactionIndex = ObjectiveActor->GetInitialFactionIndex();
		Objective.FactionIndex = ObjectiveActor->GetFactionIndex() == -1 ? ObjectiveActor->GetInitialFactionIndex() : ObjectiveActor->GetFactionIndex();
		// NOTE: area info is set later in BeginPlay once the objective actors have initialized we have been able to check which area volumes they are in
		Objectives.Add(Objective);

		ObjectiveIndex++;
	}

	AdhocGameState->SetObjectives(Objectives);

	for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
	{
		const IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
		if (!ObjectiveActor)
		{
			continue;
		}

		FAdhocObjectiveState* Objective = AdhocGameState->FindObjectiveByRegionIDAndIndex(RegionID, ObjectiveActor->GetObjectiveIndex());
		check(Objective);

		// TArray<int64> LinkedObjectiveIDs;
		TArray<int32> LinkedObjectiveIndexes;
		for (auto& LinkedObjectiveActor : ObjectiveActor->GetLinkedObjectiveInterfaces())
		{
			// LinkedObjectiveIDs.AddUnique(LinkedObjectiveActor->GetObjectiveID());
			LinkedObjectiveIndexes.AddUnique(LinkedObjectiveActor->GetObjectiveIndex());
		}
		// Objective->LinkedObjectiveIDs = LinkedObjectiveIDs;
		Objective->LinkedObjectiveIndexes = LinkedObjectiveIndexes;
	}

#if WITH_ADHOC_PLUGIN_EXTRA
	InitStructureStates();
#endif
}

void UAdhocGameModeComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: BeginPlay: NetMode=%d"), GetNetMode());

	for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
	{
		const IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
		if (!ObjectiveActor)
		{
			continue;
		}

		// if (ObjectiveActor->GetAreaVolume())
		// {
		FAdhocObjectiveState* Objective = AdhocGameState->FindObjectiveByIndex(ObjectiveActor->GetObjectiveIndex());
		check(Objective);
		// if (Objective)
		// {
		// Objective->AreaID = ObjectiveActor->GetAreaID();
		Objective->AreaIndex = ObjectiveActor->GetAreaIndexSafe();
		// }
		// }
	}

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	if (GetNetMode() != NM_Client)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_ServerPawns, this, &UAdhocGameModeComponent::OnTimer_ServerPawns, 20, true, 20);

		// initiate stomp connection - only once we are sure this connection is established
		// will we then do an initial push/refresh all world state via REST calls etc.
		UE_LOG(LogTemp, Log, TEXT("Initializing Stomp connection..."));

		const FString& StompURL = FString::Printf(TEXT("ws://%s:80/ws/stomp/server"), *ManagerHost);
		StompClient = Stomp->CreateClient(StompURL, *BasicAuthHeaderValue);

		StompClient->OnConnected().AddUObject(this, &UAdhocGameModeComponent::OnStompConnected);
		StompClient->OnConnectionError().AddUObject(this, &UAdhocGameModeComponent::OnStompConnectionError);
		StompClient->OnError().AddUObject(this, &UAdhocGameModeComponent::OnStompError);
		StompClient->OnClosed().AddUObject(this, &UAdhocGameModeComponent::OnStompClosed);

		//FStompHeader StompHeader;
		//StompHeader.Add(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
		//StompHeader.Add(TEXT("_csrf"), TEXT("SERVER"));
		// TODO: why do we need server to send us pongs rather than us pinging them ?????
		//static const FName HeartbeatHeader(TEXT("heart-beat"));
		//StompHeader.Add(HeartbeatHeader, TEXT("0,15000"));

		StompClient->Connect(); //StompHeader);
	}
#endif
}

void UAdhocGameModeComponent::Login(APlayerController* PlayerController, APlayerState* PlayerState, const FString& Options)
{
	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: Login: Options=%s"), *Options);

	IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(PlayerController);

	UAdhocPlayerStateComponent* AdhocPlayerState =
		CastChecked<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));

	const int32 RandomFactionIndex = FMath::RandRange(0, AdhocGameState->GetNumFactions() - 1);

	AdhocPlayerState->SetFactionIndex(RandomFactionIndex);

	const int32 UserID = UGameplayStatics::GetIntOption(Options, TEXT("UserId"), -1);
	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: Login: UserId=%d"), UserID);
	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("Token"));
	UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: Login: Token=%s"), *Token);

	AdhocPlayerControllerInterface->SetFactionIndex(RandomFactionIndex);
	AdhocPlayerControllerInterface->SetUserID(UserID);
	AdhocPlayerControllerInterface->SetToken(Token);
}

void UAdhocGameModeComponent::PostLogin(APlayerController* NewPlayer)
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)

	IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(NewPlayer);

	if (AdhocPlayerControllerInterface->GetUserID() != -1 && !AdhocPlayerControllerInterface->GetToken().IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: PostLogin: Logging in player: UserID=%d Token=%s"), AdhocPlayerControllerInterface->GetUserID(),
			*AdhocPlayerControllerInterface->GetToken());

		SubmitUserJoin(NewPlayer);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AdhocGameModeComponent: PostLogin: Auto-registering player"));

		SubmitUserRegister(NewPlayer);
	}

#endif
}

void UAdhocGameModeComponent::ObjectiveTaken(FAdhocObjectiveState& OutObjective, FAdhocFactionState& Faction) const
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	if (StompClient && StompClient->IsConnected())
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("eventType"), TEXT("ObjectiveTaken"));
		Writer->WriteValue(TEXT("objectiveId"), static_cast<double>(OutObjective.ID));
		// Writer->WriteValue(FString("oldFactionId"), (double)OldFactionID);
		Writer->WriteValue(TEXT("factionId"), static_cast<double>(Faction.ID));
		Writer->WriteObjectEnd();
		Writer->Close();

		UE_LOG(LogTemp, Log, TEXT("Sending: %s"), *JsonString);
		StompClient->Send("/app/ObjectiveTaken", JsonString);
	}
	else
#endif
	{
		OnObjectiveTakenEvent(OutObjective, Faction);
	}
}

void UAdhocGameModeComponent::OnObjectiveTakenEvent(FAdhocObjectiveState& OutObjective, FAdhocFactionState& Faction) const
{
	UE_LOG(LogTemp, Log, TEXT("OnObjectiveTakenEvent: OutObjective.ID=%d Faction.ID=%d"), OutObjective.ID, Faction.ID);

	OutObjective.FactionID = Faction.ID;
	OutObjective.FactionIndex = Faction.Index;
	// UE_LOG(LogTemp, Warning,
	// 	TEXT("OnObjectiveTakenEvent: Objective.ID=%d Objective.Index=%d Objective.FactionID=%d Objective.FactionIndex=%d"),
	// 	Objective->ID, Objective->Index, Objective->FactionID, Objective->FactionIndex);

	// flip the actor in the world to the appropriate faction
	for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
	{
		IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
		if (!ObjectiveActor)
		{
			continue;
		}

		if (ObjectiveActor->GetObjectiveIndex() == OutObjective.Index)
		{
			ObjectiveActor->ObjectiveTaken(Faction.Index);
			break;
		}
	}

	OnObjectiveTakenEventDelegate.Broadcast(OutObjective, Faction);
}

#if WITH_ADHOC_PLUGIN_EXTRA == 0
// ReSharper disable once CppMemberFunctionMayBeStatic
void UAdhocGameModeComponent::CreateStructure(
	const APawn* PlayerPawn, const FGuid& UUID, const FString& Type, const FVector& Location, const FRotator& Rotation, const FVector& Scale) const
{
	UE_LOG(LogTemp, Error,
		TEXT("CreateStructure: PlayerPawn=%s UUID=%s Type=%s - Ignoring because structures require AdhocPlugin to be compiled with WITH_ADHOC_PLUGIN_EXTRA"),
		*PlayerPawn->GetName(), *UUID.ToString(EGuidFormats::DigitsWithHyphens), *Type);
}
#endif

void UAdhocGameModeComponent::UserDefeatedUser(APlayerController* PlayerController, APlayerController* DefeatedPlayerController) const
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	if (StompClient && StompClient->IsConnected())
	{
		const UAdhocPlayerStateComponent* AdhocPlayerState =
			Cast<UAdhocPlayerStateComponent>(PlayerController->GetPlayerState<APlayerState>()->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));
		const UAdhocPlayerStateComponent* DefeatedAdhocPlayerState = Cast<UAdhocPlayerStateComponent>(
			DefeatedPlayerController->GetPlayerState<APlayerState>()->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));
		check(AdhocPlayerState);
		check(DefeatedAdhocPlayerState);

		FString JsonString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("eventType"), TEXT("UserDefeatedUser"));
		Writer->WriteValue(TEXT("userId"), AdhocPlayerState->GetUserID());
		Writer->WriteValue(TEXT("defeatedUserId"), DefeatedAdhocPlayerState->GetUserID());
		Writer->WriteObjectEnd();
		Writer->Close();

		UE_LOG(LogTemp, Log, TEXT("Sending: %s"), *JsonString);
		StompClient->Send("/app/UserDefeatedUser", JsonString);

		// TODO: trigger via event?
		OnUserDefeatedUserEvent(PlayerController, DefeatedPlayerController);
	}
	else
#endif
	{
		OnUserDefeatedUserEvent(PlayerController, DefeatedPlayerController);
	}
}

void UAdhocGameModeComponent::UserDefeatedBot(APlayerController* PlayerController, AController* DefeatedBotController) const
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	if (StompClient && StompClient->IsConnected())
	{
		FString JsonString;
		const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("eventType"), TEXT("UserDefeatedBot"));
		Writer->WriteValue(TEXT("userId"),
			Cast<UAdhocPlayerStateComponent>(PlayerController->GetPlayerState<APlayerState>()->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()))
				->GetUserID());
		Writer->WriteObjectEnd();
		Writer->Close();

		UE_LOG(LogTemp, Log, TEXT("Sending: %s"), *JsonString);
		StompClient->Send("/app/UserDefeatedBot", JsonString);

		// TODO: trigger via event?
		OnUserDefeatedBotEvent(PlayerController, DefeatedBotController);
	}
	else
#endif
	{
		OnUserDefeatedBotEvent(PlayerController, DefeatedBotController);
	}
}

void UAdhocGameModeComponent::OnUserDefeatedUserEvent(APlayerController* PlayerController, APlayerController* DefeatedPlayerController) const
{
	OnUserDefeatedUserEventDelegate.Broadcast(PlayerController, DefeatedPlayerController);
}

void UAdhocGameModeComponent::OnUserDefeatedBotEvent(APlayerController* PlayerController, AController* DefeatedBotController) const
{
	OnUserDefeatedBotEventDelegate.Broadcast(PlayerController, DefeatedBotController);
}

void UAdhocGameModeComponent::PlayerEnterArea(APlayerController* PlayerController, const int32 AreaIndex) const
{
	const APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
	check(PlayerState);

	UE_LOG(LogTemp, Log, TEXT("Player enter area: PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);

	if (AdhocGameState->GetActiveAreaIndexes().Contains(AreaIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("Area already active on this server: PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
		return;
	}

	const FAdhocAreaState* Area = AdhocGameState->FindAreaByIndex(AreaIndex);
	if (!Area)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't navigate as could not find area state! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
		return;
	}

	const int32 AreaID = Area->ID;
	if (AreaID == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't navigate as area ID is not available! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
		return;
	}

	if (Cast<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()))->GetUserID() == -1)
	{
		UE_LOG(
			LogTemp, Warning, TEXT("Can't navigate as player does not have a user ID! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
		return;
	}

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
	SubmitNavigate(PlayerController, AreaID);
#endif
}

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)

void UAdhocGameModeComponent::ShutdownIfNotPlayingInEditor() const
{
	// UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
	const UWorld* World = GetWorld();
	check(World);
	if (!World->IsPlayInEditor())
	{
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}
}

void UAdhocGameModeComponent::OnStompConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString)
{
	UE_LOG(LogTemp, Log, TEXT("OnStompConnected: ProtocolVersion=%s SessionId=%s ServerString=%s"), *ProtocolVersion, *SessionId, *ServerString);

	static FStompSubscriptionEvent StompSubscriptionEvent;
	static FStompRequestCompleted StompRequestCompleted;
	StompSubscriptionEvent.BindUObject(this, &UAdhocGameModeComponent::OnStompSubscriptionEvent);
	StompRequestCompleted.BindUObject(this, &UAdhocGameModeComponent::OnStompRequestCompleted);

	StompClient->Subscribe("/topic/events", StompSubscriptionEvent, StompRequestCompleted);

	RetrieveFactions();
	RetrieveServers();

	SubmitAreas();
}

void UAdhocGameModeComponent::OnStompClosed(const FString& Reason) const
{
	UE_LOG(LogTemp, Log, TEXT("OnStompClosed: Reason=%s"), *Reason);

	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stomp connection closed - should shut down server"));
		ShutdownIfNotPlayingInEditor();
	}
}

void UAdhocGameModeComponent::OnStompConnectionError(const FString& Error) const
{
	UE_LOG(LogTemp, Log, TEXT("OnStompClientConnectionError: Error=%s"), *Error);

	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stomp connection error - should shut down server"));
		ShutdownIfNotPlayingInEditor();
	}
}

void UAdhocGameModeComponent::OnStompError(const FString& Error) const
{
	UE_LOG(LogTemp, Log, TEXT("OnStompClientError: Error=%s"), *Error);

	if (GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stomp error - should shut down server"));
		ShutdownIfNotPlayingInEditor();
	}
}

void UAdhocGameModeComponent::OnStompRequestCompleted(bool bSuccess, const FString& Error) const
{
	UE_LOG(LogTemp, Log, TEXT("OnStompRequestCompleted: bSuccess=%d Error=%s"), bSuccess, *Error);

	if (!bSuccess && GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stomp request completed unsuccessfully - should shut down server"));
		ShutdownIfNotPlayingInEditor();
	}
}

void UAdhocGameModeComponent::OnStompSubscriptionEvent(const IStompMessage& Message)
{
	UE_LOG(LogTemp, Log, TEXT("OnStompSubscriptionEvent: %s"), *Message.GetBodyAsString());

	const auto& Reader = TJsonReaderFactory<>::Create(Message.GetBodyAsString());
	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize Stomp event: %s"), *Message.GetBodyAsString());
		return;
	}
	const FString EventType = JsonObject->GetStringField("eventType");
	// UE_LOG(LogTemp, Log, TEXT("OnStompSubscriptionEvent: eventType=%s"), *eventType);

	if (EventType.Equals(TEXT("ObjectiveTaken")))
	{
		const int32 EventObjectiveID = JsonObject->GetIntegerField("objectiveId");
		const int32 EventFactionID = JsonObject->GetIntegerField("factionId");

		FAdhocObjectiveState* Objective = AdhocGameState->FindObjectiveByID(EventObjectiveID);
		FAdhocFactionState* Faction = AdhocGameState->FindFactionByID(EventFactionID);
		if (!Objective || !Faction)
		{
			// TODO: if not got control points response yet?
			UE_LOG(LogTemp, Warning, TEXT("Invalid ObjectiveTaken ID(s): EventObjectiveID=%d EventFactionID=%d"), EventObjectiveID, EventFactionID);
			return;
		}

		OnObjectiveTakenEvent(*Objective, *Faction);
	}
	else if (EventType.Equals(TEXT("ServerUpdated")))
	{
		const int64 EventServerID = JsonObject->GetIntegerField("serverId");
		const FString EventStatus = JsonObject->GetStringField("status");

		FString EventPrivateIP;
		FString EventPublicIP;
		int32 EventServerPublicWebSocketPort;
		// TArray<FString> EventNearbyPublicServerIPs;
		JsonObject->TryGetStringField("privateIp", EventPrivateIP);
		JsonObject->TryGetStringField("publicIp", EventPublicIP);
		JsonObject->TryGetNumberField("publicWebSocketPort", EventServerPublicWebSocketPort);
		// Event->TryGetStringArrayField("nearbyServerPublicIps", EventNearbyPublicServerIPs); // TODO: check success

		const int32 EventRegionID = JsonObject->GetIntegerField("regionId");

		TArray<int64> EventAreaIDs;
		TArray<int32> EventAreaIndexes;
		for (auto& AreaIDValue : JsonObject->GetArrayField("areaIds"))
		{
			EventAreaIDs.AddUnique(AreaIDValue->AsNumber());
		}
		for (auto& AreaIndexValue : JsonObject->GetArrayField("areaIndexes"))
		{
			EventAreaIndexes.AddUnique(AreaIndexValue->AsNumber());
		}

		OnServerUpdatedEvent(
			EventServerID, EventRegionID, EventStatus, EventPrivateIP, EventPublicIP, EventServerPublicWebSocketPort, EventAreaIDs, EventAreaIndexes);
	}
	else if (EventType.Equals(TEXT("WorldUpdated")))
	{
		const TSharedPtr<FJsonObject>* WorldJsonObjectPtr;

		if (!JsonObject->TryGetObjectField(TEXT("world"), WorldJsonObjectPtr))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid WorldUpdated event: Body=%s"), *Message.GetBodyAsString());
			return;
		}

		const int64 WorldID = (*WorldJsonObjectPtr)->GetIntegerField("id");
		const int64 WorldVersion = (*WorldJsonObjectPtr)->GetIntegerField("version");

		TArray<FString> WorldManagerHosts;
		for (auto& ManagerHostValue : (*WorldJsonObjectPtr)->GetArrayField("managerHosts"))
		{
			WorldManagerHosts.AddUnique(ManagerHostValue->AsString());
		}

		OnWorldUpdatedEvent(WorldID, WorldVersion, WorldManagerHosts);
	}
#if WITH_ADHOC_PLUGIN_EXTRA
	else if (EventType.Equals(TEXT("StructureCreated")))
	{
		FAdhocStructureState Structure;

		const TSharedPtr<FJsonObject>* StructureJsonObjectPtr;

		if (!JsonObject->TryGetObjectField(TEXT("structure"), StructureJsonObjectPtr))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid StructureUpdated event: Body=%s"), *Message.GetBodyAsString());
			return;
		}

		ExtractStructureFromJsonObject(*StructureJsonObjectPtr, Structure);

		OnStructureCreatedEvent(Structure);
	}
#endif
}

void UAdhocGameModeComponent::RetrieveFactions()
{
	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnFactionsResponse);
	const FString URL = FString::Printf(TEXT("http://%s:80/api/factions"), *ManagerHost);
	Request->SetURL(URL);
	Request->SetVerb("GET");
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);

	UE_LOG(LogTemp, Log, TEXT("GET %s"), *URL);
	Request->ProcessRequest();
}

void UAdhocGameModeComponent::RetrieveServers()
{
	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnServersResponse);
	const FString URL = FString::Printf(TEXT("http://%s:80/api/servers"), *ManagerHost);
	Request->SetURL(URL);
	Request->SetVerb("GET");
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);

	UE_LOG(LogTemp, Log, TEXT("GET %s"), *URL);
	Request->ProcessRequest();
}

// PUT AREAS (the map defines the areas, and should override what is on the server, but the server will choose the IDs)
void UAdhocGameModeComponent::SubmitAreas()
{
	FString JsonString;
	const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);

	Writer->WriteArrayStart();
	for (TArray<FAdhocAreaState>::TConstIterator AreaStateIter = AdhocGameState->GetAreasConstIterator(); AreaStateIter; ++AreaStateIter)
	{
		const FAdhocAreaState& AreaState = *AreaStateIter;

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("regionId"), AreaState.RegionID);
		Writer->WriteValue(TEXT("index"), AreaState.Index);
		Writer->WriteValue(TEXT("name"), AreaState.Name);
		Writer->WriteValue(TEXT("x"), static_cast<double>(-AreaState.Location.X));
		Writer->WriteValue(TEXT("y"), static_cast<double>(AreaState.Location.Y));
		Writer->WriteValue(TEXT("z"), static_cast<double>(AreaState.Location.Z));
		Writer->WriteValue(TEXT("sizeX"), static_cast<double>(AreaState.Size.X));
		Writer->WriteValue(TEXT("sizeY"), static_cast<double>(AreaState.Size.Y));
		Writer->WriteValue(TEXT("sizeZ"), static_cast<double>(AreaState.Size.Z));
		// Writer->WriteNull(TEXT("serverId"));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->Close();

	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnAreasResponse);
	const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/areas"), *ManagerHost, AdhocGameState->GetServerID());
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
	Request->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("PUT %s: %s"), *URL, *JsonString);
	Request->ProcessRequest();
}

// PUT OBJECTIVES (the map defines the objectives, and should override what is on the server, but the server will choose the IDs)
void UAdhocGameModeComponent::SubmitObjectives()
{
	FString JsonString;
	const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);

	Writer->WriteArrayStart();
	for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
	{
		const IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
		if (!ObjectiveActor)
		{
			continue;
		}

		// ignore objectives outside an area
		if (ObjectiveActor->GetAreaIndexSafe() == -1)
		{
			continue;
		}

		const FVector Location = (*ObjectiveActorIter)->GetActorLocation();
		const FVector Size = FVector(1, 1, 1); // ObjectiveActor->GetActorScale() * 200; // TODO

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("regionId"), AdhocGameState->GetRegionID());
		Writer->WriteValue(TEXT("index"), ObjectiveActor->GetObjectiveIndex());
		Writer->WriteValue(TEXT("name"), ObjectiveActor->GetFriendlyName());
		Writer->WriteValue(TEXT("x"), static_cast<double>(-Location.X));
		Writer->WriteValue(TEXT("y"), static_cast<double>(Location.Y));
		Writer->WriteValue(TEXT("z"), static_cast<double>(Location.Z));
		Writer->WriteValue(TEXT("sizeX"), static_cast<double>(Size.X));
		Writer->WriteValue(TEXT("sizeY"), static_cast<double>(Size.Y));
		Writer->WriteValue(TEXT("sizeZ"), static_cast<double>(Size.Z));

		if (ObjectiveActor->GetInitialFactionIndex() == -1)
		{
			Writer->WriteNull(TEXT("initialFactionIndex"));
		}
		else
		{
			Writer->WriteValue(TEXT("initialFactionIndex"), static_cast<double>(ObjectiveActor->GetInitialFactionIndex()));
		}

		if (ObjectiveActor->GetFactionIndex() == -1)
		{
			Writer->WriteNull(TEXT("factionIndex"));
		}
		else
		{
			Writer->WriteValue(TEXT("factionIndex"), static_cast<double>(ObjectiveActor->GetFactionIndex()));
		}

		// Writer->WriteArrayStart(TEXT("linkedObjectiveIds"));
		// for (auto& LinkedObjectiveActor : ObjectiveActor->GetLinkedObjectiveInterfaces())
		// {
		// 	Writer->WriteValue(LinkedObjectiveActor->GetObjectiveID());
		// }
		// Writer->WriteArrayEnd();
		Writer->WriteArrayStart(TEXT("linkedObjectiveIndexes"));
		for (auto& LinkedObjectiveActor : ObjectiveActor->GetLinkedObjectiveInterfaces())
		{
			// ignore objectives outside an area
			if (LinkedObjectiveActor->GetAreaIndexSafe() == -1)
			{
				continue;
			}

			Writer->WriteValue(LinkedObjectiveActor->GetObjectiveIndex());
		}
		Writer->WriteArrayEnd();

		// Writer->WriteValue(FString("areaId"), ObjectiveActor->GetAreaVolume()->GetAreaID());
		Writer->WriteValue(TEXT("areaIndex"), ObjectiveActor->GetAreaIndexSafe());

		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->Close();

	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnObjectivesResponse);
	const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/objectives"), *ManagerHost, AdhocGameState->GetServerID());
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
	Request->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("PUT %s: %s"), *URL, *JsonString);
	Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnFactionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const
{
	UE_LOG(LogTemp, Log, TEXT("Factions response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("Factions response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	if (!FJsonSerializer::Deserialize(Reader, JsonValues))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize factions response: Content=%s"), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	TArray<FAdhocFactionState> Factions;
	Factions.SetNum(JsonValues.Num());

	for (int FactionID = 0; FactionID < JsonValues.Num(); FactionID++)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValues[FactionID]->AsObject();
		Factions[FactionID].ID = JsonObject->GetIntegerField("id");
		Factions[FactionID].Index = JsonObject->GetIntegerField("index");
		Factions[FactionID].Name = JsonObject->GetStringField("name");
		Factions[FactionID].Color = FColor::FromHex(JsonObject->GetStringField("color"));
		Factions[FactionID].Score = JsonObject->GetIntegerField("score");
	}

	AdhocGameState->SetFactions(Factions);
}

void UAdhocGameModeComponent::OnServersResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const
{
	UE_LOG(LogTemp, Log, TEXT("Servers response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Servers response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	if (!FJsonSerializer::Deserialize(Reader, JsonValues))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize get servers response: Content=%s"), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	TArray<FAdhocServerState> Servers;
	Servers.SetNum(JsonValues.Num());

	for (int i = 0; i < Servers.Num(); i++)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValues[i]->AsObject();

		Servers[i].ID = JsonObject->GetIntegerField("id");
		Servers[i].Name = JsonObject->GetStringField("name");
		// Servers[i].HostingType = NewServer->GetStringField("hostingType");
		Servers[i].Status = JsonObject->GetStringField("status");
		JsonObject->TryGetStringField("privateIP", Servers[i].PrivateIP);
		JsonObject->TryGetStringField("publicIP", Servers[i].PublicIP);
		JsonObject->TryGetNumberField("publicWebSocketPort", Servers[i].PublicWebSocketPort);
		Servers[i].RegionID = JsonObject->GetIntegerField("regionId");
		for (auto& AreaIDValue : JsonObject->GetArrayField("areaIds"))
		{
			Servers[i].AreaIDs.AddUnique(AreaIDValue->AsNumber());
		}
		for (auto& AreaIndexValue : JsonObject->GetArrayField("areaIndexes"))
		{
			Servers[i].AreaIndexes.AddUnique(AreaIndexValue->AsNumber());
		}

		// if it is my server ID - what areas are assigned to this server?
		if (AdhocGameState->GetServerID() == Servers[i].ID)
		{
			SetActiveAreas(Servers[i].RegionID, Servers[i].AreaIndexes);
		}
	}

	AdhocGameState->SetServers(Servers);
}

void UAdhocGameModeComponent::OnAreasResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("Areas response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Areas response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	if (!FJsonSerializer::Deserialize(Reader, JsonValues))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize areas response: %s"), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	TArray<FAdhocAreaState> Areas;
	Areas.SetNum(JsonValues.Num());

	for (int i = 0; i < Areas.Num(); i++)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValues[i]->AsObject();

		Areas[i].ID = JsonObject->GetIntegerField("id");
		Areas[i].RegionID = JsonObject->GetIntegerField("regionId");
		Areas[i].Index = JsonObject->GetIntegerField("index");
		;
		Areas[i].Name = JsonObject->GetStringField("name");
		JsonObject->TryGetNumberField("serverId", Areas[i].ServerID);

		// // any area actors in this region should be updated with IDs etc.
		// if (Areas[i].RegionID == AdhocGameState->GetRegionID())
		// {
		// 	for (TActorIterator<AAreaVolume> AreaVolumeIter(GetWorld()); AreaVolumeIter; ++AreaVolumeIter)
		// 	{
		// 		AAreaVolume* AreaVolume = *AreaVolumeIter;
		// 		if (AreaVolume->GetAreaIndex() == Areas[i].Index)
		// 		{
		// 			AreaVolume->SetAreaID(Areas[i].ID);
		// 		}
		// 	}
		// }
	}

	AdhocGameState->SetAreas(Areas);

	SubmitObjectives();
}

void UAdhocGameModeComponent::OnObjectivesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("Objectives response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Objectives response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	if (!FJsonSerializer::Deserialize(Reader, JsonValues))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize objectives response: %s"), *Response->GetContentAsString());
		ShutdownIfNotPlayingInEditor();
		return;
	}

	TArray<FAdhocObjectiveState> Objectives;
	Objectives.SetNum(JsonValues.Num());

	for (int i = 0; i < Objectives.Num(); i++)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValues[i]->AsObject();
		Objectives[i].ID = JsonObject->GetIntegerField("id");
		Objectives[i].Name = JsonObject->GetStringField("name");
		Objectives[i].RegionID = JsonObject->GetIntegerField("regionId");
		Objectives[i].Index = JsonObject->GetIntegerField("index");
		JsonObject->TryGetNumberField(TEXT("initialFactionId"), Objectives[i].InitialFactionID);
		JsonObject->TryGetNumberField(TEXT("initialFactionIndex"), Objectives[i].InitialFactionIndex);
		JsonObject->TryGetNumberField(TEXT("factionId"), Objectives[i].FactionID);
		JsonObject->TryGetNumberField(TEXT("factionIndex"), Objectives[i].FactionIndex);
		JsonObject->TryGetNumberField(TEXT("areaId"), Objectives[i].AreaID);
		JsonObject->TryGetNumberField(TEXT("areaIndex"), Objectives[i].AreaIndex);

		TArray<int64> LinkedObjectiveIDs;
		for (auto& LinkedObjectiveID : JsonObject->GetArrayField("linkedObjectiveIds"))
		{
			LinkedObjectiveIDs.AddUnique(LinkedObjectiveID->AsNumber());
		}
		TArray<int32> LinkedObjectiveIndexes;
		for (auto& LinkedObjectiveIndex : JsonObject->GetArrayField("linkedObjectiveIndexes"))
		{
			LinkedObjectiveIndexes.AddUnique(LinkedObjectiveIndex->AsNumber());
		}
		Objectives[i].LinkedObjectiveIDs = LinkedObjectiveIDs;
		Objectives[i].LinkedObjectiveIndexes = LinkedObjectiveIndexes;

		if (Objectives[i].RegionID == AdhocGameState->GetRegionID())
		{
			// push the manager's current faction / ID information onto the actors
			for (TActorIterator<AActor> ObjectiveActorIter(GetWorld()); ObjectiveActorIter; ++ObjectiveActorIter)
			{
				IAdhocObjectiveInterface* ObjectiveActor = Cast<IAdhocObjectiveInterface>(*ObjectiveActorIter);
				if (!ObjectiveActor)
				{
					continue;
				}

				if (ObjectiveActor->GetObjectiveIndex() == Objectives[i].Index)
				{
					// ObjectiveActor->SetObjectiveID(Objectives[i].ID);
					ObjectiveActor->ObjectiveTaken(Objectives[i].FactionIndex);
				}
			}
		}
	}

	AdhocGameState->SetObjectives(Objectives);

#if WITH_ADHOC_PLUGIN_EXTRA
	SubmitStructures();
#endif
}

void UAdhocGameModeComponent::ServerStarted() const
{
	FString JsonString;
	const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("eventType"), TEXT("ServerStarted"));
	Writer->WriteValue(TEXT("serverId"), static_cast<double>(AdhocGameState->GetServerID()));
	Writer->WriteValue(TEXT("privateIp"), PrivateIP);
	Writer->WriteValue(TEXT("managerHost"), ManagerHost);
	Writer->WriteObjectEnd();
	Writer->Close();

	UE_LOG(LogTemp, Log, TEXT("Sending: %s"), *JsonString);
	StompClient->Send("/app/ServerStarted", JsonString);
}

void UAdhocGameModeComponent::OnServerUpdatedEvent(int32 EventServerID, int32 EventRegionID, const FString& EventStatus, const FString& EventPrivateIP,
	const FString& EventPublicIP, int32 EventPublicWebSocketPort, const TArray<int64>& EventAreaIDs, const TArray<int32>& EventAreaIndexes) const
{
	FAdhocServerState* Server = AdhocGameState->FindOrInsertServerByID(EventServerID);
	if (!Server)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find or insert server with ID %d!"), EventServerID);
		return;
	}

	Server->ID = EventServerID;
	Server->RegionID = EventRegionID;
	Server->Status = EventStatus;
	Server->PrivateIP = EventPrivateIP;
	Server->PublicIP = EventPublicIP;
	Server->PublicWebSocketPort = EventPublicWebSocketPort;

	Server->AreaIDs.Reset();
	for (auto& AreaIDValue : EventAreaIDs)
	{
		Server->AreaIDs.AddUnique(AreaIDValue);
	}

	Server->AreaIndexes.Reset();
	for (auto& AreaIndexValue : EventAreaIndexes)
	{
		Server->AreaIndexes.AddUnique(AreaIndexValue);
	}

	if (EventServerID == AdhocGameState->GetServerID())
	{
		SetActiveAreas(EventRegionID, EventAreaIndexes);
	}
}

void UAdhocGameModeComponent::SetActiveAreas(const int32 RegionID, const TArray<int32>& AreaIndexes) const
{
	if (RegionID != AdhocGameState->GetRegionID())
	{
		UE_LOG(LogTemp, Error, TEXT("Server active region does not match! ServerID=%d AdhocGameState->RegionID=%d RegionID=%d"), AdhocGameState->GetServerID(),
			AdhocGameState->GetRegionID(), RegionID);
	}
	else
	{
		for (auto& AreaIndex : AreaIndexes)
		{
			UE_LOG(LogTemp, Log, TEXT("This server has active area index %d"), AreaIndex);
		}
		AdhocGameState->SetActiveAreaIndexes(AreaIndexes);
	}
}

void UAdhocGameModeComponent::OnWorldUpdatedEvent(int64 WorldWorldID, int64 WorldVersion, const TArray<FString>& WorldManagerHosts)
{
	ManagerHosts = WorldManagerHosts;
}

void UAdhocGameModeComponent::SubmitUserJoin(APlayerController* PlayerController)
{
	IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(PlayerController);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("userId"), AdhocPlayerControllerInterface->GetUserID());
	Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());
	Writer->WriteValue(TEXT("token"), *AdhocPlayerControllerInterface->GetToken());
	Writer->WriteObjectEnd();
	Writer->Close();

	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnUserJoinResponse, PlayerController, true);
	const FString URL = FString::Printf(
		TEXT("http://%s:80/api/servers/%d/users/%d/join"), *ManagerHost, AdhocGameState->GetServerID(), AdhocPlayerControllerInterface->GetUserID());
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// Request->SetHeader(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
	Request->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("POST %s: %s"), *URL, *JsonString);
	Request->ProcessRequest();
}

void UAdhocGameModeComponent::SubmitUserRegister(APlayerController* PlayerController)
{
	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());
	Writer->WriteObjectEnd();
	Writer->Close();

	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnUserJoinResponse, PlayerController, false);
	const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/users/register"), *ManagerHost, AdhocGameState->GetServerID());
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// Request->SetHeader(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
	Request->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("POST %s: %s"), *URL, *JsonString);
	Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnUserJoinResponse(
	FHttpRequestPtr Request, const FHttpResponsePtr Response, const bool bWasSuccessful, APlayerController* PlayerController, const bool bKickOnFailure)
{
	UE_LOG(LogTemp, Log, TEXT("Player login response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Player login response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		if (bKickOnFailure)
		{
			GameMode->GameSession->KickPlayer(PlayerController, FText::FromString(TEXT("Login failure")));
		}
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to deserialize player login response: %s"), *Response->GetContentAsString());
		if (bKickOnFailure)
		{
			GameMode->GameSession->KickPlayer(PlayerController, FText::FromString(TEXT("Login failure")));
		}
		return;
	}

	const int64 UserID = JsonObject->GetIntegerField("id");
	const int64 UserFactionID = JsonObject->GetIntegerField("factionId");
	const int32 UserFactionIndex = JsonObject->GetIntegerField("factionIndex");
	const FString UserName = JsonObject->GetStringField("name");
	const FString UserToken = JsonObject->GetStringField("token");

	int64 ServerID;
	if (JsonObject->TryGetNumberField("serverID", ServerID))
	{
		double X = 0;
		double Y = 0;
		double Z = 0;
		double Yaw = 0;
		double Pitch = 0;
		if (JsonObject->TryGetNumberField("x", X) && JsonObject->TryGetNumberField("y", Y) && JsonObject->TryGetNumberField("z", Z)
			&& JsonObject->TryGetNumberField("yaw", Yaw) && JsonObject->TryGetNumberField("pitch", Pitch))
		{
			X = -X;

			UE_LOG(LogTemp, Log, TEXT("Player location information: ServerID=%d X=%f Y=%f Z=%f Yaw=%f Pitch=%f"), ServerID, X, Y, Z, Yaw, Pitch);

			// TODO: kick em if wrong server? (can this even happen?)
			if (ServerID != AdhocGameState->GetServerID())
			{
				UE_LOG(LogTemp, Log, TEXT("Location information server %d does not match this server %d!"), ServerID, AdhocGameState->GetServerID());
			}

			IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(PlayerController);
			AdhocPlayerControllerInterface->SetImmediateSpawnTransform(FTransform(FRotator(Pitch, Yaw, 0), FVector(X, Y, Z)));
		}
	}

	APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
	check(PlayerState);

	PlayerState->SetPlayerName(UserName);

	UAdhocPlayerStateComponent* AdhocPlayerStateComponent =
		CastChecked<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));

	AdhocPlayerStateComponent->SetUserID(UserID);
	AdhocPlayerStateComponent->SetFactionIndex(UserFactionIndex);

	IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(PlayerController);

	AdhocPlayerControllerInterface->SetFactionIndex(UserFactionIndex);
	AdhocPlayerControllerInterface->SetToken(UserToken);

	// if player is controlling a character currently, flip the faction there too
	APawn* Pawn = PlayerController->GetPawn();
	if (Pawn)
	{
		UAdhocPawnComponent* AdhocPawnComponent = Cast<UAdhocPawnComponent>(Pawn->GetComponentByClass(UAdhocPawnComponent::StaticClass()));
		if (AdhocPawnComponent)
		{
			AdhocPawnComponent->SetFactionIndex(UserFactionIndex);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Player login success: UserID=%d UserName=%s UserFactionID=%d UserFactionIndex=%d UserToken=%s"), UserID, *UserName,
		UserFactionID, UserFactionIndex, *UserToken);

	TOptional<FTransform> ImmediateSpawnTransform = AdhocPlayerControllerInterface->GetImmediateSpawnTransform();
	if (ImmediateSpawnTransform.IsSet() && PlayerController->HasActorBegunPlay())
	{
		const FVector ImmediateSpawnLocation = ImmediateSpawnTransform->GetLocation();
		const FRotator ImmediateSpawnRotation = ImmediateSpawnTransform->GetRotation().Rotator();

		UE_LOG(LogTemp, Log, TEXT("Immediately spawning new player: X=%f Y=%f Z=%f Yaw=%f Pitch=%f Roll=%f"), ImmediateSpawnLocation.X,
			ImmediateSpawnLocation.Y, ImmediateSpawnLocation.Z, ImmediateSpawnRotation.Yaw, ImmediateSpawnRotation.Pitch, ImmediateSpawnRotation.Roll);

		GameMode->RestartPlayer(PlayerController);
	}
}

void UAdhocGameModeComponent::SubmitNavigate(APlayerController* PlayerController, const int32 AreaID) const
{
	const APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
	check(PlayerState);
	const UAdhocPlayerStateComponent* AdhocPlayerState =
		Cast<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));
	const APawn* PlayerPawn = PlayerController->GetPawn();
	check(PlayerPawn);

	const FVector& PlayerLocation = PlayerPawn->GetActorLocation();
	const FRotator& PlayerRotation = PlayerPawn->GetViewRotation();

	FString JsonString;
	const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	Writer->WriteObjectStart();
	Writer->WriteValue("userId", AdhocPlayerState->GetUserID());
	Writer->WriteValue("areaId", AreaID);
	Writer->WriteValue("x", -PlayerLocation.X);
	Writer->WriteValue("y", PlayerLocation.Y);
	Writer->WriteValue("z", PlayerLocation.Z);
	Writer->WriteValue("yaw", PlayerRotation.Yaw);
	Writer->WriteValue("pitch", PlayerRotation.Pitch);
	Writer->WriteObjectEnd();
	Writer->Close();

	const auto& Request = Http->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnNavigateResponse, PlayerController);
	const FString URL =
		FString::Printf(TEXT("http://%s:80/api/servers/%d/users/%d/navigate"), *ManagerHost, AdhocGameState->GetServerID(), AdhocPlayerState->GetUserID());
	Request->SetURL(URL);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// Request->SetHeader(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
	Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
	Request->SetContentAsString(JsonString);

	UE_LOG(LogTemp, Log, TEXT("POST %s: %s"), *URL, *JsonString);
	Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnNavigateResponse(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, APlayerController* PlayerController) const
{
	UE_LOG(LogTemp, Log, TEXT("Navigate response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

	if (!bWasSuccessful || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Navigate response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		return;
	}

	const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to deserialize navigate response: %s"), *Response->GetContentAsString());
		return;
	}

	const int64 ServerID = JsonObject->GetIntegerField("serverId");
	// const FString ServerDomain = JsonObject->GetStringField("serverDomain");
	const FString IP = JsonObject->GetStringField("ip");
	const int32 Port = JsonObject->GetIntegerField("port");
	const FString WebSocketURL = JsonObject->GetStringField("webSocketUrl");
	if (ServerID <= 0 || IP.IsEmpty() || Port <= 0 || WebSocketURL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to process navigate response: %s"), *Response->GetContentAsString());
		return;
	}

	FString URL = FString::Printf(TEXT("%s:%d"), *IP, Port);

	URL += FString::Printf(TEXT("?WebSocketURL=%s"), *WebSocketURL);

	const IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = CastChecked<IAdhocPlayerControllerInterface>(PlayerController);

	// NavigateURL += FString::Printf(TEXT("?ServerID=%lld"), ServerID);
	// NavigateURL += FString::Printf(TEXT("?ServerDomain=%s"), *ServerDomain);

	if (AdhocPlayerControllerInterface->GetUserID() != -1)
	{
		URL += FString::Printf(TEXT("?UserID=%d"), AdhocPlayerControllerInterface->GetUserID());
	}
	if (!AdhocPlayerControllerInterface->GetToken().IsEmpty())
	{
		URL += FString::Printf(TEXT("?Token=%s"), *AdhocPlayerControllerInterface->GetToken());
	}
	UE_LOG(LogTemp, Log, TEXT("Player %s navigate to %s"), *PlayerController->GetPlayerState<APlayerState>()->GetPlayerName(), *URL);
	APawn* PlayerPawn = PlayerController->GetPawn();
	if (PlayerPawn)
	{
		PlayerController->UnPossess();
		PlayerPawn->Destroy();
	}
	PlayerController->ClientTravel(URL, TRAVEL_Absolute);
}

void UAdhocGameModeComponent::OnTimer_ServerPawns() const
{
	if (StompClient && !StompClient->IsConnected())
	{
		return;
	}

	FString JsonString;
	const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());

	Writer->WriteArrayStart(TEXT("pawns"));

	UWorld* World = GetWorld();
	check(World);

	int32 PawnIndex = 0;
	for (TActorIterator<APawn> It = TActorIterator<APawn>(World); It; ++It)
	{
		const UAdhocPawnComponent* AdhocPawnComponent = Cast<UAdhocPawnComponent>((*It)->GetComponentByClass(UAdhocPawnComponent::StaticClass()));
		if (!AdhocPawnComponent)
		{
			continue;
		}

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("uuid"), AdhocPawnComponent->GetUUID().ToString(EGuidFormats::DigitsWithHyphens));
		Writer->WriteValue(TEXT("name"), AdhocPawnComponent->GetFriendlyName());
		Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());
		Writer->WriteValue(TEXT("index"), PawnIndex);
		Writer->WriteValue(TEXT("factionIndex"), AdhocPawnComponent->GetFactionIndex());
		Writer->WriteValue(TEXT("x"), -(*It)->GetActorLocation().X);
		Writer->WriteValue(TEXT("y"), (*It)->GetActorLocation().Y);
		Writer->WriteValue(TEXT("z"), (*It)->GetActorLocation().Z);

		const IAdhocPlayerControllerInterface* AdhocPlayerControllerInterface = (*It)->GetController<IAdhocPlayerControllerInterface>();
		if (!AdhocPlayerControllerInterface || AdhocPlayerControllerInterface->GetUserID() == -1)
		{
			Writer->WriteNull(TEXT("userId"));
		}
		else
		{
			Writer->WriteValue(TEXT("userId"), AdhocPlayerControllerInterface->GetUserID());
		}

		Writer->WriteObjectEnd();

		PawnIndex++;
	}

	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();

	UE_LOG(LogTemp, Log, TEXT("Sending: %s"), *JsonString);
	StompClient->Send("/app/ServerPawns", JsonString);
}

#endif
