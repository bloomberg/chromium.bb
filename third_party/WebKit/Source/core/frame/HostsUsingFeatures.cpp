// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/HostsUsingFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"

namespace blink {

HostsUsingFeatures::~HostsUsingFeatures()
{
    updateMeasurementsAndClear();
}

HostsUsingFeatures::Value::Value()
    : m_countBits(0)
{
}

void HostsUsingFeatures::countAnyWorld(Document& document, Feature feature)
{
    document.HostsUsingFeaturesValue().count(feature);
}

void HostsUsingFeatures::countMainWorldOnly(const ScriptState* scriptState, Document& document, Feature feature)
{
    if (!scriptState || !scriptState->world().isMainWorld())
        return;
    countAnyWorld(document, feature);
}

static Document* documentFromEventTarget(EventTarget& target)
{
    ExecutionContext* executionContext = target.getExecutionContext();
    if (!executionContext)
        return nullptr;
    if (executionContext->isDocument())
        return toDocument(executionContext);
    if (LocalDOMWindow* executingWindow = executionContext->executingWindow())
        return executingWindow->document();
    return nullptr;
}

void HostsUsingFeatures::countHostOrIsolatedWorldHumanReadableName(const ScriptState* scriptState, EventTarget& target, Feature feature)
{
    if (!scriptState)
        return;
    Document* document = documentFromEventTarget(target);
    if (!document)
        return;
    if (scriptState->world().isMainWorld()) {
        document->HostsUsingFeaturesValue().count(feature);
        return;
    }
    if (Page* page = document->page())
        page->hostsUsingFeatures().countName(feature, scriptState->world().isolatedWorldHumanReadableName());
}

void HostsUsingFeatures::Value::count(Feature feature)
{
    DCHECK(feature < Feature::NumberOfFeatures);
    m_countBits |= 1 << static_cast<unsigned>(feature);
}

void HostsUsingFeatures::countName(Feature feature, const String& name)
{
    auto result = m_valueByName.add(name, Value());
    result.storedValue->value.count(feature);
}

void HostsUsingFeatures::clear()
{
    m_hostAndValues.clear();
    m_valueByName.clear();
}

void HostsUsingFeatures::documentDetached(Document& document)
{
    HostsUsingFeatures::Value counter = document.HostsUsingFeaturesValue();
    if (counter.isEmpty())
        return;

    const KURL& url = document.url();
    if (!url.protocolIsInHTTPFamily())
        return;

    m_hostAndValues.append(std::make_pair(url.host(), counter));
    document.HostsUsingFeaturesValue().clear();
    DCHECK(document.HostsUsingFeaturesValue().isEmpty());
}

void HostsUsingFeatures::updateMeasurementsAndClear()
{
    if (!m_hostAndValues.isEmpty())
        recordHostToRappor();
    if (!m_valueByName.isEmpty())
        recordNamesToRappor();
}

void HostsUsingFeatures::recordHostToRappor()
{
    DCHECK(!m_hostAndValues.isEmpty());

    // Aggregate values by hosts.
    HashMap<String, HostsUsingFeatures::Value> aggregatedByHost;
    for (const auto& hostAndValue : m_hostAndValues) {
        DCHECK(!hostAndValue.first.isEmpty());
        auto result = aggregatedByHost.add(hostAndValue.first, hostAndValue.second);
        if (!result.isNewEntry)
            result.storedValue->value.aggregate(hostAndValue.second);
    }

    // Report to RAPPOR.
    for (auto& hostAndValue : aggregatedByHost)
        hostAndValue.value.recordHostToRappor(hostAndValue.key);

    m_hostAndValues.clear();
}

void HostsUsingFeatures::recordNamesToRappor()
{
    DCHECK(!m_valueByName.isEmpty());

    for (auto& nameAndValue : m_valueByName)
        nameAndValue.value.recordNameToRappor(nameAndValue.key);

    m_valueByName.clear();
}

void HostsUsingFeatures::Value::aggregate(HostsUsingFeatures::Value other)
{
    m_countBits |= other.m_countBits;
}

void HostsUsingFeatures::Value::recordHostToRappor(const String& host)
{
    if (get(Feature::ElementCreateShadowRoot))
        Platform::current()->recordRappor("WebComponents.ElementCreateShadowRoot", host);
    if (get(Feature::ElementAttachShadow))
        Platform::current()->recordRappor("WebComponents.ElementAttachShadow", host);
    if (get(Feature::DocumentRegisterElement))
        Platform::current()->recordRappor("WebComponents.DocumentRegisterElement", host);
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath", host);
    if (get(Feature::DeviceMotionInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceMotion.Insecure", host);
    if (get(Feature::DeviceOrientationInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceOrientation.Insecure", host);
    if (get(Feature::FullscreenInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Fullscreen.Insecure", host);
    if (get(Feature::GeolocationInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Geolocation.Insecure", host);
    if (get(Feature::GetUserMediaInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.GetUserMedia.Insecure", host);
    if (get(Feature::GetUserMediaSecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.GetUserMedia.Secure", host);
    if (get(Feature::ApplicationCacheManifestSelectInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheManifestSelect.Insecure", host);
    if (get(Feature::ApplicationCacheAPIInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheAPI.Insecure", host);
}

void HostsUsingFeatures::Value::recordNameToRappor(const String& name)
{
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath.Extensions", name);
}

} // namespace blink
