/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorMemoryAgent_h
#define InspectorMemoryAgent_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Memory.h"
#include "core/leak_detector/BlinkLeakDetector.h"
#include "core/leak_detector/BlinkLeakDetectorClient.h"

namespace blink {

class InspectedFrames;

class CORE_EXPORT InspectorMemoryAgent final
    : public InspectorBaseAgent<protocol::Memory::Metainfo>,
      public BlinkLeakDetectorClient {
 public:
  static InspectorMemoryAgent* Create(InspectedFrames* frames) {
    return new InspectorMemoryAgent(frames);
  }
  ~InspectorMemoryAgent() override;

  void Trace(blink::Visitor*) override;
  void Restore() override;

  protocol::Response getDOMCounters(int* documents,
                                    int* nodes,
                                    int* js_event_listeners) override;
  void prepareForLeakDetection(
      std::unique_ptr<PrepareForLeakDetectionCallback>) override;

  // BlinkLeakDetectorClient:
  void OnLeakDetectionComplete() override;

  // Memory protocol domain:
  protocol::Response startSampling(
      protocol::Maybe<int> in_samplingInterval,
      protocol::Maybe<bool> in_suppressRandomness) override;
  protocol::Response stopSampling() override;
  protocol::Response getSamplingProfile(
      std::unique_ptr<protocol::Memory::SamplingProfile>*) override;
  protocol::Response getAllTimeSamplingProfile(
      std::unique_ptr<protocol::Memory::SamplingProfile>*) override;

 private:
  explicit InspectorMemoryAgent(InspectedFrames*);

  std::vector<std::string> Symbolize(const std::vector<void*>& addresses);
  std::unique_ptr<protocol::Memory::SamplingProfile> GetSamplingProfileById(
      uint32_t id);

  std::unique_ptr<BlinkLeakDetector> detector_;
  std::unique_ptr<PrepareForLeakDetectionCallback> callback_;
  Member<InspectedFrames> frames_;
  uint32_t profile_id_ = 0;
  HashMap<void*, std::string> symbols_cache_;

  DISALLOW_COPY_AND_ASSIGN(InspectorMemoryAgent);
};

}  // namespace blink

#endif  // !defined(InspectorMemoryAgent_h)
