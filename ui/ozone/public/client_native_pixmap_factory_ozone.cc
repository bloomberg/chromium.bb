// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace ui {
namespace {

// Thread-safe owner of the gfx::ClientNativePixmapFactory. Not a LazyInstance
// because it uses PlatformObject<>::Create() for factory construction.
// TODO(jamescook|spang): This exists to solve a startup race for chrome --mash
// http://crbug.com/807781. Removing the factory entirely would be better,
// with something like http://crrev.com/c/899949.
class PixmapFactorySingleton {
 public:
  static PixmapFactorySingleton* GetInstance() {
    return base::Singleton<PixmapFactorySingleton>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<PixmapFactorySingleton>;

  PixmapFactorySingleton() {
    TRACE_EVENT1("ozone", "CreateClientNativePixmapFactoryOzone", "platform",
                 GetOzonePlatformName());
    pixmap_factory_ = PlatformObject<gfx::ClientNativePixmapFactory>::Create();
    gfx::ClientNativePixmapFactory::SetInstance(pixmap_factory_.get());
  }

  std::unique_ptr<gfx::ClientNativePixmapFactory> pixmap_factory_;

  DISALLOW_COPY_AND_ASSIGN(PixmapFactorySingleton);
};

}  // namespace

void CreateClientNativePixmapFactoryOzone() {
  // Multiple threads may race to create the factory (e.g. when the UI service
  // and ash service are running in the same process). Create the object as a
  // side effect of a thread-safe singleton.
  PixmapFactorySingleton::GetInstance();
}

}  // namespace ui
