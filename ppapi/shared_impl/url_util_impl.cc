// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/url_util_impl.h"

#include "googleurl/src/gurl.h"

namespace ppapi {

namespace {

void ConvertComponent(const url_parse::Component& input,
                      PP_URLComponent_Dev* output) {
  output->begin = input.begin;
  output->len = input.len;
}

// Converts components from a GoogleUrl parsed to a PPAPI parsed structure.
// Output can be NULL to specify "do nothing." This rule is followed by all
// the url util functions, so we implement it once here.
//
// Output can be NULL to specify "do nothing." This rule is followed by all the
// url util functions, so we implement it once here.
void ConvertComponents(const url_parse::Parsed& input,
                       PP_URLComponents_Dev* output) {
  if (!output)
    return;

  ConvertComponent(input.scheme, &output->scheme);
  ConvertComponent(input.username, &output->username);
  ConvertComponent(input.password, &output->password);
  ConvertComponent(input.host, &output->host);
  ConvertComponent(input.port, &output->port);
  ConvertComponent(input.path, &output->path);
  ConvertComponent(input.query, &output->query);
  ConvertComponent(input.ref, &output->ref);
}

}  // namespace

// static
PP_Var URLUtilImpl::Canonicalize(StringFromVar string_from_var,
                                 VarFromUtf8 var_from_utf8,
                                 PP_Module pp_module,
                                 PP_Var url,
                                 PP_URLComponents_Dev* components) {
  const std::string* url_string = string_from_var(url);
  if (!url_string)
    return PP_MakeNull();
  return GenerateURLReturn(var_from_utf8, pp_module,
                           GURL(*url_string), components);
}

// static
PP_Var URLUtilImpl::ResolveRelativeToURL(StringFromVar string_from_var,
                                         VarFromUtf8 var_from_utf8,
                                         PP_Module pp_module,
                                         PP_Var base_url,
                                         PP_Var relative,
                                         PP_URLComponents_Dev* components) {
  const std::string* base_url_string = string_from_var(base_url);
  const std::string* relative_string = string_from_var(relative);
  if (!base_url_string || !relative_string)
    return PP_MakeNull();

  GURL base_gurl(*base_url_string);
  if (!base_gurl.is_valid())
    return PP_MakeNull();
  return GenerateURLReturn(var_from_utf8, pp_module,
                           base_gurl.Resolve(*relative_string),
                           components);
}

// static
PP_Bool URLUtilImpl::IsSameSecurityOrigin(StringFromVar string_from_var,
                                          PP_Var url_a, PP_Var url_b) {
  const std::string* url_a_string = string_from_var(url_a);
  const std::string* url_b_string = string_from_var(url_b);
  if (!url_a_string || !url_b_string)
    return PP_FALSE;

  GURL gurl_a(*url_a_string);
  GURL gurl_b(*url_b_string);
  if (!gurl_a.is_valid() || !gurl_b.is_valid())
    return PP_FALSE;

  return gurl_a.GetOrigin() == gurl_b.GetOrigin() ? PP_TRUE : PP_FALSE;
}

// Used for returning the given GURL from a PPAPI function, with an optional
// out param indicating the components.
PP_Var URLUtilImpl::GenerateURLReturn(VarFromUtf8 var_from_utf8,
                                      PP_Module module,
                                      const GURL& url,
                                      PP_URLComponents_Dev* components) {
  if (!url.is_valid())
    return PP_MakeNull();
  ConvertComponents(url.parsed_for_possibly_invalid_spec(), components);
  return var_from_utf8(module, url.possibly_invalid_spec().c_str(),
      static_cast<uint32_t>(url.possibly_invalid_spec().size()));
}

}  // namespace ppapi
