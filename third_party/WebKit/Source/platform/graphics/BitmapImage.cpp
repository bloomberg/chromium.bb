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
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

PassRefPtr<BitmapImage> BitmapImage::CreateWithOrientationForTesting(
    const SkBitmap& bitmap,
    ImageOrientation orientation) {
  if (bitmap.isNull()) {
    return BitmapImage::Create();
  }

  RefPtr<BitmapImage> result = AdoptRef(new BitmapImage(bitmap));
  result->frames_[0].orientation_ = orientation;
  if (orientation.UsesWidthAsHeight())
    result->size_respecting_orientation_ = result->size_.TransposedSize();
  return result.Release();
}

BitmapImage::BitmapImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      current_frame_(0),
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
                       ->CompositorTaskRunner()) {}

BitmapImage::BitmapImage(const SkBitmap& bitmap, ImageObserver* observer)
    : Image(observer),
      size_(bitmap.width(), bitmap.height()),
      current_frame_(0),
      cached_frame_(SkImage::MakeFromBitmap(bitmap)),
      cached_frame_index_(0),
      animation_policy_(kImageAnimationPolicyAllowed),
      animation_finished_(true),
      all_data_received_(true),
      have_size_(true),
      size_available_(true),
      have_frame_count_(true),
      repetition_count_status_(kUnknown),
      repetition_count_(kAnimationNone),
      repetitions_complete_(0),
      frame_count_(1),
      task_runner_(Platform::Current()
                       ->CurrentThread()
                       ->Scheduler()
                       ->CompositorTaskRunner()) {
  // Since we don't have a decoder, we can't figure out the image orientation.
  // Set m_sizeRespectingOrientation to be the same as m_size so it's not 0x0.
  size_respecting_orientation_ = size_;

  frames_.Grow(1);
  frames_[0].has_alpha_ = !bitmap.isOpaque();
  frames_[0].have_metadata_ = true;
}

BitmapImage::~BitmapImage() {
  StopAnimation();
}

bool BitmapImage::CurrentFrameHasSingleSecurityOrigin() const {
  return true;
}

void BitmapImage::DestroyDecodedData() {
  cached_frame_.reset();
  for (size_t i = 0; i < frames_.size(); ++i)
    frames_[i].Clear(true);
  source_.ClearCacheExceptFrame(kNotFound);
  NotifyMemoryChanged();
}

PassRefPtr<SharedBuffer> BitmapImage::Data() {
  return source_.Data();
}

void BitmapImage::NotifyMemoryChanged() {
  if (GetImageObserver())
    GetImageObserver()->DecodedSizeChangedTo(this, TotalFrameBytes());
}

size_t BitmapImage::TotalFrameBytes() {
  const size_t num_frames = FrameCount();
  size_t total_bytes = 0;
  for (size_t i = 0; i < num_frames; ++i)
    total_bytes += source_.FrameBytesAtIndex(i);
  return total_bytes;
}

sk_sp<SkImage> BitmapImage::DecodeAndCacheFrame(size_t index) {
  size_t num_frames = FrameCount();
  if (frames_.size() < num_frames)
    frames_.Grow(num_frames);

  // We are caching frame snapshots.  This is OK even for partially decoded
  // frames, as they are cleared by dataChanged() when new data arrives.
  sk_sp<SkImage> image = source_.CreateFrameAtIndex(index);
  cached_frame_ = image;
  cached_frame_index_ = index;

  frames_[index].orientation_ = source_.OrientationAtIndex(index);
  frames_[index].have_metadata_ = true;
  frames_[index].is_complete_ = source_.FrameIsCompleteAtIndex(index);
  if (RepetitionCount(false) != kAnimationNone)
    frames_[index].duration_ = source_.FrameDurationAtIndex(index);
  frames_[index].has_alpha_ = source_.FrameHasAlphaAtIndex(index);
  frames_[index].frame_bytes_ = source_.FrameBytesAtIndex(index);

  NotifyMemoryChanged();
  return image;
}

void BitmapImage::UpdateSize() const {
  if (!size_available_ || have_size_)
    return;

  size_ = source_.size();
  size_respecting_orientation_ = source_.size(kRespectImageOrientation);
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
  return source_.GetHotSpot(hot_spot);
}

Image::SizeAvailability BitmapImage::SetData(RefPtr<SharedBuffer> data,
                                             bool all_data_received) {
  if (!data)
    return kSizeAvailable;

  int length = data->size();
  if (!length)
    return kSizeAvailable;

  // If ImageSource::setData() fails, we know that this is a decode error.
  // Report size available so that it gets registered as such in
  // ImageResourceContent.
  if (!source_.SetData(std::move(data), all_data_received))
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
        cached_frame_.reset();
    }
  }

  // Feed all the data we've seen so far to the image decoder.
  all_data_received_ = all_data_received;

  have_frame_count_ = false;
  return IsSizeAvailable() ? kSizeAvailable : kSizeUnavailable;
}

bool BitmapImage::HasColorProfile() const {
  return source_.HasColorProfile();
}

String BitmapImage::FilenameExtension() const {
  return source_.FilenameExtension();
}

void BitmapImage::Draw(
    PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode clamp_mode) {
  TRACE_EVENT0("skia", "BitmapImage::draw");

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // It's too early and we don't have an image yet.

  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::Make(image.sk_image()->bounds()));

  if (adjusted_src_rect.IsEmpty() || dst_rect.IsEmpty())
    return;  // Nothing to draw.

  ImageOrientation orientation = kDefaultImageOrientation;
  if (should_respect_image_orientation == kRespectImageOrientation)
    orientation = FrameOrientationAtIndex(current_frame_);

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

  uint32_t unique_id = image.sk_image()->uniqueID();
  bool is_lazy_generated = image.sk_image()->isLazyGenerated();
  canvas->drawImageRect(std::move(image), adjusted_src_rect, adjusted_dst_rect,
                        &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));

  if (is_lazy_generated)
    PlatformInstrumentation::DidDrawLazyPixelRef(unique_id);

  StartAnimation();
}

size_t BitmapImage::FrameCount() {
  if (!have_frame_count_) {
    frame_count_ = source_.FrameCount();
    // If decoder is not initialized yet, m_source.frameCount() returns 0.
    if (frame_count_)
      have_frame_count_ = true;
  }

  return frame_count_;
}

static inline bool HasVisibleImageSize(IntSize size) {
  return (size.Width() > 1 || size.Height() > 1);
}

bool BitmapImage::IsSizeAvailable() {
  if (size_available_)
    return true;

  size_available_ = source_.IsSizeAvailable();

  if (size_available_ && HasVisibleImageSize(Size())) {
    BitmapImageMetrics::CountDecodedImageType(source_.FilenameExtension());
    if (source_.FilenameExtension() == "jpg")
      BitmapImageMetrics::CountImageOrientation(
          source_.OrientationAtIndex(0).Orientation());
  }

  return size_available_;
}

sk_sp<SkImage> BitmapImage::FrameAtIndex(size_t index) {
  if (index >= FrameCount())
    return nullptr;

  if (index == cached_frame_index_ && cached_frame_)
    return cached_frame_;

  return DecodeAndCacheFrame(index);
}

bool BitmapImage::FrameIsCompleteAtIndex(size_t index) const {
  if (index < frames_.size() && frames_[index].have_metadata_ &&
      frames_[index].is_complete_)
    return true;

  return source_.FrameIsCompleteAtIndex(index);
}

float BitmapImage::FrameDurationAtIndex(size_t index) const {
  if (index < frames_.size() && frames_[index].have_metadata_)
    return frames_[index].duration_;

  return source_.FrameDurationAtIndex(index);
}

sk_sp<SkImage> BitmapImage::ImageForCurrentFrame() {
  return FrameAtIndex(CurrentFrame());
}

PassRefPtr<Image> BitmapImage::ImageForDefaultFrame() {
  if (FrameCount() > 1) {
    sk_sp<SkImage> first_frame = FrameAtIndex(0);
    if (first_frame)
      return StaticBitmapImage::Create(std::move(first_frame));
  }

  return Image::ImageForDefaultFrame();
}

bool BitmapImage::FrameHasAlphaAtIndex(size_t index) {
  if (frames_.size() <= index)
    return true;

  if (frames_[index].have_metadata_ && !frames_[index].has_alpha_)
    return false;

  // m_hasAlpha may change after m_haveMetadata is set to true, so always ask
  // ImageSource for the value if the cached value is the default value.
  bool has_alpha = source_.FrameHasAlphaAtIndex(index);

  if (frames_[index].have_metadata_)
    frames_[index].has_alpha_ = has_alpha;

  return has_alpha;
}

bool BitmapImage::CurrentFrameKnownToBeOpaque(MetadataMode metadata_mode) {
  if (metadata_mode == kPreCacheMetadata) {
    // frameHasAlphaAtIndex() conservatively returns false for uncached frames.
    // To increase the chance of an accurate answer, pre-cache the current frame
    // metadata.
    FrameAtIndex(CurrentFrame());
  }
  return !FrameHasAlphaAtIndex(CurrentFrame());
}

bool BitmapImage::CurrentFrameIsComplete() {
  return FrameIsCompleteAtIndex(CurrentFrame());
}

bool BitmapImage::CurrentFrameIsLazyDecoded() {
  sk_sp<SkImage> image = FrameAtIndex(CurrentFrame());
  return image && image->isLazyGenerated();
}

ImageOrientation BitmapImage::CurrentFrameOrientation() {
  return FrameOrientationAtIndex(CurrentFrame());
}

ImageOrientation BitmapImage::FrameOrientationAtIndex(size_t index) {
  if (frames_.size() <= index)
    return kDefaultImageOrientation;

  if (frames_[index].have_metadata_)
    return frames_[index].orientation_;

  return source_.OrientationAtIndex(index);
}

int BitmapImage::RepetitionCount(bool image_known_to_be_complete) {
  if ((repetition_count_status_ == kUnknown) ||
      ((repetition_count_status_ == kUncertain) &&
       image_known_to_be_complete)) {
    // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
    // repetition count may not be accurate yet for GIFs; in this case the
    // decoder will default to cAnimationLoopOnce, and we'll try and read
    // the count again once the whole image is decoded.
    repetition_count_ = source_.RepetitionCount();
    repetition_count_status_ =
        (image_known_to_be_complete || repetition_count_ == kAnimationNone)
            ? kCertain
            : kUncertain;
  }
  return repetition_count_;
}

bool BitmapImage::ShouldAnimate() {
  bool animated = RepetitionCount(false) != kAnimationNone &&
                  !animation_finished_ && GetImageObserver();
  if (animated && animation_policy_ == kImageAnimationPolicyNoAnimation)
    animated = false;
  return animated;
}

void BitmapImage::StartAnimation(CatchUpAnimation catch_up_if_necessary) {
  if (frame_timer_ || !ShouldAnimate() || FrameCount() <= 1)
    return;

  // If we aren't already animating, set now as the animation start time.
  const double time = MonotonicallyIncreasingTime();
  if (!desired_frame_start_time_)
    desired_frame_start_time_ = time;

  // Don't advance the animation to an incomplete frame.
  size_t next_frame = (current_frame_ + 1) % FrameCount();
  if (!all_data_received_ && !FrameIsCompleteAtIndex(next_frame))
    return;

  // Don't advance past the last frame if we haven't decoded the whole image
  // yet and our repetition count is potentially unset.  The repetition count
  // in a GIF can potentially come after all the rest of the image data, so
  // wait on it.
  if (!all_data_received_ &&
      (RepetitionCount(false) == kAnimationLoopOnce ||
       animation_policy_ == kImageAnimationPolicyAnimateOnce) &&
      current_frame_ >= (FrameCount() - 1))
    return;

  // Determine time for next frame to start.  By ignoring paint and timer lag
  // in this calculation, we make the animation appear to run at its desired
  // rate regardless of how fast it's being repainted.
  const double current_duration = FrameDurationAtIndex(current_frame_);
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

  if (catch_up_if_necessary == kDoNotCatchUp ||
      time < desired_frame_start_time_) {
    // Haven't yet reached time for next frame to start; delay until then.
    frame_timer_ = WTF::WrapUnique(new TaskRunnerTimer<BitmapImage>(
        task_runner_, this, &BitmapImage::AdvanceAnimation));
    frame_timer_->StartOneShot(std::max(desired_frame_start_time_ - time, 0.),
                               BLINK_FROM_HERE);
  } else {
    // We've already reached or passed the time for the next frame to start.
    // See if we've also passed the time for frames after that to start, in
    // case we need to skip some frames entirely.  Remember not to advance
    // to an incomplete frame.
    for (size_t frame_after_next = (next_frame + 1) % FrameCount();
         FrameIsCompleteAtIndex(frame_after_next);
         frame_after_next = (next_frame + 1) % FrameCount()) {
      // Should we skip the next frame?
      double frame_after_next_start_time =
          desired_frame_start_time_ + FrameDurationAtIndex(next_frame);
      if (time < frame_after_next_start_time)
        break;

      // Skip the next frame by advancing the animation forward one frame.
      if (!InternalAdvanceAnimation(kSkipFramesToCatchUp)) {
        DCHECK(animation_finished_);
        return;
      }
      desired_frame_start_time_ = frame_after_next_start_time;
      next_frame = frame_after_next;
    }

    // Post a task to advance the frame immediately. m_desiredFrameStartTime
    // may be in the past, meaning the next time through this function we'll
    // kick off the next advancement sooner than this frame's duration would
    // suggest.
    frame_timer_ = WTF::WrapUnique(new TaskRunnerTimer<BitmapImage>(
        task_runner_, this, &BitmapImage::AdvanceAnimationWithoutCatchUp));
    frame_timer_->StartOneShot(0, BLINK_FROM_HERE);
  }
}

void BitmapImage::StopAnimation() {
  // This timer is used to animate all occurrences of this image.  Don't
  // invalidate the timer unless all renderers have stopped drawing.
  frame_timer_.reset();
}

void BitmapImage::ResetAnimation() {
  StopAnimation();
  current_frame_ = 0;
  repetitions_complete_ = 0;
  desired_frame_start_time_ = 0;
  animation_finished_ = false;
  cached_frame_.reset();
}

bool BitmapImage::MaybeAnimated() {
  if (animation_finished_)
    return false;
  if (FrameCount() > 1)
    return true;

  return source_.RepetitionCount() != kAnimationNone;
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

void BitmapImage::AdvanceAnimationWithoutCatchUp(TimerBase*) {
  if (InternalAdvanceAnimation())
    StartAnimation(kDoNotCatchUp);
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

  if (current_frame_ + 1 < FrameCount()) {
    current_frame_++;
  } else {
    repetitions_complete_++;

    // Get the repetition count again. If we weren't able to get a
    // repetition count before, we should have decoded the whole image by
    // now, so it should now be available.
    // We don't need to special-case cAnimationLoopOnce here because it is
    // 0 (see comments on its declaration in ImageAnimation.h).
    if ((RepetitionCount(true) != kAnimationLoopInfinite &&
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
    current_frame_ = 0;
  }

  // We need to draw this frame if we advanced to it while not skipping.
  if (advancement != kSkipFramesToCatchUp)
    GetImageObserver()->AnimationAdvanced(this);

  return true;
}

void BitmapImage::NotifyObserversOfAnimationAdvance(TimerBase*) {
  if (GetImageObserver())
    GetImageObserver()->AnimationAdvanced(this);
}

}  // namespace blink
