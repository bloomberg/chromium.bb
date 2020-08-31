// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11_4.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/display_helper.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "media/media_buildflags.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace media {

namespace {

// Holder class, so that we don't keep creating CommandBufferHelpers every time
// somebody calls a callback.  We can't actually create it until we're on the
// right thread.
struct CommandBufferHelperHolder
    : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder> {
  CommandBufferHelperHolder(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>(
            std::move(task_runner)) {}
  scoped_refptr<CommandBufferHelper> helper;

 private:
  ~CommandBufferHelperHolder() = default;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>;
  friend class base::DeleteHelper<CommandBufferHelperHolder>;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelperHolder);
};

scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper(
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    scoped_refptr<CommandBufferHelperHolder> holder) {
  gpu::CommandBufferStub* stub = get_stub_cb.Run();
  if (!stub)
    return nullptr;

  DCHECK(holder);
  if (!holder->helper)
    holder->helper = CommandBufferHelper::Create(stub);

  return holder->helper;
}

}  // namespace

std::unique_ptr<VideoDecoder> D3D11VideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    D3D11VideoDecoder::GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs,
    bool is_hdr_supported) {
  // We create |impl_| on the wrong thread, but we never use it here.
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  std::unique_ptr<MediaLog> cloned_media_log = media_log->Clone();
  auto get_helper_cb =
      base::BindRepeating(CreateCommandBufferHelper, std::move(get_stub_cb),
                          scoped_refptr<CommandBufferHelperHolder>(
                              new CommandBufferHelperHolder(gpu_task_runner)));
  return base::WrapUnique<VideoDecoder>(new D3D11VideoDecoder(
      gpu_task_runner, std::move(media_log), gpu_preferences, gpu_workarounds,
      base::SequenceBound<D3D11VideoDecoderImpl>(
          gpu_task_runner, std::move(cloned_media_log), get_helper_cb),
      get_helper_cb, std::move(get_d3d11_device_cb),
      std::move(supported_configs), is_hdr_supported));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::SequenceBound<D3D11VideoDecoderImpl> impl,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs,
    bool is_hdr_supported)
    : media_log_(std::move(media_log)),
      impl_(std::move(impl)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      decoder_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      already_initialized_(false),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      get_d3d11_device_cb_(std::move(get_d3d11_device_cb)),
      get_helper_cb_(std::move(get_helper_cb)),
      supported_configs_(std::move(supported_configs)),
      is_hdr_supported_(is_hdr_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_);
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  // Post destruction to the main thread.  When this executes, it will also
  // cancel pending callbacks into |impl_| via |impl_weak_|.  Callbacks out
  // from |impl_| will be cancelled by |weak_factory_| when we return.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  impl_.Reset();

  // Explicitly destroy the decoder, since it can reference picture buffers.
  accelerated_video_decoder_.reset();

  if (already_initialized_)
    AddLifetimeProgressionStage(D3D11LifetimeProgression::kPlaybackSucceeded);
}

std::string D3D11VideoDecoder::GetDisplayName() const {
  return "D3D11VideoDecoder";
}

HRESULT D3D11VideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config,
    ComD3D11VideoDecoder video_decoder) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::InitializeAcceleratedDecoder");
  // If we got an 11.1 D3D11 Device, we can use a |ID3D11VideoContext1|,
  // otherwise we have to make sure we only use a |ID3D11VideoContext|.
  HRESULT hr;

  // |device_context_| is the primary display context, but currently
  // we share it for decoding purposes.
  auto video_context = VideoContextWrapper::CreateWrapper(usable_feature_level_,
                                                          device_context_, &hr);

  if (!SUCCEEDED(hr))
    return hr;

  profile_ = config.profile();
  if (config.codec() == kCodecVP9) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3D11VP9Accelerator>(this, media_log_.get(),
                                              video_decoder, video_device_,
                                              std::move(video_context)),
        profile_, config.color_space_info());
    return hr;
  }

  if (config.codec() == kCodecH264) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3D11H264Accelerator>(this, media_log_.get(),
                                               video_decoder, video_device_,
                                               std::move(video_context)),
        profile_, config.color_space_info());
    return hr;
  }

  return E_FAIL;
}

void D3D11VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* /* cdm_context */,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& /* waiting_cb */) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Initialize");
  if (already_initialized_)
    AddLifetimeProgressionStage(D3D11LifetimeProgression::kPlaybackSucceeded);
  AddLifetimeProgressionStage(D3D11LifetimeProgression::kInitializeStarted);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb);

  state_ = State::kInitializing;

  config_ = config;
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;

  // Verify that |config| matches one of the supported configurations.  This
  // helps us skip configs that are supported by the VDA but not us, since
  // GpuMojoMediaClient merges them.  This is not hacky, even in the tiniest
  // little bit, nope.  Definitely not.  Convinced?
  bool is_supported = false;
  for (const auto& supported_config : supported_configs_) {
    if (supported_config.Matches(config)) {
      is_supported = true;
      break;
    }
  }

  if (!is_supported) {
    NotifyError("D3D11VideoDecoder does not support this config");
    return;
  }

  if (config.is_encrypted()) {
    NotifyError("D3D11VideoDecoder does not support encrypted stream");
    return;
  }

  // Initialize the video decoder.

  // Note that we assume that this is the ANGLE device, since we don't implement
  // texture sharing properly.  That also implies that this is the GPU main
  // thread, since we use non-threadsafe properties of the device (e.g., we get
  // the immediate context).
  //
  // Also note that we don't technically have a guarantee that the ANGLE device
  // will use the most recent version of D3D11; it might be configured to use
  // D3D9.  In practice, though, it seems to use 11.1 if it's available, unless
  // it's been specifically configured via switch to avoid d3d11.
  //
  // TODO(liberato): On re-init, we can probably re-use the device.
  // TODO(liberato): This isn't allowed off the main thread, since the callback
  // does who-knows-what.  Either we should be given the angle device, or we
  // should thread-hop to get it.
  device_ = get_d3d11_device_cb_.Run();
  if (!device_) {
    // This happens if, for example, if chrome is configured to use
    // D3D9 for ANGLE.
    NotifyError("ANGLE did not provide D3D11 device");
    return;
  }

  if (!GetD3D11FeatureLevel(device_, gpu_workarounds_,
                            &usable_feature_level_)) {
    NotifyError("D3D11 feature level not supported");
    return;
  }

  device_->GetImmediateContext(device_context_.ReleaseAndGetAddressOf());

  HRESULT hr;

  // TODO(liberato): Handle cleanup better.  Also consider being less chatty in
  // the logs, since this will fall back.

  hr = device_.CopyTo(video_device_.ReleaseAndGetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get video device");
    return;
  }

  decoder_configurator_ = D3D11DecoderConfigurator::Create(
      gpu_preferences_, gpu_workarounds_, config, media_log_.get());
  if (!decoder_configurator_) {
    NotifyError("D3DD11: Config provided unsupported profile");
    return;
  }

  if (!decoder_configurator_->SupportsDevice(video_device_)) {
    NotifyError("D3D11: Device does not support decoder GUID");
    return;
  }

  FormatSupportChecker format_checker(device_);
  if (!format_checker.Initialize()) {
    // Don't fail; it'll just return no support a lot.
    MEDIA_LOG(WARNING, media_log_)
        << "Could not create format checker, continuing";
  }

  // Use IsHDRSupported to guess whether the compositor can output HDR textures.
  // See TextureSelector for notes about why the decoder should not care.
  texture_selector_ = TextureSelector::Create(
      gpu_preferences_, gpu_workarounds_,
      decoder_configurator_->TextureFormat(),
      is_hdr_supported_ ? TextureSelector::HDRMode::kSDROrHDR
                        : TextureSelector::HDRMode::kSDROnly,
      &format_checker, media_log_.get());
  if (!texture_selector_) {
    NotifyError("D3DD11: Cannot get TextureSelector for format");
    return;
  }

  // TODO(liberato): dxva does this.  don't know if we need to.
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderSkipMultithreaded)) {
    ComD3D11Multithread multi_threaded;
    hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
    if (!SUCCEEDED(hr)) {
      NotifyError("Failed to query ID3D11Multithread");
      return;
    }
    // TODO(liberato): This is a hack, since the unittest returns
    // success without providing |multi_threaded|.
    if (multi_threaded)
      multi_threaded->SetMultithreadProtected(TRUE);
  }

  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(
      decoder_configurator_->DecoderDescriptor(), &config_count);
  if (FAILED(hr) || config_count == 0) {
    NotifyError("Failed to get video decoder config count");
    return;
  }

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  bool found = false;

  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(
        decoder_configurator_->DecoderDescriptor(), i, &dec_config);
    if (FAILED(hr)) {
      NotifyError("Failed to get decoder config");
      return;
    }

    if (config.codec() == kCodecVP9 && dec_config.ConfigBitstreamRaw == 1) {
      // DXVA VP9 specification mentions ConfigBitstreamRaw "shall be 1".
      found = true;
      break;
    }

    if (config.codec() == kCodecH264 && dec_config.ConfigBitstreamRaw == 2) {
      // ConfigBitstreamRaw == 2 means the decoder uses DXVA_Slice_H264_Short.
      found = true;
      break;
    }
  }
  if (!found) {
    NotifyError("Failed to find decoder config");
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(
      decoder_configurator_->DecoderDescriptor(), &dec_config, &video_decoder);
  if (!video_decoder.Get()) {
    NotifyError("Failed to create a video decoder");
    return;
  }

  hr = InitializeAcceleratedDecoder(config, video_decoder);

  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get device context");
    return;
  }

  // At this point, playback is supported so add a line in the media log to help
  // us figure that out.
  MEDIA_LOG(INFO, media_log_) << "Video is supported by D3D11VideoDecoder";

  if (base::FeatureList::IsEnabled(kD3D11PrintCodecOnCrash)) {
    static base::debug::CrashKeyString* codec_name =
        base::debug::AllocateCrashKeyString("d3d11_playback_video_codec",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(codec_name,
                                   config.GetHumanReadableCodecName());
  }

  auto impl_init_cb = base::BindOnce(&D3D11VideoDecoder::OnGpuInitComplete,
                                     weak_factory_.GetWeakPtr());

  auto get_picture_buffer_cb =
      base::BindRepeating(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                          weak_factory_.GetWeakPtr());

  AddLifetimeProgressionStage(D3D11LifetimeProgression::kInitializeSucceeded);

  // Initialize the gpu side.  It would be nice if we could ask SB<> to elide
  // the post if we're already on that thread, but it can't.
  // Bind our own init / output cb that hop to this thread, so we don't call
  // the originals on some other thread.
  // Important but subtle note: base::Bind will copy |config_| since it's a
  // const ref.
  impl_.Post(FROM_HERE, &D3D11VideoDecoderImpl::Initialize,
             BindToCurrentLoop(std::move(impl_init_cb)));
}

void D3D11VideoDecoder::AddLifetimeProgressionStage(
    D3D11LifetimeProgression stage) {
  already_initialized_ =
      (stage == D3D11LifetimeProgression::kInitializeSucceeded);
  const std::string uma_name("Media.D3D11.DecoderLifetimeProgression");
  base::UmaHistogramEnumeration(uma_name, stage);
}

void D3D11VideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::ReceivePictureBufferFromClient");

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3D11VideoDecoder::OnGpuInitComplete(
    bool success,
    D3D11VideoDecoderImpl::ReleaseMailboxCB release_mailbox_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OnGpuInitComplete");

  if (!init_cb_) {
    // We already failed, so just do nothing.
    DCHECK_EQ(state_, State::kError);
    return;
  }

  DCHECK_EQ(state_, State::kInitializing);

  if (!success) {
    NotifyError("Gpu init failed");
    return;
  }

  release_mailbox_cb_ = std::move(release_mailbox_cb);

  state_ = State::kRunning;
  std::move(init_cb_).Run(OkStatus());
}

void D3D11VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Decode");

  // If we aren't given a decode cb, then record that.
  // crbug.com/1012464 .
  if (!decode_cb)
    base::debug::DumpWithoutCrashing();

  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    std::move(decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  input_buffer_queue_.push_back(
      std::make_pair(std::move(buffer), std::move(decode_cb)));

  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::DoDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::DoDecode");

  if (state_ != State::kRunning) {
    DVLOG(2) << __func__ << ": Do nothing in " << static_cast<int>(state_)
             << " state.";
    return;
  }

  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = std::move(input_buffer_queue_.front().first);
    current_decode_cb_ = std::move(input_buffer_queue_.front().second);
    // If we pop a null decode cb off the stack, record it so we can see if this
    // is from a top-level call, or through Decode.
    // crbug.com/1012464 .
    if (!current_decode_cb_)
      base::debug::DumpWithoutCrashing();
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // Flush, then signal the decode cb once all pictures have been output.
      current_buffer_ = nullptr;
      if (!accelerated_video_decoder_->Flush()) {
        // This will also signal error |current_decode_cb_|.
        NotifyError("Flush failed");
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      return;
    }
    // This must be after checking for EOS because there is no timestamp for an
    // EOS buffer.
    current_timestamp_ = current_buffer_->timestamp();

    accelerated_video_decoder_->SetStream(-1, *current_buffer_);
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError)
      return;

    // If somebody cleared the buffer, then stop and post.
    // TODO(liberato): It's unclear to me how this might happen.  If it does
    // fix the crash, then more investigation is required.  Please see
    // crbug.com/1012464 for more information.
    if (!current_buffer_)
      break;

    // Record if we get here with a buffer, but without a decode cb.  This
    // shouldn't happen, but does.  This will prevent the crash, and record how
    // we got here.
    // crbug.com/1012464 .
    if (!current_decode_cb_) {
      base::debug::DumpWithoutCrashing();
      current_buffer_ = nullptr;
      break;
    }

    media::AcceleratedVideoDecoder::DecodeResult result =
        accelerated_video_decoder_->Decode();
    if (state_ == State::kError) {
      // Transitioned to an error at some point.  The h264 accelerator can do
      // this if picture output fails, at least.  Until that's fixed, check
      // here and exit if so.
      return;
    }
    // TODO(liberato): switch + class enum.
    if (result == media::AcceleratedVideoDecoder::kRanOutOfStreamData) {
      current_buffer_ = nullptr;
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      break;
    } else if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      // At this point, we know the picture size.
      // If we haven't allocated picture buffers yet, then allocate some now.
      // Otherwise, stop here.  We'll restart when a picture comes back.
      if (picture_buffers_.size())
        return;
      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kConfigChange) {
      if (profile_ != accelerated_video_decoder_->GetProfile()) {
        // TODO(crbug.com/1022246): Handle profile change.
        LOG(ERROR) << "Profile change is not supported";
        NotifyError("Profile change is not supported");
        return;
      }
      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      LOG(ERROR) << "Try again is not supported";
      NotifyError("Try again is not supported");
      return;
    } else {
      LOG(ERROR) << "VDA Error " << result;
      NotifyError("Accelerated decode failed");
      return;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitializing);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Reset");

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::ABORTED);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::ABORTED);
  input_buffer_queue_.clear();

  // TODO(liberato): how do we signal an error?
  accelerated_video_decoder_->Reset();

  std::move(closure).Run();
}

bool D3D11VideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return true;
}

bool D3D11VideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return false;
}

int D3D11VideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return 4;
}

void D3D11VideoDecoder::CreatePictureBuffers() {
  // TODO(liberato): When we run off the gpu main thread, this call will need
  // to signal success / failure asynchronously.  We'll need to transition into
  // a "waiting for pictures" state, since D3D11PictureBuffer will post the gpu
  // thread work.
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::CreatePictureBuffers");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_configurator_);
  DCHECK(texture_selector_);
  gfx::Size size = accelerated_video_decoder_->GetPicSize();

  // Create an input texture array.
  ComD3D11Texture2D in_texture =
      decoder_configurator_->CreateOutputTexture(device_, size);
  if (!in_texture) {
    NotifyError("Failed to create a Texture2D for PictureBuffers");
    return;
  }

  HDRMetadata stream_metadata;
  if (config_.hdr_metadata())
    stream_metadata = *config_.hdr_metadata();
  // else leave |stream_metadata| default-initialized.  We might use it anyway.

  base::Optional<DXGI_HDR_METADATA_HDR10> display_metadata;
  if (decoder_configurator_->TextureFormat() == DXGI_FORMAT_P010) {
    // For HDR formats, try to get the display metadata.  This may fail, which
    // is okay.  We'll just skip sending the metadata.
    DisplayHelper display_helper(device_);
    display_metadata = display_helper.GetDisplayMetadata();
  }

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    DCHECK(!buffer->in_picture_use());
  picture_buffers_.clear();

  // Create each picture buffer.
  for (size_t i = 0; i < D3D11DecoderConfigurator::BUFFER_COUNT; i++) {
    auto tex_wrapper = texture_selector_->CreateTextureWrapper(
        device_, video_device_, device_context_, size);
    if (!tex_wrapper) {
      NotifyError("Unable to allocate a texture for a CopyingTexture2DWrapper");
      return;
    }

    picture_buffers_.push_back(new D3D11PictureBuffer(
        decoder_task_runner_, in_texture, std::move(tex_wrapper), size, i));
    if (!picture_buffers_[i]->Init(
            gpu_task_runner_, get_helper_cb_, video_device_,
            decoder_configurator_->DecoderGuid(), media_log_->Clone())) {
      NotifyError("Unable to allocate PictureBuffer");
      return;
    }

    // If we have display metadata, then tell the processor.  Note that the
    // order of these calls is important, and we must set the display metadata
    // if we set the stream metadata, else it can crash on some AMD cards.
    if (display_metadata) {
      if (config_.hdr_metadata() ||
          gpu_workarounds_.use_empty_video_hdr_metadata) {
        // It's okay if this has an empty-initialized metadata.
        picture_buffers_[i]->texture_wrapper()->SetStreamHDRMetadata(
            stream_metadata);
      }
      picture_buffers_[i]->texture_wrapper()->SetDisplayHDRMetadata(
          *display_metadata);
    }
  }
}

D3D11PictureBuffer* D3D11VideoDecoder::GetPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      buffer->timestamp_ = current_timestamp_;
      return buffer.get();
    }
  }

  return nullptr;
}

bool D3D11VideoDecoder::OutputResult(const CodecPicture* picture,
                                     D3D11PictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OutputResult");

  picture_buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect = picture->visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // TODO(https://crbug.com/843150): Use aspect ratio from decoder (SPS) if
  // stream metadata doesn't overrride it.
  double pixel_aspect_ratio = config_.GetPixelAspectRatio();

  base::TimeDelta timestamp = picture_buffer->timestamp_;

  MailboxHolderArray mailbox_holders;
  gfx::ColorSpace output_color_space;
  if (!picture_buffer->ProcessTexture(
          picture->get_colorspace().ToGfxColorSpace(), &mailbox_holders,
          &output_color_space)) {
    NotifyError("Unable to process texture");
    return false;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      texture_selector_->PixelFormat(), mailbox_holders,
      VideoFrame::ReleaseMailboxCB(), picture_buffer->size(), visible_rect,
      GetNaturalSize(visible_rect, pixel_aspect_ratio), timestamp);

  if (!frame) {
    // This can happen if, somehow, we get an unsupported combination of
    // pixel format, etc.
    NotifyError("Failed to construct video frame");
    return false;
  }

  // Remember that this will likely thread-hop to the GPU main thread.  Note
  // that |picture_buffer| will delete on sequence, so it's okay even if
  // |wait_complete_cb| doesn't ever run.
  auto wait_complete_cb = BindToCurrentLoop(
      base::BindOnce(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<D3D11PictureBuffer>(picture_buffer)));
  frame->SetReleaseMailboxCB(
      base::BindOnce(release_mailbox_cb_, std::move(wait_complete_cb)));

  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);
  // For NV12, overlay is allowed by default. If the decoder is going to support
  // non-NV12 textures, then this may have to be conditionally set. Also note
  // that ALLOW_OVERLAY is required for encrypted video path.
  //
  // Since all of our picture buffers allow overlay, we just use the finch
  // feature.  However, we may choose to set ALLOW_OVERLAY to false even if
  // the finch flag is enabled.  We may not choose to set ALLOW_OVERLAY if the
  // flag is off, however.
  //
  // Also note that, since we end up binding textures with GLImageDXGI, it's
  // probably okay just to allow overlay always, and let the swap chain
  // presenter decide if it wants to.
  const bool allow_overlay =
      base::FeatureList::IsEnabled(kD3D11VideoDecoderAllowOverlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);

  frame->set_color_space(output_color_space);
  output_cb_.Run(frame);
  return true;
}

// TODO(tmathmeyer) eventually have this take a Status and pass it through
// to each of the callbacks.
void D3D11VideoDecoder::NotifyError(const char* reason) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::NotifyError");
  state_ = State::kError;
  DLOG(ERROR) << reason;

  // TODO(tmathmeyer) - Remove this after plumbing Status through the
  // decode_cb and input_buffer_queue cb's.
  MEDIA_LOG(ERROR, media_log_) << reason;

  if (init_cb_)
    std::move(init_cb_).Run(
        Status(StatusCode::kDecoderInitializeNeverCompleted, reason));

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::DECODE_ERROR);
  input_buffer_queue_.clear();
}

// static
bool D3D11VideoDecoder::GetD3D11FeatureLevel(
    ComD3D11Device dev,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    D3D_FEATURE_LEVEL* feature_level) {
  if (!dev || !feature_level)
    return false;

  *feature_level = dev->GetFeatureLevel();
  if (*feature_level < D3D_FEATURE_LEVEL_11_0)
    return false;

  // TODO(tmathmeyer) should we log this to UMA?
  if (gpu_workarounds.limit_d3d11_video_decoder_to_11_0 &&
      !base::FeatureList::IsEnabled(kD3D11VideoDecoderIgnoreWorkarounds)) {
    *feature_level = D3D_FEATURE_LEVEL_11_0;
  }

  return true;
}

// static
std::vector<SupportedVideoDecoderConfig>
D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3D11DeviceCB get_d3d11_device_cb) {
  const std::string uma_name("Media.D3D11.WasVideoSupported");

  // This workaround accounts for almost half of all startup results, and it's
  // unclear that it's relevant here.  If it's off, or if we're allowed to copy
  // pictures in case binding isn't allowed, then proceed with init.
  // NOTE: experimentation showed that, yes, it does actually matter.
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderCopyPictures)) {
    // Must allow zero-copy of nv12 textures.
    if (!gpu_preferences.enable_zero_copy_dxgi_video) {
      UMA_HISTOGRAM_ENUMERATION(uma_name,
                                NotSupportedReason::kZeroCopyNv12Required);
      return {};
    }

    if (gpu_workarounds.disable_dxgi_zero_copy_video) {
      UMA_HISTOGRAM_ENUMERATION(uma_name,
                                NotSupportedReason::kZeroCopyVideoRequired);
      return {};
    }
  }

  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderIgnoreWorkarounds)) {
    // Allow all of d3d11 to be turned off by workaround.
    if (gpu_workarounds.disable_d3d11_video_decoder) {
      UMA_HISTOGRAM_ENUMERATION(uma_name, NotSupportedReason::kOffByWorkaround);
      return {};
    }
  }

  // Remember that this might query the angle device, so this won't work if
  // we're not on the GPU main thread.  Also remember that devices are thread
  // safe (contexts are not), so we could use the angle device from any thread
  // as long as we're not calling into possible not-thread-safe things to get
  // it.  I.e., if this cached it, then it'd be fine.  It's up to our caller
  // to guarantee that, though.
  //
  // Note also that, currently, we are called from the GPU main thread only.
  auto d3d11_device = get_d3d11_device_cb.Run();
  if (!d3d11_device) {
    UMA_HISTOGRAM_ENUMERATION(uma_name,
                              NotSupportedReason::kCouldNotGetD3D11Device);
    return {};
  }

  D3D_FEATURE_LEVEL usable_feature_level;
  if (!GetD3D11FeatureLevel(d3d11_device, gpu_workarounds,
                            &usable_feature_level)) {
    UMA_HISTOGRAM_ENUMERATION(
        uma_name, NotSupportedReason::kInsufficientD3D11FeatureLevel);
    return {};
  }

  std::vector<SupportedVideoDecoderConfig> configs;
  // VP9 has no default resolutions since it may not even be supported.
  ResolutionPair max_h264_resolutions(gfx::Size(1920, 1088), gfx::Size());
  ResolutionPair max_vp8_resolutions;
  ResolutionPair max_vp9_profile0_resolutions;
  ResolutionPair max_vp9_profile2_resolutions;
  const gfx::Size min_resolution(64, 64);

  GetResolutionsForDecoders(
      {D3D11_DECODER_PROFILE_H264_VLD_NOFGT}, d3d11_device, gpu_workarounds,
      &max_h264_resolutions, &max_vp8_resolutions,
      &max_vp9_profile0_resolutions, &max_vp9_profile2_resolutions);

  if (max_h264_resolutions.first.width() > 0) {
    // Push H264 configs, except HIGH10.
    // landscape
    configs.push_back(SupportedVideoDecoderConfig(
        H264PROFILE_MIN,  // profile_min
        static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE -
                                       1),  // profile_max
        min_resolution,                     // coded_size_min
        max_h264_resolutions.first,         // coded_size_max
        false,                              // allow_encrypted
        false));                            // require_encrypted
    configs.push_back(SupportedVideoDecoderConfig(
        static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE +
                                       1),  // profile_min
        H264PROFILE_MAX,                    // profile_max
        min_resolution,                     // coded_size_min
        max_h264_resolutions.first,         // coded_size_max
        false,                              // allow_encrypted
        false));                            // require_encrypted

    // portrait
    configs.push_back(SupportedVideoDecoderConfig(
        H264PROFILE_MIN,  // profile_min
        static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE -
                                       1),  // profile_max
        min_resolution,                     // coded_size_min
        max_h264_resolutions.second,        // coded_size_max
        false,                              // allow_encrypted
        false));                            // require_encrypted
    configs.push_back(SupportedVideoDecoderConfig(
        static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE +
                                       1),  // profile_min
        H264PROFILE_MAX,                    // profile_max
        min_resolution,                     // coded_size_min
        max_h264_resolutions.second,        // coded_size_max
        false,                              // allow_encrypted
        false));                            // require_encrypted
  }

  // TODO(liberato): Fill this in for VP8.

  if (max_vp9_profile0_resolutions.first.width()) {
    // landscape
    configs.push_back(SupportedVideoDecoderConfig(
        VP9PROFILE_PROFILE0,                 // profile_min
        VP9PROFILE_PROFILE0,                 // profile_max
        min_resolution,                      // coded_size_min
        max_vp9_profile0_resolutions.first,  // coded_size_max
        false,                               // allow_encrypted
        false));                             // require_encrypted
    // portrait
    configs.push_back(SupportedVideoDecoderConfig(
        VP9PROFILE_PROFILE0,                  // profile_min
        VP9PROFILE_PROFILE0,                  // profile_max
        min_resolution,                       // coded_size_min
        max_vp9_profile0_resolutions.second,  // coded_size_max
        false,                                // allow_encrypted
        false));                              // require_encrypted
  }

  if (base::FeatureList::IsEnabled(kD3D11VideoDecoderVP9Profile2)) {
    if (max_vp9_profile2_resolutions.first.width()) {
      // landscape
      configs.push_back(SupportedVideoDecoderConfig(
          VP9PROFILE_PROFILE2,                 // profile_min
          VP9PROFILE_PROFILE2,                 // profile_max
          min_resolution,                      // coded_size_min
          max_vp9_profile2_resolutions.first,  // coded_size_max
          false,                               // allow_encrypted
          false));                             // require_encrypted
      // portrait
      configs.push_back(SupportedVideoDecoderConfig(
          VP9PROFILE_PROFILE2,                  // profile_min
          VP9PROFILE_PROFILE2,                  // profile_max
          min_resolution,                       // coded_size_min
          max_vp9_profile2_resolutions.second,  // coded_size_max
          false,                                // allow_encrypted
          false));                              // require_encrypted
    }
  }

  // TODO(liberato): Should we separate out h264 and vp9?
  UMA_HISTOGRAM_ENUMERATION(uma_name, NotSupportedReason::kVideoIsSupported);

  return configs;
}

}  // namespace media
