// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/dev/audio_input_dev.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/utility/completion_callback_factory.h"

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
        waiting_for_flush_completion_(false),
        audio_input_0_1_(0),
        audio_input_interface_0_1_(NULL) {
  }
  virtual ~MyInstance() {
    if (!audio_input_.is_null()) {
      audio_input_.Close();
    } else if (audio_input_0_1_ != 0) {
      audio_input_interface_0_1_->StopCapture(audio_input_0_1_);
      pp::Module::Get()->core()->ReleaseResource(audio_input_0_1_);
    }

    delete[] samples_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    // This sample frequency is guaranteed to work.
    const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
    const uint32_t kSampleCount = 1024;
    const uint32_t kChannelCount = 1;

    sample_count_ = pp::AudioConfig::RecommendSampleFrameCount(this,
                                                               kSampleFrequency,
                                                               kSampleCount);
    PP_DCHECK(sample_count_ > 0);
    channel_count_ = kChannelCount;
    pp::AudioConfig config = pp::AudioConfig(this,
                                             kSampleFrequency,
                                             sample_count_);
    samples_ = new int16_t[sample_count_ * channel_count_];
    memset(samples_, 0, sample_count_ * channel_count_ * sizeof(int16_t));
    audio_input_ = pp::AudioInput_Dev(this, config, CaptureCallback, this);

    audio_input_interface_0_1_ = static_cast<const PPB_AudioInput_Dev_0_1*>(
        pp::Module::Get()->GetBrowserInterface(
            PPB_AUDIO_INPUT_DEV_INTERFACE_0_1));
    if (!audio_input_interface_0_1_)
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

  virtual void HandleMessage(const pp::Var& message_data) {
    if (message_data.is_string()) {
      std::string event = message_data.AsString();
      if (event == "PageInitialized") {
        pp::CompletionCallbackWithOutput<std::vector<pp::DeviceRef_Dev> >
            callback = callback_factory_.NewCallbackWithOutput(
                &MyInstance::EnumerateDevicesFinished);
        int32_t result = audio_input_.EnumerateDevices(callback);
        if (result != PP_OK_COMPLETIONPENDING)
          PostMessage(pp::Var("EnumerationFailed"));
      } else if (event == "UseDefault") {
        Open(pp::DeviceRef_Dev());
      } else if (event == "UseDefault(v0.1)") {
        audio_input_0_1_ = audio_input_interface_0_1_->Create(
            pp_instance(), audio_input_.config().pp_resource(),
            CaptureCallback, this);
        if (audio_input_0_1_ != 0) {
          if (!audio_input_interface_0_1_->StartCapture(audio_input_0_1_))
            PostMessage(pp::Var("StartFailed"));
        } else {
          PostMessage(pp::Var("OpenFailed"));
        }

        audio_input_ = pp::AudioInput_Dev();
      } else if (event == "Stop") {
        Stop();
      } else if (event == "Start") {
        Start();
      }
    } else if (message_data.is_number()) {
      int index = message_data.AsInt();
      if (index >= 0 && index < static_cast<int>(devices_.size())) {
        Open(devices_[index]);
      } else {
        PP_NOTREACHED();
      }
    }
  }

 private:
  void ScheduleNextTimer() {
    PP_DCHECK(timer_interval_ > 0);
    pp::Module::Get()->core()->CallOnMainThread(
        timer_interval_,
        callback_factory_.NewCallback(&MyInstance::OnTimer),
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
          callback_factory_.NewCallback(&MyInstance::DidFlush));
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
    uint32_t buffer_size =
        thiz->sample_count_ * thiz->channel_count_ * sizeof(int16_t);
    PP_DCHECK(num_bytes <= buffer_size);
    PP_DCHECK(num_bytes % (thiz->channel_count_ * sizeof(int16_t)) == 0);
    memcpy(thiz->samples_, samples, num_bytes);
    memset(reinterpret_cast<char*>(thiz->samples_) + num_bytes, 0,
           buffer_size - num_bytes);
  }

  void Open(const pp::DeviceRef_Dev& device) {
    pp::CompletionCallback callback = callback_factory_.NewCallback(
        &MyInstance::OpenFinished);
    int32_t result = audio_input_.Open(device, callback);
    if (result != PP_OK_COMPLETIONPENDING)
      PostMessage(pp::Var("OpenFailed"));
  }

  void Stop() {
    if (!audio_input_.is_null()) {
      if (!audio_input_.StopCapture())
        PostMessage(pp::Var("StopFailed"));
    } else if (audio_input_0_1_ != 0) {
      if (!audio_input_interface_0_1_->StopCapture(audio_input_0_1_))
        PostMessage(pp::Var("StopFailed"));
    }
  }

  void Start() {
    if (!audio_input_.is_null()) {
      if (!audio_input_.StartCapture())
        PostMessage(pp::Var("StartFailed"));
    } else if (audio_input_0_1_ != 0) {
      if (!audio_input_interface_0_1_->StartCapture(audio_input_0_1_))
        PostMessage(pp::Var("StartFailed"));
    }
  }

  void EnumerateDevicesFinished(int32_t result,
                                std::vector<pp::DeviceRef_Dev>& devices) {
    static const char* const kDelimiter = "#__#";

    if (result == PP_OK) {
      devices_.swap(devices);
      std::string device_names;
      for (size_t index = 0; index < devices_.size(); ++index) {
        pp::Var name = devices_[index].GetName();
        PP_DCHECK(name.is_string());

        if (index != 0)
          device_names += kDelimiter;
        device_names += name.AsString();
      }
      PostMessage(pp::Var(device_names));
    } else {
      PostMessage(pp::Var("EnumerationFailed"));
    }
  }

  void OpenFinished(int32_t result) {
    if (result == PP_OK) {
      if (!audio_input_.StartCapture())
        PostMessage(pp::Var("StartFailed"));
    } else {
      PostMessage(pp::Var("OpenFailed"));
    }
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

  PP_Resource audio_input_0_1_;
  const PPB_AudioInput_Dev_0_1* audio_input_interface_0_1_;

  std::vector<pp::DeviceRef_Dev> devices_;
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
