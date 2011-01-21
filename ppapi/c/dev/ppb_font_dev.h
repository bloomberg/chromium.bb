/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_FONT_DEV_H_
#define PPAPI_C_DEV_PPB_FONT_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FONT_DEV_INTERFACE "PPB_Font(Dev);0.5"

struct PP_Point;
struct PP_Rect;

typedef enum {
  // Uses the user's default web page font (normally either the default serif
  // or sans serif font).
  PP_FONTFAMILY_DEFAULT = 0,

  // These families will use the default web page font corresponding to the
  // given family.
  PP_FONTFAMILY_SERIF = 1,
  PP_FONTFAMILY_SANSSERIF = 2,
  PP_FONTFAMILY_MONOSPACE = 3
} PP_FontFamily_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FontFamily_Dev, 4);

typedef enum {
  PP_FONTWEIGHT_100 = 0,
  PP_FONTWEIGHT_200,
  PP_FONTWEIGHT_300,
  PP_FONTWEIGHT_400,
  PP_FONTWEIGHT_500,
  PP_FONTWEIGHT_600,
  PP_FONTWEIGHT_700,
  PP_FONTWEIGHT_800,
  PP_FONTWEIGHT_900,
  PP_FONTWEIGHT_NORMAL = PP_FONTWEIGHT_400,
  PP_FONTWEIGHT_BOLD = PP_FONTWEIGHT_700
} PP_FontWeight_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FontWeight_Dev, 4);

struct PP_FontDescription_Dev {
  // Font face name as a string. This can also be a Null var, in which case the
  // generic family will be obeyed.
  struct PP_Var face;

  // When face is a Null string, this specifies the generic font family type
  // to use. If the face is specified, this will be ignored.
  PP_FontFamily_Dev family;

  uint32_t size;

  // Normally you will use either PP_FONTWEIGHT_NORMAL or PP_FONTWEIGHT_BOLD.
  PP_FontWeight_Dev weight;

  PP_Bool italic;
  PP_Bool small_caps;

  // Adjustment to apply to letter and word spacing, respectively. Initialize
  // to 0 to get normal spacing. Negative values bring letters/words closer
  // together, positive values separate them.
  int32_t letter_spacing;
  int32_t word_spacing;

  // Ensure that this struct is 48-bytes wide by padding the end.  In some
  // compilers, PP_Var is 8-byte aligned, so those compilers align this struct
  // on 8-byte boundaries as well and pad it to 16 bytes even without this
  // padding attribute.  This padding makes its size consistent across
  // compilers.
  int32_t padding;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_FontDescription_Dev, 48);

struct PP_FontMetrics_Dev {
  int32_t height;
  int32_t ascent;
  int32_t descent;
  int32_t line_spacing;
  int32_t x_height;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_FontMetrics_Dev, 20);

struct PP_TextRun_Dev {
  // This var must either be a string or a null var (which will be treated as
  // a 0-length string).
  struct PP_Var text;

  // Set to PP_TRUE if the text is right-to-left.
  PP_Bool rtl;

  // Set to PP_TRUE to force the directionality of the text regardless of
  // content
  PP_Bool override_direction;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_TextRun_Dev, 24);

struct PPB_Font_Dev {
  // Returns a font which best matches the given description. The return value
  // will have a non-zero ID on success, or zero on failure.
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_FontDescription_Dev* description);

  // Returns PP_TRUE if the given resource is a Font. Returns PP_FALSE if the
  // resource is invalid or some type other than a Font.
  PP_Bool (*IsFont)(PP_Resource resource);

  // Loads the description and metrics of the font into the given structures.
  // The description will be different than the description the font was
  // created with since it will be filled with the real values from the font
  // that was actually selected.
  //
  // The PP_Var in the description should be of type Void on input. On output,
  // this will contain the string and will have a reference count of 1. The
  // plugin is responsible for calling Release on this var.
  //
  // Returns PP_TRUE on success, PP_FALSE if the font is invalid or if the Var
  // in the description isn't Null (to prevent leaks).
  PP_Bool (*Describe)(PP_Resource font,
                      struct PP_FontDescription_Dev* description,
                      struct PP_FontMetrics_Dev* metrics);

  // Draws the text to the image buffer.
  //
  // The given point represents the baseline of the left edge of the font,
  // regardless of whether it is left-to-right or right-to-left (in the case of
  // RTL text, this will actually represent the logical end of the text).
  //
  // The clip is optional and may be NULL. In this case, the text will be
  // clipped to the image.
  //
  // The image_data_is_opaque flag indicates whether subpixel antialiasing can
  // be performend, if it is supported. When the image below the text is
  // opaque, subpixel antialiasing is supported and you should set this to
  // PP_TRUE to pick up the user's default preferences. If your plugin is
  // partially transparent, then subpixel antialiasing is not possible and
  // grayscale antialiasing will be used instead (assuming the user has
  // antialiasing enabled at all).
  PP_Bool (*DrawTextAt)(PP_Resource font,
                        PP_Resource image_data,
                        const struct PP_TextRun_Dev* text,
                        const struct PP_Point* position,
                        uint32_t color,
                        const struct PP_Rect* clip,
                        PP_Bool image_data_is_opaque);

  // Returns the width of the given string. If the font is invalid or the var
  // isn't a valid string, this will return -1.
  //
  // Note that this function handles complex scripts such as Arabic, combining
  // accents, etc. so that adding the width of substrings won't necessarily
  // produce the correct width of the entire string.
  //
  // Returns -1 on failure.
  int32_t (*MeasureText)(PP_Resource font,
                         const struct PP_TextRun_Dev* text);

  // Returns the character at the given pixel X position from the beginning of
  // the string. This handles complex scripts such as Arabic, where characters
  // may be combined or replaced depending on the context. Returns (uint32)-1
  // on failure.
  uint32_t (*CharacterOffsetForPixel)(PP_Resource font,
                                      const struct PP_TextRun_Dev* text,
                                      int32_t pixel_position);

  // Returns the horizontal advance to the given character if the string was
  // placed at the given position. This handles complex scripts such as Arabic,
  // where characters may be combined or replaced depending on context. Returns
  // -1 on error.
  int32_t (*PixelOffsetForCharacter)(PP_Resource font,
                                     const struct PP_TextRun_Dev* text,
                                     uint32_t char_offset);
};

#endif  /* PPAPI_C_DEV_PPB_FONT_DEV_H_ */

