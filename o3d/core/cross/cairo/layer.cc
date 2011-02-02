/*
 * Copyright 2010, Google Inc.
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


#include "core/cross/cairo/layer.h"

#include "core/cross/error.h"
#include "core/cross/renderer.h"
#include "core/cross/cairo/renderer_cairo.h"

namespace o3d {

namespace o2d {

O3D_DEFN_CLASS(Layer, ObjectBase);

Layer::~Layer() {
  Renderer* renderer = service_locator()->GetService<Renderer>();
  RendererCairo* renderer_cairo = down_cast<RendererCairo*>(renderer);
  renderer_cairo->RemoveLayer(this);
}

Layer::Layer(ServiceLocator* service_locator)
    : ObjectBase(service_locator),
      visible_(true),
      everywhere_(false),
      alpha_(0.0),
      x_(0),
      y_(0),
      z_(0),
      width_(0),
      height_(0),
      scale_x_(1.0),
      scale_y_(1.0),
      paint_operator_(BLEND) {
  DLOG(INFO) << "Create Layer";
  Renderer* renderer = service_locator->GetService<Renderer>();
  RendererCairo* renderer_cairo = down_cast<RendererCairo*>(renderer);
  renderer_cairo->AddLayer(this);
}

ObjectBase::Ref Layer::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }

  Layer* layer = new Layer(service_locator);

  return ObjectBase::Ref(layer);
}

}  // namespace o2d

}  // namespace o3d
