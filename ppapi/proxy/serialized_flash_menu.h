// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_FLASH_MENU_H_
#define PPAPI_PROXY_SERIALIZED_FLASH_MENU_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

struct PP_Flash_Menu;

namespace IPC {
class Message;
}

namespace pp {
namespace proxy {

class SerializedFlashMenu {
 public:
  SerializedFlashMenu();
  ~SerializedFlashMenu();

  bool SetPPMenu(const PP_Flash_Menu* menu);

  const PP_Flash_Menu* pp_menu() const { return pp_menu_; }

  void WriteToMessage(IPC::Message* m) const;
  bool ReadFromMessage(const IPC::Message* m, void** iter);

 private:
  const PP_Flash_Menu* pp_menu_;
  bool own_menu_;
  DISALLOW_COPY_AND_ASSIGN(SerializedFlashMenu);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_SERIALIZED_FLASH_MENU_H_
