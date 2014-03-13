// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebKitGamepad_h
#define WebKitGamepad_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "public/platform/WebGamepad.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class WebKitGamepad FINAL : public RefCountedWillBeGarbageCollectedFinalized<WebKitGamepad>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<WebKitGamepad> create()
    {
        return adoptRefWillBeNoop(new WebKitGamepad);
    }
    ~WebKitGamepad();

    typedef Vector<float> FloatVector;

    const String& id() const { return m_id; }
    void setId(const String& id) { m_id = id; }

    unsigned index() const { return m_index; }
    void setIndex(unsigned val) { m_index = val; }

    bool connected() const { return m_connected; }
    void setConnected(bool val) { m_connected = val; }

    unsigned long long timestamp() const { return m_timestamp; }
    void setTimestamp(unsigned long long val) { m_timestamp = val; }

    const String& mapping() const { return m_mapping; }
    void setMapping(const String& val) { m_mapping = val; }

    const FloatVector& axes() const { return m_axes; }
    void setAxes(unsigned count, const float* data);

    const FloatVector& buttons() const { return m_buttons; }
    void setButtons(unsigned count, const blink::WebGamepadButton* data);

    void trace(Visitor*);

private:
    WebKitGamepad();
    String m_id;
    unsigned m_index;
    bool m_connected;
    unsigned long long m_timestamp;
    String m_mapping;
    FloatVector m_axes;
    FloatVector m_buttons;
};

} // namespace WebCore

#endif // WebKitGamepad_h
