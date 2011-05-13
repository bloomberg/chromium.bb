// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_URL_UTIL_IMPL_H_
#define PPAPI_SHARED_IMPL_URL_UTIL_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/url_parse.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

class GURL;

namespace ppapi {

// Contains the implementation of PPB_URLUtil that is shared between the proxy
// and the renderer.
class URLUtilImpl {
 public:
  // The functions here would normally take the var interface for constructing
  // return strings. However, at the current time there's some mixup between
  // using Var and VarDeprecated. To resolve this, we instead pass the pointer
  // to the string creation function so can be used independently of this.
  typedef PP_Var (*VarFromUtf8)(PP_Module, const char*, uint32_t);

  // Function that converts the given var to a std::string or NULL if the
  // var is not a string or is invalid.
  //
  // We could use PPB_Var for this, but that interface requires an additional
  // string conversion. Both the proxy and the host side maintain the strings
  // in a std::string, and the form we want for passing to GURL is also a
  // std::string. Parameterizing this separately saves this, and also solves
  // the same problem that VarFromUtf8 does.
  typedef const std::string* (*StringFromVar)(PP_Var var);

  // PPB_URLUtil shared functions.
  static PP_Var Canonicalize(StringFromVar string_from_var,
                             VarFromUtf8 var_from_utf8,
                             PP_Module pp_module,
                             PP_Var url,
                             PP_URLComponents_Dev* components);
  static PP_Var ResolveRelativeToURL(StringFromVar string_from_var,
                                     VarFromUtf8 var_from_utf8,
                                     PP_Module pp_module,
                                     PP_Var base_url,
                                     PP_Var relative,
                                     PP_URLComponents_Dev* components);
  static PP_Bool IsSameSecurityOrigin(StringFromVar string_from_var,
                                      PP_Var url_a, PP_Var url_b);

  // Used for returning the given GURL from a PPAPI function, with an optional
  // out param indicating the components.
  static PP_Var GenerateURLReturn(VarFromUtf8 var_from_utf8,
                                  PP_Module pp_module,
                                  const GURL& url,
                                  PP_URLComponents_Dev* components);
};

}  // namespace ppapi

#endif
