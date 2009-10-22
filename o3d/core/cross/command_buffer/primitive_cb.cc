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


// This file contains the implementation of the PrimitiveCB class.

#include "core/cross/precompile.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/primitive_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/stream_bank_cb.h"
#include "core/cross/error.h"
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/client/cross/o3d_cmd_helper.h"

// TODO: add unit tests.

namespace o3d {

using command_buffer::ResourceId;
using command_buffer::O3DCmdHelper;
using command_buffer::CommandBufferEntry;
using command_buffer::GAPIInterface;
using command_buffer::kInvalidResource;
namespace vertex_struct = command_buffer::vertex_struct;

PrimitiveCB::PrimitiveCB(ServiceLocator* service_locator, RendererCB *renderer)
    : Primitive(service_locator),
      renderer_(renderer) {
}

PrimitiveCB::~PrimitiveCB() {
}

// Converts an O3D primitive type to a command-buffer one.
static command_buffer::PrimitiveType GetCBPrimitiveType(
    Primitive::PrimitiveType primitive_type) {
  switch (primitive_type) {
    case Primitive::LINELIST:
      return command_buffer::kLines;
    case Primitive::LINESTRIP:
      return command_buffer::kLineStrips;
    case Primitive::TRIANGLELIST:
      return command_buffer::kTriangles;
    case Primitive::TRIANGLESTRIP:
      return command_buffer::kTriangleStrips;
    case Primitive::TRIANGLEFAN:
      return command_buffer::kTriangleFans;
    default:
      // Note that POINTLIST falls into this case, for compatibility with D3D.
      return command_buffer::kMaxPrimitiveType;
  }
}

// Sends the draw commands to the command buffers.
void PrimitiveCB::PlatformSpecificRender(Renderer* renderer,
                                         DrawElement* draw_element,
                                         Material* material,
                                         ParamObject* override,
                                         ParamCache* param_cache) {
  DLOG_ASSERT(draw_element);
  DLOG_ASSERT(param_cache);
  DLOG_ASSERT(material);

  EffectCB *effect_cb = down_cast<EffectCB*>(material->effect());
  DLOG_ASSERT(effect_cb);
  StreamBankCB* stream_bank_cb = down_cast<StreamBankCB*>(stream_bank());
  DLOG_ASSERT(stream_bank_cb);

  ParamCacheCB *param_cache_cb = down_cast<ParamCacheCB *>(param_cache);

  if (!param_cache_cb->ValidateAndCacheParams(effect_cb,
                                              draw_element,
                                              this,
                                              stream_bank_cb,
                                              material,
                                              override)) {
    // TODO: should we do this here, or on the service side ?
    // InsertMissingVertexStreams();
  }

  IndexBufferCB *index_buffer_cb =
      down_cast<IndexBufferCB *>(index_buffer());
  if (!index_buffer_cb) {
    // TODO(gman): draw non-indexed primitives.
    DLOG(INFO) << "Trying to draw with an empty index buffer.";
    return;
  }
  command_buffer::PrimitiveType cb_primitive_type =
      GetCBPrimitiveType(primitive_type_);
  if (cb_primitive_type == command_buffer::kMaxPrimitiveType) {
    DLOG(INFO) << "Invalid primitive type (" << primitive_type_ << ").";
    return;
  }

  // Make sure our streams are up to date (skinned, etc..)
  stream_bank_cb->UpdateStreams();

  stream_bank_cb->BindStreamsForRendering();

  O3DCmdHelper* helper = renderer_->helper();

  // Sets current effect.
  // TODO: cache current effect ?
  helper->SetEffect(effect_cb->resource_id());
  param_cache_cb->RunHandlers(helper);

  // Draws.
  helper->DrawIndexed(cb_primitive_type, index_buffer_cb->resource_id(),
                       0, number_primitives_, 0, number_vertices_ - 1);
}

}  // namespace o3d
