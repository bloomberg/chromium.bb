// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollectedFinalized<CustomElementDefinition> {
    WTF_MAKE_NONCOPYABLE(CustomElementDefinition);
public:
    CustomElementDefinition(const CustomElementDescriptor&);
    virtual ~CustomElementDefinition();

    const CustomElementDescriptor& descriptor() { return m_descriptor; }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

private:
    const CustomElementDescriptor m_descriptor;
};

} // namespace blink

#endif // CustomElementDefinition_h
