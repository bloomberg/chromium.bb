// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_
#define UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/client_native_pixmap.h"

namespace base {

struct FileDescriptor;

}  // namespace base

namespace ui {

// The Ozone interface allows external implementations to hook into Chromium to
// provide a client pixmap for non-GPU processes.
class OZONE_EXPORT ClientNativePixmapFactory {
 public:
  static ClientNativePixmapFactory* GetInstance();
  static void SetInstance(ClientNativePixmapFactory* instance);

  static scoped_ptr<ClientNativePixmapFactory> Create();

  virtual ~ClientNativePixmapFactory();

  struct Configuration {
    gfx::BufferFormat format;
    gfx::BufferUsage usage;
  };

  // Gets supported format/usage configurations.
  virtual std::vector<Configuration> GetSupportedConfigurations() const = 0;

  // TODO(dshwang): implement it. crbug.com/475633
  // Import the native pixmap from |handle| to be used in non-GPU processes.
  virtual scoped_ptr<ClientNativePixmap> ImportNativePixmap(
      const base::FileDescriptor& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) = 0;

 protected:
  ClientNativePixmapFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_
