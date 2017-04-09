/*
 * Copyright (C) 2003, 2007, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WindowFeatures_h
#define WindowFeatures_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

namespace blink {

class IntRect;

struct CORE_EXPORT WindowFeatures {
  DISALLOW_NEW();
  WindowFeatures()
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
        dialog(false),
        noopener(false) {}
  explicit WindowFeatures(const String& window_features_string);
  WindowFeatures(const String& dialog_features_string,
                 const IntRect& screen_available_rect);

  int x;
  bool x_set;
  int y;
  bool y_set;
  int width;
  bool width_set;
  int height;
  bool height_set;

  bool menu_bar_visible;
  bool status_bar_visible;
  bool tool_bar_visible;
  bool location_bar_visible;
  bool scrollbars_visible;
  bool resizable;

  bool fullscreen;
  bool dialog;

  bool noopener;

  Vector<String> additional_features;

 private:
  using DialogFeaturesMap = HashMap<String, String>;
  static void ParseDialogFeatures(const String&, HashMap<String, String>&);
  static bool BoolFeature(const DialogFeaturesMap&,
                          const char* key,
                          bool default_value = false);
  static int IntFeature(const DialogFeaturesMap&,
                        const char* key,
                        int min,
                        int max,
                        int default_value);
  void SetWindowFeature(const String& key_string, const String& value_string);
};

}  // namespace blink

#endif  // WindowFeatures_h
