// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_MENU_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_MENU_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_flash_menu_api.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/webkit_plugins_export.h"

struct WebMenuItem;

namespace webkit {
namespace ppapi {

class PPB_Flash_Menu_Impl : public ::ppapi::Resource,
                            public ::ppapi::thunk::PPB_Flash_Menu_API {
 public:
  virtual ~PPB_Flash_Menu_Impl();

  static PP_Resource Create(PP_Instance instance,
                            const PP_Flash_Menu* menu_data);

  // Resource.
  virtual ::ppapi::thunk::PPB_Flash_Menu_API* AsPPB_Flash_Menu_API() OVERRIDE;

  // PPB_Flash_Menu implementation.
  virtual int32_t Show(const PP_Point* location,
                       int32_t* selected_id_out,
                       PP_CompletionCallback callback) OVERRIDE;

  // Called to complete |Show()|.
  WEBKIT_PLUGINS_EXPORT void CompleteShow(int32_t result, unsigned action);

  typedef std::vector<WebMenuItem> MenuData;
  const MenuData& menu_data() const { return menu_data_; }

 private:
  explicit PPB_Flash_Menu_Impl(PP_Instance instance);

  bool Init(const PP_Flash_Menu* menu_data);

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
