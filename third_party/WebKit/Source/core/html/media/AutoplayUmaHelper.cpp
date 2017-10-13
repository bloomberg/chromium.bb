// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/AutoplayUmaHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ElementVisibilityObserver.h"
#include "core/dom/events/Event.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/html/media/HTMLMediaElement.h"
#include "platform/Histogram.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

const int32_t kMaxOffscreenDurationUmaMS = 60 * 60 * 1000;
const int32_t kOffscreenDurationUmaBucketCount = 50;
const int32_t kMaxWaitTimeUmaMS = 30 * 1000;
const int32_t kWaitTimeBucketCount = 50;

}  // namespace

AutoplayUmaHelper* AutoplayUmaHelper::Create(HTMLMediaElement* element) {
  return new AutoplayUmaHelper(element);
}

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : EventListener(kCPPEventListenerType),
      ContextLifecycleObserver(nullptr),
      element_(element),
      muted_video_play_method_visibility_observer_(nullptr),
      muted_video_autoplay_offscreen_start_time_ms_(0),
      muted_video_autoplay_offscreen_duration_ms_(0),
      is_visible_(false),
      muted_video_offscreen_duration_visibility_observer_(nullptr),
      load_start_time_ms_(0.0) {
  element->addEventListener(EventTypeNames::loadstart, this, false);
}

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

bool AutoplayUmaHelper::operator==(const EventListener& other) const {
  return this == &other;
}

void AutoplayUmaHelper::OnLoadStarted() {
  if (element_->GetLoadType() == WebMediaPlayer::kLoadTypeURL)
    load_start_time_ms_ = MonotonicallyIncreasingTimeMS();
}

void AutoplayUmaHelper::OnAutoplayInitiated(AutoplaySource source) {
  int32_t autoplay_wait_time_ms = -1;
  if (load_start_time_ms_ != 0.0) {
    autoplay_wait_time_ms = static_cast<int32_t>(
        std::min<int64_t>(MonotonicallyIncreasingTimeMS() - load_start_time_ms_,
                          std::numeric_limits<int32_t>::max()));
  }
  DEFINE_STATIC_LOCAL(EnumerationHistogram, video_histogram,
                      ("Media.Video.Autoplay",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, muted_video_histogram,
                      ("Media.Video.Autoplay.Muted",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, audio_histogram,
                      ("Media.Audio.Autoplay",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, blocked_muted_video_histogram,
      ("Media.Video.Autoplay.Muted.Blocked", kAutoplayBlockedReasonMax));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, wait_time_video_attrib_histogram,
                      ("Media.Video.Autoplay.Attribute.WaitTime", 1,
                       kMaxWaitTimeUmaMS, kWaitTimeBucketCount));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, wait_time_audio_attrib_histogram,
                      ("Media.Audio.Autoplay.Attribute.WaitTime", 1,
                       kMaxWaitTimeUmaMS, kWaitTimeBucketCount));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, wait_time_video_play_histogram,
                      ("Media.Video.Autoplay.PlayMethod.WaitTime", 1,
                       kMaxWaitTimeUmaMS, kWaitTimeBucketCount));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, wait_time_audio_play_histogram,
                      ("Media.Audio.Autoplay.PlayMethod.WaitTime", 1,
                       kMaxWaitTimeUmaMS, kWaitTimeBucketCount));

  // Autoplay already initiated
  if (sources_.count(source))
    return;

  sources_.insert(source);

  // Record the source.
  if (element_->IsHTMLVideoElement()) {
    video_histogram.Count(static_cast<int>(source));
    if (element_->muted())
      muted_video_histogram.Count(static_cast<int>(source));
    if (autoplay_wait_time_ms >= 0) {
      if (source == AutoplaySource::kAttribute) {
        wait_time_video_attrib_histogram.Count(autoplay_wait_time_ms);
      } else if (source == AutoplaySource::kMethod) {
        wait_time_video_play_histogram.Count(autoplay_wait_time_ms);
      }
    }
  } else {
    audio_histogram.Count(static_cast<int>(source));
    if (autoplay_wait_time_ms >= 0) {
      if (source == AutoplaySource::kAttribute) {
        wait_time_audio_attrib_histogram.Count(autoplay_wait_time_ms);
      } else if (source == AutoplaySource::kMethod) {
        wait_time_audio_play_histogram.Count(autoplay_wait_time_ms);
      }
    }
  }

  // Record dual source.
  if (sources_.size() ==
      static_cast<size_t>(AutoplaySource::kNumberOfSources)) {
    if (element_->IsHTMLVideoElement()) {
      video_histogram.Count(static_cast<int>(AutoplaySource::kDualSource));
      if (element_->muted())
        muted_video_histogram.Count(
            static_cast<int>(AutoplaySource::kDualSource));
    } else {
      audio_histogram.Count(static_cast<int>(AutoplaySource::kDualSource));
    }
  }

  // Record the child frame and top-level frame URLs for autoplay muted videos
  // by attribute.
  if (element_->IsHTMLVideoElement() && element_->muted()) {
    if (sources_.size() ==
        static_cast<size_t>(AutoplaySource::kNumberOfSources)) {
      Platform::Current()->RecordRapporURL(
          "Media.Video.Autoplay.Muted.DualSource.Frame",
          element_->GetDocument().Url());
    } else if (source == AutoplaySource::kAttribute) {
      Platform::Current()->RecordRapporURL(
          "Media.Video.Autoplay.Muted.Attribute.Frame",
          element_->GetDocument().Url());
    } else {
      DCHECK(source == AutoplaySource::kMethod);
      Platform::Current()->RecordRapporURL(
          "Media.Video.Autoplay.Muted.PlayMethod.Frame",
          element_->GetDocument().Url());
    }
  }

  // Record if it will be blocked by Data Saver or Autoplay setting.
  if (element_->IsHTMLVideoElement() && element_->muted() &&
      RuntimeEnabledFeatures::AutoplayMutedVideosEnabled()) {
    bool data_saver_enabled =
        element_->GetDocument().GetSettings() &&
        element_->GetDocument().GetSettings()->GetDataSaverEnabled();
    bool blocked_by_setting =
        !element_->GetAutoplayPolicy().IsAutoplayAllowedPerSettings();

    if (data_saver_enabled && blocked_by_setting) {
      blocked_muted_video_histogram.Count(
          kAutoplayBlockedReasonDataSaverAndSetting);
    } else if (data_saver_enabled) {
      blocked_muted_video_histogram.Count(kAutoplayBlockedReasonDataSaver);
    } else if (blocked_by_setting) {
      blocked_muted_video_histogram.Count(kAutoplayBlockedReasonSetting);
    }
  }

  element_->addEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::RecordCrossOriginAutoplayResult(
    CrossOriginAutoplayResult result) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplay_result_histogram,
      ("Media.Autoplay.CrossOrigin.Result",
       static_cast<int>(CrossOriginAutoplayResult::kNumberOfResults)));

  if (!element_->IsHTMLVideoElement())
    return;
  if (!element_->IsInCrossOriginFrame())
    return;

  // Record each metric only once per element, since the metric focuses on the
  // site distribution. If a page calls play() multiple times, it will be
  // recorded only once.
  if (recorded_cross_origin_autoplay_results_.count(result))
    return;

  switch (result) {
    case CrossOriginAutoplayResult::kAutoplayAllowed:
      // Record metric
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.Allowed.ChildFrame",
          element_->GetDocument().Url());
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.Allowed.TopLevelFrame",
          element_->GetDocument().TopDocument().Url());
      autoplay_result_histogram.Count(static_cast<int>(result));
      recorded_cross_origin_autoplay_results_.insert(result);
      break;
    case CrossOriginAutoplayResult::kAutoplayBlocked:
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.Blocked.ChildFrame",
          element_->GetDocument().Url());
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.Blocked.TopLevelFrame",
          element_->GetDocument().TopDocument().Url());
      autoplay_result_histogram.Count(static_cast<int>(result));
      recorded_cross_origin_autoplay_results_.insert(result);
      break;
    case CrossOriginAutoplayResult::kPlayedWithGesture:
      // Record this metric only when the video has been blocked from autoplay
      // previously. This is to record the sites having videos that are blocked
      // to autoplay but the user starts the playback by gesture.
      if (!recorded_cross_origin_autoplay_results_.count(
              CrossOriginAutoplayResult::kAutoplayBlocked)) {
        return;
      }
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.PlayedWithGestureAfterBlock.ChildFrame",
          element_->GetDocument().Url());
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.PlayedWithGestureAfterBlock."
          "TopLevelFrame",
          element_->GetDocument().TopDocument().Url());
      autoplay_result_histogram.Count(static_cast<int>(result));
      recorded_cross_origin_autoplay_results_.insert(result);
      break;
    case CrossOriginAutoplayResult::kUserPaused:
      if (!ShouldRecordUserPausedAutoplayingCrossOriginVideo())
        return;
      if (element_->ended() || element_->seeking())
        return;
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.UserPausedAutoplayingVideo.ChildFrame",
          element_->GetDocument().Url());
      Platform::Current()->RecordRapporURL(
          "Media.Autoplay.CrossOrigin.UserPausedAutoplayingVideo."
          "TopLevelFrame",
          element_->GetDocument().TopDocument().Url());
      autoplay_result_histogram.Count(static_cast<int>(result));
      recorded_cross_origin_autoplay_results_.insert(result);
      break;
    default:
      NOTREACHED();
  }
}

void AutoplayUmaHelper::RecordAutoplayUnmuteStatus(
    AutoplayUnmuteActionStatus status) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplay_unmute_histogram,
      ("Media.Video.Autoplay.Muted.UnmuteAction",
       static_cast<int>(AutoplayUnmuteActionStatus::kNumberOfStatus)));

  autoplay_unmute_histogram.Count(static_cast<int>(status));
}

void AutoplayUmaHelper::VideoWillBeDrawnToCanvas() {
  if (HasSource() && !IsVisible()) {
    UseCounter::Count(element_->GetDocument(),
                      WebFeature::kHiddenAutoplayedVideoInCanvas);
  }
}

void AutoplayUmaHelper::DidMoveToNewDocument(Document& old_document) {
  if (!ShouldListenToContextDestroyed())
    return;

  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::OnVisibilityChangedForMutedVideoPlayMethodBecomeVisible(
    bool is_visible) {
  if (!is_visible || !muted_video_play_method_visibility_observer_)
    return;

  MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(true);
}

void AutoplayUmaHelper::OnVisibilityChangedForMutedVideoOffscreenDuration(
    bool is_visible) {
  if (is_visible == is_visible_)
    return;

  if (is_visible) {
    muted_video_autoplay_offscreen_duration_ms_ +=
        static_cast<int64_t>(MonotonicallyIncreasingTimeMS()) -
        muted_video_autoplay_offscreen_start_time_ms_;
  } else {
    muted_video_autoplay_offscreen_start_time_ms_ =
        static_cast<int64_t>(MonotonicallyIncreasingTimeMS());
  }

  is_visible_ = is_visible;
}

void AutoplayUmaHelper::handleEvent(ExecutionContext* execution_context,
                                    Event* event) {
  if (event->type() == EventTypeNames::loadstart)
    OnLoadStarted();
  else if (event->type() == EventTypeNames::playing)
    HandlePlayingEvent();
  else if (event->type() == EventTypeNames::pause)
    HandlePauseEvent();
  else
    NOTREACHED();
}

void AutoplayUmaHelper::HandlePlayingEvent() {
  MaybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  MaybeStartRecordingMutedVideoOffscreenDuration();

  element_->removeEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::HandlePauseEvent() {
  MaybeStopRecordingMutedVideoOffscreenDuration();
  MaybeRecordUserPausedAutoplayingCrossOriginVideo();
}

void AutoplayUmaHelper::ContextDestroyed(ExecutionContext*) {
  HandleContextDestroyed();
}

void AutoplayUmaHelper::HandleContextDestroyed() {
  MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(false);
  MaybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoPlayMethodBecomeVisible() {
  if (!sources_.count(AutoplaySource::kMethod) ||
      !element_->IsHTMLVideoElement() || !element_->muted())
    return;

  muted_video_play_method_visibility_observer_ = new ElementVisibilityObserver(
      element_,
      WTF::Bind(&AutoplayUmaHelper::
                    OnVisibilityChangedForMutedVideoPlayMethodBecomeVisible,
                WrapWeakPersistent(this)));
  muted_video_play_method_visibility_observer_->Start();
  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(
    bool visible) {
  if (!muted_video_play_method_visibility_observer_)
    return;

  DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                      ("Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible"));

  histogram.Count(visible);
  muted_video_play_method_visibility_observer_->Stop();
  muted_video_play_method_visibility_observer_ = nullptr;
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoOffscreenDuration() {
  if (!element_->IsHTMLVideoElement() || !element_->muted() ||
      !sources_.count(AutoplaySource::kMethod))
    return;

  // Start recording muted video playing offscreen duration.
  muted_video_autoplay_offscreen_start_time_ms_ =
      static_cast<int64_t>(MonotonicallyIncreasingTimeMS());
  is_visible_ = false;
  muted_video_offscreen_duration_visibility_observer_ =
      new ElementVisibilityObserver(
          element_,
          WTF::Bind(&AutoplayUmaHelper::
                        OnVisibilityChangedForMutedVideoOffscreenDuration,
                    WrapWeakPersistent(this)));
  muted_video_offscreen_duration_visibility_observer_->Start();
  element_->addEventListener(EventTypeNames::pause, this, false);
  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::MaybeStopRecordingMutedVideoOffscreenDuration() {
  if (!muted_video_offscreen_duration_visibility_observer_)
    return;

  if (!is_visible_) {
    muted_video_autoplay_offscreen_duration_ms_ +=
        static_cast<int64_t>(MonotonicallyIncreasingTimeMS()) -
        muted_video_autoplay_offscreen_start_time_ms_;
  }

  // Since histograms uses int32_t, the duration needs to be limited to
  // std::numeric_limits<int32_t>::max().
  int32_t bounded_time = static_cast<int32_t>(
      std::min<int64_t>(muted_video_autoplay_offscreen_duration_ms_,
                        std::numeric_limits<int32_t>::max()));

  DCHECK(sources_.count(AutoplaySource::kMethod));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, duration_histogram,
      ("Media.Video.Autoplay.Muted.PlayMethod.OffscreenDuration", 1,
       kMaxOffscreenDurationUmaMS, kOffscreenDurationUmaBucketCount));
  duration_histogram.Count(bounded_time);

  muted_video_offscreen_duration_visibility_observer_->Stop();
  muted_video_offscreen_duration_visibility_observer_ = nullptr;
  muted_video_autoplay_offscreen_duration_ms_ = 0;
  MaybeUnregisterMediaElementPauseListener();
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeRecordUserPausedAutoplayingCrossOriginVideo() {
  RecordCrossOriginAutoplayResult(CrossOriginAutoplayResult::kUserPaused);
  MaybeUnregisterMediaElementPauseListener();
}

void AutoplayUmaHelper::MaybeUnregisterContextDestroyedObserver() {
  if (!ShouldListenToContextDestroyed()) {
    SetContext(nullptr);
  }
}

void AutoplayUmaHelper::MaybeUnregisterMediaElementPauseListener() {
  if (muted_video_offscreen_duration_visibility_observer_)
    return;
  if (ShouldRecordUserPausedAutoplayingCrossOriginVideo())
    return;
  element_->removeEventListener(EventTypeNames::pause, this, false);
}

bool AutoplayUmaHelper::ShouldListenToContextDestroyed() const {
  return muted_video_play_method_visibility_observer_ ||
         muted_video_offscreen_duration_visibility_observer_;
}

bool AutoplayUmaHelper::ShouldRecordUserPausedAutoplayingCrossOriginVideo()
    const {
  return element_->IsInCrossOriginFrame() && element_->IsHTMLVideoElement() &&
         !sources_.empty() &&
         !recorded_cross_origin_autoplay_results_.count(
             CrossOriginAutoplayResult::kUserPaused);
}

DEFINE_TRACE(AutoplayUmaHelper) {
  EventListener::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(element_);
  visitor->Trace(muted_video_play_method_visibility_observer_);
  visitor->Trace(muted_video_offscreen_duration_visibility_observer_);
}

}  // namespace blink
