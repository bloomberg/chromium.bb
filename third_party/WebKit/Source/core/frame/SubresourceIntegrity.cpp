// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/ConsoleTypes.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
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
    // FIXME: This should be checking base64url encoding, not base64 encoding.

    // Check if it's a base64 encoded value.
    return isASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '=';
}

static void logErrorToConsole(const String& message, Document& document)
{
    document.addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
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

static String algorithmToString(HashAlgorithm algorithm)
{
    static const struct {
        HashAlgorithm algorithm;
        const char* name;
    } kAlgorithmToString[] = {
        {  HashAlgorithmSha256, "SHA-256" },
        {  HashAlgorithmSha384, "SHA-384" },
        {  HashAlgorithmSha512, "SHA-512" }
    };

    // See comment in parseIntegrityAttribute about why sizeof() is used
    // instead of WTF_ARRAY_LENGTH.
    size_t i = 0;
    size_t kSupportedAlgorithmsLength = sizeof(kAlgorithmToString) / sizeof(kAlgorithmToString[0]);
    for (; i < kSupportedAlgorithmsLength; i++) {
        if (kAlgorithmToString[i].algorithm == algorithm)
            return kAlgorithmToString[i].name;
    }

    ASSERT_NOT_REACHED();
    return String();
}

static String digestToString(const DigestValue& digest)
{
    return base64Encode(reinterpret_cast<const char*>(digest.data()), digest.size(), Base64DoNotInsertLFs);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const Element& element, const String& source, const KURL& resourceUrl)
{
    if (!RuntimeEnabledFeatures::subresourceIntegrityEnabled())
        return true;

    if (!element.fastHasAttribute(HTMLNames::integrityAttr))
        return true;

    Document& document = element.document();

    // Instead of just checking SecurityOrigin::isSecure on resourceUrl, this
    // checks canAccessFeatureRequiringSecureOrigin so that file:// protocols
    // and localhost resources can be allowed. These may be useful for testing
    // and are allowed for features requiring authenticated origins, so Chrome
    // allows them here.
    String insecureOriginMsg = "";
    RefPtr<SecurityOrigin> resourceSecurityOrigin = SecurityOrigin::create(resourceUrl);
    if (!document.securityOrigin()->canAccessFeatureRequiringSecureOrigin(insecureOriginMsg)) {
        UseCounter::count(document, UseCounter::SRIElementWithIntegrityAttributeAndInsecureOrigin);
        // FIXME: This console message should probably utilize
        // inesecureOriginMsg to give a more helpful message to the user.
        logErrorToConsole("The 'integrity' attribute may only be used in documents in secure origins.", document);
        return false;
    }
    if (!resourceSecurityOrigin->canAccessFeatureRequiringSecureOrigin(insecureOriginMsg)) {
        UseCounter::count(document, UseCounter::SRIElementWithIntegrityAttributeAndInsecureResource);
        logErrorToConsole("The 'integrity' attribute may only be used with resources on secure origins.", document);
        return false;
    }

    String integrity;
    HashAlgorithm algorithm;
    String attribute = element.fastGetAttribute(HTMLNames::integrityAttr);
    if (!parseIntegrityAttribute(attribute, integrity, algorithm, document)) {
        // An error is logged to the console during parsing; we don't need to log one here.
        UseCounter::count(document, UseCounter::SRIElementWithUnparsableIntegrityAttribute);
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
            UseCounter::count(document, UseCounter::SRIElementWithMatchingIntegrityAttribute);
            return true;
        } else {
            // This message exposes the digest of the resource to the console.
            // Because this is only to the console, that's okay for now, but we
            // need to be very careful not to expose this in exceptions or
            // JavaScript, otherwise it risks exposing information about the
            // resource cross-origin.
            logErrorToConsole("The computed " + algorithmToString(algorithm) + " integrity '" + digestToString(digest) + "' does not match the 'integrity' attribute '" + integrity + "' for resource '" + resourceUrl.elidedString() + "'.", document);
        }
    } else {
        logErrorToConsole("There was an error computing an 'integrity' value for resource '" + resourceUrl.elidedString() + "'.", document);
    }

    UseCounter::count(document, UseCounter::SRIElementWithNonMatchingIntegrityAttribute);
    return false;
}

// Before:
//
// ni:///[algorithm];[hash]
//       ^                 ^
//       position          end
//
// After:
//
// ni:///[algorithm];[hash]
//                  ^      ^
//           position    end
bool SubresourceIntegrity::parseAlgorithm(const UChar*& position, const UChar* end, HashAlgorithm& algorithm)
{
    // Any additions or subtractions from this struct should also modify the
    // respective entries in the kAlgorithmMap array in checkDigest() as well
    // as the array in algorithmToString().
    static const struct {
        const char* prefix;
        HashAlgorithm algorithm;
    } kSupportedPrefixes[] = {
        { "sha256", HashAlgorithmSha256 },
        { "sha384", HashAlgorithmSha384 },
        { "sha512", HashAlgorithmSha512 }
    };

    for (auto& prefix : kSupportedPrefixes) {
        if (skipToken<UChar>(position, end, prefix.prefix)) {
            algorithm = prefix.algorithm;
            return true;
        }
    }

    return false;
}

// Before:
//
// ni:///[algorithm];[hash]     OR      ni:///[algorithm];[hash]?[params]
//                   ^     ^                              ^              ^
//            position   end                       position            end
//
// After:
//
// ni:///[algorithm];[hash]     OR      ni:///[algorithm];[hash]?[params]
//                         ^                                    ^        ^
//                         position/end                  position      end
bool SubresourceIntegrity::parseDigest(const UChar*& position, const UChar* end, String& digest)
{
    const UChar* begin = position;
    skipWhile<UChar, isIntegrityCharacter>(position, end);

    if (position == begin || (position != end && *position != '?')) {
        digest = emptyString();
        return false;
    }

    digest = String(begin, position - begin);
    return true;
}

bool SubresourceIntegrity::parseIntegrityAttribute(const String& attribute, String& digest, HashAlgorithm& algorithm, Document& document)
{
    Vector<UChar> characters;
    attribute.stripWhiteSpace().appendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();

    if (!skipToken<UChar>(position, end, "ni:///")) {
        logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The value must begin with 'ni:///'.", document);
        return false;
    }

    if (!parseAlgorithm(position, end, algorithm)) {
        logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The specified hash algorithm must be one of 'sha256', 'sha384', or 'sha512'.", document);
        return false;
    }

    if (!skipExactly<UChar>(position, end, ';')) {
        logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The hash algorithm must be followed by a ';' character.", document);
        return false;
    }

    if (!parseDigest(position, end, digest)) {
        logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The digest must be a valid, base64-encoded value.", document);
        return false;
    }

    // FIXME: Parse params in order to get content type (e.g. "?ct=application/javascript")

    return true;
}

} // namespace blink
