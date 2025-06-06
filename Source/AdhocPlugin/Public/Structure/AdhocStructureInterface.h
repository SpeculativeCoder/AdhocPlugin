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

#include "UObject/Interface.h"

#include "AdhocStructureInterface.generated.h"

UINTERFACE()
class UAdhocStructureInterface : public UInterface
{
    GENERATED_BODY()
};


class IAdhocStructureInterface
{
    GENERATED_BODY()

public:
    virtual const FGuid& GetUUID() const = 0;
    virtual const FString& GetFriendlyName() const = 0;
    virtual const FString& GetType() const = 0;
    virtual int32 GetFactionIndex() const = 0;
    virtual int64 GetUserID() const = 0;

    virtual void SetUUID(const FGuid& NewUUID) = 0;
    virtual void SetFriendlyName(const FString& NewFriendlyName) = 0;
    virtual void SetFactionIndex(int32 NewFactionIndex) = 0;
    virtual void SetUserID(int64 NewUserID) = 0;
};
