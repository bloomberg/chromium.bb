// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_URL_UTIL_DEV_H_
#define PPAPI_CPP_DEV_URL_UTIL_DEV_H_

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/cpp/var.h"

namespace pp {

class Instance;
class Module;

// Simple wrapper around the PPB_UrlUtil interface.
class UrlUtil_Dev {
 public:
  // This class is just a collection of random functions that aren't
  // particularly attached to anything. So this getter returns a cached
  // instance of this interface. This may return NULL if the browser doesn't
  // support the UrlUtil inteface. Since this is a singleton, don't delete the
  // pointer.
  static const UrlUtil_Dev* Get();

  Var Canonicalize(const Var& url,
                   PP_UrlComponents_Dev* components = NULL) const;

  Var ResolveRelativeToUrl(const Var& base_url,
                           const Var& relative_string,
                           PP_UrlComponents_Dev* components = NULL) const;
  Var ResoveRelativeToDocument(const Instance& instance,
                               const Var& relative_string,
                               PP_UrlComponents_Dev* components = NULL) const;

  bool IsSameSecurityOrigin(const Var& url_a, const Var& url_b) const;
  bool DocumentCanRequest(const Instance& instance, const Var& url) const;
  bool DocumentCanAccessDocument(const Instance& active,
                                 const Instance& target) const;

 private:
  UrlUtil_Dev() : interface_(NULL) {}

  // Copy and assignment are disallowed.
  UrlUtil_Dev(const UrlUtil_Dev& other);
  UrlUtil_Dev& operator=(const UrlUtil_Dev& other);

  const PPB_UrlUtil_Dev* interface_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_URL_UTIL_DEV_H_

