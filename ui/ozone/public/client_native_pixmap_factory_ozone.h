// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_OZONE_H_

#include "ui/ozone/ozone_export.h"

namespace ui {

// Creates a factory for pixmaps that can use be transported from the client to
// the GPU process using a low-level ozone-provided platform specific mechanism.
// The factory is installed as the gfx::ClientNativePixmapFactory instance.
OZONE_EXPORT void CreateClientNativePixmapFactoryOzone();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_OZONE_H_
