// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlTimelineMetrics.h"

#include <stdint.h>
#include <cmath>
#include <limits>
#include "platform/KeyboardCodes.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

namespace {

// Correponds to UMA MediaTimelineSeekType enum. Enum values can be added, but
// must never be renumbered or deleted and reused.
enum class SeekType {
  kClick = 0,
  kDragFromCurrentPosition = 1,
  kDragFromElsewhere = 2,
  kKeyboardArrowKey = 3,
  kKeyboardPageUpDownKey = 4,
  kKeyboardHomeEndKey = 5,
  // Update kLast when adding new values.
  kLast = kKeyboardHomeEndKey
};

// Exclusive upper bounds for the positive buckets of UMA MediaTimelinePercent
// enum, which are reflected to form the negative buckets. The custom enum is
// because UMA count histograms don't support negative values. Values must not
// be added/modified/removed due to the way the negative buckets are formed.
constexpr double kPercentIntervals[] = {
    0,  // Dedicated zero bucket so upper bound is inclusive, unlike the others.
    0.1,  0.2,  0.3,  0.5,  0.7,  1.0,  1.5,  2.0,  3.0,  5.0,  7.0,  10.0,
    15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 60.0, 70.0, 80.0, 90.0,
    100.0  // 100% upper bound is inclusive, unlike the others.
};
// Must match length of UMA MediaTimelinePercent enum.
constexpr int32_t kPercentBucketCount = 51;
static_assert(arraysize(kPercentIntervals) * 2 - 1 == kPercentBucketCount,
              "Intervals must match UMA MediaTimelinePercent enum");

// Corresponds to two UMA enums of different sizes! Values are the exclusive
// upper bounds for buckets of UMA MediaTimelineAbsTimeDelta enum with the same
// index, and also for the positive buckets of UMA MediaTimelineTimeDelta enum,
// which are reflected to form the negative buckets. MediaTimelineTimeDelta
// needed a custom enum because UMA count histograms don't support negative
// values, and MediaTimelineAbsTimeDelta uses the same mechanism so the values
// can be compared easily. Values must not be added/modified/removed due to the
// way the negative buckets are formed.
constexpr double kTimeDeltaMSIntervals[] = {
    1,         // 1ms
    16,        // 16ms
    32,        // 32ms
    64,        // 64ms
    128,       // 128ms
    256,       // 256ms
    512,       // 512ms
    1000,      // 1s
    2000,      // 2s
    4000,      // 4s
    8000,      // 8s
    15000,     // 15s
    30000,     // 30s
    60000,     // 1m
    120000,    // 2m
    240000,    // 4m
    480000,    // 8m
    900000,    // 15m
    1800000,   // 30m
    3600000,   // 1h
    7200000,   // 2h
    14400000,  // 4h
    28800000,  // 8h
    57600000,  // 16h
    std::numeric_limits<double>::infinity()};
// Must match length of UMA MediaTimelineAbsTimeDelta enum.
constexpr int32_t kAbsTimeDeltaBucketCount = 25;
// Must match length of UMA MediaTimelineTimeDelta enum.
constexpr int32_t kTimeDeltaBucketCount = 49;
static_assert(arraysize(kTimeDeltaMSIntervals) == kAbsTimeDeltaBucketCount,
              "Intervals must match UMA MediaTimelineAbsTimeDelta enum");
static_assert(arraysize(kTimeDeltaMSIntervals) * 2 - 1 == kTimeDeltaBucketCount,
              "Intervals must match UMA MediaTimelineTimeDelta enum");

// Calculates index of UMA MediaTimelinePercent enum corresponding to |percent|.
// Negative values use kPercentIntervals in reverse.
int32_t toPercentSample(double percent) {
  constexpr int32_t nonNegativeBucketCount = arraysize(kPercentIntervals);
  constexpr int32_t negativeBucketCount = arraysize(kPercentIntervals) - 1;
  bool negative = percent < 0;
  double absPercent = std::abs(percent);
  if (absPercent == 0)
    return negativeBucketCount;  // Dedicated zero bucket.
  for (int32_t i = 0; i < nonNegativeBucketCount; i++) {
    if (absPercent < kPercentIntervals[i])
      return negativeBucketCount + (negative ? -i : +i);
  }
  // No NOTREACHED since the +/-100 bounds are inclusive (even if they are
  // slightly exceeded due to floating point inaccuracies).
  return negative ? 0 : kPercentBucketCount - 1;
}

// Calculates index of UMA MediaTimelineAbsTimeDelta enum corresponding to
// |sumAbsDeltaSeconds|.
int32_t toAbsTimeDeltaSample(double sumAbsDeltaSeconds) {
  double sumAbsDeltaMS = 1000 * sumAbsDeltaSeconds;
  if (sumAbsDeltaMS == 0)
    return 0;  // Dedicated zero bucket.
  for (int32_t i = 0; i < kAbsTimeDeltaBucketCount; i++) {
    if (sumAbsDeltaMS < kTimeDeltaMSIntervals[i])
      return i;
  }
  NOTREACHED() << "sumAbsDeltaSeconds shouldn't be infinite";
  return kAbsTimeDeltaBucketCount - 1;
}

// Calculates index of UMA MediaTimelineTimeDelta enum corresponding to
// |deltaSeconds|. Negative values use kTimeDeltaMSIntervals in reverse.
int32_t toTimeDeltaSample(double deltaSeconds) {
  constexpr int32_t nonNegativeBucketCount = arraysize(kTimeDeltaMSIntervals);
  constexpr int32_t negativeBucketCount = arraysize(kTimeDeltaMSIntervals) - 1;
  bool negative = deltaSeconds < 0;
  double absDeltaMS = 1000 * std::abs(deltaSeconds);
  if (absDeltaMS == 0)
    return negativeBucketCount;  // Dedicated zero bucket.
  for (int32_t i = 0; i < nonNegativeBucketCount; i++) {
    if (absDeltaMS < kTimeDeltaMSIntervals[i])
      return negativeBucketCount + (negative ? -i : +i);
  }
  NOTREACHED() << "deltaSeconds shouldn't be infinite";
  return negative ? 0 : kTimeDeltaBucketCount - 1;
}

// Helper for RECORD_TIMELINE_UMA_BY_WIDTH.
#define ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, minWidth, maxWidth, metric, \
                                         sample, HistogramType, ...)        \
  else if (width >= minWidth) {                                             \
    DEFINE_STATIC_LOCAL(                                                    \
        HistogramType, metric##minWidth##_##maxWidth##Histogram,            \
        ("Media.Timeline." #metric "." #minWidth "_" #maxWidth,             \
         ##__VA_ARGS__));                                                   \
    metric##minWidth##_##maxWidth##Histogram.count(sample);                 \
  }

// Records UMA with a histogram suffix based on timelineWidth.
#define RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, metric, sample,            \
                                     HistogramType, ...)                       \
  do {                                                                         \
    int width = timelineWidth; /* Avoid multiple evaluation. */                \
    if (false) {                                                               \
      /* This if(false) allows all the conditions below to start with else. */ \
    }                                                                          \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 512, inf, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 256, 511, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 128, 255, metric, sample,          \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 80, 127, metric, sample,           \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 48, 79, metric, sample,            \
                                     HistogramType, ##__VA_ARGS__)             \
    ELSEIF_WIDTH_RECORD_TIMELINE_UMA(width, 32, 47, metric, sample,            \
                                     HistogramType, ##__VA_ARGS__)             \
    else {                                                                     \
      /* Skip logging if timeline is narrower than minimum suffix bucket. */   \
      /* If this happens a lot, it'll show up in Media.Timeline.Width. */      \
    }                                                                          \
  } while (false)

void recordDragGestureDurationByWidth(int timelineWidth, TimeDelta duration) {
  int32_t sample = static_cast<int32_t>(duration.InMilliseconds());
  RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, DragGestureDuration, sample,
                               CustomCountHistogram, 1 /* 1 ms */,
                               60000 /* 1 minute */, 50);
}
void recordDragPercentByWidth(int timelineWidth, double percent) {
  int32_t sample = toPercentSample(percent);
  RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, DragPercent, sample,
                               EnumerationHistogram, kPercentBucketCount);
}
void recordDragSumAbsTimeDeltaByWidth(int timelineWidth,
                                      double sumAbsDeltaSeconds) {
  int32_t sample = toAbsTimeDeltaSample(sumAbsDeltaSeconds);
  RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, DragSumAbsTimeDelta, sample,
                               EnumerationHistogram, kAbsTimeDeltaBucketCount);
}
void recordDragTimeDeltaByWidth(int timelineWidth, double deltaSeconds) {
  int32_t sample = toTimeDeltaSample(deltaSeconds);
  RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, DragTimeDelta, sample,
                               EnumerationHistogram, kTimeDeltaBucketCount);
}
void recordSeekTypeByWidth(int timelineWidth, SeekType type) {
  int32_t sample = static_cast<int32_t>(type);
  constexpr int32_t bucketCount = static_cast<int32_t>(SeekType::kLast) + 1;
  RECORD_TIMELINE_UMA_BY_WIDTH(timelineWidth, SeekType, sample,
                               EnumerationHistogram, bucketCount);
}

#undef RECORD_TIMELINE_UMA_BY_WIDTH
#undef ELSEIF_WIDTH_RECORD_TIMELINE_UMA

}  // namespace

void MediaControlTimelineMetrics::startGesture(bool fromThumb) {
  // Initialize gesture tracking.
  m_state = fromThumb ? State::kGestureFromThumb : State::kGestureFromElsewhere;
  m_dragStartTimeTicks = TimeTicks::Now();
  m_dragDeltaMediaSeconds = 0;
  m_dragSumAbsDeltaMediaSeconds = 0;
}

void MediaControlTimelineMetrics::recordEndGesture(
    int timelineWidth,
    double mediaDurationSeconds) {
  State endState = m_state;
  m_state = State::kInactive;  // Reset tracking.

  SeekType seekType = SeekType::kLast;  // Arbitrary inital val to appease MSVC.
  switch (endState) {
    case State::kInactive:
    case State::kKeyDown:
      return;  // Pointer and keys were interleaved. Skip UMA in this edge case.
    case State::kGestureFromThumb:
    case State::kGestureFromElsewhere:
      return;  // Empty gesture with no calls to gestureInput.
    case State::kDragFromThumb:
      seekType = SeekType::kDragFromCurrentPosition;
      break;
    case State::kClick:
      seekType = SeekType::kClick;
      break;
    case State::kDragFromElsewhere:
      seekType = SeekType::kDragFromElsewhere;
      break;
  }

  recordSeekTypeByWidth(timelineWidth, seekType);

  if (seekType == SeekType::kClick)
    return;  // Metrics below are only for drags.

  recordDragGestureDurationByWidth(timelineWidth,
                                   TimeTicks::Now() - m_dragStartTimeTicks);
  if (std::isfinite(mediaDurationSeconds)) {
    recordDragPercentByWidth(
        timelineWidth, 100.0 * m_dragDeltaMediaSeconds / mediaDurationSeconds);
  }
  recordDragSumAbsTimeDeltaByWidth(timelineWidth,
                                   m_dragSumAbsDeltaMediaSeconds);
  recordDragTimeDeltaByWidth(timelineWidth, m_dragDeltaMediaSeconds);
}

void MediaControlTimelineMetrics::startKey() {
  m_state = State::kKeyDown;
}

void MediaControlTimelineMetrics::recordEndKey(int timelineWidth, int keyCode) {
  State endState = m_state;
  m_state = State::kInactive;  // Reset tracking.
  if (endState != State::kKeyDown)
    return;  // Pointer and keys were interleaved. Skip UMA in this edge case.

  SeekType type;
  switch (keyCode) {
    case VKEY_UP:
    case VKEY_DOWN:
    case VKEY_LEFT:
    case VKEY_RIGHT:
      type = SeekType::kKeyboardArrowKey;
      break;
    case VKEY_PRIOR:  // PageUp
    case VKEY_NEXT:   // PageDown
      type = SeekType::kKeyboardPageUpDownKey;
      break;
    case VKEY_HOME:
    case VKEY_END:
      type = SeekType::kKeyboardHomeEndKey;
      break;
    default:
      return;  // Other keys don't seek (at time of writing).
  }
  recordSeekTypeByWidth(timelineWidth, type);
}

void MediaControlTimelineMetrics::onInput(double fromSeconds,
                                          double toSeconds) {
  switch (m_state) {
    case State::kInactive:
      // Unexpected input.
      m_state = State::kInactive;
      break;
    case State::kGestureFromThumb:
      // Drag confirmed now input has been received.
      m_state = State::kDragFromThumb;
      break;
    case State::kGestureFromElsewhere:
      // Click/drag confirmed now input has been received. Assume it's a click
      // until further input is received.
      m_state = State::kClick;
      break;
    case State::kClick:
      // Drag confirmed now further input has been received.
      m_state = State::kDragFromElsewhere;
      break;
    case State::kDragFromThumb:
    case State::kDragFromElsewhere:
      // Continue tracking drag.
      break;
    case State::kKeyDown:
      // Continue tracking key.
      break;
  }

  // The following tracking is only for drags. Note that we exclude kClick here,
  // as even if it progresses to a kDragFromElsewhere, the first input event
  // corresponds to the position jump from the pointer down on the track.
  if (m_state != State::kDragFromThumb && m_state != State::kDragFromElsewhere)
    return;

  float deltaMediaSeconds = static_cast<float>(toSeconds - fromSeconds);
  m_dragDeltaMediaSeconds += deltaMediaSeconds;
  m_dragSumAbsDeltaMediaSeconds += std::abs(deltaMediaSeconds);
}

void MediaControlTimelineMetrics::recordPlaying(
    WebScreenOrientationType orientation,
    bool isFullscreen,
    int timelineWidth) {
  bool isPortrait = false;  // Arbitrary initial value to appease MSVC.
  switch (orientation) {
    case WebScreenOrientationPortraitPrimary:
    case WebScreenOrientationPortraitSecondary:
      isPortrait = true;
      break;
    case WebScreenOrientationLandscapePrimary:
    case WebScreenOrientationLandscapeSecondary:
      isPortrait = false;
      break;
    case WebScreenOrientationUndefined:
      return;  // Skip UMA in the unlikely event we fail to detect orientation.
  }

  // Only record the first time each media element enters the playing state.
  if (!m_hasNeverBeenPlaying)
    return;
  m_hasNeverBeenPlaying = false;

  constexpr int32_t min = 1;
  constexpr int32_t max = 7680;  // Equivalent to an 80inch wide 8K monitor.
  constexpr int32_t bucketCount = 50;
  // Record merged histogram for all configurations.
  DEFINE_STATIC_LOCAL(CustomCountHistogram, allConfigurationsWidthHistogram,
                      ("Media.Timeline.Width", min, max, bucketCount));
  allConfigurationsWidthHistogram.count(timelineWidth);
  // Record configuration-specific histogram.
  if (!isFullscreen) {
    if (isPortrait) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, widthHistogram,
          ("Media.Timeline.Width.InlinePortrait", min, max, bucketCount));
      widthHistogram.count(timelineWidth);
    } else {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, widthHistogram,
          ("Media.Timeline.Width.InlineLandscape", min, max, bucketCount));
      widthHistogram.count(timelineWidth);
    }
  } else {
    if (isPortrait) {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, widthHistogram,
          ("Media.Timeline.Width.FullscreenPortrait", min, max, bucketCount));
      widthHistogram.count(timelineWidth);
    } else {
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, widthHistogram,
          ("Media.Timeline.Width.FullscreenLandscape", min, max, bucketCount));
      widthHistogram.count(timelineWidth);
    }
  }
}

}  // namespace blink
