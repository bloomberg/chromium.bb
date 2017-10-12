/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/BitmapImage.h"

#include "base/metrics/histogram_macros.h"
#include "platform/Timer.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/instrumentation/PlatformInstrumentation.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebSettings.h"

namespace blink {

int GetRepetitionCountWithPolicyOverride(int actual_count,
                                         ImageAnimationPolicy policy) {
  switch (policy) {
    case kImageAnimationPolicyAllowed:
      // Default policy, no count override.
      return actual_count;
    case kImageAnimationPolicyAnimateOnce:
      // Only a single loop allowed.
      return kAnimationLoopOnce;
    case kImageAnimationPolicyNoAnimation:
      // Dont animate.
      return kAnimationNone;
  }

  NOTREACHED();
  return actual_count;
}

BitmapImage::BitmapImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      current_frame_index_(0),
      cached_frame_index_(0),
      animation_policy_(kImageAnimationPolicyAllowed),
      animation_finished_(false),
      all_data_received_(false),
      have_size_(false),
      size_available_(false),
      have_frame_count_(false),
      repetition_count_status_(kUnknown),
      repetition_count_(kAnimationNone),
      repetitions_complete_(0),
      desired_frame_start_time_(0),
      frame_count_(0),
      task_runner_(Platform::Current()
                       ->CurrentThread()
                       ->Scheduler()
                       ->CompositorTaskRunner()),
      weak_factory_(this) {}

BitmapImage::~BitmapImage() {
  StopAnimation();
}

bool BitmapImage::CurrentFrameHasSingleSecurityOrigin() const {
  return true;
}

void BitmapImage::DestroyDecodedData() {
  cached_frame_ = PaintImage();
  for (size_t i = 0; i < frames_.size(); ++i)
    frames_[i].Clear(true);
  if (decoder_)
    decoder_->ClearCacheExceptFrame(kNotFound);
  NotifyMemoryChanged();
}

RefPtr<SharedBuffer> BitmapImage::Data() {
  return decoder_ ? decoder_->Data() : nullptr;
}

void BitmapImage::NotifyMemoryChanged() {
  if (GetImageObserver())
    GetImageObserver()->DecodedSizeChangedTo(this, TotalFrameBytes());
}

size_t BitmapImage::TotalFrameBytes() {
  size_t total_bytes = 0;
  for (size_t i = 0; i < frames_.size(); ++i)
    total_bytes += frames_[i].frame_bytes_;
  return total_bytes;
}

PaintImage BitmapImage::CreateAndCacheFrame(size_t index) {
  sk_sp<PaintImageGenerator> generator =
      decoder_ ? decoder_->CreateGenerator(index) : nullptr;
  if (!generator)
    return PaintImage();

  size_t num_frames = FrameCount();
  if (frames_.size() < num_frames)
    frames_.Grow(num_frames);

  frames_[index].orientation_ = decoder_->OrientationAtIndex(index);
  frames_[index].have_metadata_ = true;
  frames_[index].is_complete_ = decoder_->FrameIsReceivedAtIndex(index);
  if (RepetitionCount() != kAnimationNone)
    frames_[index].duration_ = decoder_->FrameDurationAtIndex(index);
  frames_[index].has_alpha_ = decoder_->FrameHasAlphaAtIndex(index);
  frames_[index].frame_bytes_ =
      decoder_->Size().Area() * sizeof(ImageFrame::PixelData);

  auto completion_state = all_data_received_
                              ? PaintImage::CompletionState::DONE
                              : PaintImage::CompletionState::PARTIALLY_DONE;
  auto builder =
      CreatePaintImageBuilder()
          .set_paint_image_generator(std::move(generator))
          .set_frame_index(index)
          .set_repetition_count(GetRepetitionCountWithPolicyOverride(
              repetition_count_, animation_policy_))
          .set_completion_state(completion_state)
          .set_reset_animation_sequence_id(reset_animation_sequence_id_);

  // We are caching frame snapshots.  This is OK even for partially decoded
  // frames, as they are cleared by dataChanged() when new data arrives.
  cached_frame_ = builder.TakePaintImage();
  cached_frame_index_ = index;

  // Create the SkImage backing for this PaintImage here to ensure that copies
  // of the PaintImage share the same SkImage. Skia's caching of the decoded
  // output of this image is tied to the lifetime of the SkImage. So we create
  // the SkImage here and cache the PaintImage to keep the decode alive in
  // skia's cache.
  cached_frame_.GetSkImage();

  NotifyMemoryChanged();
  return cached_frame_;
}

void BitmapImage::UpdateSize() const {
  if (!size_available_ || have_size_ || !decoder_)
    return;

  size_ = decoder_->FrameSizeAtIndex(0);
  if (decoder_->OrientationAtIndex(0).UsesWidthAsHeight())
    size_respecting_orientation_ = size_.TransposedSize();
  else
    size_respecting_orientation_ = size_;
  have_size_ = true;
}

IntSize BitmapImage::Size() const {
  UpdateSize();
  return size_;
}

IntSize BitmapImage::SizeRespectingOrientation() const {
  UpdateSize();
  return size_respecting_orientation_;
}

bool BitmapImage::GetHotSpot(IntPoint& hot_spot) const {
  return decoder_ && decoder_->HotSpot(hot_spot);
}

Image::SizeAvailability BitmapImage::SetData(RefPtr<SharedBuffer> data,
                                             bool all_data_received) {
  if (!data)
    return kSizeAvailable;

  int length = data->size();
  if (!length)
    return kSizeAvailable;

  if (decoder_) {
    decoder_->SetData(std::move(data), all_data_received);
    return DataChanged(all_data_received);
  }

  bool has_enough_data = ImageDecoder::HasSufficientDataToSniffImageType(*data);
  decoder_ = DeferredImageDecoder::Create(std::move(data), all_data_received,
                                          ImageDecoder::kAlphaPremultiplied,
                                          ColorBehavior::Tag());
  // If we had enough data but couldn't create a decoder, it implies a decode
  // failure.
  if (has_enough_data && !decoder_)
    return kSizeAvailable;
  return DataChanged(all_data_received);
}

Image::SizeAvailability BitmapImage::DataChanged(bool all_data_received) {
  TRACE_EVENT0("blink", "BitmapImage::dataChanged");

  // Clear all partially-decoded frames. For most image formats, there is only
  // one frame, but at least GIF and ICO can have more. With GIFs, the frames
  // come in order and we ask to decode them in order, waiting to request a
  // subsequent frame until the prior one is complete. Given that we clear
  // incomplete frames here, this means there is at most one incomplete frame
  // (even if we use destroyDecodedData() -- since it doesn't reset the
  // metadata), and it is after all the complete frames.
  //
  // With ICOs, on the other hand, we may ask for arbitrary frames at
  // different times (e.g. because we're displaying a higher-resolution image
  // in the content area and using a lower-resolution one for the favicon),
  // and the frames aren't even guaranteed to appear in the file in the same
  // order as in the directory, so an arbitrary number of the frames might be
  // incomplete (if we ask for frames for which we've not yet reached the
  // start of the frame data), and any or none of them might be the particular
  // frame affected by appending new data here. Thus we have to clear all the
  // incomplete frames to be safe.
  for (size_t i = 0; i < frames_.size(); ++i) {
    // NOTE: Don't call frameIsCompleteAtIndex() here, that will try to
    // decode any uncached (i.e. never-decoded or
    // cleared-on-a-previous-pass) frames!
    if (frames_[i].have_metadata_ && !frames_[i].is_complete_) {
      frames_[i].Clear(true);
      if (i == cached_frame_index_)
        cached_frame_ = PaintImage();
    }
  }

  // If the image is being animated by the compositor, clear the cached_frame_
  // on a data update to push it to the compositor. Since we never advance the
  // animation here, the |cached_frame_index_| is always the first frame and the
  // |cached_frame_| might have not have been cleared in the loop above.
  if (RuntimeEnabledFeatures::CompositorImageAnimationsEnabled()
      && MaybeAnimated())
    cached_frame_ = PaintImage();

  // Feed all the data we've seen so far to the image decoder.
  all_data_received_ = all_data_received;

  have_frame_count_ = false;

  // Reset the cached image if the metadata has changed.
  if (cached_frame_) {
    PaintImage::CompletionState new_completion_state =
        all_data_received_ ? PaintImage::CompletionState::DONE
                           : PaintImage::CompletionState::PARTIALLY_DONE;
    const bool metadata_changed =
        cached_frame_.repetition_count() != RepetitionCount() ||
        cached_frame_.completion_state() != new_completion_state;
    if (metadata_changed)
      cached_frame_ = PaintImage();
  }

  return IsSizeAvailable() ? kSizeAvailable : kSizeUnavailable;
}

bool BitmapImage::HasColorProfile() const {
  return decoder_ && decoder_->HasEmbeddedColorSpace();
}

String BitmapImage::FilenameExtension() const {
  return decoder_ ? decoder_->FilenameExtension() : String();
}

void BitmapImage::Draw(
    PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode decode_mode) {
  TRACE_EVENT0("skia", "BitmapImage::draw");

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // It's too early and we don't have an image yet.

  auto paint_image_decoding_mode = ToPaintImageDecodingMode(decode_mode);
  if (image.decoding_mode() != paint_image_decoding_mode) {
    image = PaintImageBuilder::WithCopy(std::move(image))
                .set_decoding_mode(paint_image_decoding_mode)
                .TakePaintImage();
  }

  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::MakeWH(image.width(), image.height()));

  if (adjusted_src_rect.IsEmpty() || dst_rect.IsEmpty())
    return;  // Nothing to draw.

  ImageOrientation orientation = kDefaultImageOrientation;
  if (should_respect_image_orientation == kRespectImageOrientation)
    orientation = FrameOrientationAtIndex(current_frame_index_);

  PaintCanvasAutoRestore auto_restore(canvas, false);
  FloatRect adjusted_dst_rect = dst_rect;
  if (orientation != kDefaultImageOrientation) {
    canvas->save();

    // ImageOrientation expects the origin to be at (0, 0)
    canvas->translate(adjusted_dst_rect.X(), adjusted_dst_rect.Y());
    adjusted_dst_rect.SetLocation(FloatPoint());

    canvas->concat(AffineTransformToSkMatrix(
        orientation.TransformFromDefault(adjusted_dst_rect.Size())));

    if (orientation.UsesWidthAsHeight()) {
      // The destination rect will have its width and height already reversed
      // for the orientation of the image, as it was needed for page layout, so
      // we need to reverse it back here.
      adjusted_dst_rect =
          FloatRect(adjusted_dst_rect.X(), adjusted_dst_rect.Y(),
                    adjusted_dst_rect.Height(), adjusted_dst_rect.Width());
    }
  }

  uint32_t unique_id = image.GetSkImage()->uniqueID();
  bool is_lazy_generated = image.IsLazyGenerated();
  canvas->drawImageRect(std::move(image), adjusted_src_rect, adjusted_dst_rect,
                        &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));

  if (is_lazy_generated)
    PlatformInstrumentation::DidDrawLazyPixelRef(unique_id);

  StartAnimation();
}

size_t BitmapImage::FrameCount() {
  if (!have_frame_count_) {
    frame_count_ = decoder_ ? decoder_->FrameCount() : 0;
    have_frame_count_ = frame_count_ > 0;
  }
  return frame_count_;
}

static inline bool HasVisibleImageSize(IntSize size) {
  return (size.Width() > 1 || size.Height() > 1);
}

bool BitmapImage::IsSizeAvailable() {
  if (size_available_)
    return true;

  size_available_ = decoder_ && decoder_->IsSizeAvailable();
  if (size_available_ && HasVisibleImageSize(Size())) {
    BitmapImageMetrics::CountDecodedImageType(decoder_->FilenameExtension());
    if (decoder_->FilenameExtension() == "jpg") {
      BitmapImageMetrics::CountImageOrientation(
          decoder_->OrientationAtIndex(0).Orientation());
    }
  }

  return size_available_;
}

PaintImage BitmapImage::FrameAtIndex(size_t index) {
  if (index >= FrameCount())
    return PaintImage();

  if (index == cached_frame_index_ && cached_frame_)
    return cached_frame_;

  return CreateAndCacheFrame(index);
}

bool BitmapImage::FrameIsReceivedAtIndex(size_t index) const {
  if (index < frames_.size() && frames_[index].have_metadata_ &&
      frames_[index].is_complete_)
    return true;

  return decoder_ && decoder_->FrameIsReceivedAtIndex(index);
}

TimeDelta BitmapImage::FrameDurationAtIndex(size_t index) const {
  if (index < frames_.size() && frames_[index].have_metadata_)
    return frames_[index].duration_;

  if (!decoder_)
    return TimeDelta();
  return decoder_->FrameDurationAtIndex(index);
}

PaintImage BitmapImage::PaintImageForCurrentFrame() {
  return FrameAtIndex(current_frame_index_);
}

RefPtr<Image> BitmapImage::ImageForDefaultFrame() {
  if (FrameCount() > 1) {
    PaintImage paint_image = FrameAtIndex(PaintImage::kDefaultFrameIndex);
    if (paint_image.ShouldAnimate()) {
      // To prevent the compositor from animating this image, we set the
      // animation count to kAnimationNone. This makes the image essentially
      // static.
      paint_image = PaintImageBuilder::WithCopy(std::move(paint_image))
                        .set_repetition_count(kAnimationNone)
                        .TakePaintImage();
    }
    return StaticBitmapImage::Create(std::move(paint_image));
  }

  return Image::ImageForDefaultFrame();
}

bool BitmapImage::FrameHasAlphaAtIndex(size_t index) {
  if (frames_.size() <= index)
    return true;

  if (frames_[index].have_metadata_ && !frames_[index].has_alpha_)
    return false;

  // has_alpha may change after have_metadata_ is set to true, so always ask
  // |decoder_| for the value if the cached value is the default value.
  bool has_alpha = !decoder_ || decoder_->FrameHasAlphaAtIndex(index);

  if (frames_[index].have_metadata_)
    frames_[index].has_alpha_ = has_alpha;

  return has_alpha;
}

bool BitmapImage::CurrentFrameKnownToBeOpaque(MetadataMode metadata_mode) {
  if (metadata_mode == kPreCacheMetadata) {
    // frameHasAlphaAtIndex() conservatively returns false for uncached frames.
    // To increase the chance of an accurate answer, pre-cache the current frame
    // metadata.
    FrameAtIndex(current_frame_index_);
  }
  return !FrameHasAlphaAtIndex(current_frame_index_);
}

bool BitmapImage::CurrentFrameIsComplete() {
  return FrameIsReceivedAtIndex(current_frame_index_);
}

bool BitmapImage::CurrentFrameIsLazyDecoded() {
  // BitmapImage supports only lazy generated images.
  return true;
}

ImageOrientation BitmapImage::CurrentFrameOrientation() {
  return FrameOrientationAtIndex(current_frame_index_);
}

ImageOrientation BitmapImage::FrameOrientationAtIndex(size_t index) {
  if (!decoder_ || frames_.size() <= index)
    return kDefaultImageOrientation;

  if (frames_[index].have_metadata_)
    return frames_[index].orientation_;

  return decoder_->OrientationAtIndex(index);
}

int BitmapImage::RepetitionCount() {
  if ((repetition_count_status_ == kUnknown) ||
      ((repetition_count_status_ == kUncertain) && all_data_received_)) {
    // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
    // repetition count may not be accurate yet for GIFs; in this case the
    // decoder will default to cAnimationLoopOnce, and we'll try and read
    // the count again once the whole image is decoded.
    repetition_count_ = decoder_ ? decoder_->RepetitionCount() : kAnimationNone;
    repetition_count_status_ =
        (all_data_received_ || repetition_count_ == kAnimationNone)
            ? kCertain
            : kUncertain;
  }
  return repetition_count_;
}

bool BitmapImage::ShouldAnimate() {
  if (RuntimeEnabledFeatures::CompositorImageAnimationsEnabled())
    return false;

  bool animated = RepetitionCount() != kAnimationNone && !animation_finished_ &&
                  GetImageObserver();
  if (animated && animation_policy_ == kImageAnimationPolicyNoAnimation)
    animated = false;
  return animated;
}

void BitmapImage::StartAnimation() {
  last_num_frames_skipped_ =
      StartAnimationInternal(MonotonicallyIncreasingTime());
  if (!last_num_frames_skipped_.has_value())
    return;

  UMA_HISTOGRAM_COUNTS_100000("AnimatedImage.NumOfFramesSkipped.Main",
                              last_num_frames_skipped_.value());
}

Optional<size_t> BitmapImage::StartAnimationInternal(const double time) {
  // If the |frame_timer_| is set, it indicates that a task is already pending
  // to advance the current frame of the animation. We don't need to schedule
  // a task to advance the animation in that case.
  if (frame_timer_ || !ShouldAnimate() || FrameCount() <= 1)
    return WTF::nullopt;

  // If we aren't already animating, set now as the animation start time.
  if (!desired_frame_start_time_)
    desired_frame_start_time_ = time;

  // Don't advance the animation to an incomplete frame.
  size_t next_frame = (current_frame_index_ + 1) % FrameCount();
  if (!FrameIsReceivedAtIndex(next_frame))
    return WTF::nullopt;

  // Don't advance past the last frame if we haven't decoded the whole image
  // yet and our repetition count is potentially unset.  The repetition count
  // in a GIF can potentially come after all the rest of the image data, so
  // wait on it.
  if (!all_data_received_ &&
      (RepetitionCount() == kAnimationLoopOnce ||
       animation_policy_ == kImageAnimationPolicyAnimateOnce) &&
      current_frame_index_ >= (FrameCount() - 1))
    return WTF::nullopt;

  // Determine time for next frame to start.  By ignoring paint and timer lag
  // in this calculation, we make the animation appear to run at its desired
  // rate regardless of how fast it's being repainted.
  // TODO(vmpstr): This function can probably deal in TimeTicks/TimeDelta
  // instead.
  const double current_duration =
      FrameDurationAtIndex(current_frame_index_).InSecondsF();
  desired_frame_start_time_ += current_duration;

  // When an animated image is more than five minutes out of date, the
  // user probably doesn't care about resyncing and we could burn a lot of
  // time looping through frames below.  Just reset the timings.
  const double kCAnimationResyncCutoff = 5 * 60;
  if ((time - desired_frame_start_time_) > kCAnimationResyncCutoff)
    desired_frame_start_time_ = time + current_duration;

  // The image may load more slowly than it's supposed to animate, so that by
  // the time we reach the end of the first repetition, we're well behind.
  // Clamp the desired frame start time in this case, so that we don't skip
  // frames (or whole iterations) trying to "catch up".  This is a tradeoff:
  // It guarantees users see the whole animation the second time through and
  // don't miss any repetitions, and is closer to what other browsers do; on
  // the other hand, it makes animations "less accurate" for pages that try to
  // sync an image and some other resource (e.g. audio), especially if users
  // switch tabs (and thus stop drawing the animation, which will pause it)
  // during that initial loop, then switch back later.
  if (next_frame == 0 && repetitions_complete_ == 0 &&
      desired_frame_start_time_ < time)
    desired_frame_start_time_ = time;

  if (time < desired_frame_start_time_) {
    // Haven't yet reached time for next frame to start; delay until then
    frame_timer_ = WTF::WrapUnique(new TaskRunnerTimer<BitmapImage>(
        task_runner_, this, &BitmapImage::AdvanceAnimation));
    frame_timer_->StartOneShot(std::max(desired_frame_start_time_ - time, 0.),
                               BLINK_FROM_HERE);

    // No frames needed to be skipped to advance to the next frame.
    return Optional<size_t>(0u);
  }

  // We've already reached or passed the time for the next frame to start.
  // See if we've also passed the time for frames after that to start, in
  // case we need to skip some frames entirely.  Remember not to advance
  // to an incomplete frame.
  // Note that |desired_frame_start_time_| is always set to the time at which
  // |next_frame| should be displayed.
  size_t frames_advanced = 0u;
  for (; FrameIsReceivedAtIndex(next_frame);
       next_frame = (current_frame_index_ + 1) % FrameCount()) {
    // Should we skip the next frame?
    // TODO(vmpstr): This function can probably deal in TimeTicks/TimeDelta
    // instead.
    if (time < desired_frame_start_time_)
      break;

    // Skip the next frame by advancing the animation forward one frame.
    if (!InternalAdvanceAnimation(kSkipFramesToCatchUp)) {
      DCHECK(animation_finished_);

      // No frames skipped, we simply marked the animation as finished on the
      // first attempt to advance it.
      if (frames_advanced == 0u)
        return WTF::nullopt;

      // Don't include the |current_frame_index_|, the last frame we will be
      // painting when finishing this animation, in the number of frames
      // skipped.
      return Optional<size_t>(frames_advanced - 1);
    }

    DCHECK_EQ(current_frame_index_, next_frame);
    frames_advanced++;
    desired_frame_start_time_ +=
        FrameDurationAtIndex(current_frame_index_).InSecondsF();
  }

  DCHECK_GT(frames_advanced, 0u);

  // Since we just advanced a bunch of frames during catch up, post a
  // notification to the observers. Note this has to be async because the
  // animation can happen during painting and this invalidation is required
  // after the current paint.
  task_runner_->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&BitmapImage::NotifyObserversOfAnimationAdvance,
                weak_factory_.CreateWeakPtr(), nullptr));

  // Reset the |desired_frame_start_time_| to the time for starting the
  // |current_frame_index_|. Whenever StartAnimationInternal decides to schedule
  // the task for the next frame (which may not happen in the call below), it
  // always updates the |desired_frame_start_time_| based on the current frame
  // duration.
  desired_frame_start_time_ -=
      FrameDurationAtIndex(current_frame_index_).InSecondsF();

  // Set up the timer for the next frame if required. Note that we have
  // already advanced to the current_frame_index_ after catching up. And in
  // the loop above, we either could not advance the animation further or we
  // advanced it up till the desired time for the current frame. This ensures
  // that calling StartAnimationInternal here with the same |time| will not
  // need to perform any catch up skipping.
  StartAnimationInternal(time);

  // At this point, we've advanced to the |current_frame_index_|, and requested
  // an invalidation from the observers, and potentially scheduled a task for
  // further advancing the animation. If the task runs before the next draw,
  // current_frame_index_ will be skipped, if not, we will draw with it. For the
  // purpose of keeping the UMA tracking simple, we always exclude the
  // |current_frame_index_|, since if we do end up drawing before the task runs,
  // we won't emit an UMA entry for advancing to the next frame with no
  // skipping.
  return Optional<size_t>(frames_advanced - 1);
}

void BitmapImage::StopAnimation() {
  // This timer is used to animate all occurrences of this image.  Don't
  // invalidate the timer unless all renderers have stopped drawing.
  frame_timer_.reset();
}

void BitmapImage::ResetAnimation() {
  StopAnimation();
  current_frame_index_ = 0;
  repetitions_complete_ = 0;
  desired_frame_start_time_ = 0;
  animation_finished_ = false;
  cached_frame_ = PaintImage();
  reset_animation_sequence_id_++;
}

bool BitmapImage::MaybeAnimated() {
  if (animation_finished_)
    return false;
  if (FrameCount() > 1)
    return true;

  return decoder_ && decoder_->RepetitionCount() != kAnimationNone;
}

void BitmapImage::AdvanceTime(double delta_time_in_seconds) {
  if (desired_frame_start_time_)
    desired_frame_start_time_ -= delta_time_in_seconds;
  else
    desired_frame_start_time_ =
        MonotonicallyIncreasingTime() - delta_time_in_seconds;
}

void BitmapImage::AdvanceAnimation(TimerBase*) {
  InternalAdvanceAnimation();
  // At this point the image region has been marked dirty, and if it's
  // onscreen, we'll soon make a call to draw(), which will call
  // startAnimation() again to keep the animation moving.
}

bool BitmapImage::InternalAdvanceAnimation(AnimationAdvancement advancement) {
  // Stop the animation.
  StopAnimation();

  if (!GetImageObserver())
    return false;

  // See if anyone is still paying attention to this animation.  If not, we
  // don't advance, and will remain suspended at the current frame until the
  // animation is resumed.
  if (advancement != kSkipFramesToCatchUp &&
      GetImageObserver()->ShouldPauseAnimation(this))
    return false;

  if (current_frame_index_ + 1 < FrameCount()) {
    current_frame_index_++;
  } else {
    repetitions_complete_++;

    // We don't need to special-case cAnimationLoopOnce here because it is
    // 0 (see comments on its declaration in ImageAnimation.h).
    if ((RepetitionCount() != kAnimationLoopInfinite &&
         repetitions_complete_ > repetition_count_) ||
        animation_policy_ == kImageAnimationPolicyAnimateOnce) {
      animation_finished_ = true;
      desired_frame_start_time_ = 0;

      // We skipped to the last frame and cannot advance further. The
      // observer will not receive animationAdvanced notifications while
      // skipping but we still need to notify the observer to draw the
      // last frame. Skipping frames occurs while painting so we do not
      // synchronously notify the observer which could cause a layout.
      if (advancement == kSkipFramesToCatchUp) {
        frame_timer_ = WTF::WrapUnique(new TaskRunnerTimer<BitmapImage>(
            task_runner_, this,
            &BitmapImage::NotifyObserversOfAnimationAdvance));
        frame_timer_->StartOneShot(0, BLINK_FROM_HERE);
      }

      return false;
    }

    // Loop the animation back to the first frame.
    current_frame_index_ = 0;
  }

  // We need to draw this frame if we advanced to it while not skipping.
  if (advancement != kSkipFramesToCatchUp)
    GetImageObserver()->AnimationAdvanced(this);

  return true;
}

void BitmapImage::SetAnimationPolicy(ImageAnimationPolicy policy) {
  if (animation_policy_ == policy)
    return;

  animation_policy_ = policy;
  ResetAnimation();
}

void BitmapImage::NotifyObserversOfAnimationAdvance(TimerBase*) {
  if (GetImageObserver())
    GetImageObserver()->AnimationAdvanced(this);
}

STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyAllowed,
                   kImageAnimationPolicyAllowed);
STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyAnimateOnce,
                   kImageAnimationPolicyAnimateOnce);
STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyNoAnimation,
                   kImageAnimationPolicyNoAnimation);

}  // namespace blink
