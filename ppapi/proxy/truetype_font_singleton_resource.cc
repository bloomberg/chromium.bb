// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/truetype_font_singleton_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

TrueTypeFontSingletonResource::TrueTypeFontSingletonResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_TrueTypeFontSingleton_Create());
}

TrueTypeFontSingletonResource::~TrueTypeFontSingletonResource() {
}

thunk::PPB_TrueTypeFont_Singleton_API*
TrueTypeFontSingletonResource::AsPPB_TrueTypeFont_Singleton_API() {
  return this;
}

int32_t TrueTypeFontSingletonResource::GetFontFamilies(
    PP_Instance instance,
    const PP_ArrayOutput& output,
    const scoped_refptr<TrackedCallback>& callback) {
  Call<PpapiPluginMsg_TrueTypeFontSingleton_GetFontFamiliesReply>(BROWSER,
      PpapiHostMsg_TrueTypeFontSingleton_GetFontFamilies(),
      base::Bind(
          &TrueTypeFontSingletonResource::OnPluginMsgGetFontFamiliesComplete,
          this, callback, output));
  return PP_OK_COMPLETIONPENDING;
}

void TrueTypeFontSingletonResource::OnPluginMsgGetFontFamiliesComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_ArrayOutput array_output,
    const ResourceMessageReplyParams& params,
    const std::vector<std::string>& font_families) {
  // The result code should contain the data size if it's positive.
  int32_t result = params.result();
  DCHECK((result < 0 && font_families.size() == 0) ||
         result == static_cast<int32_t>(font_families.size()));

  ArrayWriter output;
  output.set_pp_array_output(array_output);
  if (output.is_valid()) {
    std::vector< scoped_refptr<Var> > font_family_vars;
    for (size_t i = 0; i < font_families.size(); i++)
      font_family_vars.push_back(
          scoped_refptr<Var>(new StringVar(font_families[i])));
    output.StoreVarVector(font_family_vars);
  } else {
    result = PP_ERROR_FAILED;
  }

  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
