/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"

namespace nacl {
  class StreamBuffer;
}

namespace nacl_srpc {

/*
 * Closure base class.  Following our C++ style rules, we have
 * an explicit Run method rather than overload the operator().
 *
 * This is pretty generic, actually, and not restricted to be used for
 * notification callbacks.
 */
class Closure {
 public:
  Closure(Plugin* plugin, nacl::string url) :
    plugin_(plugin), url_(url), buffer_(NULL) {
    if (NULL != plugin_) {
      plugin_identifier_ =
          plugin_->GetPortablePluginInterface()->GetPluginIdentifier();
    }
  }
  virtual ~Closure() {}
  virtual void Run(NPStream *stream, const char *fname) = 0;
  virtual void Run(const char *url, const void* buffer, int32_t size) = 0;
  bool StartDownload();
  void set_plugin(nacl_srpc::Plugin* plugin) {
    plugin_ = plugin;
    plugin_identifier_ =
        plugin_->GetPortablePluginInterface()->GetPluginIdentifier();
  }
  void set_buffer(nacl::StreamBuffer* buffer) { buffer_ = buffer; }
  nacl::StreamBuffer* buffer() { return buffer_; }
 protected:
  Plugin* plugin() { return plugin_; }
 private:
  Plugin* plugin_;
  nacl::string url_;
  nacl::StreamBuffer *buffer_;
  nacl_srpc::PluginIdentifier plugin_identifier_;
};

class LoadNaClAppNotify : public Closure {
 public:
  LoadNaClAppNotify(Plugin *plugin, nacl::string url);
  virtual ~LoadNaClAppNotify();
  virtual void Run(NPStream* stream, const char* fname);
  virtual void Run(const char *url, const void* buffer, int32_t size);
};

class UrlAsNaClDescNotify : public Closure {
 public:
  UrlAsNaClDescNotify(Plugin *plugin, nacl::string url, void *callback_obj);
  virtual ~UrlAsNaClDescNotify();
  virtual void Run(NPStream *stream, const char *fname);
  virtual void Run(const char *url, const void* buffer, int32_t size);
 private:
  NPObject* np_callback_;
};

class NpGetUrlClosure : public Closure {
 public:
  NpGetUrlClosure(NPP npp, nacl::NPModule* module, nacl::string url);
  virtual ~NpGetUrlClosure();
  virtual void Run(NPStream* stream, const char* fname);
  virtual void Run(const char* url, const void* buffer, int32_t size);

 private:
  nacl::NPModule* module_;
  NPP npp_;
};
}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_
