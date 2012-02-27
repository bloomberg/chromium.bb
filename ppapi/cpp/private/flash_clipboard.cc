// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_clipboard.h"

#include <vector>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_Clipboard>() {
  return PPB_FLASH_CLIPBOARD_INTERFACE;
}

template <> const char* interface_name<PPB_Flash_Clipboard_3_0>() {
  return PPB_FLASH_CLIPBOARD_INTERFACE_3_0;
}

}  // namespace

namespace flash {

// static
bool Clipboard::IsAvailable() {
  return has_interface<PPB_Flash_Clipboard>() ||
      has_interface<PPB_Flash_Clipboard_3_0>();
}

// static
bool Clipboard::IsFormatAvailable(const InstanceHandle& instance,
                                  PP_Flash_Clipboard_Type clipboard_type,
                                  PP_Flash_Clipboard_Format format) {
  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    rv = PP_ToBool(get_interface<PPB_Flash_Clipboard>()->IsFormatAvailable(
        instance.pp_instance(), clipboard_type, format));
  }
  return rv;
}

// static
bool Clipboard::ReadData(
    const InstanceHandle& instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format clipboard_format,
    Var* out) {
  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    PP_Var result = get_interface<PPB_Flash_Clipboard>()->ReadData(
        instance.pp_instance(),
        clipboard_type,
        clipboard_format);
    *out = Var(PASS_REF, result);
    rv = true;
  } else if (has_interface<PPB_Flash_Clipboard_3_0>() &&
             clipboard_format == PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) {
    PP_Var result = get_interface<PPB_Flash_Clipboard_3_0>()->ReadPlainText(
        instance.pp_instance(),
        clipboard_type);
    *out = Var(PASS_REF, result);
    rv = true;
  }
  return rv;
}

// static
bool Clipboard::WriteData(
    const InstanceHandle& instance,
    PP_Flash_Clipboard_Type clipboard_type,
    const std::vector<PP_Flash_Clipboard_Format>& formats,
    const std::vector<Var>& data_items) {
  if (formats.size() != data_items.size())
    return false;

  bool rv = false;
  if (has_interface<PPB_Flash_Clipboard>()) {
    // Convert vector of pp::Var into a vector of PP_Var.
    std::vector<PP_Var> data_items_vector;
    for (uint32_t i = 0; i < data_items.size(); ++i)
      data_items_vector.push_back(data_items[i].pp_var());

    // Ensure that we don't dereference the memory in empty vectors. We still
    // want to call WriteData because it has the effect of clearing the
    // clipboard.
    const PP_Flash_Clipboard_Format* formats_ptr(NULL);
    const PP_Var* data_items_ptr(NULL);
    if (data_items.size() > 0) {
      formats_ptr = &formats[0];
      data_items_ptr = &data_items_vector[0];
    }

    rv = (get_interface<PPB_Flash_Clipboard>()->WriteData(
        instance.pp_instance(),
        clipboard_type,
        data_items.size(),
        formats_ptr,
        data_items_ptr) == PP_OK);
  } else if (has_interface<PPB_Flash_Clipboard_3_0>()) {
    // The API specifies that only the last item of each format needs to be
    // written. Since we are only writing plain text items for the 3_0
    // interface, we just need to write the last one in the array.
    for (int32_t i = formats.size() - 1; i >= 0; --i) {
      if (formats[i] == PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) {
        rv = (get_interface<PPB_Flash_Clipboard_3_0>()->WritePlainText(
            instance.pp_instance(),
            clipboard_type,
            data_items[i].pp_var()) == PP_OK);
        break;
      }
    }
  }

  return rv;
}

}  // namespace flash
}  // namespace pp
