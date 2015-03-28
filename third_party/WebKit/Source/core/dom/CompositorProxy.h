// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxy_h
#define CompositorProxy_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class CompositorProxy final : public GarbageCollectedFinalized<CompositorProxy>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CompositorProxy* create(ExecutionContext*, Element*, const Vector<String>& attributeArray, ExceptionState&);
    static CompositorProxy* create(uint64_t element, uint32_t attributeFlags);
    virtual ~CompositorProxy();

    enum class Attributes {
        NONE        = 0,
        OPACITY     = 1 << 0,
        SCROLL_LEFT = 1 << 2,
        SCROLL_TOP  = 1 << 3,
        TOUCH       = 1 << 4,
        TRANSFORM   = 1 << 5,
    };

    DEFINE_INLINE_TRACE() { }

    uint64_t elementId() const { return m_elementId; }
    uint32_t bitfieldsSupported() const { return m_bitfieldsSupported; }
    bool supports(const String& attribute) const;

protected:
    CompositorProxy(Element&, const Vector<String>& attributeArray);
    CompositorProxy(uint64_t element, uint32_t attributeFlags);

private:
    const uint64_t m_elementId;
    const uint32_t m_bitfieldsSupported;
};

} // namespace blink

#endif // CompositorProxy_h
