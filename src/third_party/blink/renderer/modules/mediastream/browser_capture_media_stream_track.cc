// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/token.h"
#include "build/build_config.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CropToResult {
  kResolved = 0,
  kUnsupportedPlatform = 1,
  kInvalidCropTargetFormat = 2,
  kRejectedWithErrorGeneric = 3,
  kRejectedWithUnsupportedCaptureDevice = 4,
  kRejectedWithErrorUnknownDeviceId = 5,
  kRejectedWithNotImplemented = 6,
  kNonIncreasingCropVersion = 7,
  kMaxValue = kNonIncreasingCropVersion
};

void RecordUma(CropToResult result) {
  base::UmaHistogramEnumeration("Media.RegionCapture.CropTo.Result", result);
}

#if !BUILDFLAG(IS_ANDROID)
// If crop_id is the empty string, returns an empty base::Token.
// If crop_id is a valid UUID, returns a base::Token representing the ID.
// Otherwise, returns nullopt.
absl::optional<base::Token> CropIdStringToToken(const String& crop_id) {
  if (crop_id.IsEmpty()) {
    return base::Token();
  }
  if (!crop_id.ContainsOnlyASCIIOrEmpty()) {
    return absl::nullopt;
  }
  const base::GUID guid = base::GUID::ParseCaseInsensitive(crop_id.Ascii());
  return guid.is_valid() ? absl::make_optional<base::Token>(GUIDToToken(guid))
                         : absl::nullopt;
}

void RaiseCropException(ScriptPromiseResolver* resolver,
                        DOMExceptionCode exception_code,
                        const WTF::String& exception_text) {
  resolver->Reject(
      MakeGarbageCollected<DOMException>(exception_code, exception_text));
}

void ResolveCropPromiseHelper(ScriptPromiseResolver* resolver,
                              media::mojom::CropRequestResult result) {
  DCHECK(IsMainThread());

  if (!resolver) {
    return;
  }

  switch (result) {
    case media::mojom::CropRequestResult::kSuccess:
      RecordUma(CropToResult::kResolved);
      // TODO(crbug.com/1247761): Delay reporting success to the Web-application
      // until "seeing" the last frame cropped to the previous crop-target.
      resolver->Resolve();
      return;
    case media::mojom::CropRequestResult::kErrorGeneric:
      RecordUma(CropToResult::kRejectedWithErrorGeneric);
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unknown error.");
      return;
    case media::mojom::CropRequestResult::kUnsupportedCaptureDevice:
      RecordUma(CropToResult::kRejectedWithUnsupportedCaptureDevice);
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unsupported device.");
      return;
    case media::mojom::CropRequestResult::kErrorUnknownDeviceId:
      RecordUma(CropToResult::kRejectedWithErrorUnknownDeviceId);
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unknown device.");
      return;
    case media::mojom::CropRequestResult::kNotImplemented:
      RecordUma(CropToResult::kRejectedWithNotImplemented);
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Not implemented.");
      return;
    case media::mojom::CropRequestResult::kNonIncreasingCropVersion:
      // This should rarely happen, as the browser process would issue
      // a BadMessage in this case. But if that message has to hop from
      // the IO thread to the UI thread, it could theoretically happen
      // that Blink receives this callback before being killed, so we
      // can't quite DCHECK this.
      RecordUma(CropToResult::kNonIncreasingCropVersion);
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Non-increasing crop version.");
      return;
  }

  NOTREACHED();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : BrowserCaptureMediaStreamTrack(execution_context,
                                     component,
                                     component->Source()->GetReadyState(),
                                     std::move(callback),
                                     descriptor_id,
                                     is_clone) {}

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    MediaStreamSource::ReadyState ready_state,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : FocusableMediaStreamTrack(execution_context,
                                component,
                                ready_state,
                                std::move(callback),
                                descriptor_id,
                                is_clone) {}

#if !BUILDFLAG(IS_ANDROID)
void BrowserCaptureMediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(pending_promises_);
  FocusableMediaStreamTrack::Trace(visitor);
}
#endif  // !BUILDFLAG(IS_ANDROID)

ScriptPromise BrowserCaptureMediaStreamTrack::cropTo(
    ScriptState* script_state,
    const String& crop_id,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

#if BUILDFLAG(IS_ANDROID)
  RecordUma(CropToResult::kUnsupportedPlatform);
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kUnknownError, "Not supported on Android."));
  return promise;
#else

  // TODO(crbug.com/1298159): Reject cropTo() on clones.

  const absl::optional<base::Token> crop_id_token =
      CropIdStringToToken(crop_id);
  if (!crop_id_token.has_value()) {
    RecordUma(CropToResult::kInvalidCropTargetFormat);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Invalid crop-ID."));
    return promise;
  }

  pending_promises_.Set(++current_crop_version_, resolver);

  // We don't currently instantiate BrowserCaptureMediaStreamTrack for audio
  // tracks. If we do in the future, we'll have to raise an exception if
  // cropTo() is called on a non-video track.
  DCHECK(Component());
  DCHECK(Component()->Source());
  MediaStreamSource* const source = Component()->Source();
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* const native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);

  native_source->Crop(
      crop_id_token.value(), current_crop_version_,
      WTF::Bind(&BrowserCaptureMediaStreamTrack::ResolveCropPromise,
                WrapPersistent(this), current_crop_version_));

  return promise;
#endif
}

BrowserCaptureMediaStreamTrack* BrowserCaptureMediaStreamTrack::clone(
    ScriptState* script_state) {
  // Instantiate the clone.
  BrowserCaptureMediaStreamTrack* cloned_track =
      MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          ExecutionContext::From(script_state), Component()->Clone(),
          GetReadyState(), base::DoNothing(), descriptor_id(),
          /*is_clone=*/true);

  // Copy state.
  CloneInternal(cloned_track);

  return cloned_track;
}

void BrowserCaptureMediaStreamTrack::CloneInternal(
    BrowserCaptureMediaStreamTrack* cloned_track) {
  // Clone parent classes' state.
  FocusableMediaStreamTrack::CloneInternal(cloned_track);

  // Clone this class's state.
#if !BUILDFLAG(IS_ANDROID)
  // Note that cropTo() cannot be called on a clone, but we still copy,
  // for completeness' sake.
  current_crop_version_ = cloned_track->current_crop_version_;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void BrowserCaptureMediaStreamTrack::ResolveCropPromise(
    uint32_t crop_version,
    media::mojom::CropRequestResult result) {
  const auto promise_it = pending_promises_.find(crop_version);
  if (promise_it == pending_promises_.end()) {
    return;
  }
  ScriptPromiseResolver* const resolver = promise_it->value;
  pending_promises_.erase(promise_it);
  ResolveCropPromiseHelper(resolver, result);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
