// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <algorithm>
#include <limits>

#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/audio_input_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        sample_count_(0),
        channel_count_(0),
        samples_(NULL),
        timer_interval_(0),
        pending_paint_(false),
        waiting_for_flush_completion_(false) {
  }
  virtual ~MyInstance() {
    audio_input_.StopCapture();
    delete[] samples_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    // This sample frequency is guaranteed to work.
    const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
    const uint32_t kSampleCount = 1024;
    const uint32_t kChannelCount = 1;

    sample_count_ = pp::AudioConfig::RecommendSampleFrameCount(kSampleFrequency,
                                                               kSampleCount);
    PP_DCHECK(sample_count_ > 0);
    channel_count_ = kChannelCount;
    pp::AudioConfig config = pp::AudioConfig(this,
                                             kSampleFrequency,
                                             sample_count_);
    samples_ = new int16_t[sample_count_ * channel_count_];
    memset(samples_, 0, sample_count_ * channel_count_ * sizeof(int16_t));
    audio_input_ = pp::AudioInput_Dev(this, config, CaptureCallback, this);
    if (!audio_input_.StartCapture())
      return false;

    // Try to ensure that we pick up a new set of samples between each
    // timer-generated repaint.
    timer_interval_ = (sample_count_ * 1000) / kSampleFrequency + 5;
    ScheduleNextTimer();

    return true;
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size() == size_)
      return;

    size_ = position.size();
    device_context_ = pp::Graphics2D(this, size_, false);
    if (!BindGraphics(device_context_))
      return;

    Paint();
  }

 private:
  void ScheduleNextTimer() {
    PP_DCHECK(timer_interval_ > 0);
    pp::Module::Get()->core()->CallOnMainThread(
        timer_interval_,
        callback_factory_.NewRequiredCallback(&MyInstance::OnTimer),
        0);
  }

  void OnTimer(int32_t) {
    ScheduleNextTimer();
    Paint();
  }

  void DidFlush(int32_t result) {
    waiting_for_flush_completion_ = false;
    if (pending_paint_)
      Paint();
  }

  void Paint() {
    if (waiting_for_flush_completion_) {
      pending_paint_ = true;
      return;
    }

    pending_paint_ = false;

    if (size_.IsEmpty())
      return;  // Nothing to do.

    pp::ImageData image = PaintImage(size_);
    if (!image.is_null()) {
      device_context_.ReplaceContents(&image);
      waiting_for_flush_completion_ = true;
      device_context_.Flush(
          callback_factory_.NewRequiredCallback(&MyInstance::DidFlush));
    }
  }

  pp::ImageData PaintImage(const pp::Size& size) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, false);
    if (image.is_null())
      return image;

    // Clear to dark grey.
    for (int y = 0; y < size.height(); y++) {
      for (int x = 0; x < size.width(); x++)
        *image.GetAddr32(pp::Point(x, y)) = 0xff202020;
    }

    int mid_height = size.height() / 2;
    int max_amplitude = size.height() * 4 / 10;

    // Draw some lines.
    for (int x = 0; x < size.width(); x++) {
      *image.GetAddr32(pp::Point(x, mid_height)) = 0xff606060;
      *image.GetAddr32(pp::Point(x, mid_height + max_amplitude)) = 0xff404040;
      *image.GetAddr32(pp::Point(x, mid_height - max_amplitude)) = 0xff404040;
    }

    // Draw our samples.
    for (int x = 0, i = 0;
         x < std::min(size.width(), static_cast<int>(sample_count_));
         x++, i += channel_count_) {
      int y = samples_[i] * max_amplitude /
              (std::numeric_limits<int16_t>::max() + 1) + mid_height;
      *image.GetAddr32(pp::Point(x, y)) = 0xffffffff;
    }

    return image;
  }

  // TODO(viettrungluu): Danger! We really should lock, but which thread
  // primitives to use? In any case, the |StopCapture()| in the destructor
  // shouldn't return until this callback is done, so at least we should be
  // writing to a valid region of memory.
  static void CaptureCallback(const void* samples,
                              uint32_t num_bytes,
                              void* ctx) {
    MyInstance* thiz = static_cast<MyInstance*>(ctx);
    PP_DCHECK(num_bytes ==
              thiz->sample_count_ * thiz->channel_count_ * sizeof(int16_t));
    memcpy(thiz->samples_, samples, num_bytes);
  }

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;

  uint32_t sample_count_;
  uint32_t channel_count_;
  int16_t* samples_;

  int32_t timer_interval_;

  // Painting stuff.
  pp::Size size_;
  pp::Graphics2D device_context_;
  bool pending_paint_;
  bool waiting_for_flush_completion_;

  pp::AudioInput_Dev audio_input_;
};

class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
