// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/OriginsUsingFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "public/platform/Platform.h"

namespace blink {

OriginsUsingFeatures::~OriginsUsingFeatures()
{
    updateMeasurementsAndClear();
}

OriginsUsingFeatures::Value::Value()
    : m_countBits(0)
{
}

void OriginsUsingFeatures::count(const ScriptState* scriptState, Document& document, Feature feature)
{
    if (!scriptState || !scriptState->world().isMainWorld())
        return;
    document.originsUsingFeaturesValue().count(feature);
}

void OriginsUsingFeatures::Value::count(Feature feature)
{
    ASSERT(feature < Feature::NumberOfFeatures);
    m_countBits |= 1 << static_cast<unsigned>(feature);
}

void OriginsUsingFeatures::documentDetached(Document& document)
{
    OriginsUsingFeatures::Value counter = document.originsUsingFeaturesValue();
    if (counter.isEmpty())
        return;

    const KURL& url = document.url();
    if (!url.protocolIsInHTTPFamily())
        return;

    m_originAndValues.append(std::make_pair(url.host(), counter));
    document.originsUsingFeaturesValue().clear();
    ASSERT(document.originsUsingFeaturesValue().isEmpty());
}

void OriginsUsingFeatures::updateMeasurementsAndClear()
{
    if (m_originAndValues.isEmpty())
        return;

    // Aggregate values by origins.
    HashMap<String, OriginsUsingFeatures::Value> aggregatedByOrigin;
    for (const auto& originAndValue : m_originAndValues) {
        ASSERT(!originAndValue.first.isEmpty());
        auto result = aggregatedByOrigin.add(originAndValue.first, originAndValue.second);
        if (!result.isNewEntry)
            result.storedValue->value.aggregate(originAndValue.second);
    }

    // Report to RAPPOR.
    for (auto& originAndValue : aggregatedByOrigin)
        originAndValue.value.updateMeasurements(originAndValue.key);

    m_originAndValues.clear();
}

void OriginsUsingFeatures::Value::aggregate(OriginsUsingFeatures::Value other)
{
    m_countBits |= other.m_countBits;
}

void OriginsUsingFeatures::Value::updateMeasurements(const String& origin)
{
    if (get(Feature::ElementCreateShadowRoot))
        Platform::current()->recordRappor("WebComponents.ElementCreateShadowRoot", origin);
    if (get(Feature::DocumentRegisterElement))
        Platform::current()->recordRappor("WebComponents.DocumentRegisterElement", origin);
}

} // namespace blink
