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
#include "NativeGameplayTags.h"

#include "AdhocAreaComponent.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Adhoc_Area);

UCLASS()
class ADHOCPLUGIN_API UAdhocAreaComponent : public UActorComponent
{
    GENERATED_BODY()

    UPROPERTY(Category="Adhoc Area", EditInstanceOnly)
    FString FriendlyName = TEXT("Area");

    int32 AreaIndex = -1;

    //class UAdhocGameStateComponent* AdhocGameState;

    //FTimerHandle TimerHandle_CheckOverlappingPawns;

public:
    FORCEINLINE const FString& GetFriendlyName() const { return FriendlyName; }
    FORCEINLINE int32 GetAreaIndex() const { return AreaIndex; }
    //FORCEINLINE class UAdhocGameStateComponent* GetAdhocGameState() const { return AdhocGameState; }

    FORCEINLINE void SetAreaIndex(const int32 NewAreaIndex) { AreaIndex = NewAreaIndex; }

    FORCEINLINE AActor* GetArea() const { return GetOwner(); }

private:
    explicit UAdhocAreaComponent(const FObjectInitializer& ObjectInitializer);

    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;

    /** Overlap event will check if player needs to navigate to another server. */
    UFUNCTION()
    void OnAreaActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

    //void OnTimer_CheckOverlappingPawns() const;
};
