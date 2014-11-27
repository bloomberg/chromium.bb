// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushMessageData.h"

namespace blink {

PushMessageData::PushMessageData()
{
}

PushMessageData::PushMessageData(const String& messageData)
    : m_messageData(messageData)
{
}

PushMessageData::~PushMessageData()
{
}

const String& PushMessageData::text() const
{
    return m_messageData;
}

void PushMessageData::trace(Visitor*)
{
}

} // namespace blink
