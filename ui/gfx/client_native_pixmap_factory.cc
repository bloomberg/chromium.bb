// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/client_native_pixmap_factory.h"

namespace gfx {

namespace {

ClientNativePixmapFactory* g_instance = nullptr;

}  // namespace

// static
ClientNativePixmapFactory* ClientNativePixmapFactory::GetInstance() {
  return g_instance;
}

// static
void ClientNativePixmapFactory::ResetInstance() {
  g_instance = nullptr;
}

// static
void ClientNativePixmapFactory::SetInstance(
    ClientNativePixmapFactory* instance) {
  DCHECK(!g_instance);
  DCHECK(instance);
  g_instance = instance;
}

ClientNativePixmapFactory::ClientNativePixmapFactory() {}

ClientNativePixmapFactory::~ClientNativePixmapFactory() {}

}  // namespace gfx
