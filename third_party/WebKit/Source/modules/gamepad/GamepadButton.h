// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadButton_h
#define GamepadButton_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class GamepadButton: public RefCounted<GamepadButton>, public ScriptWrappable {
public:
    static PassRefPtr<GamepadButton> create();
    ~GamepadButton();

    float value() const { return m_value; }
    void value(float val) { m_value = val; }

    bool pressed() const { return m_pressed; }
    void pressed(bool val) { m_pressed = val; }

private:
    GamepadButton();
    float m_value;
    bool m_pressed;
};

typedef Vector<RefPtr<GamepadButton> > GamepadButtonVector;

} // namespace WebCore

#endif // GamepadButton_h
