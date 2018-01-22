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

#include "core/inspector/InspectorMemoryAgent.h"

#include "base/debug/stack_trace.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/InspectedFrames.h"
#include "platform/InstanceCounters.h"
#include "platform/memory_profiler/SamplingNativeHeapProfiler.h"

namespace blink {

const unsigned kDefaultNativeMemorySamplingInterval = 128 * 1024;

namespace MemoryAgentState {
static const char samplingProfileInterval[] =
    "memoryAgentSamplingProfileInterval";
}  // namespace MemoryAgentState

using PrepareForLeakDetectionCallback =
    protocol::Memory::Backend::PrepareForLeakDetectionCallback;
using protocol::Response;

InspectorMemoryAgent::InspectorMemoryAgent(InspectedFrames* inspected_frames)
    : detector_(nullptr), callback_(nullptr), frames_(inspected_frames) {}

InspectorMemoryAgent::~InspectorMemoryAgent() = default;

Response InspectorMemoryAgent::getDOMCounters(int* documents,
                                              int* nodes,
                                              int* js_event_listeners) {
  *documents =
      InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter);
  *nodes = InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
  *js_event_listeners =
      InstanceCounters::CounterValue(InstanceCounters::kJSEventListenerCounter);
  return Response::OK();
}

void InspectorMemoryAgent::prepareForLeakDetection(
    std::unique_ptr<PrepareForLeakDetectionCallback> callback) {
  callback_ = std::move(callback);
  detector_.reset(new BlinkLeakDetector(this));
  detector_->PrepareForLeakDetection(frames_->Root()->Client()->GetWebFrame());
  detector_->CollectGarbage();
}

void InspectorMemoryAgent::OnLeakDetectionComplete() {
  DCHECK(callback_);
  callback_->sendSuccess();
  callback_.reset();
  detector_.reset();
}

void InspectorMemoryAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorMemoryAgent::Restore() {
  int sampling_interval = 0;
  state_->getInteger(MemoryAgentState::samplingProfileInterval,
                     &sampling_interval);
  // The action below won't start sampling if the sampling_interval is zero.
  startSampling(protocol::Maybe<int>(sampling_interval),
                protocol::Maybe<bool>());
}

Response InspectorMemoryAgent::startSampling(
    protocol::Maybe<int> in_sampling_interval,
    protocol::Maybe<bool> in_suppressRandomness) {
  int interval =
      in_sampling_interval.fromMaybe(kDefaultNativeMemorySamplingInterval);
  if (interval <= 0)
    return Response::Error("Invalid sampling rate.");
  SamplingNativeHeapProfiler::GetInstance()->SetSamplingInterval(interval);
  state_->setInteger(MemoryAgentState::samplingProfileInterval, interval);
  if (in_suppressRandomness.fromMaybe(false))
    SamplingNativeHeapProfiler::GetInstance()->SuppressRandomnessForTest();
  SamplingNativeHeapProfiler::GetInstance()->Start();
  return Response::OK();
}

Response InspectorMemoryAgent::stopSampling() {
  int sampling_interval = 0;
  state_->getInteger(MemoryAgentState::samplingProfileInterval,
                     &sampling_interval);
  if (!sampling_interval)
    return Response::Error("Sampling profiler is not started.");
  SamplingNativeHeapProfiler::GetInstance()->Stop();
  state_->setInteger(MemoryAgentState::samplingProfileInterval, 0);
  return Response::OK();
}

Response InspectorMemoryAgent::getSamplingProfile(
    std::unique_ptr<protocol::Memory::SamplingProfile>* out_profile) {
  std::unique_ptr<protocol::Array<protocol::Memory::SamplingProfileNode>>
      samples =
          protocol::Array<protocol::Memory::SamplingProfileNode>::create();
  std::vector<SamplingNativeHeapProfiler::Sample> raw_samples =
      SamplingNativeHeapProfiler::GetInstance()->GetSamples();
  // TODO(alph): Only report samples recorded within the current session.
  for (auto it = raw_samples.begin(); it != raw_samples.end(); ++it) {
    std::unique_ptr<protocol::Array<protocol::String>> stack =
        protocol::Array<protocol::String>::create();
    std::vector<std::string> source_stack = Symbolize(it->stack);
    for (auto it2 = source_stack.begin(); it2 != source_stack.end(); ++it2)
      stack->addItem(it2->c_str());
    samples->addItem(protocol::Memory::SamplingProfileNode::create()
                         .setSize(it->size)
                         .setCount(it->count)
                         .setStack(std::move(stack))
                         .build());
  }
  std::unique_ptr<protocol::Memory::SamplingProfile> result =
      protocol::Memory::SamplingProfile::create()
          .setSamples(std::move(samples))
          .build();
  *out_profile = std::move(result);
  return Response::OK();
}

std::vector<std::string> InspectorMemoryAgent::Symbolize(
    const std::vector<void*>& addresses) {
  // TODO(alph): Move symbolization to the client.
  std::vector<std::string> result;
  base::debug::StackTrace trace(addresses.data(), addresses.size());
  std::string text = trace.ToString();

  size_t next_pos;
  for (size_t pos = 0;; pos = next_pos + 1) {
    next_pos = text.find('\n', pos);
    if (next_pos == std::string::npos)
      break;
    std::string line = text.substr(pos, next_pos - pos);
    size_t space_pos = line.rfind(' ');
    result.push_back(
        line.substr(space_pos == std::string::npos ? 0 : space_pos + 1));
  }
  return result;
}

}  // namespace blink
