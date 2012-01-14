// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FLASH_CLIPBOARD_H_
#define PPAPI_CPP_PRIVATE_FLASH_CLIPBOARD_H_

#include <string>

#include "ppapi/c/private/ppb_flash_clipboard.h"

namespace pp {

class Instance;

namespace flash {

class Clipboard {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  // Returns true if the given format is available from the given clipboard.
  static bool IsFormatAvailable(Instance* instance,
                                PP_Flash_Clipboard_Type clipboard_type,
                                PP_Flash_Clipboard_Format format);

  // Returns true on success, in which case |text_out| will be filled with plain
  // text read from the given clipboard.
  static bool ReadPlainText(Instance* instance,
                            PP_Flash_Clipboard_Type clipboard_type,
                            std::string* text_out);

  // Returns true on success (it may fail if |text| is too big).
  static bool WritePlainText(Instance* instance,
                             PP_Flash_Clipboard_Type clipboard_type,
                             const std::string& text);
};

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_CLIPBOARD_H_
