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

#include "CoreMinimal.h"

#include "AdhocStructureState.generated.h"

USTRUCT(BlueprintType)
struct FAdhocStructureState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 ID = -1;

    UPROPERTY(BlueprintReadOnly)
    int64 Version = -1;

    UPROPERTY(BlueprintReadOnly)
    FGuid UUID;

    UPROPERTY(BlueprintReadOnly)
    FString Name;
    UPROPERTY(BlueprintReadOnly)
    FString Type;

    UPROPERTY(BlueprintReadOnly)
    int64 RegionID = -1;

    UPROPERTY(BlueprintReadOnly)
    FVector Location;
    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation;
    UPROPERTY(BlueprintReadOnly)
    FVector Scale;
    UPROPERTY(BlueprintReadOnly)
    FVector Size;

    UPROPERTY(BlueprintReadOnly)
    int64 FactionID = -1;
    UPROPERTY(BlueprintReadOnly)
    int32 FactionIndex = -1;

    UPROPERTY(BlueprintReadOnly)
    int64 UserID = -1;
};
