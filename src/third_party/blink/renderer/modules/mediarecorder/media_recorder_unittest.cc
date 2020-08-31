// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {
namespace {

MediaStream* CreateMediaStream(V8TestingScope* scope) {
  WebMediaStreamSource blink_source;
  blink_source.Initialize("video source id", WebMediaStreamSource::kTypeVideo,
                          "video source name", false /* remote */);
  auto native_source = std::make_unique<MockMediaStreamVideoSource>();
  MockMediaStreamVideoSource* native_source_ptr = native_source.get();
  blink_source.SetPlatformSource(std::move(native_source));
  auto* component = MakeGarbageCollected<MediaStreamComponent>(blink_source);
  component->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      native_source_ptr, MediaStreamVideoSource::ConstraintsOnceCallback(),
      true /* enabled */));
  auto* track = MakeGarbageCollected<MediaStreamTrack>(
      scope->GetExecutionContext(), component);
  return MediaStream::Create(scope->GetExecutionContext(),
                             MediaStreamTrackVector{track});
}
}  // namespace

// This is a regression test for crbug.com/1999203
TEST(MediaRecorderTest,
     AcceptsAllTracksEndedEventWhenExecutionContextDestroyed) {
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  {
    V8TestingScope scope;
    MediaStream* stream = CreateMediaStream(&scope);
    MediaRecorder* recorder = MakeGarbageCollected<MediaRecorder>(
        scope.GetExecutionContext(), stream, MediaRecorderOptions::Create(),
        scope.GetExceptionState());
    recorder->start(scope.GetExceptionState());
    for (const auto& track : stream->getTracks())
      track->Component()->GetPlatformTrack()->Stop();
  }
  platform->RunUntilIdle();
  WebHeap::CollectAllGarbageForTesting();
}

}  // namespace blink
