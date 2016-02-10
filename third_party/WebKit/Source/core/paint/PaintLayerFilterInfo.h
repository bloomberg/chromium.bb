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
#include "core/svg/SVGResourceClient.h"
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

// PaintLayerFilterInfo holds the filter information for painting.
// https://drafts.fxtf.org/filters/
//
// Because PaintLayer is not allocated for SVG objects, SVG filters (both
// software and hardware-accelerated) use a different code path to paint the
// filters (SVGFilterPainter), but both code paths use the same abstraction for
// painting non-hardware accelerated filters (FilterEffect). Hardware
// accelerated CSS filters use CompositorFilterOperations, that is backed by cc.
//
// PaintLayerFilterInfo is allocated when filters are present and stored in an
// internal map (s_filterMap) to save memory as 'filter' should be a rare
// property.
class PaintLayerFilterInfo final : public DocumentResourceClient, public SVGResourceClient {
    USING_FAST_MALLOC(PaintLayerFilterInfo);
    WTF_MAKE_NONCOPYABLE(PaintLayerFilterInfo);
public:
    // Queries the PaintLayerFilterInfo for the associated PaintLayer.
    // The function returns nullptr if there is no associated
    // |PaintLayerFilterInfo|.
    static PaintLayerFilterInfo* filterInfoForLayer(const PaintLayer*);

    // Creates a new PaintLayerFilterInfo for the associated PaintLayer.
    // If there is one already, it returns it instead of creating a new one.
    //
    // This function will never return nullptr.
    static PaintLayerFilterInfo* createFilterInfoForLayerIfNeeded(PaintLayer*);

    // Remove the PaintLayerFilterInfo associated with PaintLayer.
    // If there is none, this function does nothing.
    static void removeFilterInfoForLayer(PaintLayer*);

    FilterEffectBuilder* builder() const { return m_builder.get(); }
    void setBuilder(PassRefPtrWillBeRawPtr<FilterEffectBuilder>);

    void updateReferenceFilterClients(const FilterOperations&);
    void notifyFinished(Resource*) override;
    String debugName() const override { return "PaintLayerFilterInfo"; }
    void removeReferenceFilterClients();

    void filterNeedsInvalidation() override;

private:
    PaintLayerFilterInfo(PaintLayer*);
    ~PaintLayerFilterInfo() override;

    PaintLayer* m_layer;

    RefPtrWillBePersistent<FilterEffectBuilder> m_builder;

    static PaintLayerFilterInfoMap* s_filterMap;

    // This stores SVG reference filters (filter: url(#someElement)) where the
    // reference belongs to a different document.
    WillBePersistentHeapVector<RefPtrWillBeMember<DocumentResource>> m_externalSVGReferences;
};

} // namespace blink


#endif // PaintLayerFilterInfo_h
