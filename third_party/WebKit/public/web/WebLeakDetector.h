/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 */

#ifndef WebLeakDetector_h
#define WebLeakDetector_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebFrame;

class WebLeakDetectorClient {
 public:
  struct Result {
    unsigned number_of_live_audio_nodes;
    unsigned number_of_live_documents;
    unsigned number_of_live_nodes;
    unsigned number_of_live_layout_objects;
    unsigned number_of_live_resources;
    unsigned number_of_live_pausable_objects;
    unsigned number_of_live_script_promises;
    unsigned number_of_live_frames;
    unsigned number_of_live_v8_per_context_data;
    unsigned number_of_worker_global_scopes;
    unsigned number_of_live_ua_css_resources;
  };

  virtual void OnLeakDetectionComplete(const Result&) = 0;
};

// |WebLeakDetector| detects leaks of various Blink objects, counting
// the ones remaining after having reset Blink's global state and
// collected all garbage. See |WebLeakDetectorClient::Results|
// for the kinds of objects supported.
class WebLeakDetector {
 public:
  virtual ~WebLeakDetector() = default;

  BLINK_EXPORT static WebLeakDetector* Create(WebLeakDetectorClient*);

  // Leak detection is performed in two stages,
  // |prepareForLeakDetection()| and |collectGarbageAndReport()|.
  //
  // The first clearing out various global resources that a frame
  // keeps, with the second clearing out all garbage in Blink's
  // managed heaps before reporting leak counts.
  //
  // |WebLeakDetectorClient::onLeakDetectionComplete()| is called
  // with the result of the (leaked) resource counting.
  //
  // By separating into two, the caller is able to inject any
  // additional releasing of resources needed before the garbage
  // collections go ahead.

  // Perform initial stage of preparing for leak detection,
  // releasing references to resources held globally.
  virtual void PrepareForLeakDetection(WebFrame*) = 0;

  // Garbage collect Blink's heaps and report leak counts.
  // |WebLeakDetectorClient::onLeakDetectionComplete()| is called
  // upon completion.
  virtual void CollectGarbageAndReport() = 0;
};

}  // namespace blink

#endif  // WebLeakDetector_h
