/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
 */

#ifndef ResourceError_h
#define ResourceError_h

// TODO(toyoshim): Move net/base inclusion from header file.
#include <iosfwd>
#include "net/base/net_errors.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class WebURL;
enum class ResourceRequestBlockedReason;
struct WebURLError;

// Used for errors that won't be exposed to clients.
PLATFORM_EXPORT extern const char kErrorDomainBlinkInternal[];

class PLATFORM_EXPORT ResourceError final {
  DISALLOW_NEW();

 public:
  enum Error {
    ACCESS_DENIED = net::ERR_ACCESS_DENIED,
    BLOCKED_BY_XSS_AUDITOR = net::ERR_BLOCKED_BY_XSS_AUDITOR
  };

  static ResourceError CancelledError(const String& failing_url);
  static ResourceError CancelledDueToAccessCheckError(
      const String& failing_url,
      ResourceRequestBlockedReason);
  static ResourceError CancelledDueToAccessCheckError(
      const String& failing_url,
      ResourceRequestBlockedReason,
      const String& localized_description);

  // Only for Blink internal usage.
  static ResourceError CacheMissError(const String& failing_url);

  ResourceError()
      : error_code_(0),
        is_null_(true),
        is_cancellation_(false),
        is_access_check_(false),
        is_timeout_(false),
        stale_copy_in_cache_(false),
        was_ignored_by_handler_(false),
        is_cache_miss_(false),
        should_collapse_initiator_(false) {}

  ResourceError(const String& domain,
                int error_code,
                const String& failing_url,
                const String& localized_description)
      : domain_(domain),
        error_code_(error_code),
        failing_url_(failing_url),
        localized_description_(localized_description),
        is_null_(false),
        is_cancellation_(false),
        is_access_check_(false),
        is_timeout_(false),
        stale_copy_in_cache_(false),
        was_ignored_by_handler_(false),
        is_cache_miss_(false),
        should_collapse_initiator_(false) {}

  // Makes a deep copy. Useful for when you need to use a ResourceError on
  // another thread.
  ResourceError Copy() const;

  bool IsNull() const { return is_null_; }

  const String& Domain() const { return domain_; }
  int ErrorCode() const { return error_code_; }
  const String& FailingURL() const { return failing_url_; }
  const String& LocalizedDescription() const { return localized_description_; }

  void SetIsCancellation(bool is_cancellation) {
    is_cancellation_ = is_cancellation;
  }
  bool IsCancellation() const { return is_cancellation_; }

  void SetIsAccessCheck(bool is_access_check) {
    is_access_check_ = is_access_check;
  }
  bool IsAccessCheck() const { return is_access_check_; }

  void SetIsTimeout(bool is_timeout) { is_timeout_ = is_timeout; }
  bool IsTimeout() const { return is_timeout_; }
  void SetStaleCopyInCache(bool stale_copy_in_cache) {
    stale_copy_in_cache_ = stale_copy_in_cache;
  }
  bool StaleCopyInCache() const { return stale_copy_in_cache_; }

  void SetWasIgnoredByHandler(bool ignored_by_handler) {
    was_ignored_by_handler_ = ignored_by_handler;
  }
  bool WasIgnoredByHandler() const { return was_ignored_by_handler_; }

  void SetIsCacheMiss(bool is_cache_miss) { is_cache_miss_ = is_cache_miss; }
  bool IsCacheMiss() const { return is_cache_miss_; }
  bool WasBlockedByResponse() const {
    return error_code_ == net::ERR_BLOCKED_BY_RESPONSE;
  }

  void SetShouldCollapseInitiator(bool should_collapse_initiator) {
    should_collapse_initiator_ = should_collapse_initiator;
  }
  bool ShouldCollapseInitiator() const { return should_collapse_initiator_; }

  static bool Compare(const ResourceError&, const ResourceError&);

  static void InitializeWebURLError(WebURLError*,
                                    const WebURL&,
                                    bool stale_copy_in_cache,
                                    int reason);

 private:
  String domain_;
  int error_code_;
  String failing_url_;
  String localized_description_;
  bool is_null_;
  bool is_cancellation_;
  bool is_access_check_;
  bool is_timeout_;
  bool stale_copy_in_cache_;
  bool was_ignored_by_handler_;
  bool is_cache_miss_;
  bool should_collapse_initiator_;
};

inline bool operator==(const ResourceError& a, const ResourceError& b) {
  return ResourceError::Compare(a, b);
}
inline bool operator!=(const ResourceError& a, const ResourceError& b) {
  return !(a == b);
}

// Pretty printer for gtest. Declared here to avoid ODR violations.
std::ostream& operator<<(std::ostream&, const ResourceError&);

}  // namespace blink

#endif  // ResourceError_h
