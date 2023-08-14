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
#include "Components/ActorComponent.h"

#include "AdhocControllerComponent.generated.h"

UCLASS(Transient)
class ADHOCPLUGIN_API UAdhocControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRepFactionIndexDelegate, int32 OldFactionIndex);

	FOnRepFactionIndexDelegate OnRepFactionIndexDelegate;

protected:
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_FactionIndex)
	int32 FactionIndex = -1;

public:
	FORCEINLINE int32 GetFactionIndex() const { return FactionIndex; }

	FORCEINLINE void SetFactionIndex(const int64 NewFactionIndex) { FactionIndex = NewFactionIndex; }

protected:
	explicit UAdhocControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION()
	void OnRep_FactionIndex(int32 OldFactionIndex) const;
};
