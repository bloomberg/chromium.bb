// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_font.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));
const nacl_abi_size_t kPPRectBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Rect));
const nacl_abi_size_t kPPTextRunBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_TextRun_Dev));
const nacl_abi_size_t kPPFontMetricsBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_FontMetrics_Dev));
const nacl_abi_size_t kPPFontDescriptionBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_FontDescription_Dev));
const int32_t kInvalidOffset = -1;

PP_Var GetFontFamilies(PP_Instance instance) {
  DebugPrintf("PPB_Font::GetFontFamilies: instance=%"NACL_PRIu32"\n",
              instance);
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  PP_Var font_families = PP_MakeUndefined();
  nacl_abi_size_t var_size = kMaxVarSize;
  nacl::scoped_array<char> var_bytes(new char[kMaxVarSize]);

  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_GetFontFamilies(
          channel,
          instance,
          &var_size,
          var_bytes.get());

  DebugPrintf("PPB_Font::GetFontFamilies: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    (void) DeserializeTo(
        channel, var_bytes.get(), var_size, 1, &font_families);
  return font_families;
}

PP_Resource Create(PP_Instance instance,
                   const struct PP_FontDescription_Dev* description) {
  DebugPrintf("PPB_Font::Create: instance=%"NACL_PRIu32"\n", instance);

  nacl_abi_size_t face_size = kMaxVarSize;
  nacl::scoped_array<char> face_bytes(
      Serialize(&description->face, 1, &face_size));

  PP_Resource resource = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_Create(
          GetMainSrpcChannel(),
          instance,
          kPPFontDescriptionBytes,
          reinterpret_cast<char*>(
              const_cast<struct PP_FontDescription_Dev*>(description)),
          face_size,
          face_bytes.get(),
          &resource);

  DebugPrintf("PPB_Font::Create: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginFont> font =
        PluginResource::AdoptAs<PluginFont>(resource);
    if (font.get())
      return resource;
  }
  return kInvalidResourceId;
}

PP_Bool IsFont(PP_Resource resource) {
  DebugPrintf("PPB_Font::IsFont: resource=%"NACL_PRIu32"\n", resource);

  int32_t is_font = 0;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_IsFont(GetMainSrpcChannel(),
                                        resource,
                                        &is_font);

  DebugPrintf("PPB_Font::IsFont: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && is_font)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool Describe(PP_Resource font,
                 struct PP_FontDescription_Dev* description,
                 struct PP_FontMetrics_Dev* metrics) {
  DebugPrintf("PPB_Font::Describe: font=%"NACL_PRIu32"\n", font);
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  int32_t success = 0;
  nacl_abi_size_t description_size = kPPFontDescriptionBytes;
  nacl_abi_size_t face_size = kMaxVarSize;
  nacl::scoped_array<char> face_bytes(new char[kMaxVarSize]);
  nacl_abi_size_t metrics_size = kPPFontMetricsBytes;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_Describe(
          channel,
          font,
          &description_size,
          reinterpret_cast<char*>(description),
          &face_size,
          face_bytes.get(),
          &metrics_size,
          reinterpret_cast<char*>(metrics),
          &success);

  DebugPrintf("PPB_Font::Describe: %s\n", NaClSrpcErrorString(srpc_result));

  description->face = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK && success) {
    (void) DeserializeTo(
        channel, face_bytes.get(), face_size, 1, &description->face);
    return PP_TRUE;
  }
  return PP_FALSE;
}

PP_Bool DrawTextAt(PP_Resource font,
                   PP_Resource image_data,
                   const struct PP_TextRun_Dev* text_run,
                   const struct PP_Point* position,
                   uint32_t color,
                   const struct PP_Rect* clip,
                   PP_Bool image_data_is_opaque) {
  DebugPrintf("PPB_Font::DrawTextAt: font=%"NACL_PRIu32"\n", font);

  nacl_abi_size_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(
      Serialize(&text_run->text, 1, &text_size));

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_DrawTextAt(
          GetMainSrpcChannel(),
          font,
          image_data,
          kPPTextRunBytes,
          reinterpret_cast<char*>(
              const_cast<struct PP_TextRun_Dev*>(text_run)),
          text_size,
          text_bytes.get(),
          kPPPointBytes,
          reinterpret_cast<char*>(const_cast<struct PP_Point*>(position)),
          color,
          kPPRectBytes,
          reinterpret_cast<char*>(const_cast<struct PP_Rect*>(clip)),
          image_data_is_opaque,
          &success);

  DebugPrintf("PPB_Font::DrawTextAt: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t MeasureText(PP_Resource font,
                    const struct PP_TextRun_Dev* text_run) {
  DebugPrintf("PPB_Font::MeasureText: font=%"NACL_PRIu32"\n", font);

  nacl_abi_size_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(
      Serialize(&text_run->text, 1, &text_size));
  int32_t width = 0;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_MeasureText(
          GetMainSrpcChannel(),
          font,
          kPPTextRunBytes,
          reinterpret_cast<char*>(
              const_cast<struct PP_TextRun_Dev*>(text_run)),
          text_size,
          text_bytes.get(),
          &width);

  DebugPrintf("PPB_Font::MeasureText: %s\n", NaClSrpcErrorString(srpc_result));
  return width;
}

uint32_t CharacterOffsetForPixel(PP_Resource font,
                                 const struct PP_TextRun_Dev* text_run,
                                 int32_t pixel_position) {
  DebugPrintf("PPB_Font::CharacterOffsetForPixel: font=%"NACL_PRIu32"\n", font);

  nacl_abi_size_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(
      Serialize(&text_run->text, 1, &text_size));
  int32_t offset = kInvalidOffset;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_CharacterOffsetForPixel(
          GetMainSrpcChannel(),
          font,
          kPPTextRunBytes,
          reinterpret_cast<char*>(
              const_cast<struct PP_TextRun_Dev*>(text_run)),
          text_size,
          text_bytes.get(),
          pixel_position,
          &offset);

  DebugPrintf("PPB_Font::CharacterOffsetForPixel: %s\n",
              NaClSrpcErrorString(srpc_result));
  return offset;
}

int32_t PixelOffsetForCharacter(PP_Resource font,
                                const struct PP_TextRun_Dev* text_run,
                                uint32_t char_offset) {
  DebugPrintf("PPB_Font::PixelOffsetForCharacter: font=%"NACL_PRIu32"\n", font);

  nacl_abi_size_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(
      Serialize(&text_run->text, 1, &text_size));
  int32_t offset = kInvalidOffset;
  NaClSrpcError srpc_result =
      PpbFontRpcClient::PPB_Font_PixelOffsetForCharacter(
          GetMainSrpcChannel(),
          font,
          kPPTextRunBytes,
          reinterpret_cast<char*>(
              const_cast<struct PP_TextRun_Dev*>(text_run)),
          text_size,
          text_bytes.get(),
          char_offset,
          &offset);

  DebugPrintf("PPB_Font::PixelOffsetForCharacter: %s\n",
              NaClSrpcErrorString(srpc_result));
  return offset;
}

}  // namespace

const PPB_Font_Dev* PluginFont::GetInterface() {
  static const PPB_Font_Dev font_interface = {
    GetFontFamilies,
    Create,
    IsFont,
    Describe,
    DrawTextAt,
    MeasureText,
    CharacterOffsetForPixel,
    PixelOffsetForCharacter
  };
  return &font_interface;
}

}  // namespace ppapi_proxy

