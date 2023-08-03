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

#include "Area/AdhocAreaVolume.h"

#include "AdhocGameModeComponent.h"
#include "Area/AdhocAreaComponent.h"
#include "Components/BrushComponent.h"
#include "GameFramework/GameModeBase.h"

AAdhocAreaVolume::AAdhocAreaVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = false;

	bColored = true;
	BrushColor.R = 100;
	BrushColor.G = 150;
	BrushColor.B = 250;
	BrushColor.A = 255;

	GetBrushComponent()->SetCollisionProfileName(TEXT("Area"));

	AdhocArea = CreateDefaultSubobject<UAdhocAreaComponent>(TEXT("AdhocArea"));
}

void AAdhocAreaVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetBrushComponent()->OnComponentBeginOverlap.AddDynamic(this, &AAdhocAreaVolume::OnBeginOverlap);
	}
}

// ReSharper disable once CppMemberFunctionMayBeConst
void AAdhocAreaVolume::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& Hit)
{
	check(HasAuthority());

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Pawn->Controller);
	if (!PlayerController)
	{
		return;
	}

	const UWorld* World = GetWorld();
	check(World);
	const AGameModeBase* GameMode = World->GetAuthGameMode();
	check(GameMode);
	const UAdhocGameModeComponent* AdhocGameMode = Cast<UAdhocGameModeComponent>(GameMode->GetComponentByClass(UAdhocGameModeComponent::StaticClass()));
	check(AdhocGameMode);

	AdhocGameMode->PlayerEnterArea(PlayerController, GetAdhocArea()->GetAreaIndex());
}
