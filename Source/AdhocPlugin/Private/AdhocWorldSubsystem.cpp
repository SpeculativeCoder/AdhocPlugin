﻿// Copyright (c) 2022-2025 SpeculativeCoder (https://github.com/SpeculativeCoder)
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

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAdhocWorldSubsystem, Log, All)

void UAdhocWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UWorld* World = GetWorld();
    check(World);

    UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("Initialize: World=%s"), *World->GetName());
}

bool UAdhocWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    UE_LOG(LogAdhocWorldSubsystem, Verbose, TEXT("DoesSupportWorldType: WorldType=%d"), WorldType);

    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
