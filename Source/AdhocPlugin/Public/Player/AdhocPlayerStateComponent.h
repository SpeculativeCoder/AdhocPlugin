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

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerState.h"

#include "AdhocPlayerStateComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocPlayerStateComponent : public UActorComponent
{
    GENERATED_BODY()

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
    int32 FactionIndex = -1;

    UPROPERTY(VisibleInstanceOnly, Replicated)
    int32 UserID = -1;

public:
    FORCEINLINE class APlayerState* GetPlayerState() const { return GetOwner<APlayerState>(); }

    FORCEINLINE int32 GetFactionIndex() const { return FactionIndex; }
    FORCEINLINE int32 GetUserID() const { return UserID; }

    FORCEINLINE void SetFactionIndex(const int32 NewFactionIndex) { FactionIndex = NewFactionIndex; }
    FORCEINLINE void SetUserID(const int32 NewUserID) { UserID = NewUserID; }

private:
    explicit UAdhocPlayerStateComponent(const FObjectInitializer& ObjectInitializer);
};
