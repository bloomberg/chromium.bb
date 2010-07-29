// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FONT_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FONT_H_

#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _ppb_Font PPB_Font;

namespace pepper {

class PluginInstance;

class Font : public Resource {
 public:
  Font(PluginModule* module, int fd);
  virtual ~Font();

  // Returns a pointer to the interface implementing PPB_Font that is exposed to
  // the plugin.
  static const PPB_Font* GetInterface();

  // Resource overrides.
  Font* AsFont() { return this; }

  // PPB_Font implementation.
  bool GetFontTable(uint32_t table,
                    void* output,
                    uint32_t* output_length);

 private:
  int fd_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FONT_H_
