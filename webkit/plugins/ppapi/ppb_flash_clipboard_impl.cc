// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_clipboard_impl.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "webkit/glue/clipboard_client.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::StringVar;

namespace webkit {
namespace ppapi {

namespace {

const size_t kMaxClipboardWriteSize = 1000000;

ui::Clipboard::Buffer ConvertClipboardType(
    PP_Flash_Clipboard_Type type) {
  switch (type) {
    case PP_FLASH_CLIPBOARD_TYPE_STANDARD:
      return ui::Clipboard::BUFFER_STANDARD;
    case PP_FLASH_CLIPBOARD_TYPE_SELECTION:
      return ui::Clipboard::BUFFER_SELECTION;
  }
  NOTREACHED();
  return ui::Clipboard::BUFFER_STANDARD;
}

}  // namespace

PPB_Flash_Clipboard_Impl::PPB_Flash_Clipboard_Impl(PluginInstance* instance)
    : instance_(instance),
      client_() {
}

bool PPB_Flash_Clipboard_Impl::Init() {
  // Initialize the ClipboardClient for writing to the clipboard.
  if (!client_.get()) {
    if (!instance_)
      return false;
    PluginDelegate* plugin_delegate = instance_->delegate();
    if (!plugin_delegate)
      return false;
    client_.reset(plugin_delegate->CreateClipboardClient());
  }
  return true;
}

PPB_Flash_Clipboard_Impl::~PPB_Flash_Clipboard_Impl() {
}

::ppapi::thunk::PPB_Flash_Clipboard_FunctionAPI*
PPB_Flash_Clipboard_Impl::AsPPB_Flash_Clipboard_FunctionAPI() {
  return this;
}

PP_Bool PPB_Flash_Clipboard_Impl::IsFormatAvailable(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!Init())
    return PP_FALSE;

  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_FALSE;
  }

  ui::Clipboard::Buffer buffer_type = ConvertClipboardType(clipboard_type);
  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: {
      bool plain = client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextFormatType(), buffer_type);
      bool plainw = client_->IsFormatAvailable(
          ui::Clipboard::GetPlainTextWFormatType(), buffer_type);
      return BoolToPPBool(plain || plainw);
    }
    case PP_FLASH_CLIPBOARD_FORMAT_HTML:
      return BoolToPPBool(client_->IsFormatAvailable(
          ui::Clipboard::GetHtmlFormatType(), buffer_type));
    case PP_FLASH_CLIPBOARD_FORMAT_RTF:
      return BoolToPPBool(client_->IsFormatAvailable(
          ui::Clipboard::GetRtfFormatType(), buffer_type));
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
      break;
  }

  return PP_FALSE;
}

PP_Var PPB_Flash_Clipboard_Impl::ReadData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!Init())
    return PP_MakeUndefined();

  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_MakeUndefined();
  }

  if (!IsFormatAvailable(instance, clipboard_type, format)) {
    return PP_MakeNull();
  }

  ui::Clipboard::Buffer buffer_type = ConvertClipboardType(clipboard_type);

  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: {
      if (client_->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                     buffer_type)) {
        string16 text;
        client_->ReadText(buffer_type, &text);
        if (!text.empty())
          return StringVar::StringToPPVar(UTF16ToUTF8(text));
      }

      if (client_->IsFormatAvailable(ui::Clipboard::GetPlainTextFormatType(),
                                     buffer_type)) {
        std::string text;
        client_->ReadAsciiText(buffer_type, &text);
        if (!text.empty())
          return StringVar::StringToPPVar(text);
      }

      return PP_MakeNull();
    }
    case PP_FLASH_CLIPBOARD_FORMAT_HTML: {
      string16 html_stdstr;
      GURL gurl;
      uint32 fragment_start;
      uint32 fragment_end;
      client_->ReadHTML(buffer_type,
                        &html_stdstr,
                        &gurl,
                        &fragment_start,
                        &fragment_end);
      return StringVar::StringToPPVar(UTF16ToUTF8(html_stdstr));
    }
    case PP_FLASH_CLIPBOARD_FORMAT_RTF: {
      std::string result;
      client_->ReadRTF(buffer_type, &result);
      return ::ppapi::PpapiGlobals::Get()->GetVarTracker()->
          MakeArrayBufferPPVar(result.size(), result.data());
    }
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
      break;
  }

  return PP_MakeUndefined();
}

int32_t PPB_Flash_Clipboard_Impl::WriteDataItem(
    const PP_Flash_Clipboard_Format format,
    const PP_Var& data,
    ScopedClipboardWriterGlue* scw) {
  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: {
      StringVar* text_string = StringVar::FromPPVar(data);
      if (!text_string)
        return PP_ERROR_BADARGUMENT;

      if (text_string->value().length() > kMaxClipboardWriteSize)
        return PP_ERROR_NOSPACE;

      scw->WriteText(UTF8ToUTF16(text_string->value()));
      return PP_OK;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_HTML: {
      StringVar* text_string = StringVar::FromPPVar(data);
      if (!text_string)
        return PP_ERROR_BADARGUMENT;

      if (text_string->value().length() > kMaxClipboardWriteSize)
        return PP_ERROR_NOSPACE;

      scw->WriteHTML(UTF8ToUTF16(text_string->value()), "");
      return PP_OK;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_RTF: {
      ::ppapi::ArrayBufferVar* rtf_data =
          ::ppapi::ArrayBufferVar::FromPPVar(data);
      if (!rtf_data)
        return PP_ERROR_BADARGUMENT;

      if (rtf_data->ByteLength() > kMaxClipboardWriteSize)
        return PP_ERROR_NOSPACE;

      scw->WriteRTF(std::string(static_cast<char*>(rtf_data->Map()),
                                rtf_data->ByteLength()));
      return PP_OK;
    }
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
      break;
  }

  return PP_ERROR_BADARGUMENT;
}

int32_t PPB_Flash_Clipboard_Impl::WriteData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    uint32_t data_item_count,
    const PP_Flash_Clipboard_Format formats[],
    const PP_Var data_items[]) {
  if (!Init())
    return PP_ERROR_FAILED;

  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  if (data_item_count == 0) {
    client_->Clear(ConvertClipboardType(clipboard_type));
    return PP_OK;
  }
  ScopedClipboardWriterGlue scw(client_.get());
  for (uint32_t i = 0; i < data_item_count; ++i) {
    int32_t res = WriteDataItem(formats[i], data_items[i], &scw);
    if (res != PP_OK) {
      // Need to clear the objects so nothing is written.
      scw.Reset();
      return res;
    }
  }

  return PP_OK;
}

}  // namespace ppapi
}  // namespace webkit
