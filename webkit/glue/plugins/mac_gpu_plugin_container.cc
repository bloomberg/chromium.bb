// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/mac_gpu_plugin_container.h"

#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/mac_gpu_plugin_container_manager.h"
#include "chrome/common/io_surface_support_mac.h"

MacGPUPluginContainer::MacGPUPluginContainer()
    : x_(0),
      y_(0),
      surface_(NULL),
      width_(0),
      height_(0),
      texture_(0) {
}

MacGPUPluginContainer::~MacGPUPluginContainer() {
  ReleaseIOSurface();
}

void MacGPUPluginContainer::ReleaseIOSurface() {
  if (surface_) {
    CFRelease(surface_);
    surface_ = NULL;
  }
}

void MacGPUPluginContainer::SetSizeAndBackingStore(
    int32 width,
    int32 height,
    uint64 io_surface_identifier,
    MacGPUPluginContainerManager* manager) {
  ReleaseIOSurface();
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (io_surface_support) {
    surface_ = io_surface_support->IOSurfaceLookup(
        static_cast<uint32>(io_surface_identifier));
    EnqueueTextureForDeletion(manager);
    width_ = width;
    height_ = height;
  }
}

void MacGPUPluginContainer::MoveTo(
    const webkit_glue::WebPluginGeometry& geom) {
  x_ = geom.window_rect.x();
  y_ = geom.window_rect.y();
  // TODO(kbr): may need to pay attention to cutout rects.
  clipRect_ = geom.clip_rect;
}

void MacGPUPluginContainer::Draw(CGLContextObj context) {
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support)
    return;

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  if (!texture_ && surface_) {
    glGenTextures(1, &texture_);
    glBindTexture(target, texture_);
    glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Don't think we need to identify a plane.
    GLuint plane = 0;
    io_surface_support->CGLTexImageIOSurface2D(context,
                                               target,
                                               GL_RGBA,
                                               width_,
                                               height_,
                                               GL_BGRA,
                                               GL_UNSIGNED_INT_8_8_8_8_REV,
                                               surface_,
                                               plane);
  }

  if (texture_) {
    // TODO(kbr): convert this to use only OpenGL ES 2.0 functionality
    glBindTexture(target, texture_);
    glEnable(target);
    glBegin(GL_TRIANGLE_STRIP);
    // TODO(kbr): may need to pay attention to cutout rects.
    int clipX = clipRect_.x();
    int clipY = clipRect_.y();
    int clipWidth = clipRect_.width();
    int clipHeight = clipRect_.height();
    int x = x_ + clipX;
    int y = y_ + clipY;
    glTexCoord2f(clipX, height_ - clipY);
    glVertex3f(x, y, 0);
    glTexCoord2f(clipX + clipWidth, height_ - clipY);
    glVertex3f(x + clipWidth, y, 0);
    glTexCoord2f(clipX, height_ - clipY - clipHeight);
    glVertex3f(x, y + clipHeight, 0);
    glTexCoord2f(clipX + clipWidth, height_ - clipY - clipHeight);
    glVertex3f(x + clipWidth, y + clipHeight, 0);
    glDisable(target);
    glEnd();
  }
}

void MacGPUPluginContainer::EnqueueTextureForDeletion(
    MacGPUPluginContainerManager* manager) {
  manager->EnqueueTextureForDeletion(texture_);
  texture_ = 0;
}

