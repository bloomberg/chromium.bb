// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "core/svg/SVGFilterElement.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFilterElement;

class CORE_EXPORT SVGResourceClient {
public:
    virtual ~SVGResourceClient();
    void addFilterReference(SVGFilterElement*);
    void clearFilterReferences();

    virtual void filterNeedsInvalidation() = 0;

    void filterWillBeDestroyed(SVGFilterElement*);

private:
    WillBePersistentHeapHashSet<RawPtrWillBeWeakMember<SVGFilterElement>> m_filterReferences;
};

} // namespace blink

#endif // SVGResourceClient_h
