/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#ifndef BitmapImage_h
#define BitmapImage_h

#include <memory>
#include "platform/Timer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/FrameData.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/image-decoders/ImageAnimation.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT BitmapImage final : public Image {
  friend class BitmapImageTest;
  friend class CrossfadeGeneratedImage;
  friend class GeneratedImage;
  friend class GradientGeneratedImage;
  friend class GraphicsContext;

 public:
  static RefPtr<BitmapImage> Create(ImageObserver* observer = 0,
                                    bool is_multipart = false) {
    return WTF::AdoptRef(new BitmapImage(observer, is_multipart));
  }

  ~BitmapImage() override;

  bool IsBitmapImage() const override { return true; }

  bool CurrentFrameHasSingleSecurityOrigin() const override;

  IntSize Size() const override;
  IntSize SizeRespectingOrientation() const;
  bool GetHotSpot(IntPoint&) const override;
  String FilenameExtension() const override;

  SizeAvailability SetData(RefPtr<SharedBuffer> data,
                           bool all_data_received) override;
  SizeAvailability DataChanged(bool all_data_received) override;

  bool IsAllDataReceived() const { return all_data_received_; }
  bool HasColorProfile() const;

  void ResetAnimation() override;
  bool MaybeAnimated() override;

  void SetAnimationPolicy(ImageAnimationPolicy) override;
  ImageAnimationPolicy AnimationPolicy() override { return animation_policy_; }
  void AdvanceTime(double delta_time_in_seconds) override;

  RefPtr<Image> ImageForDefaultFrame() override;

  bool CurrentFrameKnownToBeOpaque(MetadataMode = kUseCurrentMetadata) override;
  bool CurrentFrameIsComplete() override;
  bool CurrentFrameIsLazyDecoded() override;
  size_t FrameCount() override;

  ImageOrientation CurrentFrameOrientation();

  // Advance the image animation by one frame.
  void AdvanceAnimationForTesting() override { InternalAdvanceAnimation(); }

  PaintImage PaintImageForCurrentFrame() override;

  void SetDecoderForTesting(std::unique_ptr<DeferredImageDecoder> decoder) {
    decoder_ = std::move(decoder);
  }
  void SetTaskRunnerForTesting(RefPtr<WebTaskRunner> task_runner) {
    task_runner_ = task_runner;
  }

  Optional<size_t> last_num_frames_skipped_for_testing() const {
    return last_num_frames_skipped_;
  }

 protected:
  bool IsSizeAvailable() override;

 private:
  enum RepetitionCountStatus : uint8_t {
    kUnknown,    // We haven't checked the source's repetition count.
    kUncertain,  // We have a repetition count, but it might be wrong (some GIFs
                 // have a count after the image data, and will report "loop
                 // once" until all data has been decoded).
    kCertain     // The repetition count is known to be correct.
  };

  BitmapImage(const SkBitmap&, ImageObserver* = nullptr);
  BitmapImage(ImageObserver* = nullptr, bool is_multi_part = false);

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;

  PaintImage FrameAtIndex(size_t);

  bool FrameIsReceivedAtIndex(size_t) const;
  TimeDelta FrameDurationAtIndex(size_t) const;
  bool FrameHasAlphaAtIndex(size_t);
  ImageOrientation FrameOrientationAtIndex(size_t);

  PaintImage CreateAndCacheFrame(size_t index);
  void UpdateSize() const;

  // Returns the total number of bytes allocated for all framebuffers, i.e.
  // the sum of m_source.frameBytesAtIndex(...) for all frames.
  size_t TotalFrameBytes();

  // Called to wipe out the entire frame buffer cache and tell the image
  // source to destroy everything; this is used when e.g. we want to free
  // some room in the image cache.
  void DestroyDecodedData() override;

  RefPtr<SharedBuffer> Data() override;

  // Notifies observers that the memory footprint has changed.
  void NotifyMemoryChanged();

  // Animation.
  // We start and stop animating lazily.  Animation starts when the image is
  // rendered, and automatically stops once no observer wants to render the
  // image.

  int RepetitionCount();

  bool ShouldAnimate();
  void StartAnimation() override;
  // Starts the animation by scheduling a task to advance to the next desired
  // frame, if possible, and catching up any frames if the time to display them
  // is in the past.
  Optional<size_t> StartAnimationInternal(const double time);
  void StopAnimation();
  void AdvanceAnimation(TimerBase*);

  // This function does the real work of advancing the animation. When
  // skipping frames to catch up, we're in the middle of a loop trying to skip
  // over a bunch of animation frames, so we should not do things like decode
  // each one or notify our observers.
  // Returns whether the animation was advanced.
  enum AnimationAdvancement { kNormal, kSkipFramesToCatchUp };
  bool InternalAdvanceAnimation(AnimationAdvancement = kNormal);

  void NotifyObserversOfAnimationAdvance(TimerBase*);

  std::unique_ptr<DeferredImageDecoder> decoder_;
  mutable IntSize size_;  // The size to use for the overall image (will just
                          // be the size of the first image).
  mutable IntSize size_respecting_orientation_;

  size_t current_frame_index_;   // The index of the current frame of animation.
  Vector<FrameData, 1> frames_;  // An array of the cached frames of the
                                 // animation. We have to ref frames to pin
                                 // them in the cache.

  PaintImage
      cached_frame_;  // A cached copy of the most recently-accessed frame.
  size_t cached_frame_index_;  // Index of the frame that is cached.

  std::unique_ptr<TaskRunnerTimer<BitmapImage>> frame_timer_;

  ImageAnimationPolicy
      animation_policy_;  // Whether or not we can play animation.

  bool animation_finished_ : 1;  // Whether we've completed the entire
                                 // animation.

  bool all_data_received_ : 1;  // Whether we've received all our data.
  mutable bool have_size_ : 1;  // Whether our |m_size| member variable has the
                                // final overall image size yet.
  bool size_available_ : 1;     // Whether we can obtain the size of the first
                                // image frame from ImageIO yet.
  bool have_frame_count_ : 1;

  RepetitionCountStatus repetition_count_status_;
  int repetition_count_;  // How many total animation loops we should do.  This
                          // will be cAnimationNone if this image type is
                          // incapable of animation.
  int repetitions_complete_;  // How many repetitions we've finished.

  double desired_frame_start_time_;  // The system time at which we hope to see
                                     // the next call to startAnimation().

  size_t frame_count_;

  PaintImage::AnimationSequenceId reset_animation_sequence_id_ = 0;

  RefPtr<WebTaskRunner> task_runner_;

  // Value used in UMA tracking for the number of animation frames skipped
  // during catch-up.
  Optional<size_t> last_num_frames_skipped_ = 0u;

  WTF::WeakPtrFactory<BitmapImage> weak_factory_;
};

DEFINE_IMAGE_TYPE_CASTS(BitmapImage);

}  // namespace blink

#endif
