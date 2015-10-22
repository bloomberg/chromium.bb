// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/test/mock_buffer_generator.h"

#include "ui/ozone/platform/drm/gpu/drm_buffer.h"

namespace ui {

MockBufferGenerator::MockBufferGenerator() {}

MockBufferGenerator::~MockBufferGenerator() {}

scoped_refptr<ScanoutBuffer> MockBufferGenerator::Create(
    const scoped_refptr<DrmDevice>& drm,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  scoped_refptr<DrmBuffer> buffer(new DrmBuffer(drm));
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  if (!buffer->Initialize(info, true /* should_register_framebuffer */))
    return NULL;

  return buffer;
}

}  // namespace ui
