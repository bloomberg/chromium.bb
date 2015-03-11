// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/fetch/Resource.h"
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
    // Check if it's a base64 encoded value. We're pretty loose here, as there's
    // not much risk in it, and it'll make it simpler for developers.
    return isASCIIAlphanumeric(c) || c == '_' || c == '-' || c == '+' || c == '/' || c == '=';
}

static bool isTypeCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '.' || c == '-';
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

    for (const auto& algorithmToString : kAlgorithmToString) {
        if (algorithmToString.algorithm == algorithm)
            return algorithmToString.name;
    }

    ASSERT_NOT_REACHED();
    return String();
}

static String digestToString(const DigestValue& digest)
{
    // We always output base64url encoded data, even though we use base64 internally.
    return base64URLEncode(reinterpret_cast<const char*>(digest.data()), digest.size(), Base64DoNotInsertLFs);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(const Element& element, const String& source, const KURL& resourceUrl, const String& resourceType, const Resource& resource)
{
    if (!RuntimeEnabledFeatures::subresourceIntegrityEnabled())
        return true;

    if (!element.fastHasAttribute(HTMLNames::integrityAttr))
        return true;

    Document& document = element.document();

    if (!resource.isEligibleForIntegrityCheck(&document)) {
        logErrorToConsole("Subresource Integrity: The resource '" + resourceUrl.elidedString() + "' has an integrity attribute, but the resource requires CORS to be enabled to check the integrity, and it is not. The resource has been blocked.", document);
        return false;
    }

    String integrity;
    HashAlgorithm algorithm;
    String type;
    String attribute = element.fastGetAttribute(HTMLNames::integrityAttr);
    if (!parseIntegrityAttribute(attribute, integrity, algorithm, type, document)) {
        // An error is logged to the console during parsing; we don't need to log one here.
        UseCounter::count(document, UseCounter::SRIElementWithUnparsableIntegrityAttribute);
        return false;
    }

    if (!type.isEmpty() && !equalIgnoringCase(type, resourceType)) {
        UseCounter::count(document, UseCounter::SRIElementWithNonMatchingIntegrityType);
        logErrorToConsole("Subresource Integrity: The resource '" + resourceUrl.elidedString() + "' was delivered as type '" + resourceType + "', which does not match the expected type '" + type + "'. The resource has been blocked.", document);
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
// After (if successful: if the method returns false, we make no promises and the caller should exit early):
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
        { "sha-256", HashAlgorithmSha256 },
        { "sha384", HashAlgorithmSha384 },
        { "sha-384", HashAlgorithmSha384 },
        { "sha512", HashAlgorithmSha512 },
        { "sha-512", HashAlgorithmSha512 }
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
// After (if successful: if the method returns false, we make no promises and the caller should exit early):
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

    // We accept base64url encoding, but normalize to "normal" base64 internally:
    digest = normalizeToBase64(String(begin, position - begin));
    return true;
}


// Before:
//
// ni:///[algorithm];[hash]     OR      ni:///[algorithm];[hash]?[params]
//                         ^                                    ^        ^
//              position/end                             position      end
//
// After (if successful: if the method returns false, we make no promises and the caller should exit early):
//
// ni:///[algorithm];[hash]     OR      ni:///[algorithm];[hash]?[params]
//                         ^                                             ^
//              position/end                                  position/end
bool SubresourceIntegrity::parseMimeType(const UChar*& position, const UChar* end, String& type)
{
    type = emptyString();
    if (position == end)
        return true;

    if (!skipToken<UChar>(position, end, "?ct="))
        return false;

    const UChar* begin = position;
    skipWhile<UChar, isASCIIAlpha>(position, end);
    if (position == end)
        return false;

    if (!skipExactly<UChar>(position, end, '/'))
        return false;

    if (position == end)
        return false;

    skipWhile<UChar, isTypeCharacter>(position, end);
    if (position != end)
        return false;

    type = String(begin, position - begin);
    return true;
}

bool SubresourceIntegrity::parseIntegrityAttribute(const String& attribute, String& digest, HashAlgorithm& algorithm, String& type, Document& document)
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

    if (!parseMimeType(position, end, type)) {
        logErrorToConsole("Error parsing 'integrity' attribute ('" + attribute + "'). The content type could not be parsed.", document);
        return false;
    }

    return true;
}

} // namespace blink
