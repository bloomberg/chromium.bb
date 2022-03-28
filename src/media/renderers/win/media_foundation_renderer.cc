// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <Audioclient.h>
#include <mferror.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "base/win/wrapped_window_proc.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

ATOM g_video_window_class = 0;

// The |g_video_window_class| atom obtained is used as the |lpClassName|
// parameter in CreateWindowEx().
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa
//
// To enable OPM
// (https://docs.microsoft.com/en-us/windows/win32/medfound/output-protection-manager)
// protection for video playback, We call CreateWindowEx() to get a window
// and pass it to MFMediaEngine as an attribute.
bool InitializeVideoWindowClass() {
  if (g_video_window_class)
    return true;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"VirtualMediaFoundationCdmVideoWindow",
      &base::win::WrappedWindowProc<::DefWindowProc>, CS_OWNDC, 0, 0, nullptr,
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr,
      nullptr, &intermediate_class);
  g_video_window_class = RegisterClassEx(&intermediate_class);
  if (!g_video_window_class) {
    HRESULT register_class_error = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "RegisterClass failed: " << PrintHr(register_class_error);
    return false;
  }

  return true;
}

const std::string GetErrorReasonString(
    const MediaFoundationRenderer::ErrorReason& reason) {
#define STRINGIFY(value)                            \
  case MediaFoundationRenderer::ErrorReason::value: \
    return #value
  switch (reason) {
    STRINGIFY(kUnknown);
    STRINGIFY(kCdmProxyReceivedInInvalidState);
    STRINGIFY(kFailedToSetSourceOnMediaEngine);
    STRINGIFY(kFailedToSetCurrentTime);
    STRINGIFY(kFailedToPlay);
    STRINGIFY(kOnPlaybackError);
    STRINGIFY(kOnDCompSurfaceHandleSetError);
    STRINGIFY(kOnConnectionError);
    STRINGIFY(kFailedToSetDCompMode);
    STRINGIFY(kFailedToGetDCompSurface);
    STRINGIFY(kFailedToDuplicateHandle);
  }
#undef STRINGIFY
}

// INVALID_HANDLE_VALUE is the official invalid handle value. Historically, 0 is
// not used as a handle value too.
bool IsInvalidHandle(const HANDLE& handle) {
  return handle == INVALID_HANDLE_VALUE || handle == nullptr;
}

}  // namespace

// static
void MediaFoundationRenderer::ReportErrorReason(ErrorReason reason) {
  base::UmaHistogramEnumeration("Media.MediaFoundationRenderer.ErrorReason",
                                reason);
}

// static
bool MediaFoundationRenderer::IsSupported() {
  return base::win::GetVersion() >= base::win::Version::WIN10;
}

MediaFoundationRenderer::MediaFoundationRenderer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    bool force_dcomp_mode_for_testing)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      force_dcomp_mode_for_testing_(force_dcomp_mode_for_testing) {
  DVLOG_FUNC(1);
}

MediaFoundationRenderer::~MediaFoundationRenderer() {
  DVLOG_FUNC(1);

  // Perform shutdown/cleanup in the order (shutdown/detach/destroy) we wanted
  // without depending on the order of destructors being invoked. We also need
  // to invoke MFShutdown() after shutdown/cleanup of MF related objects.

  StopSendingStatistics();

  if (mf_media_engine_extension_)
    mf_media_engine_extension_->Shutdown();
  if (mf_media_engine_notify_)
    mf_media_engine_notify_->Shutdown();
  if (mf_media_engine_)
    mf_media_engine_->Shutdown();

  if (mf_source_)
    mf_source_->DetachResource();

  if (dxgi_device_manager_) {
    dxgi_device_manager_.Reset();
    MFUnlockDXGIDeviceManager();
  }
  if (virtual_video_window_)
    DestroyWindow(virtual_video_window_);
}

void MediaFoundationRenderer::Initialize(MediaResource* media_resource,
                                         RendererClient* client,
                                         PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);

  renderer_client_ = client;

  // If the content is not protected then we need to start off in
  // frame server mode so that the first frame's image data is
  // available to Chromium, quite a few web tests need that image.
  bool start_in_dcomp_mode = false;
  for (DemuxerStream* stream : media_resource->GetAllStreams()) {
    if (stream->type() == DemuxerStream::Type::VIDEO &&
        stream->video_decoder_config().is_encrypted()) {
      // This conditional must match the conditional in
      // MediaFoundationRendererClient::Initialize
      start_in_dcomp_mode = true;
    }
  }

  if (!start_in_dcomp_mode) {
    rendering_mode_ = RenderingMode::FrameServer;
  } else {
    rendering_mode_ = RenderingMode::DirectComposition;
  }

  HRESULT hr = CreateMediaEngine(media_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create media engine: " << PrintHr(hr);
    std::move(init_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
  } else {
    SetVolume(volume_);
    std::move(init_cb).Run(PIPELINE_OK);
  }
}

HRESULT MediaFoundationRenderer::CreateMediaEngine(
    MediaResource* media_resource) {
  DVLOG_FUNC(1);

  if (!InitializeMediaFoundation())
    return E_FAIL;

  // TODO(frankli): Only call the followings when there is a video stream.
  RETURN_IF_FAILED(InitializeDXGIDeviceManager());
  RETURN_IF_FAILED(InitializeVirtualVideoWindow());

  // The OnXxx() callbacks are invoked by MF threadpool thread, we would like
  // to bind the callbacks to |task_runner_| MessgaeLoop.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto weak_this = weak_factory_.GetWeakPtr();
  RETURN_IF_FAILED(MakeAndInitialize<MediaEngineNotifyImpl>(
      &mf_media_engine_notify_,
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackError, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackEnded, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnFormatChange, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnLoadedData, weak_this)),
      BindToCurrentLoop(
          base::BindRepeating(&MediaFoundationRenderer::OnPlaying, weak_this)),
      BindToCurrentLoop(
          base::BindRepeating(&MediaFoundationRenderer::OnWaiting, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnTimeUpdate, weak_this))));

  ComPtr<IMFAttributes> creation_attributes;
  RETURN_IF_FAILED(MFCreateAttributes(&creation_attributes, 6));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_CALLBACK, mf_media_engine_notify_.Get()));
  RETURN_IF_FAILED(
      creation_attributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS,
                                     MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
  RETURN_IF_FAILED(creation_attributes->SetUINT32(
      MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));
  if (virtual_video_window_) {
    RETURN_IF_FAILED(creation_attributes->SetUINT64(
        MF_MEDIA_ENGINE_OPM_HWND,
        reinterpret_cast<uint64_t>(virtual_video_window_)));
  }

  if (dxgi_device_manager_) {
    RETURN_IF_FAILED(creation_attributes->SetUnknown(
        MF_MEDIA_ENGINE_DXGI_MANAGER, dxgi_device_manager_.Get()));

    // TODO(crbug.com/1276067): We'll investigate scenarios to see if we can use
    // the on-screen video window size and not the native video size.
    if (rendering_mode_ == RenderingMode::FrameServer) {
      gfx::Size max_video_size;
      bool has_video = false;
      for (auto* stream : media_resource->GetAllStreams()) {
        if (stream->type() == media::DemuxerStream::VIDEO) {
          has_video = true;
          gfx::Size video_size = stream->video_decoder_config().natural_size();
          if (video_size.height() > max_video_size.height()) {
            max_video_size.set_height(video_size.height());
          }

          if (video_size.width() > max_video_size.width()) {
            max_video_size.set_width(video_size.width());
          }
        }
      }

      if (has_video) {
        RETURN_IF_FAILED(InitializeTexturePool(max_video_size));
      }
    }
  }

  RETURN_IF_FAILED(
      MakeAndInitialize<MediaEngineExtension>(&mf_media_engine_extension_));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_EXTENSION, mf_media_engine_extension_.Get()));

  ComPtr<IMFMediaEngineClassFactory> class_factory;
  RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&class_factory)));
  // TODO(frankli): Use MF_MEDIA_ENGINE_REAL_TIME_MODE for low latency hint
  // instead of 0.
  RETURN_IF_FAILED(class_factory->CreateInstance(0, creation_attributes.Get(),
                                                 &mf_media_engine_));

  auto media_resource_type_ = media_resource->GetType();
  if (media_resource_type_ != MediaResource::Type::STREAM) {
    DLOG(ERROR) << "MediaResource is not of STREAM";
    return E_INVALIDARG;
  }

  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationSourceWrapper>(
      &mf_source_, media_resource, media_log_.get(), task_runner_));

  if (force_dcomp_mode_for_testing_)
    std::ignore = SetDCompModeInternal();

  if (!mf_source_->HasEncryptedStream()) {
    // Supports clear stream for testing.
    return SetSourceOnMediaEngine();
  }

  // Has encrypted stream.
  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationProtectionManager>(
      &content_protection_manager_, task_runner_,
      base::BindRepeating(&MediaFoundationRenderer::OnProtectionManagerWaiting,
                          weak_factory_.GetWeakPtr())));
  ComPtr<IMFMediaEngineProtectedContent> protected_media_engine;
  RETURN_IF_FAILED(mf_media_engine_.As(&protected_media_engine));
  RETURN_IF_FAILED(protected_media_engine->SetContentProtectionManager(
      content_protection_manager_.Get()));

  waiting_for_mf_cdm_ = true;
  if (!cdm_context_)
    return S_OK;

  // Has |cdm_context_|.
  if (!cdm_context_->GetMediaFoundationCdmProxy(
          base::BindOnce(&MediaFoundationRenderer::OnCdmProxyReceived,
                         weak_factory_.GetWeakPtr()))) {
    DLOG(ERROR) << __func__
                << ": CdmContext does not support MF CDM interface.";
    return MF_E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT MediaFoundationRenderer::SetSourceOnMediaEngine() {
  DVLOG_FUNC(1);

  if (!mf_source_) {
    LOG(ERROR) << "mf_source_ is null.";
    return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
  }

  ComPtr<IUnknown> source_unknown;
  RETURN_IF_FAILED(mf_source_.As(&source_unknown));
  RETURN_IF_FAILED(
      mf_media_engine_extension_->SetMediaSource(source_unknown.Get()));

  DVLOG(2) << "Set MFRendererSrc scheme as the source for MFMediaEngine.";
  base::win::ScopedBstr mf_renderer_source_scheme(
      base::ASCIIToWide("MFRendererSrc"));
  // We need to set our source scheme first in order for the MFMediaEngine to
  // load of our custom MFMediaSource.
  RETURN_IF_FAILED(
      mf_media_engine_->SetSource(mf_renderer_source_scheme.Get()));

  return S_OK;
}

HRESULT MediaFoundationRenderer::InitializeDXGIDeviceManager() {
  UINT device_reset_token;
  RETURN_IF_FAILED(
      MFLockDXGIDeviceManager(&device_reset_token, &dxgi_device_manager_));

  ComPtr<ID3D11Device> d3d11_device;
  UINT creation_flags =
      (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
       D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
  static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1};
  RETURN_IF_FAILED(
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creation_flags,
                        feature_levels, std::size(feature_levels),
                        D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr));
  RETURN_IF_FAILED(media::SetDebugName(d3d11_device.Get(), "Media_MFRenderer"));

  ComPtr<ID3D10Multithread> multithreaded_device;
  RETURN_IF_FAILED(d3d11_device.As(&multithreaded_device));
  multithreaded_device->SetMultithreadProtected(TRUE);

  return dxgi_device_manager_->ResetDevice(d3d11_device.Get(),
                                           device_reset_token);
}

HRESULT MediaFoundationRenderer::InitializeVirtualVideoWindow() {
  if (!InitializeVideoWindowClass())
    return E_FAIL;

  virtual_video_window_ =
      CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
                         WS_EX_NOREDIRECTIONBITMAP,
                     reinterpret_cast<wchar_t*>(g_video_window_class), L"",
                     WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                     nullptr, nullptr, nullptr, nullptr);
  if (!virtual_video_window_) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "Failed to create virtual window: " << PrintHr(hr);
    return hr;
  }

  return S_OK;
}

void MediaFoundationRenderer::SetCdm(CdmContext* cdm_context,
                                     CdmAttachedCB cdm_attached_cb) {
  DVLOG_FUNC(1);

  if (cdm_context_ || !cdm_context) {
    DLOG(ERROR) << "Failed in checking CdmContext.";
    std::move(cdm_attached_cb).Run(false);
    return;
  }

  cdm_context_ = cdm_context;

  if (waiting_for_mf_cdm_) {
    if (!cdm_context_->GetMediaFoundationCdmProxy(
            base::BindOnce(&MediaFoundationRenderer::OnCdmProxyReceived,
                           weak_factory_.GetWeakPtr()))) {
      DLOG(ERROR) << "Decryptor does not support MF CDM interface.";
      std::move(cdm_attached_cb).Run(false);
      return;
    }
  }

  std::move(cdm_attached_cb).Run(true);
}

void MediaFoundationRenderer::SetLatencyHint(
    absl::optional<base::TimeDelta> /*latency_hint*/) {
  // TODO(frankli): Ensure MFMediaEngine rendering pipeine is in real time mode.
  NOTIMPLEMENTED() << "We do not use the latency hint today";
}

void MediaFoundationRenderer::OnCdmProxyReceived(
    scoped_refptr<MediaFoundationCdmProxy> cdm_proxy) {
  DVLOG_FUNC(1);

  if (!waiting_for_mf_cdm_ || !content_protection_manager_) {
    OnError(PIPELINE_ERROR_INVALID_STATE,
            ErrorReason::kCdmProxyReceivedInInvalidState);
    return;
  }

  waiting_for_mf_cdm_ = false;
  cdm_proxy_ = std::move(cdm_proxy);
  content_protection_manager_->SetCdmProxy(cdm_proxy_);
  mf_source_->SetCdmProxy(cdm_proxy_);

  HRESULT hr = SetSourceOnMediaEngine();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetSourceOnMediaEngine, hr);
    return;
  }
}

void MediaFoundationRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG_FUNC(2);

  HRESULT hr = PauseInternal();
  // Ignore any Pause() error. We can continue to flush |mf_source_| instead of
  // stopping the playback with error.
  DVLOG_IF(1, FAILED(hr)) << "Failed to pause playback on flush: "
                          << PrintHr(hr);

  mf_source_->FlushStreams();
  std::move(flush_cb).Run();
}

void MediaFoundationRenderer::SetRenderingMode(RenderingMode render_mode) {
  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  HRESULT hr = mf_media_engine_.As(&mf_media_engine_ex);

  if (mf_media_engine_->HasVideo()) {
    if (render_mode == RenderingMode::FrameServer) {
      // Make sure we reinitialize the texture pool
      hr = InitializeTexturePool(native_video_size_);
    } else if (render_mode == RenderingMode::DirectComposition) {
      // If needed renegotiate the DComp visual and send it to the client for
      // presentation
    } else {
      DVLOG(1) << "Rendering mode: " << static_cast<int>(render_mode)
               << " is unsupported";
      MEDIA_LOG(ERROR, media_log_)
          << "MediaFoundationRenderer SetRenderingMode: " << (int)render_mode
          << " is not defined. No change to the rendering mode.";
      hr = E_NOT_SET;
    }

    if (SUCCEEDED(hr)) {
      hr = mf_media_engine_ex->EnableWindowlessSwapchainMode(
          render_mode == RenderingMode::DirectComposition);
      if (SUCCEEDED(hr)) {
        rendering_mode_ = render_mode;
      }
    }
  }
}

bool MediaFoundationRenderer::InFrameServerMode() {
  return rendering_mode_ == RenderingMode::FrameServer;
}

void MediaFoundationRenderer::StartPlayingFrom(base::TimeDelta time) {
  double current_time = time.InSecondsF();
  DVLOG_FUNC(2) << "current_time=" << current_time;

  // Note: It is okay for |waiting_for_mf_cdm_| to be true here. The
  // MFMediaEngine supports calls to Play/SetCurrentTime before a source is set
  // (it will apply the relevant changes to the playback state once a source is
  // set on it).

  // SetCurrentTime() completes asynchronously. When the seek operation starts,
  // the MFMediaEngine sends an MF_MEDIA_ENGINE_EVENT_SEEKING event. When the
  // seek operation completes, the MFMediaEngine sends an
  // MF_MEDIA_ENGINE_EVENT_SEEKED event.
  HRESULT hr = mf_media_engine_->SetCurrentTime(current_time);
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetCurrentTime, hr);
    return;
  }

  hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToPlay, hr);
    return;
  }
}

void MediaFoundationRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG_FUNC(2) << "playback_rate=" << playback_rate;

  HRESULT hr = mf_media_engine_->SetPlaybackRate(playback_rate);
  // Ignore error so that the media continues to play rather than stopped.
  DVLOG_IF(1, FAILED(hr)) << "Failed to set playback rate: " << PrintHr(hr);
}

void MediaFoundationRenderer::GetDCompSurface(GetDCompSurfaceCB callback) {
  DVLOG_FUNC(1);

  HRESULT hr = SetDCompModeInternal();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToSetDCompMode,
            hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  HANDLE surface_handle = INVALID_HANDLE_VALUE;
  hr = GetDCompSurfaceInternal(&surface_handle);
  // The handle could still be invalid after a non failure (e.g. S_FALSE) is
  // returned. See https://crbug.com/1307065.
  if (FAILED(hr) || IsInvalidHandle(surface_handle)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToGetDCompSurface, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  // Only need read & execute access right for the handle to be duplicated
  // without breaking in sandbox_win.cc!CheckDuplicateHandle().
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result = ::DuplicateHandle(
      process, surface_handle, process, &duplicated_handle,
      GENERIC_READ | GENERIC_EXECUTE, false, DUPLICATE_CLOSE_SOURCE);
  if (!result || IsInvalidHandle(surface_handle)) {
    hr = ::GetLastError();
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToDuplicateHandle, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  std::move(callback).Run(base::win::ScopedHandle(duplicated_handle), "");
}

// TODO(crbug.com/1070030): Investigate if we need to add
// OnSelectedVideoTracksChanged() to media renderer.mojom.
void MediaFoundationRenderer::SetVideoStreamEnabled(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;
  if (!mf_source_)
    return;

  const bool needs_restart = mf_source_->SetVideoStreamEnabled(enabled);
  if (needs_restart) {
    // If the media source indicates that we need to restart playback (e.g due
    // to a newly enabled stream being EOS), queue a pause and play operation.
    PauseInternal();
    mf_media_engine_->Play();
  }
}

void MediaFoundationRenderer::SetOutputRect(const gfx::Rect& output_rect,
                                            SetOutputRectCB callback) {
  DVLOG_FUNC(2);

  if (virtual_video_window_ &&
      !::SetWindowPos(virtual_video_window_, HWND_BOTTOM, output_rect.x(),
                      output_rect.y(), output_rect.width(),
                      output_rect.height(), SWP_NOACTIVATE)) {
    DLOG(ERROR) << "Failed to SetWindowPos: "
                << PrintHr(HRESULT_FROM_WIN32(GetLastError()));
    std::move(callback).Run(false);
    return;
  }

  if (FAILED(UpdateVideoStream(output_rect))) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

HRESULT MediaFoundationRenderer::InitializeTexturePool(const gfx::Size& size) {
  DXGIDeviceScopedHandle dxgi_device_handle(dxgi_device_manager_.Get());
  ComPtr<ID3D11Device> d3d11_device = dxgi_device_handle.GetDevice();

  if (d3d11_device.Get() == nullptr) {
    return E_UNEXPECTED;
  }

  // TODO(crbug.com/1276067): change |size| to instead use the required
  // size of the output (for example if the video is only 1280x720 instead
  // of a source frame of 1920x1080 we'd use the 1280x720 texture size).
  // However we also need to investigate the scenario of WebGL and 360 video
  // where they need the original frame size instead of the window size due
  // to later image processing.
  RETURN_IF_FAILED(texture_pool_.Initialize(d3d11_device.Get(),
                                            initialized_frame_pool_cb_, size));

  return S_OK;
}

HRESULT MediaFoundationRenderer::UpdateVideoStream(const gfx::Rect& rect) {
  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&mf_media_engine_ex));
  RECT dest_rect = {0, 0, rect.width(), rect.height()};
  RETURN_IF_FAILED(mf_media_engine_ex->UpdateVideoStream(
      /*pSrc=*/nullptr, &dest_rect, /*pBorderClr=*/nullptr));
  if (rendering_mode_ == RenderingMode::FrameServer) {
    RETURN_IF_FAILED(InitializeTexturePool(native_video_size_));
  }
  return S_OK;
}

HRESULT MediaFoundationRenderer::SetDCompModeInternal() {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->EnableWindowlessSwapchainMode(true));
  return S_OK;
}

HRESULT MediaFoundationRenderer::GetDCompSurfaceInternal(
    HANDLE* surface_handle) {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->GetVideoSwapchainHandle(surface_handle));
  return S_OK;
}

HRESULT MediaFoundationRenderer::PopulateStatistics(
    PipelineStatistics& statistics) {
  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));

  base::win::ScopedPropVariant frames_rendered;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED, frames_rendered.Receive()));
  base::win::ScopedPropVariant frames_dropped;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED, frames_dropped.Receive()));

  statistics.video_frames_decoded =
      frames_rendered.get().ulVal + frames_dropped.get().ulVal;
  statistics.video_frames_dropped = frames_dropped.get().ulVal;
  DVLOG_FUNC(3) << "video_frames_decoded=" << statistics.video_frames_decoded
                << ", video_frames_dropped=" << statistics.video_frames_dropped;
  return S_OK;
}

void MediaFoundationRenderer::SendStatistics() {
  PipelineStatistics new_stats = {};
  HRESULT hr = PopulateStatistics(new_stats);
  if (FAILED(hr)) {
    LIMITED_MEDIA_LOG(INFO, media_log_, populate_statistics_failure_count_, 3)
        << "MediaFoundationRenderer failed to populate stats: " + PrintHr(hr);
    return;
  }

  if (statistics_ != new_stats) {
    // OnStatisticsUpdate() expects delta values.
    PipelineStatistics delta;
    delta.video_frames_decoded = base::ClampSub(
        new_stats.video_frames_decoded, statistics_.video_frames_decoded);
    delta.video_frames_dropped = base::ClampSub(
        new_stats.video_frames_dropped, statistics_.video_frames_dropped);
    statistics_ = new_stats;
    renderer_client_->OnStatisticsUpdate(delta);
  }
}

void MediaFoundationRenderer::StartSendingStatistics() {
  DVLOG_FUNC(2);

  // Clear `statistics_` to reset the base for OnStatisticsUpdate(), this is
  // needed since flush will clear the internal stats in MediaFoundation.
  statistics_ = PipelineStatistics();

  const auto kPipelineStatsPollingPeriod = base::Milliseconds(500);
  statistics_timer_.Start(FROM_HERE, kPipelineStatsPollingPeriod, this,
                          &MediaFoundationRenderer::SendStatistics);
}

void MediaFoundationRenderer::StopSendingStatistics() {
  DVLOG_FUNC(2);
  statistics_timer_.Stop();
}

void MediaFoundationRenderer::SetVolume(float volume) {
  DVLOG_FUNC(2) << "volume=" << volume;
  volume_ = volume;
  if (!mf_media_engine_)
    return;

  HRESULT hr = mf_media_engine_->SetVolume(volume_);
  DVLOG_IF(1, FAILED(hr)) << "Failed to set volume: " << PrintHr(hr);
}

void MediaFoundationRenderer::SetFrameReturnCallbacks(
    FrameReturnCallback frame_available_cb,
    FramePoolInitializedCallback initialized_frame_pool_cb) {
  frame_available_cb_ = std::move(frame_available_cb);
  initialized_frame_pool_cb_ = std::move(initialized_frame_pool_cb);
}

base::TimeDelta MediaFoundationRenderer::GetMediaTime() {
// GetCurrentTime is expanded as GetTickCount in base/win/windows_types.h
#undef GetCurrentTime
  double current_time = mf_media_engine_->GetCurrentTime();
// Restore macro definition.
#define GetCurrentTime() GetTickCount()
  auto media_time = base::Seconds(current_time);
  DVLOG_FUNC(3) << "media_time=" << media_time;
  return media_time;
}

void MediaFoundationRenderer::OnPlaybackError(PipelineStatus status,
                                              HRESULT hr) {
  DVLOG_FUNC(1) << "status=" << status << ", hr=" << hr;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::UmaHistogramSparse("Media.MediaFoundationRenderer.PlaybackError", hr);

  StopSendingStatistics();
  OnError(status, ErrorReason::kOnPlaybackError, hr);
}

void MediaFoundationRenderer::OnPlaybackEnded() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  StopSendingStatistics();
  renderer_client_->OnEnded();
}

void MediaFoundationRenderer::OnFormatChange() {
  DVLOG_FUNC(3);
  OnVideoNaturalSizeChange();
}

void MediaFoundationRenderer::OnLoadedData() {
  DVLOG_FUNC(3);
  OnVideoNaturalSizeChange();
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnPlaying() {
  DVLOG_FUNC(3);
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
  // The OnPlaying callback from MediaEngineNotifyImpl lets us know that an
  // MF_MEDIA_ENGINE_EVENT_PLAYING message has been received. At this point we
  // can safely start sending Statistics as any asynchronous Flush action in
  // media engine, which would have reset the engine's statistics, will have
  // been completed.
  StartSendingStatistics();
}

void MediaFoundationRenderer::OnWaiting() {
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_NOTHING,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnTimeUpdate() {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void MediaFoundationRenderer::OnProtectionManagerWaiting(WaitingReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_client_->OnWaiting(reason);
}

void MediaFoundationRenderer::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (state == BufferingState::BUFFERING_HAVE_ENOUGH) {
    max_buffering_state_ = state;
  }

  if (state == BufferingState::BUFFERING_HAVE_NOTHING &&
      max_buffering_state_ != BufferingState::BUFFERING_HAVE_ENOUGH) {
    // Prevent sending BUFFERING_HAVE_NOTHING if we haven't previously sent a
    // BUFFERING_HAVE_ENOUGH state.
    return;
  }

  DVLOG_FUNC(2) << "state=" << state << ", reason=" << reason;
  renderer_client_->OnBufferingStateChange(state, reason);
}

HRESULT MediaFoundationRenderer::PauseInternal() {
  // Media Engine resets aggregate statistics when it flushes - such as a
  // transition to the Pause state & then back to Play state. To try and
  // avoid cases where we may get Media Engine's reset statistics call
  // StopSendingStatistics before transitioning to Pause.
  StopSendingStatistics();
  return mf_media_engine_->Pause();
}

void MediaFoundationRenderer::OnVideoNaturalSizeChange() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool has_video = mf_media_engine_->HasVideo();
  DVLOG_FUNC(2) << "has_video=" << has_video;

  // Skip if there are no video streams. This can happen because this is
  // originated from MF_MEDIA_ENGINE_EVENT_FORMATCHANGE.
  if (!has_video)
    return;

  DWORD native_width;
  DWORD native_height;
  HRESULT hr =
      mf_media_engine_->GetNativeVideoSize(&native_width, &native_height);
  if (FAILED(hr)) {
    // TODO(xhwang): Add UMA to probe if this can happen.
    DLOG(ERROR) << "Failed to get native video size from MediaEngine, using "
                   "default (640x320). hr="
                << hr;
    native_video_size_ = {640, 320};
  } else {
    native_video_size_ = {base::checked_cast<int>(native_width),
                          base::checked_cast<int>(native_height)};
  }

  // TODO(frankli): Let test code to call `UpdateVideoStream()`.
  if (force_dcomp_mode_for_testing_) {
    const gfx::Rect test_rect(/*x=*/0, /*y=*/0, /*width=*/640, /*height=*/320);
    // This invokes IMFMediaEngineEx::UpdateVideoStream() for video frames to
    // be presented. Otherwise, the Media Foundation video renderer will not
    // request video samples from our source.
    std::ignore = UpdateVideoStream(test_rect);
  }

  if (rendering_mode_ == RenderingMode::FrameServer) {
    InitializeTexturePool(native_video_size_);
  }

  renderer_client_->OnVideoNaturalSizeChange(native_video_size_);
}

void MediaFoundationRenderer::OnError(PipelineStatus status,
                                      ErrorReason reason,
                                      absl::optional<HRESULT> hresult) {
  const std::string error =
      "MediaFoundationRenderer error: " + GetErrorReasonString(reason) +
      (hresult.has_value() ? (" (" + PrintHr(hresult.value()) + ")") : "");

  DLOG(ERROR) << error;
  MEDIA_LOG(ERROR, media_log_) << error;
  ReportErrorReason(reason);

  if (!hresult.has_value()) {
    renderer_client_->OnError(status);
    return;
  }

  // HRESULT 0x8004CD12 is DRM_E_TEE_INVALID_HWDRM_STATE, which can happen
  // during OS sleep/resume, or moving video to different graphics adapters.
  // This is not an error, so special case it here.
  PipelineStatus status_to_report = status;
  if (hresult == static_cast<HRESULT>(0x8004CD12)) {
    status_to_report = PIPELINE_ERROR_HARDWARE_CONTEXT_RESET;
    if (cdm_proxy_)
      cdm_proxy_->OnHardwareContextReset();
  }

  status_to_report.WithData("hresult", static_cast<uint32_t>(hresult.value()));
  renderer_client_->OnError(status_to_report);
}

void MediaFoundationRenderer::RequestNextFrameBetweenTimestamps(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (rendering_mode_ != RenderingMode::FrameServer) {
    return;
  }

  LONGLONG presentation_timestamp_in_hns = 0;
  // OnVideoStreamTick can return S_FALSE if there is no frame available.
  if (dxgi_device_manager_ == nullptr ||
      mf_media_engine_->OnVideoStreamTick(&presentation_timestamp_in_hns) !=
          S_OK) {
    return;
  }

  // TODO(crbug.com/1276067): Change the |native_video_size_| to get the correct
  // output video size as determined by the output texture requirements.
  gfx::Size video_size = native_video_size_;

  base::UnguessableToken frame_token;
  auto d3d11_video_frame = texture_pool_.AcquireTexture(&frame_token);
  if (d3d11_video_frame.Get() == nullptr)
    return;

  RECT destination_frame_size = {0, 0, video_size.width(), video_size.height()};

  ComPtr<IDXGIKeyedMutex> texture_mutex;
  d3d11_video_frame.As(&texture_mutex);

  if (texture_mutex->AcquireSync(0, INFINITE) != S_OK) {
    texture_pool_.ReleaseTexture(frame_token);
    return;
  }

  if (FAILED(mf_media_engine_->TransferVideoFrame(
          d3d11_video_frame.Get(), nullptr, &destination_frame_size,
          nullptr))) {
    texture_mutex->ReleaseSync(0);
    texture_pool_.ReleaseTexture(frame_token);
    return;
  }
  texture_mutex->ReleaseSync(0);

// Need access to GetCurrentTime on the Media Engine.
#undef GetCurrentTime
  auto frame_timestamp = base::Seconds(mf_media_engine_->GetCurrentTime());
// Restore previous definition
#define GetCurrentTime() GetTickCount()
  frame_available_cb_.Run(frame_token, video_size, frame_timestamp);
}

void MediaFoundationRenderer::NotifyFrameReleased(
    const base::UnguessableToken& frame_token) {
  texture_pool_.ReleaseTexture(frame_token);
}

}  // namespace media
