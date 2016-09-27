// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedObject_h
#define DetectedObject_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMRect.h"
#include "modules/ModulesExport.h"

namespace blink {

class MODULES_EXPORT DetectedObject
    : public GarbageCollected<DetectedObject>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    DOMRect* boundingBox() const { return m_boundingBox.get(); }
    DEFINE_INLINE_TRACE() { visitor->trace(m_boundingBox); }

private:
    Member<DOMRect> m_boundingBox;
};

} // namespace blink

#endif // DetectedObject_h
