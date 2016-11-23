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

void checkIsBeforeThreadCreated() {
#if DCHECK_IS_ON()
  DCHECK(WTF::isBeforeThreadCreated());
#endif
}

}  // namespace

static URLSchemesSet& localURLSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, localSchemes, ());

  if (localSchemes.isEmpty())
    localSchemes.add("file");

  return localSchemes;
}

static URLSchemesSet& displayIsolatedURLSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, displayIsolatedSchemes, ());
  return displayIsolatedSchemes;
}

static URLSchemesSet& secureSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, secureSchemes,
                      ({
                          "https", "about", "data", "wss",
                      }));
  return secureSchemes;
}

static URLSchemesSet& schemesWithUniqueOrigins() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, schemesWithUniqueOrigins,
                      ({
                          "about", "javascript", "data",
                      }));
  return schemesWithUniqueOrigins;
}

static URLSchemesSet& emptyDocumentSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, emptyDocumentSchemes, ({
                                                               "about",
                                                           }));
  return emptyDocumentSchemes;
}

static HashSet<String>& schemesForbiddenFromDomainRelaxation() {
  DEFINE_STATIC_LOCAL(HashSet<String>, schemes, ());
  return schemes;
}

static URLSchemesSet& notAllowingJavascriptURLsSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, notAllowingJavascriptURLsSchemes, ());
  return notAllowingJavascriptURLsSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  localURLSchemes().add(scheme);
}

const URLSchemesSet& SchemeRegistry::localSchemes() {
  return localURLSchemes();
}

static URLSchemesSet& CORSEnabledSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, CORSEnabledSchemes, ());

  if (CORSEnabledSchemes.isEmpty()) {
    CORSEnabledSchemes.add("http");
    CORSEnabledSchemes.add("https");
    CORSEnabledSchemes.add("data");
  }

  return CORSEnabledSchemes;
}

static URLSchemesSet& serviceWorkerSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, serviceWorkerSchemes, ());

  if (serviceWorkerSchemes.isEmpty()) {
    // HTTP is required because http://localhost is considered secure.
    // Additional checks are performed to ensure that other http pages
    // are filtered out.
    serviceWorkerSchemes.add("http");
    serviceWorkerSchemes.add("https");
  }

  return serviceWorkerSchemes;
}

static URLSchemesSet& fetchAPISchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, fetchAPISchemes, ());

  if (fetchAPISchemes.isEmpty()) {
    fetchAPISchemes.add("http");
    fetchAPISchemes.add("https");
  }

  return fetchAPISchemes;
}

static URLSchemesSet& firstPartyWhenTopLevelSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, firstPartyWhenTopLevelSchemes, ());
  return firstPartyWhenTopLevelSchemes;
}

static URLSchemesMap<SchemeRegistry::PolicyAreas>&
ContentSecurityPolicyBypassingSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesMap<SchemeRegistry::PolicyAreas>, schemes, ());
  return schemes;
}

static URLSchemesSet& secureContextBypassingSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, secureContextBypassingSchemes, ());
  return secureContextBypassingSchemes;
}

static URLSchemesSet& allowedInReferrerSchemes() {
  DEFINE_STATIC_LOCAL(URLSchemesSet, allowedInReferrerSchemes, ());

  if (allowedInReferrerSchemes.isEmpty()) {
    allowedInReferrerSchemes.add("http");
    allowedInReferrerSchemes.add("https");
  }

  return allowedInReferrerSchemes;
}

// All new maps should be added here. Must be called before we create other
// threads to avoid racy static local initialization.
void SchemeRegistry::initialize() {
  localURLSchemes();
  displayIsolatedURLSchemes();
  secureSchemes();
  schemesWithUniqueOrigins();
  emptyDocumentSchemes();
  schemesForbiddenFromDomainRelaxation();
  notAllowingJavascriptURLsSchemes();
  CORSEnabledSchemes();
  serviceWorkerSchemes();
  fetchAPISchemes();
  firstPartyWhenTopLevelSchemes();
  ContentSecurityPolicyBypassingSchemes();
  secureContextBypassingSchemes();
  allowedInReferrerSchemes();
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return localURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  schemesWithUniqueOrigins().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return schemesWithUniqueOrigins().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  displayIsolatedURLSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return displayIsolatedURLSchemes().contains(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsRestrictingMixedContent(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  return scheme == "https";
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  secureSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return secureSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  emptyDocumentSchemes().add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return emptyDocumentSchemes().contains(scheme);
}

void SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return;

  if (forbidden)
    schemesForbiddenFromDomainRelaxation().add(scheme);
  else
    schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool SchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  return scheme == "blob" || scheme == "filesystem";
}

void SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  notAllowingJavascriptURLsSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return notAllowingJavascriptURLsSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  CORSEnabledSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return CORSEnabledSchemes().contains(scheme);
}

String SchemeRegistry::listOfCORSEnabledURLSchemes() {
  StringBuilder builder;
  bool addSeparator = false;
  for (const auto& scheme : CORSEnabledSchemes()) {
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
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  serviceWorkerSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkers(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return serviceWorkerSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  fetchAPISchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return fetchAPISchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  firstPartyWhenTopLevelSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  firstPartyWhenTopLevelSchemes().remove(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return firstPartyWhenTopLevelSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  allowedInReferrerSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeAsAllowedForReferrer(const String& scheme) {
  checkIsBeforeThreadCreated();
  allowedInReferrerSchemes().remove(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.lower());
  if (scheme.isEmpty())
    return false;
  return allowedInReferrerSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policyAreas) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  ContentSecurityPolicyBypassingSchemes().add(scheme, policyAreas);
}

void SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policyAreas) {
  ASSERT(policyAreas != PolicyAreaNone);
  if (scheme.isEmpty() || policyAreas == PolicyAreaNone)
    return false;

  // get() returns 0 (PolicyAreaNone) if there is no entry in the map.
  // Thus by default, schemes do not bypass CSP.
  return (ContentSecurityPolicyBypassingSchemes().get(scheme) & policyAreas) ==
         policyAreas;
}

void SchemeRegistry::registerURLSchemeBypassingSecureContextCheck(
    const String& scheme) {
  checkIsBeforeThreadCreated();
  DCHECK_EQ(scheme, scheme.lower());
  secureContextBypassingSchemes().add(scheme.lower());
}

bool SchemeRegistry::schemeShouldBypassSecureContextCheck(
    const String& scheme) {
  if (scheme.isEmpty())
    return false;
  return secureContextBypassingSchemes().contains(scheme.lower());
}

}  // namespace blink
