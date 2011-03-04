// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_clipboard_impl.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebClipboard.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

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
    case PP_FLASH_CLIPBOARD_TYPE_DRAG:
      return WebKit::WebClipboard::BufferDrag;
    default:
      NOTREACHED();
      return WebKit::WebClipboard::BufferStandard;
  }
}

PP_Var ReadPlainText(PP_Instance instance_id,
                     PP_Flash_Clipboard_Type clipboard_type) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeNull();

  WebKit::WebClipboard* web_clipboard = WebKit::webKitClient()->clipboard();
  if (!web_clipboard) {
    NOTREACHED();
    return PP_MakeNull();
  }

  WebKit::WebCString s =
      web_clipboard->readPlainText(ConvertClipboardType(clipboard_type)).utf8();
  return StringVar::StringToPPVar(instance->module(), s);
}

int32_t WritePlainText(PP_Instance instance_id,
                       PP_Flash_Clipboard_Type clipboard_type,
                       PP_Var text) {
  scoped_refptr<StringVar> text_string(StringVar::FromPPVar(text));
  if (!text_string)
    return PP_ERROR_BADARGUMENT;

  if (text_string->value().length() > kMaxClipboardWriteSize)
    return PP_ERROR_NOSPACE;

  if (clipboard_type != PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
    NOTIMPLEMENTED();
    return PP_ERROR_FAILED;
  }

  WebKit::WebClipboard* web_clipboard = WebKit::webKitClient()->clipboard();
  if (!web_clipboard) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  web_clipboard->writePlainText(
      WebKit::WebCString(text_string->value()).utf16());
  return PP_OK;
}

const PPB_Flash_Clipboard ppb_flash_clipboard = {
  &ReadPlainText,
  &WritePlainText,
};

}  // namespace

// static
const PPB_Flash_Clipboard*
    PPB_Flash_Clipboard_Impl::GetInterface() {
  return &ppb_flash_clipboard;
}

}  // namespace ppapi
}  // namespace webkit
