// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_shm_buffer.h"

#include <cairo.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "base/logging.h"
#include "ui/wayland/wayland_display.h"

namespace ui {

WaylandShmBuffer::WaylandShmBuffer(WaylandDisplay* display,
                                   uint32_t width,
                                   uint32_t height)
    : data_surface_(NULL) {
  int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  int allocation = stride * height;

  char filename[] = "/tmp/wayland-shm-XXXXXX";
  int fd = mkstemp(filename);
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open";
    return;
  }
  if (ftruncate(fd, allocation) < 0) {
    PLOG(ERROR) << "Failed to ftruncate";
    close(fd);
    return;
  }

  unsigned char* data = static_cast<unsigned char*>(
      mmap(NULL, allocation, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  unlink(filename);

  if (data == MAP_FAILED) {
    PLOG(ERROR) << "Failed to mmap /dev/zero";
    close(fd);
    return;
  }
  data_surface_ = cairo_image_surface_create_for_data(
      data, CAIRO_FORMAT_ARGB32, width, height, stride);
  buffer_ = wl_shm_create_buffer(display->shm(), fd,
                                 width, height, stride,
                                 WL_SHM_FORMAT_PREMULTIPLIED_ARGB32);
  close(fd);
}

WaylandShmBuffer::~WaylandShmBuffer() {
  if (buffer_) {
    wl_buffer_destroy(buffer_);
    buffer_ = NULL;
  }
  if (data_surface_) {
    cairo_surface_destroy(data_surface_);
    data_surface_ = NULL;
  }
}

}  // namespace ui
