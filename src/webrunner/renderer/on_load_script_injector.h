// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
#define WEBRUNNER_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "webrunner/common/on_load_script_injector.mojom.h"

namespace webrunner {

// Injects one or more scripts into a RenderFrame at the earliest possible time
// during the page load process.
class OnLoadScriptInjector : public content::RenderFrameObserver,
                             public mojom::OnLoadScriptInjector {
 public:
  explicit OnLoadScriptInjector(content::RenderFrame* frame);

  void BindToRequest(mojom::OnLoadScriptInjectorAssociatedRequest request);

  void AddOnLoadScript(const base::string16& script) override;
  void ClearOnLoadScripts() override;

  // RenderFrameObserver override:
  void OnDestruct() override;
  void DidClearWindowObject() override;

 private:
  // Called by OnDestruct(), when the RenderFrame is destroyed.
  ~OnLoadScriptInjector() override;

  std::vector<base::string16> on_load_scripts_;
  bool is_handling_clear_window_object_ = false;
  mojo::AssociatedBindingSet<mojom::OnLoadScriptInjector> bindings_;
  base::WeakPtrFactory<OnLoadScriptInjector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OnLoadScriptInjector);
};

}  // namespace webrunner

#endif  // WEBRUNNER_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
