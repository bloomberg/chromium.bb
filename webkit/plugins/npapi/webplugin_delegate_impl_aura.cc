// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#include "webkit/plugins/npapi/plugin_instance.h"

using WebKit::WebCursorInfo;
using WebKit::WebInputEvent;

namespace webkit {
namespace npapi {

WebPluginDelegateImpl::WebPluginDelegateImpl(PluginInstance* instance) {
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  // Nothing to do here.
}

void WebPluginDelegateImpl::Paint(WebKit::WebCanvas* canvas,
                                  const gfx::Rect& rect) {
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
}

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  return true;
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursorInfo* cursor_info) {
  return false;
}

}  // namespace npapi
}  // namespace webkit
