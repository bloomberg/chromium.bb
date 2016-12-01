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

#include "platform/weborigin/SchemeRegistry.h"

#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class URLSchemesRegistry final {
 public:
  URLSchemesRegistry()
      : localSchemes({"file"}),
        secureSchemes({"https", "about", "data", "wss"}),
        schemesWithUniqueOrigins({"about", "javascript", "data"}),
        emptyDocumentSchemes({"about"}),
        CORSEnabledSchemes({"http", "https", "data"}),
        // For ServiceWorker schemes: HTTP is required because http://localhost
        // is considered secure. Additional checks are performed to ensure that
        // other http pages are filtered out.
        serviceWorkerSchemes({"http", "https"}),
        fetchAPISchemes({"http", "https"}),
        allowedInReferrerSchemes({"http", "https"}) {}
  ~URLSchemesRegistry() = default;

  URLSchemesSet localSchemes;
  URLSchemesSet displayIsolatedURLSchemes;
  URLSchemesSet secureSchemes;
  URLSchemesSet schemesWithUniqueOrigins;
  URLSchemesSet emptyDocumentSchemes;
  URLSchemesSet schemesForbiddenFromDomainRelaxation;
  URLSchemesSet notAllowingJavascriptURLsSchemes;
  URLSchemesSet CORSEnabledSchemes;
  URLSchemesSet serviceWorkerSchemes;
  URLSchemesSet fetchAPISchemes;
  URLSchemesSet firstPartyWhenTopLevelSchemes;
  URLSchemesMap<SchemeRegistry::PolicyAreas>
      contentSecurityPolicyBypassingSchemes;
  URLSchemesSet secureContextBypassingSchemes;
  URLSchemesSet allowedInReferrerSchemes;

 private:
  friend const URLSchemesRegistry& getURLSchemesRegistry();
  friend URLSchemesRegistry& getMutableURLSchemesRegistry();

  static URLSchemesRegistry& getInstance() {
    DEFINE_STATIC_LOCAL(URLSchemesRegistry, schemes, ());
    return schemes;
  }
};

const URLSchemesRegistry& getURLSchemesRegistry() {
  return URLSchemesRegistry::getInstance();
}

URLSchemesRegistry& getMutableURLSchemesRegistry() {
#if DCHECK_IS_ON()
  DCHECK(WTF::isBeforeThreadCreated());
#endif
  return URLSchemesRegistry::getInstance();
}

}  // namespace

// Must be called before we create other threads to avoid racy static local
// initialization.
void SchemeRegistry::initialize() {
  getURLSchemesRegistry();
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().localSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().localSchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().schemesWithUniqueOrigins.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().schemesWithUniqueOrigins.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().displayIsolatedURLSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().displayIsolatedURLSchemes.contains(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsRestrictingMixedContent(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  return scheme == "https";
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().secureSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().secureSchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().emptyDocumentSchemes.add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().emptyDocumentSchemes.contains(scheme);
}

void SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return;

  if (forbidden) {
    getMutableURLSchemesRegistry().schemesForbiddenFromDomainRelaxation.add(
        scheme);
  } else {
    getMutableURLSchemesRegistry().schemesForbiddenFromDomainRelaxation.remove(
        scheme);
  }
}

bool SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().schemesForbiddenFromDomainRelaxation.contains(
      scheme);
}

bool SchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  return scheme == "blob" || scheme == "filesystem";
}

void SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().notAllowingJavascriptURLsSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().notAllowingJavascriptURLsSchemes.contains(
      scheme);
}

void SchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().CORSEnabledSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().CORSEnabledSchemes.contains(scheme);
}

String SchemeRegistry::listOfCORSEnabledURLSchemes() {
  StringBuilder builder;
  bool addSeparator = false;
  for (const auto& scheme : getURLSchemesRegistry().CORSEnabledSchemes) {
    if (addSeparator)
      builder.append(", ");
    else
      addSeparator = true;

    builder.append(scheme);
  }
  return builder.toString();
}

bool SchemeRegistry::shouldTreatURLSchemeAsLegacy(const String& scheme) {
  return scheme == "ftp" || scheme == "gopher";
}

void SchemeRegistry::registerURLSchemeAsAllowingServiceWorkers(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().serviceWorkerSchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().serviceWorkerSchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().fetchAPISchemes.add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().fetchAPISchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().firstPartyWhenTopLevelSchemes.add(scheme);
}

void SchemeRegistry::removeURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().firstPartyWhenTopLevelSchemes.remove(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().firstPartyWhenTopLevelSchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().allowedInReferrerSchemes.add(scheme);
}

void SchemeRegistry::removeURLSchemeAsAllowedForReferrer(const String& scheme) {
  getMutableURLSchemesRegistry().allowedInReferrerSchemes.remove(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return getURLSchemesRegistry().allowedInReferrerSchemes.contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policyAreas) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().contentSecurityPolicyBypassingSchemes.add(
      scheme, policyAreas);
}

void SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().contentSecurityPolicyBypassingSchemes.remove(
      scheme);
}

bool SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policyAreas) {
  ASSERT(policyAreas != PolicyAreaNone);
  if (scheme.isEmpty() || policyAreas == PolicyAreaNone)
    return false;

  // get() returns 0 (PolicyAreaNone) if there is no entry in the map.
  // Thus by default, schemes do not bypass CSP.
  return (getURLSchemesRegistry().contentSecurityPolicyBypassingSchemes.get(
              scheme) &
          policyAreas) == policyAreas;
}

void SchemeRegistry::registerURLSchemeBypassingSecureContextCheck(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  getMutableURLSchemesRegistry().secureContextBypassingSchemes.add(scheme);
}

bool SchemeRegistry::schemeShouldBypassSecureContextCheck(
    const String& scheme) {
  if (scheme.isEmpty())
    return false;
  DCHECK_EQ(scheme, scheme.lower());
  return getURLSchemesRegistry().secureContextBypassingSchemes.contains(scheme);
}

}  // namespace blink
