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
#include "AdhocControllerComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"

#include "AdhocPlayerControllerComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocPlayerControllerComponent : public UAdhocControllerComponent
{
	GENERATED_BODY()

private:
	UPROPERTY(Replicated)
	int64 UserID = -1;

	FString Token;

	/** When set, player will spawn at this location / rotation.
	 * Typically this is set when the player has travelled from an area on another server to an area on this server
	 * and we wish to immediately spawn them at the location they were previously at. */
	TOptional<FTransform> ImmediateSpawnTransform;

public:
	FORCEINLINE class APlayerController* GetPlayerController() const { return GetOwner<APlayerController>(); }

	FORCEINLINE int64 GetUserID() const { return UserID; }
	FORCEINLINE FString GetToken() const { return Token; }
	FORCEINLINE TOptional<FTransform> GetImmediateSpawnTransform() const { return ImmediateSpawnTransform; }

	FORCEINLINE void SetUserID(const int64 NewUserID) { UserID = NewUserID; }
	FORCEINLINE void SetToken(const FString& NewToken) { Token = NewToken; }
	FORCEINLINE void SetImmediateSpawnTransform(const TOptional<FTransform>& NewImmediateSpawnTransform) { ImmediateSpawnTransform = NewImmediateSpawnTransform; }

	FORCEINLINE void ClearImmediateSpawnTransform() { ImmediateSpawnTransform = TOptional<FTransform>(); }

private:
	explicit UAdhocPlayerControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
