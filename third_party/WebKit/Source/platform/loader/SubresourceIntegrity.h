// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceIntegrity_h
#define SubresourceIntegrity_h

#include "base/gtest_prod_util.h"
#include "platform/PlatformExport.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class KURL;
class Resource;

class PLATFORM_EXPORT SubresourceIntegrity final {
  STATIC_ONLY(SubresourceIntegrity);

 public:
  class ReportInfo final {
   public:
    enum class UseCounterFeature {
      kSRIElementWithMatchingIntegrityAttribute,
      kSRIElementWithNonMatchingIntegrityAttribute,
      kSRIElementIntegrityAttributeButIneligible,
      kSRIElementWithUnparsableIntegrityAttribute,
    };

    void AddUseCount(UseCounterFeature);
    void AddConsoleErrorMessage(const String&);
    void Clear();

    const Vector<UseCounterFeature>& UseCounts() const { return use_counts_; }
    const Vector<String>& ConsoleErrorMessages() const {
      return console_error_messages_;
    }

   private:
    Vector<UseCounterFeature> use_counts_;
    Vector<String> console_error_messages_;
  };

  enum IntegrityParseResult {
    kIntegrityParseValidResult,
    kIntegrityParseNoValidResult
  };

  // The versions with the IntegrityMetadataSet passed as the first argument
  // assume that the integrity attribute has already been parsed, and the
  // IntegrityMetadataSet represents the result of that parsing.
  static bool CheckSubresourceIntegrity(const String& integrity_attribute,
                                        const char* content,
                                        size_t,
                                        const KURL& resource_url,
                                        const Resource&,
                                        ReportInfo&);
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const char* content,
                                        size_t,
                                        const KURL& resource_url,
                                        const Resource&,
                                        ReportInfo&);
  static bool CheckSubresourceIntegrity(const String&,
                                        const char*,
                                        size_t,
                                        const KURL& resource_url,
                                        ReportInfo&);
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const char*,
                                        size_t,
                                        const KURL& resource_url,
                                        ReportInfo&);

  // The IntegrityMetadataSet arguments are out parameters which contain the
  // set of all valid, parsed metadata from |attribute|.
  static IntegrityParseResult ParseIntegrityAttribute(
      const WTF::String& attribute,
      IntegrityMetadataSet&);
  static IntegrityParseResult ParseIntegrityAttribute(
      const WTF::String& attribute,
      IntegrityMetadataSet&,
      ReportInfo*);

 private:
  friend class SubresourceIntegrityTest;
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Parsing);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Prioritization);

  enum AlgorithmParseResult {
    kAlgorithmValid,
    kAlgorithmUnparsable,
    kAlgorithmUnknown
  };

  static IntegrityAlgorithm GetPrioritizedHashFunction(IntegrityAlgorithm,
                                                       IntegrityAlgorithm);
  static AlgorithmParseResult ParseAlgorithm(const UChar*& begin,
                                             const UChar* end,
                                             IntegrityAlgorithm&);
  static bool ParseDigest(const UChar*& begin,
                          const UChar* end,
                          String& digest);
};

}  // namespace blink

#endif
