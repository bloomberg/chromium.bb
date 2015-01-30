/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "platform/weborigin/SchemeRegistry.h"

#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static Mutex& threadSpecificMutex()
{
    // The first call to this should be made before or during blink
    // initialization to avoid racy static local initialization.
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static URLSchemesSet& localURLSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, localSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (localSchemes->isEmpty())
        localSchemes->add("file");

    return *localSchemes;
}

static URLSchemesSet& displayIsolatedURLSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, displayIsolatedSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());
    return *displayIsolatedSchemes;
}

static URLSchemesSet& mixedContentRestrictingSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, mixedContentRestrictingSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (mixedContentRestrictingSchemes->isEmpty())
        mixedContentRestrictingSchemes->add("https");

    return *mixedContentRestrictingSchemes;
}

static URLSchemesSet& secureSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, secureSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (secureSchemes->isEmpty()) {
        secureSchemes->add("https");
        secureSchemes->add("about");
        secureSchemes->add("data");
        secureSchemes->add("wss");
    }

    return *secureSchemes;
}

static URLSchemesSet& schemesWithUniqueOrigins()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, schemesWithUniqueOrigins, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (schemesWithUniqueOrigins->isEmpty()) {
        schemesWithUniqueOrigins->add("about");
        schemesWithUniqueOrigins->add("javascript");
        // This is a willful violation of HTML5.
        // See https://bugs.webkit.org/show_bug.cgi?id=11885
        schemesWithUniqueOrigins->add("data");
    }

    return *schemesWithUniqueOrigins;
}

static URLSchemesSet& emptyDocumentSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, emptyDocumentSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (emptyDocumentSchemes->isEmpty())
        emptyDocumentSchemes->add("about");

    return *emptyDocumentSchemes;
}

static HashSet<String>& schemesForbiddenFromDomainRelaxation()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<HashSet<String>>, schemes, new ThreadSpecific<HashSet<String>>(), threadSpecificMutex());
    return *schemes;
}

static URLSchemesSet& notAllowingJavascriptURLsSchemes()
{
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, notAllowingJavascriptURLsSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());
    return *notAllowingJavascriptURLsSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    localURLSchemes().add(scheme);
}

const URLSchemesSet& SchemeRegistry::localSchemes()
{
    return localURLSchemes();
}

static URLSchemesSet& CORSEnabledSchemes()
{
    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=77160
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<URLSchemesSet>, CORSEnabledSchemes, new ThreadSpecific<URLSchemesSet>(), threadSpecificMutex());

    if (CORSEnabledSchemes->isEmpty()) {
        CORSEnabledSchemes->add("http");
        CORSEnabledSchemes->add("https");
        CORSEnabledSchemes->add("data");
    }

    return *CORSEnabledSchemes;
}

static URLSchemesMap<SchemeRegistry::PolicyAreas>& ContentSecurityPolicyBypassingSchemes()
{
    using PolicyAreasMap = URLSchemesMap<SchemeRegistry::PolicyAreas>;
    AtomicallyInitializedStaticReferenceWithLock(ThreadSpecific<PolicyAreasMap>, schemes, new ThreadSpecific<PolicyAreasMap>(), threadSpecificMutex());
    return *schemes;
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return localURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme)
{
    schemesWithUniqueOrigins().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesWithUniqueOrigins().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme)
{
    displayIsolatedURLSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return displayIsolatedURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsRestrictingMixedContent(const String& scheme)
{
    mixedContentRestrictingSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsRestrictingMixedContent(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return mixedContentRestrictingSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme)
{
    secureSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return secureSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme)
{
    emptyDocumentSchemes().add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return emptyDocumentSchemes().contains(scheme);
}

void SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const String& scheme)
{
    if (scheme.isEmpty())
        return;

    if (forbidden)
        schemesForbiddenFromDomainRelaxation().add(scheme);
    else
        schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool SchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme)
{
    return equalIgnoringCase("blob", scheme) || equalIgnoringCase("filesystem", scheme);
}

void SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    notAllowingJavascriptURLsSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return notAllowingJavascriptURLsSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme)
{
    CORSEnabledSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return CORSEnabledSchemes().contains(scheme);
}

String SchemeRegistry::listOfCORSEnabledURLSchemes()
{
    StringBuilder builder;
    bool addSeparator = false;
    for (const auto& scheme : CORSEnabledSchemes()) {
        if (addSeparator)
            builder.appendLiteral(", ");
        else
            addSeparator = true;

        builder.append(scheme);
    }
    return builder.toString();
}

bool SchemeRegistry::shouldTreatURLSchemeAsLegacy(const String& scheme)
{
    return equalIgnoringCase("ftp", scheme) || equalIgnoringCase("gopher", scheme);
}

void SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme, PolicyAreas policyAreas)
{
    ContentSecurityPolicyBypassingSchemes().add(scheme, policyAreas);
}

void SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool SchemeRegistry::schemeShouldBypassContentSecurityPolicy(const String& scheme, PolicyAreas policyAreas)
{
    ASSERT(policyAreas != PolicyAreaNone);
    if (scheme.isEmpty() || policyAreas == PolicyAreaNone)
        return false;

    // get() returns 0 (PolicyAreaNone) if there is no entry in the map.
    // Thus by default, schemes do not bypass CSP.
    return (ContentSecurityPolicyBypassingSchemes().get(scheme) & policyAreas) == policyAreas;
}

} // namespace blink
