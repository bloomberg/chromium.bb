// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_SERVICE_CONTEXT_IMPL_H_
#define WEBRUNNER_SERVICE_CONTEXT_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>
#include <memory>

#include "base/macros.h"
#include "chromium/web/cpp/fidl.h"
#include "webrunner/common/webrunner_export.h"

namespace webrunner {

// Implementation of Context from //webrunner/fidl/context.fidl.
// Owns a BrowserContext instance and uses it to create new WebContents/Frames.
// All created Frames are owned by this object.
class WEBRUNNER_EXPORT ContextImpl : public chromium::web::Context {
 public:
  ContextImpl();

  // Tears down the Context, destroying any active Frames in the process.
  ~ContextImpl() override;

  // chromium::web::Context implementation.
  void CreateFrame(
      ::fidl::InterfaceHandle<chromium::web::FrameObserver> observer,
      ::fidl::InterfaceRequest<chromium::web::Frame> frame) override;

 private:
  fidl::BindingSet<chromium::web::Frame, std::unique_ptr<chromium::web::Frame>>
      frame_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ContextImpl);
};

}  // namespace webrunner

#endif  // WEBRUNNER_SERVICE_CONTEXT_IMPL_H_
