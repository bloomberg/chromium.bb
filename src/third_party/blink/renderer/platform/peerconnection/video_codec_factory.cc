// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_software_fallback_wrapper.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/engine/encoder_simulcast_proxy.h"
#include "third_party/webrtc/media/engine/internal_decoder_factory.h"
#include "third_party/webrtc/media/engine/internal_encoder_factory.h"
#include "third_party/webrtc/media/engine/simulcast_encoder_adapter.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace blink {

namespace {

template <typename Factory>
bool IsFormatSupported(const Factory* factory,
                       const webrtc::SdpVideoFormat& format) {
  return factory && format.IsCodecInList(factory->GetSupportedFormats());
}

// Merge |formats1| and |formats2|, but avoid adding duplicate formats.
std::vector<webrtc::SdpVideoFormat> MergeFormats(
    std::vector<webrtc::SdpVideoFormat> formats1,
    const std::vector<webrtc::SdpVideoFormat>& formats2) {
  for (const webrtc::SdpVideoFormat& format : formats2) {
    // Don't add same format twice.
    if (!format.IsCodecInList(formats1))
      formats1.push_back(format);
  }
  return formats1;
}

std::unique_ptr<webrtc::VideoDecoder> CreateDecoder(
    webrtc::VideoDecoderFactory* factory,
    const webrtc::SdpVideoFormat& format) {
  if (!IsFormatSupported(factory, format))
    return nullptr;
  return factory->CreateVideoDecoder(format);
}

std::unique_ptr<webrtc::VideoDecoder> Wrap(
    std::unique_ptr<webrtc::VideoDecoder> software_decoder,
    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder) {
  if (software_decoder && hardware_decoder) {
    return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(
        std::move(software_decoder), std::move(hardware_decoder));
  }
  return hardware_decoder ? std::move(hardware_decoder)
                          : std::move(software_decoder);
}

std::unique_ptr<webrtc::VideoEncoder> Wrap(
    std::unique_ptr<webrtc::VideoEncoder> software_encoder,
    std::unique_ptr<webrtc::VideoEncoder> hardware_encoder) {
  if (software_encoder && hardware_encoder) {
    return webrtc::CreateVideoEncoderSoftwareFallbackWrapper(
        std::move(software_encoder), std::move(hardware_encoder));
  }
  return hardware_encoder ? std::move(hardware_encoder)
                          : std::move(software_encoder);
}

// This class combines a hardware factory with the internal factory and adds
// internal SW codecs, simulcast, and SW fallback wrappers.
class EncoderAdapter : public webrtc::VideoEncoderFactory {
 public:
  explicit EncoderAdapter(
      std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory)
      : hardware_encoder_factory_(std::move(hardware_encoder_factory)) {}

  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    const webrtc::VideoEncoderFactory* factory =
        IsFormatSupported(hardware_encoder_factory_.get(), format)
            ? hardware_encoder_factory_.get()
            : &software_encoder_factory_;
    return factory->QueryVideoEncoder(format);
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    const bool supported_in_software =
        IsFormatSupported(&software_encoder_factory_, format);
    const bool supported_in_hardware =
        IsFormatSupported(hardware_encoder_factory_.get(), format);

    if (!supported_in_software && !supported_in_hardware)
      return nullptr;

    if (base::EqualsCaseInsensitiveASCII(format.name.c_str(),
                                         cricket::kVp9CodecName) ||
        base::EqualsCaseInsensitiveASCII(format.name.c_str(),
                                         cricket::kAv1CodecName)) {
      // For VP9 and AV1 we don't use simulcast.
      if (supported_in_hardware && supported_in_software) {
        return Wrap(software_encoder_factory_.CreateVideoEncoder(format),
                    hardware_encoder_factory_->CreateVideoEncoder(format));
      } else if (supported_in_software) {
        return software_encoder_factory_.CreateVideoEncoder(format);
      }
      return hardware_encoder_factory_->CreateVideoEncoder(format);
    }

    if (!supported_in_hardware || !hardware_encoder_factory_.get()) {
      return std::make_unique<webrtc::SimulcastEncoderAdapter>(
          &software_encoder_factory_, nullptr, format);
    } else if (!supported_in_software) {
      return std::make_unique<webrtc::SimulcastEncoderAdapter>(
          hardware_encoder_factory_.get(), nullptr, format);
    }

    return std::make_unique<webrtc::SimulcastEncoderAdapter>(
        hardware_encoder_factory_.get(), &software_encoder_factory_, format);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_encoder_factory_.GetSupportedFormats();
    return hardware_encoder_factory_
               ? MergeFormats(software_formats,
                              hardware_encoder_factory_->GetSupportedFormats())
               : software_formats;
  }

  webrtc::VideoEncoderFactory::CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override {
    webrtc::VideoEncoderFactory::CodecSupport codec_support =
        hardware_encoder_factory_
            ? hardware_encoder_factory_->QueryCodecSupport(format,
                                                           scalability_mode)
            : webrtc::VideoEncoderFactory::CodecSupport();
    if (!codec_support.is_supported) {
      codec_support =
          software_encoder_factory_.QueryCodecSupport(format, scalability_mode);
    }
    return codec_support;
  }

 private:
  webrtc::InternalEncoderFactory software_encoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory_;
};

// This class combines a hardware codec factory with the internal factory and
// adds internal SW codecs and SW fallback wrappers.
class DecoderAdapter : public webrtc::VideoDecoderFactory {
 public:
  explicit DecoderAdapter(
      std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory)
      : hardware_decoder_factory_(std::move(hardware_decoder_factory)) {}

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoDecoder> software_decoder =
        CreateDecoder(&software_decoder_factory_, format);

    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder =
        CreateDecoder(hardware_decoder_factory_.get(), format);

    return Wrap(std::move(software_decoder), std::move(hardware_decoder));
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_decoder_factory_.GetSupportedFormats();
    return hardware_decoder_factory_
               ? MergeFormats(software_formats,
                              hardware_decoder_factory_->GetSupportedFormats())
               : software_formats;
  }

  webrtc::VideoDecoderFactory::CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      bool reference_scaling) const override {
    webrtc::VideoDecoderFactory::CodecSupport codec_support =
        hardware_decoder_factory_
            ? hardware_decoder_factory_->QueryCodecSupport(format,
                                                           reference_scaling)
            : webrtc::VideoDecoderFactory::CodecSupport();
    if (!codec_support.is_supported) {
      codec_support = software_decoder_factory_.QueryCodecSupport(
          format, reference_scaling);
    }
    return codec_support;
  }

 private:
  webrtc::InternalDecoderFactory software_decoder_factory_;
  const std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory_;
};

}  // namespace

std::unique_ptr<webrtc::VideoEncoderFactory> CreateHWVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;

  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled() &&
      Platform::Current()->IsWebRtcHWEncodingEnabled()) {
    encoder_factory = std::make_unique<RTCVideoEncoderFactory>(gpu_factories);
  }

  return encoder_factory;
}

std::unique_ptr<webrtc::VideoEncoderFactory> CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  return std::make_unique<EncoderAdapter>(
      CreateHWVideoEncoderFactory(gpu_factories));
}

std::unique_ptr<webrtc::VideoDecoderFactory> CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::DecoderFactory* media_decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space) {
  const bool use_hw_decoding = gpu_factories != nullptr &&
                               gpu_factories->IsGpuVideoAcceleratorEnabled() &&
                               Platform::Current()->IsWebRtcHWDecodingEnabled();

  // If RTCVideoDecoderStreamAdapter is used then RTCVideoDecoderFactory can
  // support both SW and HW decoding, and should therefore always be
  // instantiated regardless of whether HW decoding is enabled or not.
  std::unique_ptr<RTCVideoDecoderFactory> decoder_factory;
  if (use_hw_decoding ||
      base::FeatureList::IsEnabled(media::kUseDecoderStreamForWebRTC)) {
    decoder_factory = std::make_unique<RTCVideoDecoderFactory>(
        use_hw_decoding ? gpu_factories : nullptr, media_decoder_factory,
        std::move(media_task_runner), render_color_space);
  }

  return std::make_unique<DecoderAdapter>(std::move(decoder_factory));
}

}  // namespace blink
