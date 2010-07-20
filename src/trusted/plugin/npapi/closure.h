/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Generic and specialized closure classes.
// TODO(sehr): Closure support needs to be factored to be made portable.


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_CLOSURE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_CLOSURE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"
#include "native_client/src/trusted/plugin/plugin.h"

namespace plugin {

  class StreamShmBuffer;

// Closure base class.  Following our C++ style rules, we have
// an explicit Run method rather than overload the operator().
//
// This is pretty generic, actually, and not restricted to be used for
// notification callbacks.
class Closure {
 public:
  Closure(Plugin* plugin, nacl::string requested_url) :
    plugin_(plugin), requested_url_(requested_url), buffer_(NULL) {
    if (NULL != plugin_) {
      npp_ = InstanceIdentifierToNPP(plugin_->instance_id());
    }
  }
  virtual ~Closure() {}

  virtual void RunFromFile(NPStream* stream, const nacl::string& fname) = 0;
  virtual void RunFromBuffer(const nacl::string& url,
                             StreamShmBuffer* shmbufp) = 0;

  bool StartDownload();

  void set_plugin(Plugin* plugin) {
    plugin_ = plugin;
    npp_ = InstanceIdentifierToNPP(plugin_->instance_id());
  }
  void set_buffer(StreamShmBuffer* buffer) { buffer_ = buffer; }
  StreamShmBuffer* buffer() const { return buffer_; }

 protected:
  Plugin* plugin() const { return plugin_; }
  nacl::string requested_url() const { return requested_url_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Closure);
  Plugin* plugin_;
  nacl::string requested_url_;
  StreamShmBuffer* buffer_;
  NPP npp_;
};

class LoadNaClAppNotify : public Closure {
 public:
  LoadNaClAppNotify(Plugin* plugin, nacl::string url);
  virtual ~LoadNaClAppNotify();
  virtual void RunFromFile(NPStream* stream, const nacl::string& fname);
  virtual void RunFromBuffer(const nacl::string& url, StreamShmBuffer* shmbufp);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadNaClAppNotify);
};

class UrlAsNaClDescNotify : public Closure {
 public:
  UrlAsNaClDescNotify(Plugin* plugin, nacl::string url, void* callback_obj);
  virtual ~UrlAsNaClDescNotify();
  virtual void RunFromFile(NPStream* stream, const nacl::string& fname);
  virtual void RunFromBuffer(const nacl::string& url, StreamShmBuffer* shmbufp);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UrlAsNaClDescNotify);
  NPObject* np_callback_;
};

class NpGetUrlClosure : public Closure {
 public:
  NpGetUrlClosure(NPP npp,
                  nacl::NPModule* module,
                  nacl::string url,
                  int32_t notify_data,
                  bool call_url_notify);
  virtual ~NpGetUrlClosure();
  virtual void RunFromFile(NPStream* stream, const nacl::string& fname);
  virtual void RunFromBuffer(const nacl::string& url, StreamShmBuffer* shmbufp);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NpGetUrlClosure);
  nacl::NPModule* module_;
  NPP npp_;
  int32_t notify_data_;
  bool call_url_notify_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_CLOSURE_H_
