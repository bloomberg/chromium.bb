/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebWindowFeatures_h
#define WebWindowFeatures_h

#include "../platform/WebCommon.h"
#include "../platform/WebString.h"
#include "../platform/WebVector.h"

#if BLINK_IMPLEMENTATION
#include "core/page/WindowFeatures.h"
#endif

namespace blink {

struct WebWindowFeatures {
  float x;
  bool x_set;
  float y;
  bool y_set;
  float width;
  bool width_set;
  float height;
  bool height_set;

  bool menu_bar_visible;
  bool status_bar_visible;
  bool tool_bar_visible;
  bool location_bar_visible;
  bool scrollbars_visible;
  bool resizable;

  bool fullscreen;
  bool dialog;
  WebVector<WebString> additional_features;

  WebWindowFeatures()
      : x(0),
        x_set(false),
        y(0),
        y_set(false),
        width(0),
        width_set(false),
        height(0),
        height_set(false),
        menu_bar_visible(true),
        status_bar_visible(true),
        tool_bar_visible(true),
        location_bar_visible(true),
        scrollbars_visible(true),
        resizable(true),
        fullscreen(false),
        dialog(false) {}

#if BLINK_IMPLEMENTATION
  WebWindowFeatures(const WindowFeatures& f)
      : x(f.x),
        x_set(f.x_set),
        y(f.y),
        y_set(f.y_set),
        width(f.width),
        width_set(f.width_set),
        height(f.height),
        height_set(f.height_set),
        menu_bar_visible(f.menu_bar_visible),
        status_bar_visible(f.status_bar_visible),
        tool_bar_visible(f.tool_bar_visible),
        location_bar_visible(f.location_bar_visible),
        scrollbars_visible(f.scrollbars_visible),
        resizable(f.resizable),
        fullscreen(f.fullscreen),
        dialog(f.dialog),
        additional_features(f.additional_features) {}

  operator WindowFeatures() const {
    WindowFeatures result;
    result.x = x;
    result.x_set = x_set;
    result.y = y;
    result.y_set = y_set;
    result.width = width;
    result.width_set = width_set;
    result.height = height;
    result.height_set = height_set;
    result.menu_bar_visible = menu_bar_visible;
    result.status_bar_visible = status_bar_visible;
    result.tool_bar_visible = tool_bar_visible;
    result.location_bar_visible = location_bar_visible;
    result.scrollbars_visible = scrollbars_visible;
    result.resizable = resizable;
    result.fullscreen = fullscreen;
    result.dialog = dialog;
    for (size_t i = 0; i < additional_features.size(); ++i)
      result.additional_features.push_back(additional_features[i]);
    return result;
  }
#endif
};

}  // namespace blink

#endif
