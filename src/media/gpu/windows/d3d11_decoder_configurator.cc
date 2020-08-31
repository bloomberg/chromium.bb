// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decoder_configurator.h"

#include <d3d11.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_surface_win.h"

namespace media {

D3D11DecoderConfigurator::D3D11DecoderConfigurator(
    DXGI_FORMAT decoder_output_dxgifmt,
    GUID decoder_guid,
    gfx::Size coded_size,
    bool is_encrypted,
    bool supports_swap_chain)
    : dxgi_format_(decoder_output_dxgifmt), decoder_guid_(decoder_guid) {
  SetUpDecoderDescriptor(coded_size);
  SetUpTextureDescriptor(supports_swap_chain, is_encrypted);
}

// static
std::unique_ptr<D3D11DecoderConfigurator> D3D11DecoderConfigurator::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const VideoDecoderConfig& config,
    MediaLog* media_log) {
  bool supports_nv12_decode_swap_chain =
      gl::DirectCompositionSurfaceWin::IsDecodeSwapChainSupported();

  DXGI_FORMAT decoder_dxgi_format = DXGI_FORMAT_NV12;
  GUID decoder_guid = {};
  if (config.codec() == kCodecH264) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is using h264 / NV12";
    decoder_guid = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
  } else if (config.profile() == VP9PROFILE_PROFILE0) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is using vp9p0 / NV12";
    decoder_guid = D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0;
  } else if (config.profile() == VP9PROFILE_PROFILE2) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is using vp9p2 / P010";
    decoder_guid = D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2;
    decoder_dxgi_format = DXGI_FORMAT_P010;
  } else {
    // TODO(tmathmeyer) support other profiles in the future.
    MEDIA_LOG(INFO, media_log)
        << "D3D11VideoDecoder does not support codec " << config.codec();
    return nullptr;
  }

  return std::make_unique<D3D11DecoderConfigurator>(
      decoder_dxgi_format, decoder_guid, config.coded_size(),
      config.is_encrypted(), supports_nv12_decode_swap_chain);
}

bool D3D11DecoderConfigurator::SupportsDevice(
    ComD3D11VideoDevice video_device) {
  for (UINT i = video_device->GetVideoDecoderProfileCount(); i--;) {
    GUID profile = {};
    if (SUCCEEDED(video_device->GetVideoDecoderProfile(i, &profile))) {
      if (profile == decoder_guid_)
        return true;
    }
  }
  return false;
}

ComD3D11Texture2D D3D11DecoderConfigurator::CreateOutputTexture(
    ComD3D11Device device,
    gfx::Size size) {
  output_texture_desc_.Width = size.width();
  output_texture_desc_.Height = size.height();

  ComD3D11Texture2D result;
  if (!SUCCEEDED(
          device->CreateTexture2D(&output_texture_desc_, nullptr, &result)))
    return nullptr;

  return result;
}

// private
void D3D11DecoderConfigurator::SetUpDecoderDescriptor(
    const gfx::Size& coded_size) {
  decoder_desc_ = {};
  decoder_desc_.Guid = decoder_guid_;
  decoder_desc_.SampleWidth = coded_size.width();
  decoder_desc_.SampleHeight = coded_size.height();
  decoder_desc_.OutputFormat = dxgi_format_;
}

// private
void D3D11DecoderConfigurator::SetUpTextureDescriptor(bool supports_swap_chain,
                                                      bool is_encrypted) {
  output_texture_desc_ = {};
  output_texture_desc_.MipLevels = 1;
  output_texture_desc_.ArraySize = D3D11DecoderConfigurator::BUFFER_COUNT;
  output_texture_desc_.Format = dxgi_format_;
  output_texture_desc_.SampleDesc.Count = 1;
  output_texture_desc_.Usage = D3D11_USAGE_DEFAULT;
  output_texture_desc_.BindFlags =
      D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

  // Decode swap chains do not support shared resources.
  // TODO(sunnyps): Find a workaround for when the decoder moves to its own
  // thread and D3D device.  See https://crbug.com/911847
  // TODO(liberato): This depends on the configuration of the TextureSelector,
  // to some degree.  If it's copying, then it can be set up to use our device
  // to make the copy, and this can always be unset.  If it's binding, then it
  // depends on whether we're on the angle device or not.
  output_texture_desc_.MiscFlags =
      supports_swap_chain ? 0 : D3D11_RESOURCE_MISC_SHARED;

  if (is_encrypted)
    output_texture_desc_.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
}

}  // namespace media
