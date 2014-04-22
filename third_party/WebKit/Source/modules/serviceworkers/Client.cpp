// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Client.h"

#include "wtf/RefPtr.h"

namespace WebCore {

PassRefPtr<Client> Client::create(unsigned id)
{
    return adoptRef(new Client(id));
}

Client::Client(unsigned id)
    : m_id(id)
{
    ScriptWrappable::init(this);
}

Client::~Client()
{
}

} // namespace WebCore
