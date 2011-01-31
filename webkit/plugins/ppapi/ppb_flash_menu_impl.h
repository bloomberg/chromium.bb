// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_MENU_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_MENU_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/resource.h"

struct WebMenuItem;

namespace webkit {
namespace ppapi {

class PPB_Flash_Menu_Impl : public Resource {
 public:
  explicit PPB_Flash_Menu_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_Menu_Impl();

  static const PPB_Flash_Menu* GetInterface();

  bool Init(const PP_Flash_Menu* menu_data);

  // Resource override.
  virtual PPB_Flash_Menu_Impl* AsPPB_Flash_Menu_Impl();

  // PPB_Flash_Menu implementation.
  int32_t Show(const PP_Point* location,
               int32_t* selected_id_out,
               PP_CompletionCallback callback);

  // Called to complete |Show()|.
  void CompleteShow(int32_t result, unsigned action);

  typedef std::vector<WebMenuItem> MenuData;
  const MenuData& menu_data() const { return menu_data_; }

 private:
  MenuData menu_data_;

  // We send |WebMenuItem|s, which have an |unsigned| "action" field instead of
  // an |int32_t| ID. (Chrome also limits the range of valid values for
  // actions.) This maps actions to IDs.
  std::vector<int32_t> menu_id_map_;

  // Any pending callback (for |Show()|).
  scoped_refptr<TrackedCompletionCallback> callback_;

  // Output buffers to be filled in when the callback is completed successfully.
  int32_t* selected_id_out_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Menu_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_MENU_IMPL_H_
