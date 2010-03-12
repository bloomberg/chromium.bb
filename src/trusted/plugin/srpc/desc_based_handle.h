/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_

#include <stdio.h>
#include <map>

#include "base/basictypes.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

  struct DescHandleInitializer : PortableHandleInitializer {
    nacl::DescWrapper* wrapper_;
    Plugin* plugin_;
    DescHandleInitializer(PortablePluginInterface* plugin_interface,
                          nacl::DescWrapper* wrapper,
                          Plugin *plugin):
        PortableHandleInitializer(plugin_interface),
        wrapper_(wrapper),
        plugin_(plugin) {}
  };


  // PortableHandle is the struct used to represent handles that are opaque to
  // the javascript bridge.
  class DescBasedHandle : public PortableHandle {
   public:

    // Get the contained descriptor.
    nacl::DescWrapper* wrapper() { return wrapper_; }
    struct NaClDesc* desc() { return wrapper_->desc(); }

    // There are derived classes, so the constructor and destructor must
    // be visible.
    explicit DescBasedHandle();
    virtual ~DescBasedHandle();

    bool Init(struct PortableHandleInitializer* init_info);


    void LoadMethods();
    virtual Plugin* GetPlugin();

    static int number_alive() { return number_alive_counter; }

   private:
    // class-specific methods - to be invoked by the browser
    static bool Map(void *obj, SrpcParams *params);

   public:
    Plugin* plugin_;
   private:
    nacl::DescWrapper* wrapper_;
    static int number_alive_counter;
  };

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_DESC_BASED_HANDLE_H_
