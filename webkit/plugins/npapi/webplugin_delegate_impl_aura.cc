// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/metrics/stats_counters.h"
#include "base/string_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/blit.h"
#include "webkit/plugins/npapi/gtk_plugin_container.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/plugin_stream_url.h"
#include "webkit/plugins/npapi/webplugin.h"

#include "third_party/npapi/bindings/npapi_x11.h"

using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

namespace webkit {
namespace npapi {

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    PluginInstance *instance) {
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
