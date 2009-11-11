/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the SamplerCB class.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/command_buffer/sampler_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "gpu/command_buffer/common/o3d_cmd_format.h"
#include "gpu/command_buffer/client/o3d_cmd_helper.h"

namespace o3d {

using command_buffer::CommandBufferEntry;
using command_buffer::O3DCmdHelper;
using command_buffer::ResourceId;
namespace sampler = command_buffer::sampler;

namespace {

sampler::AddressingMode AddressModeToCB(Sampler::AddressMode o3d_mode) {
  switch (o3d_mode) {
    case Sampler::WRAP:
      return sampler::kWrap;
    case Sampler::MIRROR:
      return sampler::kMirrorRepeat;
    case Sampler::CLAMP:
      return sampler::kClampToEdge;
    case Sampler::BORDER:
      return sampler::kClampToBorder;
    default:
      DLOG(ERROR) << "Unknown Address mode " << static_cast<int>(o3d_mode);
      return sampler::kWrap;
  }
}

sampler::FilteringMode FilterTypeToCB(Sampler::FilterType o3d_mode) {
  switch (o3d_mode) {
    case Sampler::NONE:
      return sampler::kNone;
    case Sampler::POINT:
      return sampler::kPoint;
    case Sampler::LINEAR:
    case Sampler::ANISOTROPIC:
      return sampler::kLinear;
    default:
      return sampler::kNone;
  }
}

}  // anonymous namespace

SamplerCB::SamplerCB(ServiceLocator* service_locator, RendererCB* renderer)
    : Sampler(service_locator),
      renderer_(renderer) {
  DCHECK(renderer_);
  resource_id_ = renderer_->sampler_ids().AllocateID();
  renderer_->helper()->CreateSampler(resource_id_);
}

SamplerCB::~SamplerCB() {
  renderer_->helper()->DestroySampler(resource_id_);
  renderer_->sampler_ids().FreeID(resource_id_);
}

void SamplerCB::SetTextureAndStates() {
  O3DCmdHelper* helper = renderer_->helper();
  sampler::AddressingMode address_mode_u_cb = AddressModeToCB(address_mode_u());
  sampler::AddressingMode address_mode_v_cb = AddressModeToCB(address_mode_v());
  sampler::AddressingMode address_mode_w_cb = AddressModeToCB(address_mode_w());
  sampler::FilteringMode mag_filter_cb = FilterTypeToCB(mag_filter());
  sampler::FilteringMode min_filter_cb = FilterTypeToCB(min_filter());
  sampler::FilteringMode mip_filter_cb = FilterTypeToCB(mip_filter());
  if (mag_filter_cb == sampler::kNone) mag_filter_cb = sampler::kPoint;
  if (min_filter_cb == sampler::kNone) min_filter_cb = sampler::kPoint;
  int max_max_anisotropy =
      command_buffer::o3d::SetSamplerStates::MaxAnisotropy::kMask;
  unsigned int max_anisotropy_cb =
      std::max(1, std::min(max_max_anisotropy, max_anisotropy()));
  if (min_filter() != Sampler::ANISOTROPIC) {
    max_anisotropy_cb = 1;
  }
  helper->SetSamplerStates(
      resource_id_,
      address_mode_u_cb,
      address_mode_v_cb,
      address_mode_w_cb,
      mag_filter_cb,
      min_filter_cb,
      mip_filter_cb,
      max_anisotropy_cb);

  Float4 color = border_color();
  helper->SetSamplerBorderColor(resource_id_,
                                color[0], color[1], color[2], color[3]);

  Texture *texture_object = texture();
  if (!texture_object) {
    texture_object = renderer_->error_texture();
    if (!texture_object) {
      O3D_ERROR(service_locator())
          << "Missing texture for sampler " << name();
      texture_object = renderer_->fallback_error_texture();
    }
  }

  if (texture_object) {
    helper->SetSamplerTexture(
        resource_id_,
        reinterpret_cast<uint32>(texture_object->GetTextureHandle()));
  }
}

}  // namespace o3d
