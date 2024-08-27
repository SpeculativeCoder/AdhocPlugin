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

#include "Game/AdhocGameModeComponent.h"

#include "AIController.h"
#include "Area/AdhocAreaComponent.h"
#include "Area/AdhocAreaVolume.h"
#include "Faction/AdhocFactionState.h"
#include "Game/AdhocGameStateComponent.h"
#include "Pawn/AdhocPawnComponent.h"
#include "Player/AdhocPlayerControllerComponent.h"
#include "Player/AdhocPlayerStateComponent.h"
#include "Server/AdhocServerState.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "AI/AdhocAIControllerComponent.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Online/WebSockets/Public/WebSocketsModule.h"
#include "Runtime/Online/Stomp/Public/IStompMessage.h"
#include "Runtime/Online/Stomp/Public/StompModule.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "Misc/Base64.h"
#include "Objective/AdhocObjectiveComponent.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogAdhocGameModeComponent)

UAdhocGameModeComponent::UAdhocGameModeComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bWantsInitializeComponent = true;

    PrimaryComponentTick.bCanEverTick = false;
}

void UAdhocGameModeComponent::InitializeComponent()
{
    Super::InitializeComponent();

    int64 ServerID = 1;
    int64 RegionID = 1;
    FParse::Value(FCommandLine::Get(), TEXT("ServerID="), ServerID);
    FParse::Value(FCommandLine::Get(), TEXT("RegionID="), RegionID);

    UE_LOG(LogAdhocGameModeComponent, Log, TEXT("InitializeComponent: ServerID=%d RegionID=%d"), ServerID, RegionID);

    GameMode = GetOwner<AGameModeBase>();
    check(GameMode);

    GameState = GameMode->GetGameState<AGameStateBase>();
    check(GameState);

    AdhocGameState = Cast<UAdhocGameStateComponent>(GameState->GetComponentByClass(UAdhocGameStateComponent::StaticClass()));
    check(AdhocGameState);

    AdhocGameState->SetServerID(ServerID);
    AdhocGameState->SetRegionID(RegionID);

    InitFactionStates();
    InitAreaStates();
    InitServerStates();
    InitObjectiveStates();
#if WITH_ADHOC_PLUGIN_EXTRA
    InitStructureStates();
#endif

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    FParse::Value(FCommandLine::Get(), TEXT("PrivateIP="), PrivateIP);
    FParse::Value(FCommandLine::Get(), TEXT("ManagerHost="), ManagerHost);

    UE_LOG(LogAdhocGameModeComponent, Log, TEXT("InitializeComponent: PrivateIP=%s ManagerHost=%s"), *PrivateIP, *ManagerHost);

    BasicAuthPassword = FPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_BASIC_AUTH_PASSWORD"));
    if (BasicAuthPassword.IsEmpty())
    {
        const UWorld* World = GetWorld();
        check(World);

        if (!InEditor())
        {
            UE_LOG(LogAdhocGameModeComponent, Error, TEXT("SERVER_BASIC_AUTH_PASSWORD environment variable not set - should shut down server"));
            ShutdownIfNotInEditor();
        }
        else
        {
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("SERVER_BASIC_AUTH_PASSWORD environment variable not set - we are in editor so using default"));
            BasicAuthPassword = DefaultPlayInEditorBasicAuthPassword;
        }
    }

    const FString EncodedUsernameAndPassword = FBase64::Encode(FString::Printf(TEXT("%s:%s"), *BasicAuthUsername, *BasicAuthPassword));
    BasicAuthHeaderValue = FString::Printf(TEXT("Basic %s"), *EncodedUsernameAndPassword);

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
}

void UAdhocGameModeComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogAdhocGameModeComponent, Log, TEXT("BeginPlay: NetMode=%d"), GetNetMode());

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    if (GetNetMode() != NM_Client)
    {
        GetWorld()->GetTimerManager().SetTimer(TimerHandle_ServerPawns, this, &UAdhocGameModeComponent::OnTimer_ServerPawns, 5, true, 5);

#if WITH_ADHOC_PLUGIN_EXTRA
        GetWorld()->GetTimerManager().SetTimer(TimerHandle_RecentEmissions, this, &UAdhocGameModeComponent::OnTimer_RecentEmissions, 2, true, 2);
#endif

        // initiate stomp connection - only once we are sure this connection is established
        // will we then do an initial push/refresh all world state via REST calls etc.
        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Initializing Stomp connection..."));

        const FString& StompURL = FString::Printf(TEXT("ws://%s:80/ws/stomp/server"), *ManagerHost);
        StompClient = Stomp->CreateClient(StompURL, *BasicAuthHeaderValue);

        StompClient->OnConnected().AddUObject(this, &UAdhocGameModeComponent::OnStompConnected);
        StompClient->OnConnectionError().AddUObject(this, &UAdhocGameModeComponent::OnStompConnectionError);
        StompClient->OnError().AddUObject(this, &UAdhocGameModeComponent::OnStompError);
        StompClient->OnClosed().AddUObject(this, &UAdhocGameModeComponent::OnStompClosed);

        // FStompHeader StompHeader;
        // StompHeader.Add(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
        // StompHeader.Add(TEXT("_csrf"), TEXT("SERVER"));
        //  TODO: why do we need server to send us pongs rather than us pinging them ?????
        // static const FName HeartbeatHeader(TEXT("heart-beat"));
        // StompHeader.Add(HeartbeatHeader, TEXT("0,15000"));

        StompClient->Connect(); // StompHeader);
    }
#endif
}

void UAdhocGameModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    if (StompClient->IsConnected())
    {
        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Stopping Stomp connection..."));
        StompClient->Disconnect();
    }
#endif
}

void UAdhocGameModeComponent::InitFactionStates() const
{
    // set up some default factions (will be overridden once we contact the manager server)
    TArray<FAdhocFactionState> Factions;
    Factions.SetNum(3);

    Factions[0].ID = 1;
    Factions[0].Index = 0;
    Factions[0].Name = TEXT("Alpha");
    Factions[0].Color = FColor::FromHex(TEXT("#FFFF00"));
    Factions[0].Score = 0;

    Factions[1].ID = 2;
    Factions[1].Index = 1;
    Factions[1].Name = TEXT("Beta");
    Factions[1].Color = FColor::FromHex(TEXT("#00AAFF"));
    Factions[1].Score = 0;

    Factions[2].ID = 3;
    Factions[2].Index = 2;
    Factions[2].Name = TEXT("Gamma");
    Factions[2].Color = FColor::FromHex(TEXT("#AA00FF"));
    Factions[2].Score = 0;

    // Factions[3].ID = 4;
    // Factions[3].Index = 3;
    // Factions[3].Name = TEXT("Delta");
    // Factions[3].Color = FColor::FromHex(TEXT("#FF2200"));
    // Factions[3].Score = 0;

    AdhocGameState->SetFactions(Factions);
}

void UAdhocGameModeComponent::InitAreaStates() const
{
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
        Area.RegionID = AdhocGameState->GetRegionID();
        Area.Index = AreaVolume->GetAdhocArea()->GetAreaIndex();
        Area.Name = AreaVolume->GetAdhocArea()->GetFriendlyName();
        Area.Location = AreaVolume->GetActorLocation();
        Area.Size = AreaVolume->GetActorScale() * 200;
        Area.ServerID = AdhocGameState->GetServerID();
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
    // Areas.Reset();
    // ActiveAreaIndexes.Reset();

    if (Areas.Num() <= 0)
    {
        FAdhocAreaState Area;
        // Area.ID = AreaVolume->GetAreaID();
        Area.RegionID = AdhocGameState->GetRegionID();
        Area.Index = 0;
        Area.Name = TEXT("A");
        Area.ServerID = AdhocGameState->GetServerID();
        Area.Location = FVector::ZeroVector;
        // TODO: determine map size based on geometry
        Area.Size = FVector(10000, 10000, 10000);
        Areas.Add(Area);

        ActiveAreaIndexes.AddUnique(Area.Index);
    }

    AdhocGameState->SetAreas(Areas);
    // AdhocGameState->SetActiveAreaIDs(ActiveAreaIDs);
    AdhocGameState->SetActiveAreaIndexes(ActiveAreaIndexes);
}

void UAdhocGameModeComponent::InitServerStates() const
{
    // assume this is the only server until we get the server(s) info
    TArray<FAdhocServerState> Servers;
    Servers.SetNum(1);

    Servers[0].ID = AdhocGameState->GetServerID();
    Servers[0].RegionID = AdhocGameState->GetRegionID();
    // Servers[0].AreaIDs = ActiveAreaIDs;
    Servers[0].AreaIndexes = AdhocGameState->GetActiveAreaIndexes();
    // Servers[0].Name = TEXT("Server");
    // Servers[0].Status = TEXT("STARTED");
    Servers[0].bEnabled = true;
    Servers[0].bActive = true;
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    Servers[0].PrivateIP = PrivateIP;
    Servers[0].PublicIP = PrivateIP;
    Servers[0].PublicWebSocketPort = 8889; // TODO: get from driver
#endif

    AdhocGameState->SetServers(Servers);
}

void UAdhocGameModeComponent::InitObjectiveStates() const
{
    // set up the known objectives
    int32 ObjectiveIndex = 0;
    TArray<FAdhocObjectiveState> Objectives;

    for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
    {
        const AActor* Actor = *ActorIter;
        UAdhocObjectiveComponent* AdhocObjective = Cast<UAdhocObjectiveComponent>(Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
        if (!AdhocObjective)
        {
            continue;
        }

        // ObjectiveActor->SetObjectiveID(NextObjectiveIndex + 1);
        AdhocObjective->SetObjectiveIndex(ObjectiveIndex);

        FAdhocObjectiveState Objective;
        // Objective.ID = ObjectiveActor->GetObjectiveID();
        Objective.RegionID = AdhocGameState->GetRegionID();
        Objective.Index = AdhocObjective->GetObjectiveIndex();
        Objective.Name = AdhocObjective->GetFriendlyName();
        Objective.Location = Actor->GetActorLocation();
        Objective.InitialFactionIndex = AdhocObjective->GetInitialFactionIndex();
        Objective.FactionIndex = AdhocObjective->GetFactionIndex();
        Objective.AreaIndex = AdhocObjective->GetAreaIndexSafe();
        Objectives.Add(Objective);

        ObjectiveIndex++;
    }

    AdhocGameState->SetObjectives(Objectives);

    for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
    {
        const AActor* Actor = *ActorIter;
        UAdhocObjectiveComponent* AdhocObjective = Cast<UAdhocObjectiveComponent>(Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
        if (!AdhocObjective)
        {
            continue;
        }

        FAdhocObjectiveState* Objective = AdhocGameState->FindObjectiveByRegionIDAndIndex(AdhocGameState->GetRegionID(), AdhocObjective->GetObjectiveIndex());
        check(Objective);

        // TArray<int64> LinkedObjectiveIDs;
        TArray<int32> LinkedObjectiveIndexes;
        for (auto& LinkedObjectiveActor : AdhocObjective->GetLinkedObjectives())
        {
            const UAdhocObjectiveComponent* LinkedAdhocObjective = Cast<UAdhocObjectiveComponent>(LinkedObjectiveActor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
            check(LinkedAdhocObjective);

            // LinkedObjectiveIDs.AddUnique(LinkedObjectiveActor->GetObjectiveID());
            LinkedObjectiveIndexes.AddUnique(LinkedAdhocObjective->GetObjectiveIndex());
        }
        // Objective->LinkedObjectiveIDs = LinkedObjectiveIDs;
        Objective->LinkedObjectiveIndexes = LinkedObjectiveIndexes;
    }
}

void UAdhocGameModeComponent::PostLogin(const APlayerController* PlayerController)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: PlayerController=%s"), *PlayerController->GetName());

    const UNetConnection* NetConnection = PlayerController->GetNetConnection();
    const FString& RequestURL = NetConnection ? NetConnection->RequestURL : TEXT(""); // TODO (listen mode does not have connection)
    UE_LOG(LogAdhocGameModeComponent, Log, TEXT("PostLogin: RequestURL=%s"), *RequestURL);

    int32 QuestionMarkPos = -1;
    RequestURL.FindChar(TCHAR('?'), QuestionMarkPos);
    const FString Options = QuestionMarkPos == -1 ? RequestURL : RequestURL.RightChop(QuestionMarkPos);
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: Options=%s"), *Options);

    const int64 UserID = UGameplayStatics::GetIntOption(Options, TEXT("UserID"), -1);
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: UserID=%d"), UserID);
    const int64 FactionID = UGameplayStatics::GetIntOption(Options, TEXT("FactionID"), -1);
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: FactionID=%d"), FactionID);
    const FString Token = UGameplayStatics::ParseOption(Options, TEXT("Token"));
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: Token=%s"), *Token);

    UAdhocPlayerControllerComponent* AdhocPlayerController = Cast<UAdhocPlayerControllerComponent>(PlayerController->GetComponentByClass(UAdhocPlayerControllerComponent::StaticClass()));
    check(AdhocPlayerController);

    // if the user logging in has provided a FactionID, assume that faction for now, otherwise fall back to a random faction
    int32 FactionIndex = -1;

    if (FactionID != -1)
    {
        const FAdhocFactionState* FactionState = AdhocGameState->FindFactionByID(FactionID);
        if (FactionState)
        {
            FactionIndex = FactionState->Index;
            UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: Using faction specified in URL. FactionIndex=%d"), FactionID, FactionIndex);
        }
    }

    if (FactionIndex == -1)
    {
        FactionIndex = FMath::RandRange(0, AdhocGameState->GetNumFactions() - 1);
        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: Picked random faction. FactionIndex=%d"), FactionID, FactionIndex);
    }

    // AdhocPlayerController->SetFriendlyName(TEXT("Anon"));
    AdhocPlayerController->SetFactionIndex(FactionIndex);
    AdhocPlayerController->SetUserID(UserID);
    AdhocPlayerController->SetToken(Token);

    APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
    check(PlayerState);

    PlayerState->SetPlayerName(AdhocPlayerController->GetFriendlyName());

    UAdhocPlayerStateComponent* AdhocPlayerState = Cast<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));
    check(AdhocPlayerState);

    // NOTE: user id in player state will only be set once we have verified the user
    AdhocPlayerState->SetFactionIndex(AdhocPlayerController->GetFactionIndex());

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PostLogin: Submitting human user join: UserID=%d Token=%s"), AdhocPlayerController->GetUserID(), *AdhocPlayerController->GetToken());

    SubmitUserJoin(AdhocPlayerController);
#endif
}

void UAdhocGameModeComponent::Logout(const AController* Controller)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Logout: Controller=%s"), *Controller->GetName());
}

void UAdhocGameModeComponent::BotJoin(const AAIController* BotController)
{
    UE_LOG(LogAdhocGameModeComponent, VeryVerbose, TEXT("BotJoin: BotController=%s"), *BotController->GetName());

    UAdhocAIControllerComponent* AdhocBotController = Cast<UAdhocAIControllerComponent>(BotController->GetComponentByClass(UAdhocAIControllerComponent::StaticClass()));
    check(AdhocBotController);

    UE_LOG(LogAdhocGameModeComponent, VeryVerbose, TEXT("BotJoin: Submitting bot user join: factionIndex=%d"), AdhocBotController->GetFactionIndex());

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    if (InEditor() && !StompClient->IsConnected())
    {
        OnUserJoinSuccess(AdhocBotController);
    }
    else
    {
        SubmitUserJoin(AdhocBotController);
    }
#endif
}

void UAdhocGameModeComponent::BotLeave(const AAIController* BotController)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("BotLeave: BotController=%s"), *BotController->GetName());
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

        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Sending: %s"), *JsonString);
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
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnObjectiveTakenEvent: OutObjective.ID=%d Faction.ID=%d"), OutObjective.ID, Faction.ID);

    OutObjective.FactionID = Faction.ID;
    OutObjective.FactionIndex = Faction.Index;
    // UE_LOG(LogAdhocGameModeComponent, Warning,
    // 	TEXT("OnObjectiveTakenEvent: Objective.ID=%d Objective.Index=%d Objective.FactionID=%d Objective.FactionIndex=%d"),
    // 	Objective->ID, Objective->Index, Objective->FactionID, Objective->FactionIndex);

    // flip the actor in the world to the appropriate faction
    for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
    {
        const AActor* Actor = *ActorIter;
        UAdhocObjectiveComponent* AdhocObjective = Cast<UAdhocObjectiveComponent>(Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
        if (!AdhocObjective)
        {
            continue;
        }

        if (AdhocObjective->GetObjectiveIndex() == OutObjective.Index)
        {
            AdhocObjective->SetFactionIndex(Faction.Index);
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
    UE_LOG(LogAdhocGameModeComponent, Error,
        TEXT("CreateStructure: PlayerPawn=%s UUID=%s Type=%s - Ignoring because structures require AdhocPlugin to be compiled with WITH_ADHOC_PLUGIN_EXTRA"),
        *PlayerPawn->GetName(), *UUID.ToString(EGuidFormats::DigitsWithHyphens), *Type);
}
#endif

void UAdhocGameModeComponent::UserDefeatedUser(AController* Controller, AController* DefeatedController) const
{
#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    if (StompClient && StompClient->IsConnected())
    {
        const UAdhocControllerComponent* AdhocController = CastChecked<UAdhocControllerComponent>(
            Controller->GetComponentByClass(UAdhocControllerComponent::StaticClass()));
        const UAdhocControllerComponent* DefeatedAdhocController = CastChecked<UAdhocControllerComponent>(
            DefeatedController->GetComponentByClass(UAdhocControllerComponent::StaticClass()));

        FString JsonString;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("eventType"), TEXT("ServerUserDefeatedUser"));
        Writer->WriteValue(TEXT("userId"), AdhocController->GetUserID());
        Writer->WriteValue(TEXT("defeatedUserId"), DefeatedAdhocController->GetUserID());
        Writer->WriteObjectEnd();
        Writer->Close();

        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Sending: %s"), *JsonString);
        StompClient->Send("/app/ServerUserDefeatedUser", JsonString);

        // TODO: trigger via event?
        OnUserDefeatedUserEvent(Controller, DefeatedController);
    }
    else
#endif
    {
        OnUserDefeatedUserEvent(Controller, DefeatedController);
    }
}

void UAdhocGameModeComponent::OnUserDefeatedUserEvent(AController* Controller, AController* DefeatedController) const
{
    OnUserDefeatedUserEventDelegate.Broadcast(Controller, DefeatedController);
}

void UAdhocGameModeComponent::PlayerEnterArea(APlayerController* PlayerController, const int32 AreaIndex) const
{
    const APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
    check(PlayerState);

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Player enter area: PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);

    if (AdhocGameState->GetActiveAreaIndexes().Contains(AreaIndex))
    {
        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Area already active on this server: PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
        return;
    }

    const FAdhocAreaState* Area = AdhocGameState->FindAreaByIndex(AreaIndex);
    if (!Area)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Can't navigate as could not find area state! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
        return;
    }

    const int32 AreaID = Area->ID;
    if (AreaID == -1)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Can't navigate as area ID is not available! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
        return;
    }

    if (Cast<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()))->GetUserID() == -1)
    {
        UE_LOG(
            LogAdhocGameModeComponent, Warning, TEXT("Can't navigate as player does not have a user ID! PlayerName=%s AreaIndex=%d"), *PlayerState->GetPlayerName(), AreaIndex);
        return;
    }

    // TODO: check server enabled and active

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)
    UAdhocPlayerControllerComponent* AdhocPlayerController = CastChecked<UAdhocPlayerControllerComponent>(PlayerController->GetComponentByClass(UAdhocPlayerControllerComponent::StaticClass()));

    SubmitNavigate(AdhocPlayerController, AreaID);
#endif
}

#if WITH_SERVER_CODE && !defined(__EMSCRIPTEN__)

bool UAdhocGameModeComponent::InEditor() const
{
    const UWorld* World = GetWorld();
    check(World);

    return World->IsPlayInEditor() || World->IsEditorWorld();
}

void UAdhocGameModeComponent::ShutdownIfNotInEditor() const
{
    // UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);

    if (!InEditor())
    {
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}

void UAdhocGameModeComponent::KickPlayerIfNotInEditor(APlayerController* PlayerController, const FString& KickReason) const
{
    if (!InEditor())
    {
        GameMode->GameSession->KickPlayer(PlayerController, FText::FromString(KickReason));
    }
}

void UAdhocGameModeComponent::OnStompConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnStompConnected: ProtocolVersion=%s SessionId=%s ServerString=%s"), *ProtocolVersion, *SessionId, *ServerString);

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
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnStompClosed: Reason=%s"), *Reason);

    if (GetNetMode() == NM_DedicatedServer)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Stomp connection closed - should shut down server"));
        ShutdownIfNotInEditor();
    }
}

void UAdhocGameModeComponent::OnStompConnectionError(const FString& Error) const
{
    UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("OnStompClientConnectionError: Error=%s"), *Error);

    if (GetNetMode() == NM_DedicatedServer)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Stomp connection error - should shut down server"));
        ShutdownIfNotInEditor();
    }
}

void UAdhocGameModeComponent::OnStompError(const FString& Error) const
{
    UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("OnStompClientError: Error=%s"), *Error);

    if (GetNetMode() == NM_DedicatedServer)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Stomp error - should shut down server"));
        ShutdownIfNotInEditor();
    }
}

void UAdhocGameModeComponent::OnStompRequestCompleted(bool bSuccess, const FString& Error) const
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnStompRequestCompleted: bSuccess=%d Error=%s"), bSuccess, *Error);

    if (!bSuccess && GetNetMode() == NM_DedicatedServer)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Stomp request completed unsuccessfully - should shut down server"));
        ShutdownIfNotInEditor();
    }
}

void UAdhocGameModeComponent::OnStompSubscriptionEvent(const IStompMessage& Message)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnStompSubscriptionEvent: %s"), *Message.GetBodyAsString());

    const auto& Reader = TJsonReaderFactory<>::Create(Message.GetBodyAsString());
    TSharedPtr<FJsonObject> JsonObject;
    if (!FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize Stomp event: %s"), *Message.GetBodyAsString());
        return;
    }
    const FString EventType = JsonObject->GetStringField("eventType");
    // UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("OnStompSubscriptionEvent: eventType=%s"), *eventType);

    if (EventType.Equals(TEXT("ObjectiveTaken")))
    {
        const int32 EventObjectiveID = JsonObject->GetIntegerField("objectiveId");
        const int32 EventFactionID = JsonObject->GetIntegerField("factionId");

        FAdhocObjectiveState* Objective = AdhocGameState->FindObjectiveByID(EventObjectiveID);
        FAdhocFactionState* Faction = AdhocGameState->FindFactionByID(EventFactionID);
        if (!Objective || !Faction)
        {
            // TODO: if not got control points response yet?
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Invalid ObjectiveTaken ID(s): EventObjectiveID=%d EventFactionID=%d"), EventObjectiveID, EventFactionID);
            return;
        }

        OnObjectiveTakenEvent(*Objective, *Faction);
    }
    else if (EventType.Equals(TEXT("ServerUpdated")))
    {
        const int64 EventServerID = JsonObject->GetIntegerField("serverId");
        // const FString EventStatus = JsonObject->GetStringField("status");
        const bool EventEnabled = JsonObject->GetBoolField("enabled");
        const bool EventActive = JsonObject->GetBoolField("active");

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

        OnServerUpdatedEvent(EventServerID, EventRegionID, EventEnabled, EventActive, EventPrivateIP, EventPublicIP, EventServerPublicWebSocketPort, EventAreaIDs, EventAreaIndexes);
    }
    else if (EventType.Equals(TEXT("WorldUpdated")))
    {
        const TSharedPtr<FJsonObject>* WorldJsonObjectPtr;

        if (!JsonObject->TryGetObjectField(TEXT("world"), WorldJsonObjectPtr))
        {
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Invalid WorldUpdated event: Body=%s"), *Message.GetBodyAsString());
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
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Invalid StructureUpdated event: Body=%s"), *Message.GetBodyAsString());
            return;
        }

        ExtractStructureFromJsonObject(*StructureJsonObjectPtr, Structure);

        OnStructureCreatedEvent(Structure);
    }
    else if (EventType.Equals(TEXT("Emissions")))
    {
        const FString BaseTimestampString = JsonObject->GetStringField(TEXT("baseTimestamp"));
        FDateTime BaseTimestamp;
        FDateTime::ParseIso8601(*BaseTimestampString, BaseTimestamp); // TODO: error handling

        TArray<TSharedPtr<FJsonValue>> EmissionJsonValues = JsonObject->GetArrayField("emissions");

        TArray<FAdhocEmission> Emissions;
        Emissions.Reserve(EmissionJsonValues.Num());

        for (auto& EmissionJsonValue : EmissionJsonValues)
        {
            FAdhocEmission Emission;
            ExtractEmissionFromJsonObject(EmissionJsonValue->AsObject(), Emission);
            Emissions.Emplace(Emission);
        }

        OnEmissionsEvent(BaseTimestamp, Emissions);
    }
#endif
}

void UAdhocGameModeComponent::RetrieveFactions()
{
    const auto& Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnFactionsResponse);
    const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/factions"), *ManagerHost, AdhocGameState->GetServerID());
    Request->SetURL(URL);
    Request->SetVerb("GET");
    Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("GET %s"), *URL);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnFactionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Factions response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Factions response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    if (!FJsonSerializer::Deserialize(Reader, JsonValues))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize factions response: Content=%s"), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    TArray<FAdhocFactionState> Factions;
    Factions.SetNum(JsonValues.Num());

    for (int i = 0; i < JsonValues.Num(); i++)
    {
        const TSharedPtr<FJsonObject> JsonObject = JsonValues[i]->AsObject();
        Factions[i].ID = JsonObject->GetIntegerField("id");
        Factions[i].Index = JsonObject->GetIntegerField("index");
        Factions[i].Name = JsonObject->GetStringField("name");
        Factions[i].Color = FColor::FromHex(JsonObject->GetStringField("color"));
        Factions[i].Score = JsonObject->GetIntegerField("score");
    }

    AdhocGameState->SetFactions(Factions);
}

void UAdhocGameModeComponent::RetrieveServers()
{
    const auto& Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnServersResponse);
    const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/servers"), *ManagerHost, AdhocGameState->GetServerID());
    Request->SetURL(URL);
    Request->SetVerb("GET");
    Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("GET %s"), *URL);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnServersResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) const
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Servers response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Servers response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    if (!FJsonSerializer::Deserialize(Reader, JsonValues))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize get servers response: Content=%s"), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    TArray<FAdhocServerState> Servers;
    Servers.SetNum(JsonValues.Num());

    for (int i = 0; i < Servers.Num(); i++)
    {
        const TSharedPtr<FJsonObject> JsonObject = JsonValues[i]->AsObject();

        Servers[i].ID = JsonObject->GetIntegerField("id");
        // Servers[i].Name = JsonObject->GetStringField("name");
        // Servers[i].HostingType = NewServer->GetStringField("hostingType");
        // Servers[i].Status = JsonObject->GetStringField("status");
        Servers[i].bEnabled = JsonObject->GetBoolField("enabled");
        Servers[i].bActive = JsonObject->GetBoolField("active");
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

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PUT %s: %s"), *URL, *JsonString);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnAreasResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Areas response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Areas response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    if (!FJsonSerializer::Deserialize(Reader, JsonValues))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize areas response: %s"), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
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

// PUT OBJECTIVES (the map defines the objectives, and should override what is on the server, but the server will choose the IDs)
void UAdhocGameModeComponent::SubmitObjectives()
{
    FString JsonString;
    const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);

    Writer->WriteArrayStart();
    for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
    {
        const AActor* Actor = *ActorIter;
        UAdhocObjectiveComponent* AdhocObjective = Cast<UAdhocObjectiveComponent>(Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
        if (!AdhocObjective)
        {
            continue;
        }

        // ignore objectives outside an area
        if (AdhocObjective->GetAreaIndexSafe() == -1)
        {
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Objective %d has no area!"), AdhocObjective->GetObjectiveIndex());
            continue;
        }

        const FVector Location = Actor->GetActorLocation();
        const FVector Size = FVector(1, 1, 1); // ObjectiveActor->GetActorScale() * 200; // TODO

        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("regionId"), AdhocGameState->GetRegionID());
        Writer->WriteValue(TEXT("index"), AdhocObjective->GetObjectiveIndex());
        Writer->WriteValue(TEXT("name"), AdhocObjective->GetFriendlyName());
        Writer->WriteValue(TEXT("x"), static_cast<double>(-Location.X));
        Writer->WriteValue(TEXT("y"), static_cast<double>(Location.Y));
        Writer->WriteValue(TEXT("z"), static_cast<double>(Location.Z));
        Writer->WriteValue(TEXT("sizeX"), static_cast<double>(Size.X));
        Writer->WriteValue(TEXT("sizeY"), static_cast<double>(Size.Y));
        Writer->WriteValue(TEXT("sizeZ"), static_cast<double>(Size.Z));

        if (AdhocObjective->GetInitialFactionIndex() == -1)
        {
            Writer->WriteNull(TEXT("initialFactionIndex"));
        }
        else
        {
            Writer->WriteValue(TEXT("initialFactionIndex"), static_cast<double>(AdhocObjective->GetInitialFactionIndex()));
        }

        // if (AdhocObjective->GetFactionIndex() != -1)
        // {
        //     Writer->WriteNull(TEXT("factionIndex"));
        // }
        // else
        // {
        //     Writer->WriteValue(TEXT("factionIndex"), static_cast<double>(AdhocObjective->GetFactionIndex()));
        // }

        // Writer->WriteArrayStart(TEXT("linkedObjectiveIds"));
        // for (auto& LinkedObjectiveActor : ObjectiveActor->GetLinkedObjectiveInterfaces())
        // {
        // 	Writer->WriteValue(LinkedObjectiveActor->GetObjectiveID());
        // }
        // Writer->WriteArrayEnd();
        Writer->WriteArrayStart(TEXT("linkedObjectiveIndexes"));
        for (auto& LinkedObjectiveActor : AdhocObjective->GetLinkedObjectives())
        {
            const UAdhocObjectiveComponent* LinkedAdhocObjective = Cast<UAdhocObjectiveComponent>(LinkedObjectiveActor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
            check(LinkedAdhocObjective);

            // ignore objectives outside an area
            if (LinkedAdhocObjective->GetAreaIndexSafe() == -1)
            {
                UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Objective %d linked objective %d has no area!"), AdhocObjective->GetObjectiveIndex(), LinkedAdhocObjective->GetObjectiveIndex());
                continue;
            }

            Writer->WriteValue(LinkedAdhocObjective->GetObjectiveIndex());
        }
        Writer->WriteArrayEnd();

        // Writer->WriteValue(FString("areaId"), ObjectiveActor->GetAreaVolume()->GetAreaID());
        Writer->WriteValue(TEXT("areaIndex"), AdhocObjective->GetAreaIndexSafe());

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

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("PUT %s: %s"), *URL, *JsonString);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnObjectivesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Objectives response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Objectives response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    if (!FJsonSerializer::Deserialize(Reader, JsonValues))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize objectives response: %s"), *Response->GetContentAsString());
        ShutdownIfNotInEditor();
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
            for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
            {
                const AActor* Actor = *ActorIter;
                UAdhocObjectiveComponent* AdhocObjective = Cast<UAdhocObjectiveComponent>(Actor->GetComponentByClass(UAdhocObjectiveComponent::StaticClass()));
                if (!AdhocObjective)
                {
                    continue;
                }

                if (AdhocObjective->GetObjectiveIndex() == Objectives[i].Index)
                {
                    AdhocObjective->SetFactionIndex(Objectives[i].FactionIndex);
                }
            }
        }
    }

    AdhocGameState->SetObjectives(Objectives);

#if WITH_ADHOC_PLUGIN_EXTRA
    SubmitStructures();
#else
    ServerStarted();
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

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Sending: %s"), *JsonString);
    StompClient->Send("/app/ServerStarted", JsonString);
}

void UAdhocGameModeComponent::OnServerUpdatedEvent(int32 EventServerID, int32 EventRegionID, const bool bEventEnabled, const bool bEventActive, const FString& EventPrivateIP,
    const FString& EventPublicIP, int32 EventPublicWebSocketPort, const TArray<int64>& EventAreaIDs, const TArray<int32>& EventAreaIndexes) const
{
    FAdhocServerState* Server = AdhocGameState->FindOrInsertServerByID(EventServerID);
    if (!Server)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to find or insert server with ID %d!"), EventServerID);
        return;
    }

    Server->ID = EventServerID;
    Server->RegionID = EventRegionID;
    // Server->Status = EventStatus;
    Server->bEnabled = bEventEnabled;
    Server->bActive = bEventActive;
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
        UE_LOG(LogAdhocGameModeComponent, Error, TEXT("Server active region does not match! ServerID=%d AdhocGameState->RegionID=%d RegionID=%d"), AdhocGameState->GetServerID(),
            AdhocGameState->GetRegionID(), RegionID);
    }
    else
    {
        for (auto& AreaIndex : AreaIndexes)
        {
            UE_LOG(LogAdhocGameModeComponent, Log, TEXT("This server has active area index %d"), AreaIndex);
        }
        AdhocGameState->SetActiveAreaIndexes(AreaIndexes);
    }
}

void UAdhocGameModeComponent::OnWorldUpdatedEvent(int64 WorldWorldID, int64 WorldVersion, const TArray<FString>& WorldManagerHosts)
{
    ManagerHosts = WorldManagerHosts;
}

void UAdhocGameModeComponent::SubmitUserJoin(UAdhocControllerComponent* AdhocController)
{
    FString JsonString;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());

    if (AdhocController->GetUserID() != -1)
    {
        Writer->WriteValue(TEXT("userId"), AdhocController->GetUserID());
    }

    if (AdhocController->GetFactionIndex() != -1)
    {
        const FAdhocFactionState& AdhocFactionState = AdhocGameState->GetFaction(AdhocController->GetFactionIndex());
        if (AdhocFactionState.ID != -1)
        {
            Writer->WriteValue(TEXT("factionId"), AdhocFactionState.ID);
        }
    }

    // TODO
    const UAdhocPlayerControllerComponent* AdhocPlayerController = Cast<UAdhocPlayerControllerComponent>(AdhocController);
    Writer->WriteValue(TEXT("human"), !!AdhocPlayerController);

    if (AdhocPlayerController && !AdhocPlayerController->GetToken().IsEmpty())
    {
        Writer->WriteValue(TEXT("token"), *AdhocPlayerController->GetToken());
    }

    Writer->WriteObjectEnd();
    Writer->Close();

    const auto& Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnUserJoinResponse, AdhocController, true);
    const FString URL = FString::Printf(TEXT("http://%s:80/api/servers/%d/userJoin"), *ManagerHost, AdhocGameState->GetServerID());
    Request->SetURL(URL);
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    // Request->SetHeader(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
    Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
    Request->SetContentAsString(JsonString);

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("POST %s: %s"), *URL, *JsonString);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnUserJoinResponse(
    FHttpRequestPtr Request, const FHttpResponsePtr Response, const bool bWasSuccessful, UAdhocControllerComponent* AdhocController, const bool bKickOnFailure)
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("User join response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    AController* Controller = AdhocController->GetOwner<AController>();
    APlayerController* PlayerController = Cast<APlayerController>(Controller);

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("User join response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        if (PlayerController && bKickOnFailure)
        {
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Login failure - should kick player"));
            KickPlayerIfNotInEditor(PlayerController, TEXT("Login failure"));
        }

        // TODO
        OnUserJoinFailure(AdhocController);

        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TSharedPtr<FJsonObject> JsonObject;
    if (!FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize user join response: %s"), *Response->GetContentAsString());
        if (PlayerController && bKickOnFailure)
        {
            UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Login failure - should kick player"));
            KickPlayerIfNotInEditor(PlayerController, TEXT("Login failure"));
        }

        // TODO
        OnUserJoinFailure(AdhocController);

        return;
    }

    const int64 UserID = JsonObject->GetIntegerField("id");
    const int64 UserFactionID = JsonObject->GetIntegerField("factionId");
    const FString UserName = JsonObject->GetStringField("name");
    const FString UserToken = JsonObject->GetStringField("token");

    int64 ServerID;
    if (JsonObject->TryGetNumberField("serverId", ServerID))
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

            UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("User location information: ServerID=%d X=%f Y=%f Z=%f Yaw=%f Pitch=%f"), ServerID, X, Y, Z, Yaw, Pitch);

            // TODO: kick em if wrong server? (can this even happen?)
            if (ServerID != AdhocGameState->GetServerID())
            {
                UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Location information server %d does not match this server %d!"), ServerID, AdhocGameState->GetServerID());
            }

            AdhocController->SetImmediateSpawnTransform(FTransform(FRotator(Pitch, Yaw, 0), FVector(X, Y, Z)));
        }
    }

    int32 FactionIndex = -1;
    if (UserFactionID != -1)
    {
        const FAdhocFactionState* FactionState = AdhocGameState->FindFactionByID(UserFactionID);
        if (FactionState)
        {
            FactionIndex = FactionState->Index;
        }
    }

    AdhocController->SetFriendlyName(UserName);
    AdhocController->SetFactionIndex(FactionIndex);

    UAdhocPlayerControllerComponent* AdhocPlayerController = Cast<UAdhocPlayerControllerComponent>(AdhocController);
    if (AdhocPlayerController)
    {
        AdhocPlayerController->SetToken(UserToken);
    }

    UAdhocAIControllerComponent* AdhocBotControllerComponent = Cast<UAdhocAIControllerComponent>(AdhocController);
    if (AdhocBotControllerComponent)
    {
        AdhocBotControllerComponent->SetUserID(UserID);
    }

    APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>();
    if (PlayerState)
    {
        PlayerState->SetPlayerName(UserName);

        UAdhocPlayerStateComponent* AdhocPlayerState =
            CastChecked<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));

        AdhocPlayerState->SetUserID(UserID);
        AdhocPlayerState->SetFactionIndex(FactionIndex);
    }

    // if controller is controlling a pawn currently, flip the faction there too
    const APawn* Pawn = Controller->GetPawn();
    if (Pawn)
    {
        UAdhocPawnComponent* AdhocPawn = Cast<UAdhocPawnComponent>(Pawn->GetComponentByClass(UAdhocPawnComponent::StaticClass()));
        if (AdhocPawn)
        {
            AdhocPawn->SetFriendlyName(UserName);
            AdhocPawn->SetFactionIndex(FactionIndex);
        }
    }

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("User join success: UserID=%d UserName=%s UserFactionID=%d FactionIndex=%d UserToken=%s"), UserID, *UserName, UserFactionID, FactionIndex, *UserToken);

    TOptional<FTransform> ImmediateSpawnTransform = AdhocController->GetImmediateSpawnTransform();
    if (ImmediateSpawnTransform.IsSet())
    {
        const FVector ImmediateSpawnLocation = ImmediateSpawnTransform->GetLocation();
        const FRotator ImmediateSpawnRotation = ImmediateSpawnTransform->GetRotation().Rotator();

        UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Immediately spawning location: X=%f Y=%f Z=%f Yaw=%f Pitch=%f Roll=%f"), ImmediateSpawnLocation.X,
            ImmediateSpawnLocation.Y, ImmediateSpawnLocation.Z, ImmediateSpawnRotation.Yaw, ImmediateSpawnRotation.Pitch, ImmediateSpawnRotation.Roll);
    }

    OnUserJoinSuccess(AdhocController);
}

void UAdhocGameModeComponent::OnUserJoinSuccess(const UAdhocControllerComponent* AdhocController) const
{
    AController* Controller = AdhocController->GetController();
    check(Controller);

    // TODO: move more code in here

    OnUserJoinSuccessDelegate.Broadcast(Controller);
}

void UAdhocGameModeComponent::OnUserJoinFailure(const UAdhocControllerComponent* AdhocController) const
{
    AController* Controller = AdhocController->GetController();
    check(Controller);

    OnUserJoinFailureDelegate.Broadcast(Controller);
}

void UAdhocGameModeComponent::SubmitNavigate(UAdhocPlayerControllerComponent* AdhocPlayerController, const int32 AreaID) const
{
    const APlayerController* PlayerController = AdhocPlayerController->GetOwner<APlayerController>();
    check(PlayerController);
    const APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
    check(PlayerState);
    const UAdhocPlayerStateComponent* AdhocPlayerState = CastChecked<UAdhocPlayerStateComponent>(PlayerState->GetComponentByClass(UAdhocPlayerStateComponent::StaticClass()));
    const APawn* PlayerPawn = PlayerController->GetPawn();
    check(PlayerPawn);

    const FVector& PlayerLocation = PlayerPawn->GetActorLocation();
    const FRotator& PlayerRotation = PlayerPawn->GetViewRotation();

    FString JsonString;
    const auto& Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    Writer->WriteObjectStart();
    Writer->WriteValue("userId", AdhocPlayerState->GetUserID());
    Writer->WriteValue("sourceServerId", AdhocGameState->GetServerID());
    Writer->WriteValue("destinationAreaId", AreaID);
    Writer->WriteValue("x", -PlayerLocation.X);
    Writer->WriteValue("y", PlayerLocation.Y);
    Writer->WriteValue("z", PlayerLocation.Z);
    Writer->WriteValue("yaw", PlayerRotation.Yaw);
    Writer->WriteValue("pitch", PlayerRotation.Pitch);
    Writer->WriteObjectEnd();
    Writer->Close();

    const auto& Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UAdhocGameModeComponent::OnNavigateResponse, AdhocPlayerController);
    const FString URL =
        FString::Printf(TEXT("http://%s:80/api/servers/%d/userAutoNavigate"), *ManagerHost, AdhocGameState->GetServerID(), AdhocPlayerState->GetUserID());
    Request->SetURL(URL);
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    // Request->SetHeader(TEXT("X-CSRF-TOKEN"), TEXT("SERVER"));
    Request->SetHeader(BasicAuthHeaderName, BasicAuthHeaderValue);
    Request->SetContentAsString(JsonString);

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("POST %s: %s"), *URL, *JsonString);
    Request->ProcessRequest();
}

void UAdhocGameModeComponent::OnNavigateResponse(
    FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, UAdhocPlayerControllerComponent* AdhocPlayerController) const
{
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Navigate response: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());

    if (!bWasSuccessful || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Navigate response failure: ResponseCode=%d Content=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
        return;
    }

    const auto& Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    TSharedPtr<FJsonObject> JsonObject;
    if (!FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to deserialize navigate response: %s"), *Response->GetContentAsString());
        return;
    }

    const int64 DestinationServerID = JsonObject->GetIntegerField("destinationServerId");
    // const FString ServerDomain = JsonObject->GetStringField("serverDomain");
    const FString IP = JsonObject->GetStringField("ip");
    const int32 Port = JsonObject->GetIntegerField("port");
    const FString WebSocketURL = JsonObject->GetStringField("webSocketUrl");
    if (DestinationServerID <= 0 || IP.IsEmpty() || Port <= 0 || WebSocketURL.IsEmpty())
    {
        UE_LOG(LogAdhocGameModeComponent, Warning, TEXT("Failed to process navigate response: %s"), *Response->GetContentAsString());
        return;
    }

    FString URL = FString::Printf(TEXT("%s:%d"), *IP, Port);

    URL += FString::Printf(TEXT("?WebSocketURL=%s"), *WebSocketURL);

    // NavigateURL += FString::Printf(TEXT("?ServerID=%lld"), ServerID);
    // NavigateURL += FString::Printf(TEXT("?ServerDomain=%s"), *ServerDomain);

    if (AdhocPlayerController->GetUserID() != -1)
    {
        URL += FString::Printf(TEXT("?UserID=%d"), AdhocPlayerController->GetUserID());
    }
    if (AdhocPlayerController->GetFactionIndex() != -1)
    {
        const FAdhocFactionState& AdhocFactionState = AdhocGameState->GetFaction(AdhocPlayerController->GetFactionIndex());

        if (AdhocFactionState.ID != -1)
        {
            URL += FString::Printf(TEXT("?FactionID=%d"), AdhocFactionState.ID);
        }
    }
    if (!AdhocPlayerController->GetToken().IsEmpty())
    {
        URL += FString::Printf(TEXT("?Token=%s"), *AdhocPlayerController->GetToken());
    }
    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Player %s navigate to server %d via URL: %s"),
        *AdhocPlayerController->GetOwner<APlayerController>()->GetPlayerState<APlayerState>()->GetPlayerName(), DestinationServerID, *URL);

    APlayerController* PlayerController = AdhocPlayerController->GetOwner<APlayerController>();
    check(PlayerController);

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
    Writer->WriteValue(TEXT("eventType"), TEXT("ServerPawns"));
    Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());

    Writer->WriteArrayStart(TEXT("pawns"));

    UWorld* World = GetWorld();
    check(World);

    int32 PawnIndex = 0;
    for (TActorIterator<APawn> It = TActorIterator<APawn>(World); It; ++It)
    {
        const UAdhocPawnComponent* AdhocPawn = Cast<UAdhocPawnComponent>((*It)->GetComponentByClass(UAdhocPawnComponent::StaticClass()));
        if (!AdhocPawn)
        {
            continue;
        }

        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("uuid"), AdhocPawn->GetUUID().ToString(EGuidFormats::DigitsWithHyphens));
        Writer->WriteValue(TEXT("name"), AdhocPawn->GetFriendlyName());
        Writer->WriteValue(TEXT("description"), AdhocPawn->GetDescription());
        Writer->WriteValue(TEXT("serverId"), AdhocGameState->GetServerID());
        Writer->WriteValue(TEXT("index"), PawnIndex);

        Writer->WriteValue(TEXT("x"), -(*It)->GetActorLocation().X);
        Writer->WriteValue(TEXT("y"), (*It)->GetActorLocation().Y);
        Writer->WriteValue(TEXT("z"), (*It)->GetActorLocation().Z);
        Writer->WriteValue(TEXT("pitch"), (*It)->GetActorRotation().Pitch);
        Writer->WriteValue(TEXT("yaw"), (*It)->GetActorRotation().Yaw);

        const AController* Controller = (*It)->GetController();
        const UAdhocControllerComponent* AdhocController = Controller
            ? Cast<UAdhocControllerComponent>(Controller->GetComponentByClass(UAdhocControllerComponent::StaticClass()))
            : nullptr;

        if (AdhocController && AdhocController->GetUserID() != -1)
        {
            Writer->WriteValue(TEXT("userId"), AdhocController->GetUserID());
        }
        else
        {
            Writer->WriteNull(TEXT("userId"));
        }

        Writer->WriteValue(TEXT("human"), AdhocPawn->IsHuman());

        if (AdhocPawn->GetFactionIndex() != -1)
        {
            const FAdhocFactionState& Faction = AdhocGameState->GetFaction(AdhocPawn->GetFactionIndex());
            Writer->WriteValue(TEXT("factionId"), Faction.ID);
        }
        else
        {
            Writer->WriteNull(TEXT("factionId"));
        }

        Writer->WriteObjectEnd();

        PawnIndex++;
    }

    Writer->WriteArrayEnd();

    Writer->WriteObjectEnd();
    Writer->Close();

    UE_LOG(LogAdhocGameModeComponent, Verbose, TEXT("Sending: %s"), *JsonString);
    StompClient->Send("/app/ServerPawns", JsonString);
}

#endif
