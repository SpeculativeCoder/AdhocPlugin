﻿// Copyright (c) 2022-2023 SpeculativeCoder (https://github.com/SpeculativeCoder)
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
#include "UObject/Interface.h"
#include "AdhocPlayerControllerInterface.generated.h"

UINTERFACE()
class UAdhocPlayerControllerInterface : public UInterface
{
	GENERATED_BODY()
};

class IAdhocPlayerControllerInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetFactionIndex() const = 0;
	virtual int32 GetUserID() const = 0;
	virtual FString GetToken() const = 0;
	virtual TOptional<FTransform> GetImmediateSpawnTransform() const = 0;

	virtual void SetFactionIndex(const int64 NewFactionIndex) = 0;
	virtual void SetUserID(const int64 NewUserID) = 0;
	virtual void SetToken(const FString& NewToken) = 0;
	virtual void SetImmediateSpawnTransform(const TOptional<FTransform>& NewImmediateSpawnTransform) = 0;
};
