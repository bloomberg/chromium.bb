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

#include "wtf/MainThread.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static URLSchemesSet& localURLSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, localSchemes, ());

    if (localSchemes.isEmpty())
        localSchemes.add("file");

    return localSchemes;
}

static URLSchemesSet& displayIsolatedURLSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, displayIsolatedSchemes, ());
    return displayIsolatedSchemes;
}

static URLSchemesSet& mixedContentRestrictingSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, mixedContentRestrictingSchemes, ());

    if (mixedContentRestrictingSchemes.isEmpty())
        mixedContentRestrictingSchemes.add("https");

    return mixedContentRestrictingSchemes;
}

static URLSchemesSet& secureSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, secureSchemes, ());

    if (secureSchemes.isEmpty()) {
        secureSchemes.add("https");
        secureSchemes.add("about");
        secureSchemes.add("data");
        secureSchemes.add("wss");
    }

    return secureSchemes;
}

static URLSchemesSet& schemesWithUniqueOrigins()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, schemesWithUniqueOrigins, ());

    if (schemesWithUniqueOrigins.isEmpty()) {
        schemesWithUniqueOrigins.add("about");
        schemesWithUniqueOrigins.add("javascript");
        // This is a willful violation of HTML5.
        // See https://bugs.webkit.org/show_bug.cgi?id=11885
        schemesWithUniqueOrigins.add("data");
    }

    return schemesWithUniqueOrigins;
}

static URLSchemesSet& emptyDocumentSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, emptyDocumentSchemes, ());

    if (emptyDocumentSchemes.isEmpty())
        emptyDocumentSchemes.add("about");

    return emptyDocumentSchemes;
}

static HashSet<String>& schemesForbiddenFromDomainRelaxation()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, schemes, ());
    return schemes;
}

static URLSchemesSet& canDisplayOnlyIfCanRequestSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, canDisplayOnlyIfCanRequestSchemes, ());

    if (canDisplayOnlyIfCanRequestSchemes.isEmpty()) {
        canDisplayOnlyIfCanRequestSchemes.add("blob");
        canDisplayOnlyIfCanRequestSchemes.add("filesystem");
    }

    return canDisplayOnlyIfCanRequestSchemes;
}

static URLSchemesSet& notAllowingJavascriptURLsSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, notAllowingJavascriptURLsSchemes, ());
    return notAllowingJavascriptURLsSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    localURLSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsLocal(const String& scheme)
{
    if (scheme == "file")
        return;
    localURLSchemes().remove(scheme);
}

const URLSchemesSet& SchemeRegistry::localSchemes()
{
    return localURLSchemes();
}

static URLSchemesSet& CORSEnabledSchemes()
{
    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=77160
    DEFINE_STATIC_LOCAL(URLSchemesSet, CORSEnabledSchemes, ());

    if (CORSEnabledSchemes.isEmpty()) {
        CORSEnabledSchemes.add("http");
        CORSEnabledSchemes.add("https");
        CORSEnabledSchemes.add("data");
    }

    return CORSEnabledSchemes;
}

static URLSchemesSet& LegacySchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesSet, LegacySchemes, ());

    if (LegacySchemes.isEmpty()) {
        LegacySchemes.add("ftp");
        LegacySchemes.add("gopher");
    }

    return LegacySchemes;
}

static URLSchemesMap<SchemeRegistry::PolicyAreas>& ContentSecurityPolicyBypassingSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesMap<SchemeRegistry::PolicyAreas>, schemes, ());
    return schemes;
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
    if (scheme.isEmpty())
        return false;
    return canDisplayOnlyIfCanRequestSchemes().contains(scheme);
}

void SchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(const String& scheme)
{
    canDisplayOnlyIfCanRequestSchemes().add(scheme);
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

void SchemeRegistry::registerURLSchemeAsLegacy(const String& scheme)
{
    LegacySchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsLegacy(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return LegacySchemes().contains(scheme);
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
