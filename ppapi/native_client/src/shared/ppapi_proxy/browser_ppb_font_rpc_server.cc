// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Font_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::SerializeTo;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::PPBFontInterface;

void PpbFontRpcServer::PPB_Font_GetFontFamilies(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Instance instance,
      // outputs
      nacl_abi_size_t* font_families_size, char* font_families_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var font_families = PPBFontInterface()->GetFontFamilies(instance);
  DebugPrintf("PPB_Font::GetFontFamilies: type=%d\n",
              font_families.type);

  if (!SerializeTo(
      &font_families, font_families_bytes, font_families_size))
    return;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    nacl_abi_size_t description_size, char* description,
    nacl_abi_size_t face_size, char* face,
    PP_Resource* font) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (description_size != sizeof(struct PP_FontDescription_Dev))
    return;
  struct PP_FontDescription_Dev* pp_description =
      reinterpret_cast<struct PP_FontDescription_Dev*>(description);
  if (!DeserializeTo(
      rpc->channel, face, face_size, 1, &pp_description->face)) {
    return;
  }
  *font = PPBFontInterface()->Create(instance, pp_description);

  DebugPrintf("PPB_Font_Dev::Create: font=%"NACL_PRIu32"\n", *font);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_IsFont(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* is_font) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_font = PPBFontInterface()->IsFont(resource);
  *is_font = (pp_is_font == PP_TRUE);

  DebugPrintf("PPB_Font_Dev::IsFont: is_font=%"NACL_PRId32"\n", *is_font);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font,
    nacl_abi_size_t* description_size, char* description,
    nacl_abi_size_t* face_size, char* face,
    nacl_abi_size_t* metrics_size, char* metrics,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*description_size != sizeof(struct PP_FontDescription_Dev))
    return;
  if (*metrics_size != sizeof(struct PP_FontMetrics_Dev))
    return;
  struct PP_FontDescription_Dev* pp_description =
      reinterpret_cast<struct PP_FontDescription_Dev*>(description);
  pp_description->face= PP_MakeUndefined();
  struct PP_FontMetrics_Dev* pp_metrics =
      reinterpret_cast<struct PP_FontMetrics_Dev*>(metrics);
  PP_Bool pp_success = PPBFontInterface()->Describe(font,
                                                    pp_description,
                                                    pp_metrics);
  if (!SerializeTo(&pp_description->face, face, face_size))
    return;
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_Font_Dev::Describe: success=%"NACL_PRIu32"\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_DrawTextAt(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font,
    PP_Resource image_data,
    nacl_abi_size_t text_run_size, char* text_run,
    nacl_abi_size_t text_size, char* text,
    nacl_abi_size_t position_size, char* position,
    int32_t color,
    nacl_abi_size_t clip_size, char* clip,
    int32_t image_data_is_opaque,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (text_run_size != sizeof(struct PP_TextRun_Dev))
    return;
  if (position_size != sizeof(struct PP_Point))
    return;
  if (clip_size != sizeof(struct PP_Rect))
    return;
  struct PP_TextRun_Dev* pp_text_run =
      reinterpret_cast<struct PP_TextRun_Dev*>(text_run);
  if (!DeserializeTo(rpc->channel, text, text_size, 1, &pp_text_run->text))
    return;
  struct PP_Point* pp_position =
      reinterpret_cast<struct PP_Point*>(position);
  struct PP_Rect* pp_clip =
      reinterpret_cast<struct PP_Rect*>(clip);
  PP_Bool pp_image_data_is_opaque = image_data_is_opaque ? PP_TRUE : PP_FALSE;
  PP_Bool pp_success = PPBFontInterface()->DrawTextAt(font,
                                                      image_data,
                                                      pp_text_run,
                                                      pp_position,
                                                      color,
                                                      pp_clip,
                                                      pp_image_data_is_opaque);
  *success = (pp_success == PP_TRUE);
  DebugPrintf("PPB_Font_Dev::DrawTextAt: success=%"NACL_PRIu32"\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_MeasureText(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font,
    nacl_abi_size_t text_run_size, char* text_run,
    nacl_abi_size_t text_size, char* text,
    int32_t* width) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (text_run_size != sizeof(struct PP_TextRun_Dev))
    return;
  struct PP_TextRun_Dev* pp_text_run =
      reinterpret_cast<struct PP_TextRun_Dev*>(text_run);
  if (!DeserializeTo(rpc->channel, text, text_size, 1, &pp_text_run->text))
    return;
  *width = PPBFontInterface()->MeasureText(font, pp_text_run);

  DebugPrintf("PPB_Font_Dev::MeasureText: width=%"NACL_PRIu32"\n", *width);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_CharacterOffsetForPixel(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font,
    nacl_abi_size_t text_run_size, char* text_run,
    nacl_abi_size_t text_size, char* text,
    int32_t pixel_position,
    int32_t* offset) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (text_run_size != sizeof(struct PP_TextRun_Dev))
    return;
  struct PP_TextRun_Dev* pp_text_run =
      reinterpret_cast<struct PP_TextRun_Dev*>(text_run);
  if (!DeserializeTo(rpc->channel, text, text_size, 1, &pp_text_run->text))
    return;
  *offset = PPBFontInterface()->CharacterOffsetForPixel(font,
                                                        pp_text_run,
                                                        pixel_position);

  DebugPrintf("PPB_Font_Dev::CharacterOffsetForPixel: "
              "offset=%"NACL_PRId32"\n", *offset);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFontRpcServer::PPB_Font_PixelOffsetForCharacter(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource font,
    nacl_abi_size_t text_run_size, char* text_run,
    nacl_abi_size_t text_size, char* text,
    int32_t char_offset,
    int32_t* offset) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (text_run_size != sizeof(struct PP_TextRun_Dev))
    return;
  struct PP_TextRun_Dev* pp_text_run =
      reinterpret_cast<struct PP_TextRun_Dev*>(text_run);
  if (!DeserializeTo(rpc->channel, text, text_size, 1, &pp_text_run->text))
    return;
  *offset = PPBFontInterface()->PixelOffsetForCharacter(font,
                                                        pp_text_run,
                                                        char_offset);
  DebugPrintf("PPB_Font_Dev::PixelOffsetForCharacter: "
              "offset=%"NACL_PRId32"\n", *offset);
  rpc->result = NACL_SRPC_RESULT_OK;
}

