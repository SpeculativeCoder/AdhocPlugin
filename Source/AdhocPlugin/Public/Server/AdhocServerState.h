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

#pragma once

#include "CoreMinimal.h"

#include "AdhocServerState.generated.h"

USTRUCT(BlueprintType)
struct FAdhocServerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 ID = -1;

    UPROPERTY(BlueprintReadOnly)
    int64 Version = -1;

    UPROPERTY(BlueprintReadOnly)
    int64 RegionID = -1;

    UPROPERTY(BlueprintReadOnly)
    TArray<int64> AreaIDs;
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> AreaIndexes;

    // UPROPERTY(BlueprintReadOnly)
    // FString Name;

    // UPROPERTY(BlueprintReadOnly)
    // FString HostingType;

    // UPROPERTY(BlueprintReadOnly)
    // FString Status;

    UPROPERTY(BlueprintReadOnly)
    bool bEnabled;
    UPROPERTY(BlueprintReadOnly)
    bool bActive;

    UPROPERTY(BlueprintReadOnly)
    FString PrivateIP;

    UPROPERTY(BlueprintReadOnly)
    FString PublicIP;
    UPROPERTY(BlueprintReadOnly)
    int32 PublicWebSocketPort = -1;
};
