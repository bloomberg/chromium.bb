// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
#define PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_CharSet_Dev;
struct PPB_Core;

namespace pp {
namespace proxy {

class SerializedVarReturnValue;

class PPB_CharSet_Proxy : public InterfaceProxy {
 public:
  PPB_CharSet_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_CharSet_Proxy();

  static const Info* GetInfo();

  const PPB_CharSet_Dev* ppb_char_set_target() const {
    return static_cast<const PPB_CharSet_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgUTF16ToCharSet(PP_Instance instance,
                           const string16& utf16,
                           const std::string& char_set,
                           int32_t on_error,
                           std::string* output,
                           bool* output_is_success);
  void OnMsgCharSetToUTF16(PP_Instance instance,
                           const std::string& input,
                           const std::string& char_set,
                           int32_t on_error,
                           string16* output,
                           bool* output_is_success);
  void OnMsgGetDefaultCharSet(PP_Instance instance,
                              SerializedVarReturnValue result);

  // Returns the local PPB_Core interface for freeing strings.
  const PPB_Core* GetCoreInterface();

  // Possibly NULL, lazily initialized by GetCoreInterface.
  const PPB_Core* core_interface_;

  DISALLOW_COPY_AND_ASSIGN(PPB_CharSet_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_CHAR_SET_PROXY_H_
