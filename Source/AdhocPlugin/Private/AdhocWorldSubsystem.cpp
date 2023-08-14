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

#include "AdhocWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocWorldSubsystem, Log, All)

void UAdhocWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("Initialize"));

	FWorldDelegates::OnPostWorldCreation.AddUObject(this, &UAdhocWorldSubsystem::OnPostWorldCreation);
	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UAdhocWorldSubsystem::OnPreWorldInitialization);
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UAdhocWorldSubsystem::OnPostWorldInitialization);
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UAdhocWorldSubsystem::OnWorldInitializedActors);
}

bool UAdhocWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("DoesSupportWorldType: WorldType=%d"), WorldType);

	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocWorldSubsystem::OnPostWorldCreation(UWorld* World) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("OnPostWorldCreation: World=%s"), *World->GetName());
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocWorldSubsystem::OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("OnPreWorldInitialization: World=%s"), *World->GetName());

	World->OnWorldBeginPlay.AddUObject(this, &UAdhocWorldSubsystem::OnWorldBeginPlay, World);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocWorldSubsystem::OnPostWorldInitialization(UWorld* World, UWorld::InitializationValues InitializationValues) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("OnPostWorldInitialization: World=%s"), *World->GetName());
}

void UAdhocWorldSubsystem::OnWorldInitializedActors(const UWorld::FActorsInitializedParams& ActorsInitializedParams) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("OnWorldInitializedActors: World=%s"), *ActorsInitializedParams.World->GetName());
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void UAdhocWorldSubsystem::OnWorldBeginPlay(UWorld* World) const
{
	UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("OnWorldBeginPlay: World=%s"), *World->GetName());
}
