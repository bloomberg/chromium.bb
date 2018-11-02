// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/renderer/on_load_script_injector.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace webrunner {

OnLoadScriptInjector::OnLoadScriptInjector(content::RenderFrame* frame)
    : RenderFrameObserver(frame), weak_ptr_factory_(this) {
  render_frame()->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&OnLoadScriptInjector::BindToRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

OnLoadScriptInjector::~OnLoadScriptInjector() {}

void OnLoadScriptInjector::BindToRequest(
    mojom::OnLoadScriptInjectorAssociatedRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OnLoadScriptInjector::DidClearWindowObject() {
  // The script may cause the window to be cleared (e.g. by producing a page
  // load event), so the guard is used to prevent reentrancy and potential
  // infinite loops.
  if (is_handling_clear_window_object_)
    return;

  base::AutoReset<bool> clear_window_reset(&is_handling_clear_window_object_,
                                           true);
  for (const base::string16& script : on_load_scripts_)
    render_frame()->ExecuteJavaScript(script);
}

void OnLoadScriptInjector::AddOnLoadScript(const base::string16& script) {
  on_load_scripts_.push_back(script);
}

void OnLoadScriptInjector::ClearOnLoadScripts() {
  on_load_scripts_.clear();
}

void OnLoadScriptInjector::OnDestruct() {
  delete this;
}

}  // namespace webrunner
