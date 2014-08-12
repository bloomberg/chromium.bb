// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebKitGamepadList_h
#define WebKitGamepadList_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/gamepad/WebKitGamepad.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepads.h"

namespace blink {

class WebKitGamepadList FINAL : public GarbageCollected<WebKitGamepadList>, public ScriptWrappable {
public:
    static WebKitGamepadList* create()
    {
        return new WebKitGamepadList;
    }

    void set(unsigned index, WebKitGamepad*);
    WebKitGamepad* item(unsigned index);
    unsigned length() const { return WebGamepads::itemsLengthCap; }

    void trace(Visitor*);

private:
    WebKitGamepadList();
    Member<WebKitGamepad> m_items[WebGamepads::itemsLengthCap];
};

} // namespace blink

#endif // WebKitGamepadList_h
