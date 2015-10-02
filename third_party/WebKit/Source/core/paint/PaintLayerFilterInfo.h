/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef PaintLayerFilterInfo_h
#define PaintLayerFilterInfo_h

#include "core/dom/Element.h"
#include "core/fetch/DocumentResource.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/filters/FilterOperation.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class FilterEffectBuilder;
class FilterOperations;
class PaintLayer;
class PaintLayerFilterInfo;

typedef HashMap<const PaintLayer*, PaintLayerFilterInfo*> PaintLayerFilterInfoMap;

class PaintLayerFilterInfo final : public DocumentResourceClient {
    WTF_MAKE_FAST_ALLOCATED(PaintLayerFilterInfo);
    WTF_MAKE_NONCOPYABLE(PaintLayerFilterInfo);
public:
    static PaintLayerFilterInfo* filterInfoForLayer(const PaintLayer*);
    static PaintLayerFilterInfo* createFilterInfoForLayerIfNeeded(PaintLayer*);
    static void removeFilterInfoForLayer(PaintLayer*);

    FilterEffectBuilder* builder() const { return m_builder.get(); }
    void setBuilder(PassRefPtrWillBeRawPtr<FilterEffectBuilder>);

    void updateReferenceFilterClients(const FilterOperations&);
    void notifyFinished(Resource*) override;
    void removeReferenceFilterClients();

private:
    PaintLayerFilterInfo(PaintLayer*);
    ~PaintLayerFilterInfo() override;

    PaintLayer* m_layer;

    RefPtrWillBePersistent<FilterEffectBuilder> m_builder;

    static PaintLayerFilterInfoMap* s_filterMap;
    WillBePersistentHeapVector<RefPtrWillBeMember<Element>> m_internalSVGReferences;
    Vector<ResourcePtr<DocumentResource>> m_externalSVGReferences;
};

} // namespace blink


#endif // PaintLayerFilterInfo_h
