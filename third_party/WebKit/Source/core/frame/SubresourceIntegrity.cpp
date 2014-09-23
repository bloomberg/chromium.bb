// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"
#include "platform/Crypto.h"
#include "platform/ParsingUtilities.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ASCIICType.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

// FIXME: This should probably use common functions with ContentSecurityPolicy.
static bool isIntegrityCharacter(UChar c)
{
    // Check if it's a base64 encoded value.
    return isASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '=';
}

static bool DigestsEqual(const DigestValue& digest1, const DigestValue& digest2)
{
    if (digest1.size() != digest2.size())
        return false;

    for (size_t i = 0; i < digest1.size(); i++) {
        if (digest1[i] != digest2[i])
            return false;
    }

    return true;
}

// FIXME: If CheckSubresourceIntegrity fails, Blink should create a console
// message to alert the developer of the failure.
bool SubresourceIntegrity::CheckSubresourceIntegrity(const Element& element, const String& source, const KURL& resourceUrl)
{
    if (!RuntimeEnabledFeatures::subresourceIntegrityEnabled())
        return true;

    if (!element.fastHasAttribute(HTMLNames::integrityAttr))
        return true;

    // FIXME: If insecureOriginMsg is not empty after the check, Blink
    // should send a console message.
    //
    // Instead of just checking SecurityOrigin::isSecure on resourceUrl, this
    // checks canAccessFeatureRequiringSecureOrigin so that file:// protocols
    // and localhost resources can be allowed. These may be useful for testing
    // and are allowed for features requiring authenticated origins, so Chrome
    // allows them here.
    String insecureOriginMsg = "";
    RefPtr<SecurityOrigin> resourceSecurityOrigin = SecurityOrigin::create(resourceUrl);
    if (!element.document().securityOrigin()->canAccessFeatureRequiringSecureOrigin(insecureOriginMsg)) {
        UseCounter::count(element.document(), UseCounter::SRIElementWithIntegrityAttributeAndInsecureOrigin);
        return false;
    }
    if (!resourceSecurityOrigin->canAccessFeatureRequiringSecureOrigin(insecureOriginMsg)) {
        UseCounter::count(element.document(), UseCounter::SRIElementWithIntegrityAttributeAndInsecureResource);
        return false;
    }

    String integrity;
    HashAlgorithm algorithm;
    if (!parseIntegrityAttribute(element.fastGetAttribute(HTMLNames::integrityAttr), integrity, algorithm)) {
        UseCounter::count(element.document(), UseCounter::SRIElementWithUnparsableIntegrityAttribute);
        return false;
    }

    Vector<char> hashVector;
    base64Decode(integrity, hashVector);

    StringUTF8Adaptor normalizedSource(source, StringUTF8Adaptor::Normalize, WTF::EntitiesForUnencodables);

    DigestValue digest;
    bool digestSuccess = computeDigest(algorithm, normalizedSource.data(), normalizedSource.length(), digest);

    if (digestSuccess) {
        DigestValue convertedHashVector;
        convertedHashVector.append(reinterpret_cast<uint8_t*>(hashVector.data()), hashVector.size());
        if (DigestsEqual(digest, convertedHashVector)) {
            UseCounter::count(element.document(), UseCounter::SRIElementWithMatchingIntegrityAttribute);
            return true;
        }
    }

    UseCounter::count(element.document(), UseCounter::SRIElementWithNonMatchingIntegrityAttribute);
    return false;
}

bool SubresourceIntegrity::parseIntegrityAttribute(const String& attribute, String& integrity, HashAlgorithm& algorithm)
{
    DEFINE_STATIC_LOCAL(const String, integrityPrefix, ("ni://"));
    // Any additions or subtractions from this struct should also modify the
    // respective entries in the kAlgorithmMap array in checkDigest().
    static const struct {
        const char* prefix;
        HashAlgorithm algorithm;
    } kSupportedPrefixes[] = {
        { "sha256", HashAlgorithmSha256 },
        { "sha384", HashAlgorithmSha384 },
        { "sha512", HashAlgorithmSha512 }
    };
    Vector<UChar> characters;
    attribute.stripWhiteSpace().appendTo(characters);
    UChar* begin = characters.data();
    UChar* end = characters.end();

    if (characters.size() < 1)
        return false;

    if (!equalIgnoringCase(integrityPrefix.characters8(), begin, integrityPrefix.length()))
        return false;

    const UChar* algorithmStart = begin + integrityPrefix.length();
    const UChar* algorithmEnd = algorithmStart;

    skipUntil<UChar>(algorithmEnd, end, ';');

    // Instead of this sizeof() calculation to get the length of this array,
    // it would be preferable to use WTF_ARRAY_LENGTH for simplicity and to
    // guarantee a compile time calculation. Unfortunately, on some
    // compliers, the call to WTF_ARRAY_LENGTH fails on arrays of anonymous
    // stucts, so, for now, it is necessary to resort to this sizeof
    // calculation.
    size_t i = 0;
    size_t kSupportedPrefixesLength = sizeof(kSupportedPrefixes) / sizeof(kSupportedPrefixes[0]);
    for (; i < kSupportedPrefixesLength; i++) {
        if (equalIgnoringCase(kSupportedPrefixes[i].prefix, algorithmStart, strlen(kSupportedPrefixes[i].prefix))) {
            algorithm = kSupportedPrefixes[i].algorithm;
            break;
        }
    }

    if (i == kSupportedPrefixesLength)
        return false;

    const UChar* integrityStart = algorithmEnd;
    if (!skipExactly<UChar>(integrityStart, end, ';'))
        return false;

    const UChar* integrityEnd = integrityStart;
    skipWhile<UChar, isIntegrityCharacter>(integrityEnd, end);
    if (integrityEnd != end)
        return false;

    integrity = String(integrityStart, integrityEnd - integrityStart);
    return true;
}

} // namespace blink
