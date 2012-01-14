// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_clipboard.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_Clipboard>() {
  return PPB_FLASH_CLIPBOARD_INTERFACE;
}

}  // namespace

namespace flash {

// static
bool Clipboard::IsAvailable() {
  return has_interface<PPB_Flash_Clipboard>();
}

// static
bool Clipboard::IsFormatAvailable(Instance* instance,
                                  PP_Flash_Clipboard_Type clipboard_type,
                                  PP_Flash_Clipboard_Format format) {
  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    rv = PP_ToBool(get_interface<PPB_Flash_Clipboard>()->IsFormatAvailable(
        instance->pp_instance(), clipboard_type, format));
  }
  return rv;
}

// static
bool Clipboard::ReadPlainText(Instance* instance,
                              PP_Flash_Clipboard_Type clipboard_type,
                              std::string* text_out) {
  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    Var v(Var::PassRef(),
          get_interface<PPB_Flash_Clipboard>()->ReadPlainText(
              instance->pp_instance(),
              clipboard_type));
    if (v.is_string()) {
      rv = true;
      *text_out = v.AsString();
    }
  }
  return rv;
}

// static
bool Clipboard::WritePlainText(Instance* instance,
                               PP_Flash_Clipboard_Type clipboard_type,
                               const std::string& text) {
  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    rv = (get_interface<PPB_Flash_Clipboard>()->WritePlainText(
              instance->pp_instance(),
              clipboard_type,
              Var(text).pp_var()) == PP_OK);
  }
  return rv;
}

}  // namespace flash
}  // namespace pp
