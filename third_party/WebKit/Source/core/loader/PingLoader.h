/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef PingLoader_h
#define PingLoader_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebURLLoaderClient.h"

namespace blink {

class Blob;
class DOMArrayBufferView;
class EncodedFormData;
class FormData;
class LocalFrame;
class KURL;

// Issue an asynchronous, one-directional request at some resources, ignoring
// any response. The request is made independent of any LocalFrame staying
// alive, and must only stay alive until the transmission has completed
// successfully (or not -- errors are not propagated back either.) Upon
// transmission, the the load is cancelled and the loader cancels itself.
//
// The ping loader is used by audit pings, beacon transmissions and image loads
// during page unloading.
class CORE_EXPORT PingLoader {
 public:
  enum ViolationReportType {
    kContentSecurityPolicyViolationReport,
    kXSSAuditorViolationReport
  };

  static void LoadImage(LocalFrame*, const KURL&);
  static void SendLinkAuditPing(LocalFrame*,
                                const KURL& ping_url,
                                const KURL& destination_url);
  static void SendViolationReport(LocalFrame*,
                                  const KURL& report_url,
                                  scoped_refptr<EncodedFormData> report,
                                  ViolationReportType);

  // The last argument is guaranteed to be set to the size of payload if
  // these method return true. If these method returns false, the value
  // shouldn't be used.
  static bool SendBeacon(LocalFrame*, int, const KURL&, const String&, size_t&);
  static bool SendBeacon(LocalFrame*,
                         int,
                         const KURL&,
                         DOMArrayBufferView*,
                         size_t&);
  static bool SendBeacon(LocalFrame*, int, const KURL&, Blob*, size_t&);
  static bool SendBeacon(LocalFrame*, int, const KURL&, FormData*, size_t&);
};

}  // namespace blink

#endif  // PingLoader_h
