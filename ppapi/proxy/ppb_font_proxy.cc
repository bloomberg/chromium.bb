// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_font_proxy.h"

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

class Font : public PluginResource {
 public:
  Font(const HostResource& resource);
  virtual ~Font();

  // PluginResource overrides.
  virtual Font* AsFont() { return this; }

  PP_FontDescription_Dev& desc() { return desc_; }
  PP_FontDescription_Dev* desc_ptr() { return &desc_; }
  PP_FontMetrics_Dev& metrics() { return metrics_; }

 private:
  PP_FontDescription_Dev desc_;
  PP_FontMetrics_Dev metrics_;

  DISALLOW_COPY_AND_ASSIGN(Font);
};

Font::Font(const HostResource& resource) : PluginResource(resource) {
  memset(&desc_, 0, sizeof(PP_FontDescription_Dev));
  desc_.face.type = PP_VARTYPE_UNDEFINED;
  memset(&metrics_, 0, sizeof(PP_FontMetrics_Dev));
}

Font::~Font() {
  PluginVarTracker::GetInstance()->Release(desc_.face);
}

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_FontDescription_Dev* description) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  SerializedFontDescription in_description;
  in_description.SetFromPPFontDescription(dispatcher, *description, true);

  HostResource result;
  SerializedFontDescription out_description;
  std::string out_metrics;
  dispatcher->Send(new PpapiHostMsg_PPBFont_Create(
      INTERFACE_ID_PPB_FONT,
      instance, in_description, &result, &out_description, &out_metrics));

  if (result.is_null())
    return 0;  // Failure creating font.

  linked_ptr<Font> object(new Font(result));
  out_description.SetToPPFontDescription(dispatcher, object->desc_ptr(), true);

  // Convert the metrics, this is just serialized as a string of bytes.
  if (out_metrics.size() != sizeof(PP_FontMetrics_Dev))
    return 0;
  memcpy(&object->metrics(), out_metrics.data(), sizeof(PP_FontMetrics_Dev));

  return PluginResourceTracker::GetInstance()->AddResource(object);
}

PP_Bool IsFont(PP_Resource resource) {
  Font* object = PluginResource::GetAs<Font>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool Describe(PP_Resource font_id,
                 PP_FontDescription_Dev* description,
                 PP_FontMetrics_Dev* metrics) {
  Font* object = PluginResource::GetAs<Font>(font_id);
  if (!object)
    return PP_FALSE;

  // Copy the description, the caller expects its face PP_Var to have a ref
  // added to it on its behalf.
  memcpy(description, &object->desc(), sizeof(PP_FontDescription_Dev));
  PluginVarTracker::GetInstance()->AddRef(description->face);

  memcpy(metrics, &object->metrics(), sizeof(PP_FontMetrics_Dev));
  return PP_TRUE;
}

PP_Bool DrawTextAt(PP_Resource font_id,
                   PP_Resource image_data,
                   const PP_TextRun_Dev* text,
                   const PP_Point* position,
                   uint32_t color,
                   const PP_Rect* clip,
                   PP_Bool image_data_is_opaque) {
  Font* font_object = PluginResource::GetAs<Font>(font_id);
  if (!font_object)
    return PP_FALSE;
  PluginResource* image_object = PluginResourceTracker::GetInstance()->
      GetResourceObject(image_data);
  if (!image_object)
    return PP_FALSE;
  if (font_object->instance() != image_object->instance())
    return PP_FALSE;

  PPBFont_DrawTextAt_Params params;
  params.font = font_object->host_resource();
  params.image_data = image_object->host_resource();
  params.text_is_rtl = text->rtl;
  params.override_direction = text->override_direction;
  params.position = *position;
  params.color = color;
  if (clip) {
    params.clip = *clip;
    params.clip_is_null = false;
  } else {
    params.clip = PP_MakeRectFromXYWH(0, 0, 0, 0);
    params.clip_is_null = true;
  }
  params.image_data_is_opaque = image_data_is_opaque;

  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(
      image_object->instance());
  PP_Bool result = PP_FALSE;
  if (dispatcher) {
    dispatcher->Send(new PpapiHostMsg_PPBFont_DrawTextAt(
        INTERFACE_ID_PPB_FONT,
        SerializedVarSendInput(dispatcher, text->text),
        params, &result));
  }
  return result;
}

int32_t MeasureText(PP_Resource font_id, const PP_TextRun_Dev* text) {
  Font* object = PluginResource::GetAs<Font>(font_id);
  if (!object)
    return -1;

  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(object->instance());
  int32_t result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBFont_MeasureText(
      INTERFACE_ID_PPB_FONT, object->host_resource(),
      SerializedVarSendInput(dispatcher, text->text),
      text->rtl, text->override_direction, &result));
  return result;
}

uint32_t CharacterOffsetForPixel(PP_Resource font_id,
                                 const PP_TextRun_Dev* text,
                                 int32_t pixel_position) {
  Font* object = PluginResource::GetAs<Font>(font_id);
  if (!object)
    return -1;

  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(object->instance());
  uint32_t result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBFont_CharacterOffsetForPixel(
      INTERFACE_ID_PPB_FONT, object->host_resource(),
      SerializedVarSendInput(dispatcher, text->text),
      text->rtl, text->override_direction, pixel_position, &result));
  return result;
}

int32_t PixelOffsetForCharacter(PP_Resource font_id,
                                const PP_TextRun_Dev* text,
                                uint32_t char_offset) {
  Font* object = PluginResource::GetAs<Font>(font_id);
  if (!object)
    return -1;

  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(object->instance());
  int32_t result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBFont_PixelOffsetForCharacter(
      INTERFACE_ID_PPB_FONT, object->host_resource(),
      SerializedVarSendInput(dispatcher, text->text),
      text->rtl, text->override_direction, char_offset, &result));
  return result;
}

const PPB_Font_Dev ppb_font_interface = {
  &Create,
  &IsFont,
  &Describe,
  &DrawTextAt,
  &MeasureText,
  &CharacterOffsetForPixel,
  &PixelOffsetForCharacter
};

}  // namespace

PPB_Font_Proxy::PPB_Font_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Font_Proxy::~PPB_Font_Proxy() {
}

const void* PPB_Font_Proxy::GetSourceInterface() const {
  return &ppb_font_interface;
}

InterfaceID PPB_Font_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_FONT;
}

bool PPB_Font_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Font_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFont_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFont_DrawTextAt,
                        OnMsgDrawTextAt)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFont_MeasureText,
                        OnMsgMeasureText)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFont_CharacterOffsetForPixel,
                        OnMsgCharacterOffsetForPixel)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFont_PixelOffsetForCharacter,
                        OnMsgPixelOffsetForCharacter)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Font_Proxy::OnMsgCreate(
    PP_Instance instance,
    const SerializedFontDescription& in_description,
    HostResource* result,
    SerializedFontDescription* out_description,
    std::string* out_metrics) {
  // Convert the face name in the input description.
  PP_FontDescription_Dev in_pp_desc;
  in_description.SetToPPFontDescription(dispatcher(), &in_pp_desc, false);

  // Make sure the output is always defined so we can still serialize it back
  // to the plugin below.
  PP_FontDescription_Dev out_pp_desc;
  memset(&out_pp_desc, 0, sizeof(PP_FontDescription_Dev));
  out_pp_desc.face = PP_MakeUndefined();

  result->SetHostResource(instance,
                          ppb_font_target()->Create(instance, &in_pp_desc));
  if (!result->is_null()) {
    // Get the metrics and resulting description to return to the browser.
    PP_FontMetrics_Dev metrics;
    if (ppb_font_target()->Describe(result->host_resource(), &out_pp_desc,
                                    &metrics)) {
        out_metrics->assign(reinterpret_cast<const char*>(&metrics),
                            sizeof(PP_FontMetrics_Dev));
    }
  }

  // This must always get called or it will assert when trying to serialize
  // the un-filled-in SerializedFontDescription as the return value.
  out_description->SetFromPPFontDescription(dispatcher(), out_pp_desc, false);
}

void PPB_Font_Proxy::OnMsgDrawTextAt(SerializedVarReceiveInput text,
                                     const PPBFont_DrawTextAt_Params& params,
                                     PP_Bool* result) {
  PP_TextRun_Dev run;
  run.text = text.Get(dispatcher());
  run.rtl = params.text_is_rtl;
  run.override_direction = params.override_direction;

  *result = ppb_font_target()->DrawTextAt(params.font.host_resource(),
      params.image_data.host_resource(), &run, &params.position, params.color,
      params.clip_is_null ? NULL : &params.clip, params.image_data_is_opaque);
}

void PPB_Font_Proxy::OnMsgMeasureText(HostResource font,
                                      SerializedVarReceiveInput text,
                                      PP_Bool text_is_rtl,
                                      PP_Bool override_direction,
                                      int32_t* result) {
  PP_TextRun_Dev run;
  run.text = text.Get(dispatcher());
  run.rtl = text_is_rtl;
  run.override_direction = override_direction;

  *result = ppb_font_target()->MeasureText(font.host_resource(), &run);
}

void PPB_Font_Proxy::OnMsgCharacterOffsetForPixel(
    HostResource font,
    SerializedVarReceiveInput text,
    PP_Bool text_is_rtl,
    PP_Bool override_direction,
    int32_t pixel_pos,
    uint32_t* result) {
  PP_TextRun_Dev run;
  run.text = text.Get(dispatcher());
  run.rtl = text_is_rtl;
  run.override_direction = override_direction;

  *result = ppb_font_target()->CharacterOffsetForPixel(font.host_resource(),
                                                       &run, pixel_pos);
}

void PPB_Font_Proxy::OnMsgPixelOffsetForCharacter(
    HostResource font,
    SerializedVarReceiveInput text,
    PP_Bool text_is_rtl,
    PP_Bool override_direction,
    uint32_t char_offset,
    int32_t* result) {
  PP_TextRun_Dev run;
  run.text = text.Get(dispatcher());
  run.rtl = text_is_rtl;
  run.override_direction = override_direction;

  *result = ppb_font_target()->PixelOffsetForCharacter(font.host_resource(),
                                                       &run, char_offset);
}

}  // namespace proxy
}  // namespace pp
