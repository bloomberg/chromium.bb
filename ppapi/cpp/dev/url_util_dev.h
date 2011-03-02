// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_URL_UTIL_DEV_H_
#define PPAPI_CPP_DEV_URL_UTIL_DEV_H_

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/cpp/var.h"

namespace pp {

class Instance;
class Module;

// Simple wrapper around the PPB_URLUtil interface.
class URLUtil_Dev {
 public:
  // This class is just a collection of random functions that aren't
  // particularly attached to anything. So this getter returns a cached
  // instance of this interface. This may return NULL if the browser doesn't
  // support the URLUtil inteface. Since this is a singleton, don't delete the
  // pointer.
  static const URLUtil_Dev* Get();

  Var Canonicalize(const Var& url,
                   PP_URLComponents_Dev* components = NULL) const;

  Var ResolveRelativeToURL(const Var& base_url,
                           const Var& relative_string,
                           PP_URLComponents_Dev* components = NULL) const;
  Var ResoveRelativeToDocument(const Instance& instance,
                               const Var& relative_string,
                               PP_URLComponents_Dev* components = NULL) const;

  bool IsSameSecurityOrigin(const Var& url_a, const Var& url_b) const;
  bool DocumentCanRequest(const Instance& instance, const Var& url) const;
  bool DocumentCanAccessDocument(const Instance& active,
                                 const Instance& target) const;
  Var GetDocumentURL(const Instance& instance,
                     PP_URLComponents_Dev* components = NULL) const;

 private:
  URLUtil_Dev() : interface_(NULL) {}

  // Copy and assignment are disallowed.
  URLUtil_Dev(const URLUtil_Dev& other);
  URLUtil_Dev& operator=(const URLUtil_Dev& other);

  const PPB_URLUtil_Dev* interface_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_URL_UTIL_DEV_H_

