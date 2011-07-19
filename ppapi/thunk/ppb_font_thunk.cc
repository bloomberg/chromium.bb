// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_font_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var GetFontFamilies(PP_Instance instance) {
  EnterFunction<PPB_Font_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetFontFamilies(instance);
}

PP_Resource Create(PP_Instance instance,
                   const PP_FontDescription_Dev* description) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFontObject(instance, description);
}

PP_Bool IsFont(PP_Resource resource) {
  EnterResource<PPB_Font_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource font_id,
                 PP_FontDescription_Dev* description,
                 PP_FontMetrics_Dev* metrics) {
  EnterResource<PPB_Font_API> enter(font_id, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Describe(description, metrics);
}

PP_Bool DrawTextAt(PP_Resource font_id,
                   PP_Resource image_data,
                   const PP_TextRun_Dev* text,
                   const PP_Point* position,
                   uint32_t color,
                   const PP_Rect* clip,
                   PP_Bool image_data_is_opaque) {
  EnterResource<PPB_Font_API> enter(font_id, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->DrawTextAt(image_data, text, position, color, clip,
                                    image_data_is_opaque);
}

int32_t MeasureText(PP_Resource font_id, const PP_TextRun_Dev* text) {
  EnterResource<PPB_Font_API> enter(font_id, true);
  if (enter.failed())
    return -1;
  return enter.object()->MeasureText(text);
}

uint32_t CharacterOffsetForPixel(PP_Resource font_id,
                                 const PP_TextRun_Dev* text,
                                 int32_t pixel_position) {
  EnterResource<PPB_Font_API> enter(font_id, true);
  if (enter.failed())
    return -1;
  return enter.object()->CharacterOffsetForPixel(text, pixel_position);
}

int32_t PixelOffsetForCharacter(PP_Resource font_id,
                                const PP_TextRun_Dev* text,
                                uint32_t char_offset) {
  EnterResource<PPB_Font_API> enter(font_id, true);
  if (enter.failed())
    return -1;
  return enter.object()->PixelOffsetForCharacter(text, char_offset);
}

const PPB_Font_Dev g_ppb_font_thunk = {
  &GetFontFamilies,
  &Create,
  &IsFont,
  &Describe,
  &DrawTextAt,
  &MeasureText,
  &CharacterOffsetForPixel,
  &PixelOffsetForCharacter
};

}  // namespace

const PPB_Font_Dev* GetPPB_Font_Thunk() {
  return &g_ppb_font_thunk;
}

}  // namespace thunk
}  // namespace ppapi
