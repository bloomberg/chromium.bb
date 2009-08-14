/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
