/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_

#include <string>

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
  Closure(Plugin* plugin, std::string url) :
    plugin_(plugin), url_(url), buffer_(NULL) {}
  virtual ~Closure() {}
  virtual void Run(NPStream *stream, const char *fname) = 0;
  virtual void Run(const char *url, const void* buffer, int32_t size) = 0;
  bool StartDownload();
  void set_buffer(nacl::StreamBuffer* buffer) { buffer_ = buffer; }
  nacl::StreamBuffer* buffer() { return buffer_; }
 protected:
  Plugin* plugin() { return plugin_; }
 private:
  Plugin* plugin_;
  std::string url_;
  nacl::StreamBuffer *buffer_;
};

class LoadNaClAppNotify : public Closure {
 public:
  LoadNaClAppNotify(Plugin *plugin, std::string url);
  virtual ~LoadNaClAppNotify();
  virtual void Run(NPStream* stream, const char* fname);
  virtual void Run(const char *url, const void* buffer, int32_t size);
};

class UrlAsNaClDescNotify : public Closure {
 public:
  UrlAsNaClDescNotify(Plugin *plugin, std::string url, void *callback_obj);
  virtual ~UrlAsNaClDescNotify();
  virtual void Run(NPStream *stream, const char *fname);
  virtual void Run(const char *url, const void* buffer, int32_t size);
 private:
  NPObject* np_callback_;
};
}

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLOSURE_H_
