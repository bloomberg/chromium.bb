// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_WEBKIT_FORWARDING_H_
#define PPAPI_SHARED_IMPL_WEBKIT_FORWARDING_H_

#include <string>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_FontDescription_Dev;
struct PP_FontMetrics_Dev;
struct PP_Point;
struct PP_Rect;

namespace base {
class WaitableEvent;
}

namespace skia {
class PlatformCanvas;
}

namespace ppapi {

struct Preferences;

class PPAPI_SHARED_EXPORT WebKitForwarding {
 public:
  class PPAPI_SHARED_EXPORT Font {
   public:
    // C++ version of PP_TextRun_Dev. Since the functions below will be called
    // on an alternate thread in the proxy, and since there are different
    // methods of converting PP_Var -> strings in the plugin and the proxy, we
    // can't use PP_Vars in the Do* functions below.
    struct TextRun {
      std::string text;
      bool rtl;
      bool override_direction;
    };

    // DoDrawText takes too many arguments to be used with base::Bind, so we
    // use this struct to hold them.
    struct PPAPI_SHARED_EXPORT DrawTextParams {
      DrawTextParams(skia::PlatformCanvas* destination_arg,
                     const TextRun& text_arg,
                     const PP_Point* position_arg,
                     uint32_t color_arg,
                     const PP_Rect* clip_arg,
                     PP_Bool image_data_is_opaque_arg);
      ~DrawTextParams();

      skia::PlatformCanvas* destination;
      const TextRun& text;
      const PP_Point* position;
      uint32_t color;
      const PP_Rect* clip;
      PP_Bool image_data_is_opaque;
    };

    virtual ~Font();

    // The face name in the description is not filled in to avoid a dependency
    // on creating vars. Instead, the face name is placed into the given
    // string. See class description for waitable_event documentation. If
    // non-null, the given event will be set on completion.
    virtual void Describe(base::WaitableEvent* event,
                          PP_FontDescription_Dev* description,
                          std::string* face,
                          PP_FontMetrics_Dev* metrics,
                          PP_Bool* result) = 0;
    virtual void DrawTextAt(base::WaitableEvent* event,
                            const DrawTextParams& params) = 0;
    virtual void MeasureText(base::WaitableEvent* event,
                             const TextRun& text,
                             int32_t* result) = 0;
    virtual void CharacterOffsetForPixel(base::WaitableEvent* event,
                                         const TextRun& text,
                                         int32_t pixel_position,
                                         uint32_t* result) = 0;
    virtual void PixelOffsetForCharacter(base::WaitableEvent* event,
                                         const TextRun& text,
                                         uint32_t char_offset,
                                         int32_t* result) = 0;
  };

  virtual ~WebKitForwarding();

  // Creates a new font with the given description. The desc_face is the face
  // name already extracted from the description. The caller owns the result
  // pointer, which will never be NULL. If non-null, the given event will be
  // set on completion.
  virtual void CreateFontForwarding(base::WaitableEvent* event,
                                    const PP_FontDescription_Dev& desc,
                                    const std::string& desc_face,
                                    const Preferences& prefs,
                                    Font** result) = 0;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_WEBKIT_FORWARDING_H_
