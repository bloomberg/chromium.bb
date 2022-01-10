/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/metadata_module.h"

#include "perfetto/ext/base/base64.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"

#include "protos/perfetto/trace/chrome/chrome_benchmark_metadata.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "protos/perfetto/trace/trigger.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

MetadataModule::MetadataModule(TraceProcessorContext* context)
    : context_(context),
      producer_name_key_id_(context_->storage->InternString("producer_name")),
      trusted_producer_uid_key_id_(
          context_->storage->InternString("trusted_producer_uid")) {
  RegisterForField(TracePacket::kUiStateFieldNumber, context);
  RegisterForField(TracePacket::kChromeMetadataFieldNumber, context);
  RegisterForField(TracePacket::kChromeBenchmarkMetadataFieldNumber, context);
  RegisterForField(TracePacket::kTriggerFieldNumber, context);
}

ModuleResult MetadataModule::TokenizePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    TraceBlobView*,
    int64_t,
    PacketSequenceState*,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kUiStateFieldNumber: {
      auto ui_state = decoder.ui_state();
      std::string base64 = base::Base64Encode(ui_state.data, ui_state.size);
      StringId id = context_->storage->InternString(base::StringView(base64));
      context_->metadata_tracker->SetMetadata(metadata::ui_state,
                                              Variadic::String(id));
      return ModuleResult::Handled();
    }
    case TracePacket::kChromeMetadataFieldNumber: {
      ParseChromeMetadataPacket(decoder.chrome_metadata());
      // Metadata packets may also contain untyped metadata due to a bug in
      // Chrome <M92.
      // TODO(crbug.com/1194914): Replace this with Handled() once the
      // Chrome-side fix has propagated into all release channels.
      return ModuleResult::Ignored();
    }
    case TracePacket::kChromeBenchmarkMetadataFieldNumber: {
      ParseChromeBenchmarkMetadata(decoder.chrome_benchmark_metadata());
      return ModuleResult::Handled();
    }
  }
  return ModuleResult::Ignored();
}

void MetadataModule::ParsePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    const TimestampedTracePiece& ttp,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kTriggerFieldNumber:
      // We handle triggers at parse time rather at tokenization because
      // we add slices to tables which need to happen post-sorting.
      ParseTrigger(ttp.timestamp, decoder.trigger());
      break;
  }
}

void MetadataModule::ParseChromeBenchmarkMetadata(ConstBytes blob) {
  TraceStorage* storage = context_->storage.get();
  MetadataTracker* metadata = context_->metadata_tracker.get();

  protos::pbzero::ChromeBenchmarkMetadata::Decoder packet(blob.data, blob.size);
  if (packet.has_benchmark_name()) {
    auto benchmark_name_id = storage->InternString(packet.benchmark_name());
    metadata->SetMetadata(metadata::benchmark_name,
                          Variadic::String(benchmark_name_id));
  }
  if (packet.has_benchmark_description()) {
    auto benchmark_description_id =
        storage->InternString(packet.benchmark_description());
    metadata->SetMetadata(metadata::benchmark_description,
                          Variadic::String(benchmark_description_id));
  }
  if (packet.has_label()) {
    auto label_id = storage->InternString(packet.label());
    metadata->SetMetadata(metadata::benchmark_label,
                          Variadic::String(label_id));
  }
  if (packet.has_story_name()) {
    auto story_name_id = storage->InternString(packet.story_name());
    metadata->SetMetadata(metadata::benchmark_story_name,
                          Variadic::String(story_name_id));
  }
  for (auto it = packet.story_tags(); it; ++it) {
    auto story_tag_id = storage->InternString(*it);
    metadata->AppendMetadata(metadata::benchmark_story_tags,
                             Variadic::String(story_tag_id));
  }
  if (packet.has_benchmark_start_time_us()) {
    metadata->SetMetadata(metadata::benchmark_start_time_us,
                          Variadic::Integer(packet.benchmark_start_time_us()));
  }
  if (packet.has_story_run_time_us()) {
    metadata->SetMetadata(metadata::benchmark_story_run_time_us,
                          Variadic::Integer(packet.story_run_time_us()));
  }
  if (packet.has_story_run_index()) {
    metadata->SetMetadata(metadata::benchmark_story_run_index,
                          Variadic::Integer(packet.story_run_index()));
  }
  if (packet.has_had_failures()) {
    metadata->SetMetadata(metadata::benchmark_had_failures,
                          Variadic::Integer(packet.had_failures()));
  }
}

void MetadataModule::ParseChromeMetadataPacket(ConstBytes blob) {
  TraceStorage* storage = context_->storage.get();
  MetadataTracker* metadata = context_->metadata_tracker.get();

  // Typed chrome metadata proto. The untyped metadata is parsed below in
  // ParseChromeEvents().
  protos::pbzero::ChromeMetadataPacket::Decoder packet(blob.data, blob.size);

  if (packet.has_chrome_version_code()) {
    metadata->SetDynamicMetadata(
        storage->InternString("cr-playstore_version_code"),
        Variadic::Integer(packet.chrome_version_code()));
  }
  if (packet.has_enabled_categories()) {
    auto categories_id = storage->InternString(packet.enabled_categories());
    metadata->SetDynamicMetadata(storage->InternString("cr-enabled_categories"),
                                 Variadic::String(categories_id));
  }
}

void MetadataModule::ParseTrigger(int64_t ts, ConstBytes blob) {
  protos::pbzero::Trigger::Decoder trigger(blob.data, blob.size);
  StringId cat_id = kNullStringId;
  TrackId track_id = context_->track_tracker->GetOrCreateTriggerTrack();
  StringId name_id = context_->storage->InternString(trigger.trigger_name());
  context_->slice_tracker->Scoped(
      ts, track_id, cat_id, name_id,
      /* duration = */ 0,
      [&trigger, this](ArgsTracker::BoundInserter* args_table) {
        StringId producer_name =
            context_->storage->InternString(trigger.producer_name());
        if (!producer_name.is_null()) {
          args_table->AddArg(producer_name_key_id_,
                             Variadic::String(producer_name));
        }
        if (trigger.has_trusted_producer_uid()) {
          args_table->AddArg(trusted_producer_uid_key_id_,
                             Variadic::Integer(trigger.trusted_producer_uid()));
        }
      });
}

}  // namespace trace_processor
}  // namespace perfetto
