// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_clipboard_impl.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebClipboard.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::StringVar;

namespace webkit {
namespace ppapi {

namespace {

const size_t kMaxClipboardWriteSize = 1000000;

WebKit::WebClipboard::Buffer ConvertClipboardType(
    PP_Flash_Clipboard_Type type) {
  switch (type) {
    case PP_FLASH_CLIPBOARD_TYPE_STANDARD:
      return WebKit::WebClipboard::BufferStandard;
    case PP_FLASH_CLIPBOARD_TYPE_SELECTION:
      return WebKit::WebClipboard::BufferSelection;
    default:
      NOTREACHED();
      return WebKit::WebClipboard::BufferStandard;
  }
}

WebKit::WebClipboard::Format ConvertClipboardFormat(
    PP_Flash_Clipboard_Format format) {
  switch (format) {
    case PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT:
      return WebKit::WebClipboard::FormatPlainText;
    case PP_FLASH_CLIPBOARD_FORMAT_HTML:
      return WebKit::WebClipboard::FormatHTML;
    case PP_FLASH_CLIPBOARD_FORMAT_INVALID:
    default:
      NOTREACHED();
      return WebKit::WebClipboard::FormatPlainText;  // Gotta return something.
  }
}

}  // namespace

PPB_Flash_Clipboard_Impl::PPB_Flash_Clipboard_Impl(PluginInstance* instance)
    : instance_(instance) {
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
  WebKit::WebClipboard* web_clipboard =
      WebKit::webKitPlatformSupport()->clipboard();
  if (!web_clipboard) {
    NOTREACHED();
    return PP_FALSE;
  }
  return BoolToPPBool(
      web_clipboard->isFormatAvailable(ConvertClipboardFormat(format),
                                       ConvertClipboardType(clipboard_type)));
}

PP_Var PPB_Flash_Clipboard_Impl::ReadPlainText(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type) {
  WebKit::WebClipboard* web_clipboard =
      WebKit::webKitPlatformSupport()->clipboard();
  if (!web_clipboard) {
    NOTREACHED();
    return PP_MakeNull();
  }
  WebKit::WebCString s =
      web_clipboard->readPlainText(ConvertClipboardType(clipboard_type)).utf8();
  return StringVar::StringToPPVar(instance_->module()->pp_module(), s);
}

int32_t PPB_Flash_Clipboard_Impl::WritePlainText(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    const PP_Var& text) {
  StringVar* text_string = StringVar::FromPPVar(text);
  if (!text_string)
    return PP_ERROR_BADARGUMENT;

  if (text_string->value().length() > kMaxClipboardWriteSize)
    return PP_ERROR_NOSPACE;

  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  WebKit::WebClipboard* web_clipboard =
      WebKit::webKitPlatformSupport()->clipboard();
  if (!web_clipboard) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  web_clipboard->writePlainText(
      WebKit::WebCString(text_string->value()).utf16());
  return PP_OK;
}

}  // namespace ppapi
}  // namespace webkit
