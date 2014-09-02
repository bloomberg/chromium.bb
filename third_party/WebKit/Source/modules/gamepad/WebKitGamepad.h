// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebKitGamepad_h
#define WebKitGamepad_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/gamepad/GamepadCommon.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class WebKitGamepad FINAL : public GarbageCollectedFinalized<WebKitGamepad>, public GamepadCommon, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static WebKitGamepad* create()
    {
        return new WebKitGamepad();
    }
    ~WebKitGamepad();

    typedef Vector<float> FloatVector;

    const FloatVector& buttons() const { return m_buttons; }
    void setButtons(unsigned count, const WebGamepadButton* data);

    void trace(Visitor*) { }

private:
    WebKitGamepad();
    FloatVector m_buttons;
};

} // namespace blink

#endif // WebKitGamepad_h
