// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Crypto.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/dtoa/utils.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/ParsingUtilities.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"

namespace blink {

// FIXME: This should probably use common functions with ContentSecurityPolicy.
static bool IsIntegrityCharacter(UChar c) {
  // Check if it's a base64 encoded value. We're pretty loose here, as there's
  // not much risk in it, and it'll make it simpler for developers.
  return IsASCIIAlphanumeric(c) || c == '_' || c == '-' || c == '+' ||
         c == '/' || c == '=';
}

static bool IsValueCharacter(UChar c) {
  // VCHAR per https://tools.ietf.org/html/rfc5234#appendix-B.1
  return c >= 0x21 && c <= 0x7e;
}

static void LogErrorToConsole(const String& message,
                              ExecutionContext& execution_context) {
  execution_context.AddConsoleMessage(ConsoleMessage::Create(
      kSecurityMessageSource, kErrorMessageLevel, message));
}

static bool DigestsEqual(const DigestValue& digest1,
                         const DigestValue& digest2) {
  if (digest1.size() != digest2.size())
    return false;

  for (size_t i = 0; i < digest1.size(); i++) {
    if (digest1[i] != digest2[i])
      return false;
  }

  return true;
}

static String DigestToString(const DigestValue& digest) {
  return Base64Encode(reinterpret_cast<const char*>(digest.data()),
                      digest.size(), kBase64DoNotInsertLFs);
}

HashAlgorithm SubresourceIntegrity::GetPrioritizedHashFunction(
    HashAlgorithm algorithm1,
    HashAlgorithm algorithm2) {
  const HashAlgorithm kWeakerThanSha384[] = {kHashAlgorithmSha256};
  const HashAlgorithm kWeakerThanSha512[] = {kHashAlgorithmSha256,
                                             kHashAlgorithmSha384};

  DCHECK_NE(algorithm1, kHashAlgorithmSha1);
  DCHECK_NE(algorithm2, kHashAlgorithmSha1);

  if (algorithm1 == algorithm2)
    return algorithm1;

  const HashAlgorithm* weaker_algorithms = 0;
  size_t length = 0;
  switch (algorithm1) {
    case kHashAlgorithmSha256:
      break;
    case kHashAlgorithmSha384:
      weaker_algorithms = kWeakerThanSha384;
      length = WTF_ARRAY_LENGTH(kWeakerThanSha384);
      break;
    case kHashAlgorithmSha512:
      weaker_algorithms = kWeakerThanSha512;
      length = WTF_ARRAY_LENGTH(kWeakerThanSha512);
      break;
    default:
      NOTREACHED();
  };

  for (size_t i = 0; i < length; i++) {
    if (weaker_algorithms[i] == algorithm2)
      return algorithm1;
  }

  return algorithm2;
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const String& integrity_attribute,
    Document& document,
    const char* content,
    size_t size,
    const KURL& resource_url,
    const Resource& resource) {
  if (integrity_attribute.IsEmpty())
    return true;

  IntegrityMetadataSet metadata_set;
  IntegrityParseResult integrity_parse_result =
      ParseIntegrityAttribute(integrity_attribute, metadata_set, &document);
  // On failed parsing, there's no need to log an error here, as
  // parseIntegrityAttribute() will output an appropriate console message.
  if (integrity_parse_result != kIntegrityParseValidResult)
    return true;

  return CheckSubresourceIntegrity(metadata_set, document, content, size,
                                   resource_url, resource);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    Document& document,
    const char* content,
    size_t size,
    const KURL& resource_url,
    const Resource& resource) {
  if (!resource.IsSameOriginOrCORSSuccessful()) {
    UseCounter::Count(document,
                      WebFeature::kSRIElementIntegrityAttributeButIneligible);
    LogErrorToConsole("Subresource Integrity: The resource '" +
                          resource_url.ElidedString() +
                          "' has an integrity attribute, but the resource "
                          "requires the request to be CORS enabled to check "
                          "the integrity, and it is not. The resource has been "
                          "blocked because the integrity cannot be enforced.",
                      document);
    return false;
  }

  String error_message;
  bool result = CheckSubresourceIntegrity(
      metadata_set, content, size, resource_url, document, error_message);
  if (!result)
    LogErrorToConsole(error_message, document);
  return result;
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const String& integrity_metadata,
    const char* content,
    size_t size,
    const KURL& resource_url,
    ExecutionContext& execution_context,
    String& error_message) {
  IntegrityMetadataSet metadata_set;
  IntegrityParseResult integrity_parse_result = ParseIntegrityAttribute(
      integrity_metadata, metadata_set, &execution_context);
  // On failed parsing, there's no need to log an error here, as
  // parseIntegrityAttribute() will output an appropriate console message.
  if (integrity_parse_result != kIntegrityParseValidResult)
    return true;

  return CheckSubresourceIntegrity(metadata_set, content, size, resource_url,
                                   execution_context, error_message);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    const char* content,
    size_t size,
    const KURL& resource_url,
    ExecutionContext& execution_context,
    String& error_message) {
  if (!metadata_set.size())
    return true;

  HashAlgorithm strongest_algorithm = kHashAlgorithmSha256;
  for (const IntegrityMetadata& metadata : metadata_set)
    strongest_algorithm =
        GetPrioritizedHashFunction(metadata.Algorithm(), strongest_algorithm);

  DigestValue digest;
  for (const IntegrityMetadata& metadata : metadata_set) {
    if (metadata.Algorithm() != strongest_algorithm)
      continue;

    digest.clear();
    bool digest_success =
        ComputeDigest(metadata.Algorithm(), content, size, digest);

    if (digest_success) {
      Vector<char> hash_vector;
      Base64Decode(metadata.Digest(), hash_vector);
      DigestValue converted_hash_vector;
      converted_hash_vector.Append(
          reinterpret_cast<uint8_t*>(hash_vector.data()), hash_vector.size());

      if (DigestsEqual(digest, converted_hash_vector)) {
        UseCounter::Count(
            &execution_context,
            WebFeature::kSRIElementWithMatchingIntegrityAttribute);
        return true;
      }
    }
  }

  digest.clear();
  if (ComputeDigest(kHashAlgorithmSha256, content, size, digest)) {
    // This message exposes the digest of the resource to the console.
    // Because this is only to the console, that's okay for now, but we
    // need to be very careful not to expose this in exceptions or
    // JavaScript, otherwise it risks exposing information about the
    // resource cross-origin.
    error_message =
        "Failed to find a valid digest in the 'integrity' attribute for "
        "resource '" +
        resource_url.ElidedString() + "' with computed SHA-256 integrity '" +
        DigestToString(digest) + "'. The resource has been blocked.";
  } else {
    error_message =
        "There was an error computing an integrity value for resource '" +
        resource_url.ElidedString() + "'. The resource has been blocked.";
  }
  UseCounter::Count(&execution_context,
                    WebFeature::kSRIElementWithNonMatchingIntegrityAttribute);
  return false;
}

// Before:
//
// [algorithm]-[hash]
// ^                 ^
// position          end
//
// After (if successful: if the method does not return AlgorithmValid, we make
// no promises and the caller should exit early):
//
// [algorithm]-[hash]
//            ^      ^
//     position    end
SubresourceIntegrity::AlgorithmParseResult SubresourceIntegrity::ParseAlgorithm(
    const UChar*& position,
    const UChar* end,
    HashAlgorithm& algorithm) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kAlgorithmMap array in checkDigest() as well
  // as the array in algorithmToString().
  static const struct {
    const char* prefix;
    HashAlgorithm algorithm;
  } kSupportedPrefixes[] = {
      {"sha256", kHashAlgorithmSha256}, {"sha-256", kHashAlgorithmSha256},
      {"sha384", kHashAlgorithmSha384}, {"sha-384", kHashAlgorithmSha384},
      {"sha512", kHashAlgorithmSha512}, {"sha-512", kHashAlgorithmSha512}};

  const UChar* begin = position;

  for (auto& prefix : kSupportedPrefixes) {
    if (skipToken<UChar>(position, end, prefix.prefix)) {
      if (!skipExactly<UChar>(position, end, '-')) {
        position = begin;
        continue;
      }
      algorithm = prefix.algorithm;
      return kAlgorithmValid;
    }
  }

  skipUntil<UChar>(position, end, '-');
  if (position < end && *position == '-') {
    position = begin;
    return kAlgorithmUnknown;
  }

  position = begin;
  return kAlgorithmUnparsable;
}

// Before:
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//             ^     ^                        ^               ^
//      position   end                 position             end
//
// After (if successful: if the method returns false, we make no promises and
// the caller should exit early):
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//                   ^                              ^         ^
//        position/end                       position       end
bool SubresourceIntegrity::ParseDigest(const UChar*& position,
                                       const UChar* end,
                                       String& digest) {
  const UChar* begin = position;
  skipWhile<UChar, IsIntegrityCharacter>(position, end);

  if (position == begin || (position != end && *position != '?')) {
    digest = g_empty_string;
    return false;
  }

  // We accept base64url encoding, but normalize to "normal" base64 internally:
  digest = NormalizeToBase64(String(begin, position - begin));
  return true;
}

SubresourceIntegrity::IntegrityParseResult
SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityMetadataSet& metadata_set) {
  return ParseIntegrityAttribute(attribute, metadata_set, nullptr);
}

SubresourceIntegrity::IntegrityParseResult
SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityMetadataSet& metadata_set,
    ExecutionContext* execution_context) {
  Vector<UChar> characters;
  attribute.StripWhiteSpace().AppendTo(characters);
  const UChar* position = characters.data();
  const UChar* end = characters.end();
  const UChar* current_integrity_end;

  metadata_set.clear();
  bool error = false;

  // The integrity attribute takes the form:
  //    *WSP hash-with-options *( 1*WSP hash-with-options ) *WSP / *WSP
  // To parse this, break on whitespace, parsing each algorithm/digest/option
  // in order.
  while (position < end) {
    WTF::String digest;
    HashAlgorithm algorithm;

    skipWhile<UChar, IsASCIISpace>(position, end);
    current_integrity_end = position;
    skipUntil<UChar, IsASCIISpace>(current_integrity_end, end);

    // Algorithm parsing errors are non-fatal (the subresource should
    // still be loaded) because strong hash algorithms should be used
    // without fear of breaking older user agents that don't support
    // them.
    AlgorithmParseResult parse_result =
        ParseAlgorithm(position, current_integrity_end, algorithm);
    if (parse_result == kAlgorithmUnknown) {
      // Unknown hash algorithms are treated as if they're not present,
      // and thus are not marked as an error, they're just skipped.
      skipUntil<UChar, IsASCIISpace>(position, end);
      if (execution_context) {
        LogErrorToConsole("Error parsing 'integrity' attribute ('" + attribute +
                              "'). The specified hash algorithm must be one of "
                              "'sha256', 'sha384', or 'sha512'.",
                          *execution_context);
        UseCounter::Count(
            execution_context,
            WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    if (parse_result == kAlgorithmUnparsable) {
      error = true;
      skipUntil<UChar, IsASCIISpace>(position, end);
      if (execution_context) {
        LogErrorToConsole("Error parsing 'integrity' attribute ('" + attribute +
                              "'). The hash algorithm must be one of 'sha256', "
                              "'sha384', or 'sha512', followed by a '-' "
                              "character.",
                          *execution_context);
        UseCounter::Count(
            execution_context,
            WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    DCHECK_EQ(parse_result, kAlgorithmValid);

    if (!ParseDigest(position, current_integrity_end, digest)) {
      error = true;
      skipUntil<UChar, IsASCIISpace>(position, end);
      if (execution_context) {
        LogErrorToConsole(
            "Error parsing 'integrity' attribute ('" + attribute +
                "'). The digest must be a valid, base64-encoded value.",
            *execution_context);
        UseCounter::Count(
            execution_context,
            WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    // The spec defines a space in the syntax for options, separated by a
    // '?' character followed by unbounded VCHARs, but no actual options
    // have been defined yet. Thus, for forward compatibility, ignore any
    // options specified.
    if (skipExactly<UChar>(position, end, '?')) {
      const UChar* begin = position;
      skipWhile<UChar, IsValueCharacter>(position, end);
      if (begin != position && execution_context) {
        LogErrorToConsole(
            "Ignoring unrecogized 'integrity' attribute option '" +
                String(begin, position - begin) + "'.",
            *execution_context);
      }
    }

    IntegrityMetadata integrity_metadata(digest, algorithm);
    metadata_set.insert(integrity_metadata.ToPair());
  }

  if (metadata_set.size() == 0 && error)
    return kIntegrityParseNoValidResult;

  return kIntegrityParseValidResult;
}

}  // namespace blink
