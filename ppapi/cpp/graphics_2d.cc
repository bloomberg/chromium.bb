// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/graphics_2d.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics2D_1_0>() {
  return PPB_GRAPHICS_2D_INTERFACE_1_0;
}

}  // namespace

Graphics2D::Graphics2D() : Resource() {
}

Graphics2D::Graphics2D(const Graphics2D& other)
    : Resource(other),
      size_(other.size_) {
}

Graphics2D::Graphics2D(const InstanceHandle& instance,
                       const Size& size,
                       bool is_always_opaque)
    : Resource() {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return;
  PassRefFromConstructor(get_interface<PPB_Graphics2D_1_0>()->Create(
      instance.pp_instance(),
      &size.pp_size(),
      PP_FromBool(is_always_opaque)));
  if (!is_null()) {
    // Only save the size if allocation succeeded.
    size_ = size;
  }
}

Graphics2D::~Graphics2D() {
}

Graphics2D& Graphics2D::operator=(const Graphics2D& other) {
  Resource::operator=(other);
  size_ = other.size_;
  return *this;
}

void Graphics2D::PaintImageData(const ImageData& image,
                                const Point& top_left) {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return;
  get_interface<PPB_Graphics2D_1_0>()->PaintImageData(pp_resource(),
                                                      image.pp_resource(),
                                                      &top_left.pp_point(),
                                                      NULL);
}

void Graphics2D::PaintImageData(const ImageData& image,
                                const Point& top_left,
                                const Rect& src_rect) {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return;
  get_interface<PPB_Graphics2D_1_0>()->PaintImageData(pp_resource(),
                                                      image.pp_resource(),
                                                      &top_left.pp_point(),
                                                      &src_rect.pp_rect());
}

void Graphics2D::Scroll(const Rect& clip, const Point& amount) {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return;
  get_interface<PPB_Graphics2D_1_0>()->Scroll(pp_resource(),
                                              &clip.pp_rect(),
                                              &amount.pp_point());
}

void Graphics2D::ReplaceContents(ImageData* image) {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return;
  get_interface<PPB_Graphics2D_1_0>()->ReplaceContents(pp_resource(),
                                                       image->pp_resource());

  // On success, reset the image data. This is to help prevent people
  // from continuing to use the resource which will result in artifacts.
  *image = ImageData();
}

int32_t Graphics2D::Flush(const CompletionCallback& cc) {
  if (!has_interface<PPB_Graphics2D_1_0>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Graphics2D_1_0>()->Flush(
      pp_resource(), cc.pp_completion_callback());
}

}  // namespace pp
