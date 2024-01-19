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
#include "User/AdhocControllerComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"

#include "AdhocPlayerControllerComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocPlayerControllerComponent : public UAdhocControllerComponent
{
    GENERATED_BODY()

    FString Token;

    explicit UAdhocPlayerControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
    FORCEINLINE APlayerController* GetPlayerController() const { return GetOwner<APlayerController>(); }

    FORCEINLINE virtual bool IsHuman() const override { return true; }

    FORCEINLINE FString GetToken() const { return Token; }

    FORCEINLINE void SetToken(const FString& NewToken) { Token = NewToken; }
};
