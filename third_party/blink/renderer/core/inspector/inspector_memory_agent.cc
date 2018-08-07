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

#include "third_party/blink/renderer/core/inspector/inspector_memory_agent.h"

#include <cstdio>

#include "base/debug/stack_trace.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/platform/instance_counters.h"

namespace blink {

constexpr int kDefaultNativeMemorySamplingInterval = 128 * 1024;

using protocol::Response;

InspectorMemoryAgent::InspectorMemoryAgent(InspectedFrames* inspected_frames)
    : frames_(inspected_frames),
      sampling_profile_interval_(&agent_state_, /*default_value=*/0) {}

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

void InspectorMemoryAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorMemoryAgent::Restore() {
  // The action below won't start sampling if the sampling_interval is zero.
  startSampling(protocol::Maybe<int>(sampling_profile_interval_.Get()),
                protocol::Maybe<bool>());
}

Response InspectorMemoryAgent::startSampling(
    protocol::Maybe<int> in_sampling_interval,
    protocol::Maybe<bool> in_suppressRandomness) {
  int interval =
      in_sampling_interval.fromMaybe(kDefaultNativeMemorySamplingInterval);
  if (interval <= 0)
    return Response::Error("Invalid sampling rate.");
  base::SamplingHeapProfiler::GetInstance()->SetSamplingInterval(interval);
  sampling_profile_interval_.Set(interval);
  if (in_suppressRandomness.fromMaybe(false))
    base::SamplingHeapProfiler::GetInstance()->SuppressRandomnessForTest(true);
  profile_id_ = base::SamplingHeapProfiler::GetInstance()->Start();
  return Response::OK();
}

Response InspectorMemoryAgent::stopSampling() {
  if (sampling_profile_interval_.Get() == 0)
    return Response::Error("Sampling profiler is not started.");
  base::SamplingHeapProfiler::GetInstance()->Stop();
  sampling_profile_interval_.Clear();
  return Response::OK();
}

Response InspectorMemoryAgent::getAllTimeSamplingProfile(
    std::unique_ptr<protocol::Memory::SamplingProfile>* out_profile) {
  *out_profile = GetSamplingProfileById(0);
  return Response::OK();
}

Response InspectorMemoryAgent::getSamplingProfile(
    std::unique_ptr<protocol::Memory::SamplingProfile>* out_profile) {
  *out_profile = GetSamplingProfileById(profile_id_);
  return Response::OK();
}

std::unique_ptr<protocol::Memory::SamplingProfile>
InspectorMemoryAgent::GetSamplingProfileById(uint32_t id) {
  base::ModuleCache module_cache;
  std::unique_ptr<protocol::Array<protocol::Memory::SamplingProfileNode>>
      samples =
          protocol::Array<protocol::Memory::SamplingProfileNode>::create();
  std::vector<base::SamplingHeapProfiler::Sample> raw_samples =
      base::SamplingHeapProfiler::GetInstance()->GetSamples(id);

  for (auto& it : raw_samples) {
    std::unique_ptr<protocol::Array<protocol::String>> stack =
        protocol::Array<protocol::String>::create();
    for (const void* frame : it.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      module_cache.GetModuleForAddress(address);  // Populates module_cache.
    }
    std::vector<std::string> source_stack = Symbolize(it.stack);
    for (auto& it2 : source_stack)
      stack->addItem(it2.c_str());
    samples->addItem(protocol::Memory::SamplingProfileNode::create()
                         .setSize(it.size)
                         .setTotal(it.total)
                         .setStack(std::move(stack))
                         .build());
  }

  // Mix in v8 main isolate heap size as a synthetic node.
  // TODO(alph): Add workers' heap sizes.
  if (!id) {
    v8::HeapStatistics heap_stats;
    v8::Isolate::GetCurrent()->GetHeapStatistics(&heap_stats);
    size_t total_bytes = heap_stats.total_heap_size();
    std::unique_ptr<protocol::Array<protocol::String>> stack =
        protocol::Array<protocol::String>::create();
    stack->addItem("<V8 Heap>");
    samples->addItem(protocol::Memory::SamplingProfileNode::create()
                         .setSize(total_bytes)
                         .setTotal(total_bytes)
                         .setStack(std::move(stack))
                         .build());
  }

  std::unique_ptr<protocol::Array<protocol::Memory::Module>> modules =
      protocol::Array<protocol::Memory::Module>::create();
  for (const auto* module : module_cache.GetModules()) {
    modules->addItem(
        protocol::Memory::Module::create()
            .setName(module->filename.value().c_str())
            .setUuid(module->id.c_str())
            .setBaseAddress(String::Format("0x%" PRIxPTR, module->base_address))
            .setSize(static_cast<double>(module->size))
            .build());
  }

  return protocol::Memory::SamplingProfile::create()
      .setSamples(std::move(samples))
      .setModules(std::move(modules))
      .build();
}

std::vector<std::string> InspectorMemoryAgent::Symbolize(
    const std::vector<void*>& addresses) {
#if defined(OS_LINUX)
  // TODO(alph): Move symbolization to the client.
  std::vector<void*> addresses_to_symbolize;
  for (void* address : addresses) {
    if (!symbols_cache_.Contains(address))
      addresses_to_symbolize.push_back(address);
  }

  std::string text = base::debug::StackTrace(addresses_to_symbolize.data(),
                                             addresses_to_symbolize.size())
                         .ToString();
  // Populate cache with new entries.
  size_t next_pos;
  for (size_t pos = 0, i = 0;; pos = next_pos + 1, ++i) {
    next_pos = text.find('\n', pos);
    if (next_pos == std::string::npos)
      break;
    std::string line = text.substr(pos, next_pos - pos);
    size_t space_pos = line.rfind(' ');
    std::string name =
        line.substr(space_pos == std::string::npos ? 0 : space_pos + 1);
    symbols_cache_.insert(addresses_to_symbolize[i], name);
  }
#endif

  std::vector<std::string> result;
  for (void* address : addresses) {
    char buffer[20];
    std::snprintf(buffer, sizeof(buffer), "0x%" PRIxPTR,
                  reinterpret_cast<uintptr_t>(address));
    if (symbols_cache_.Contains(address))
      result.push_back(std::string(buffer) + " " + symbols_cache_.at(address));
    else
      result.push_back(buffer);
  }
  return result;
}

}  // namespace blink
