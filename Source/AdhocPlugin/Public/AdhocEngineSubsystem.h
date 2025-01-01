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

#include "Subsystems/EngineSubsystem.h"
#include "Engine/World.h"

#include "AdhocEngineSubsystem.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocEngineSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void OnPostWorldCreation(UWorld* World) const;
    void OnWorldInitializedActors(const UWorld::FActorsInitializedParams& ActorsInitializedParams) const;
    void OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues) const;
    void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues) const;
    void OnWorldBeginPlay(UWorld* World) const;

    void OnActorPreSpawnInitialization(AActor* Actor) const;
    void OnActorSpawned(AActor* Actor) const;

    void OnGameModeInitialized(AGameModeBase* GameMode) const;
    void OnGameModePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueNetId, FString& ErrorMessage) const;
    void OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* PlayerController) const;
    void OnGameModeLogout(AGameModeBase* GameMode, AController* Controller) const;
};
