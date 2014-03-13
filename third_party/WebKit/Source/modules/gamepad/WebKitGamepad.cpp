// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/WebKitGamepad.h"

namespace WebCore {

WebKitGamepad::WebKitGamepad()
    : m_index(0)
    , m_timestamp(0)
{
    ScriptWrappable::init(this);
}

void WebKitGamepad::setAxes(unsigned count, const float* data)
{
    m_axes.resize(count);
    if (count)
        std::copy(data, data + count, m_axes.begin());
}

void WebKitGamepad::setButtons(unsigned count, const blink::WebGamepadButton* data)
{
    m_buttons.resize(count);
    for (unsigned i = 0; i < count; ++i)
        m_buttons[i] = data[i].value;
}

void WebKitGamepad::trace(Visitor* visitor)
{
}

WebKitGamepad::~WebKitGamepad()
{
}

} // namespace WebCore
