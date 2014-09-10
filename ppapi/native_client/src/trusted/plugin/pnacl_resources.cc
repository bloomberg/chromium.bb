// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

static const char kPnaclBaseUrl[] = "chrome://pnacl-translator/";

std::string GetFullUrl(const std::string& partial_url) {
  return std::string(kPnaclBaseUrl) + GetNaClInterface()->GetSandboxArch() +
         "/" + partial_url;
}

}  // namespace

PnaclResources::PnaclResources(Plugin* plugin)
    : plugin_(plugin) {
  llc_file_info_ = kInvalidNaClFileInfo;
  ld_file_info_ = kInvalidNaClFileInfo;
}

PnaclResources::~PnaclResources() {
  if (llc_file_info_.handle != PP_kInvalidFileHandle)
    CloseFileHandle(llc_file_info_.handle);
  if (ld_file_info_.handle != PP_kInvalidFileHandle)
    CloseFileHandle(ld_file_info_.handle);
}

bool PnaclResources::ReadResourceInfo() {
  PP_Var pp_llc_tool_name_var;
  PP_Var pp_ld_tool_name_var;
  if (!plugin_->nacl_interface()->GetPnaclResourceInfo(
          plugin_->pp_instance(),
          &pp_llc_tool_name_var,
          &pp_ld_tool_name_var)) {
    return false;
  }
  pp::Var llc_tool_name(pp::PASS_REF, pp_llc_tool_name_var);
  pp::Var ld_tool_name(pp::PASS_REF, pp_ld_tool_name_var);
  llc_tool_name_ = GetFullUrl(llc_tool_name.AsString());
  ld_tool_name_ = GetFullUrl(ld_tool_name.AsString());
  return true;
}

PP_NaClFileInfo PnaclResources::TakeLlcFileInfo() {
  PP_NaClFileInfo to_return = llc_file_info_;
  llc_file_info_ = kInvalidNaClFileInfo;
  return to_return;
}

PP_NaClFileInfo PnaclResources::TakeLdFileInfo() {
  PP_NaClFileInfo to_return = ld_file_info_;
  ld_file_info_ = kInvalidNaClFileInfo;
  return to_return;
}

bool PnaclResources::StartLoad() {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  // Do a blocking load of each of the resources.
  plugin_->nacl_interface()->GetReadExecPnaclFd(llc_tool_name_.c_str(),
                                                &llc_file_info_);
  plugin_->nacl_interface()->GetReadExecPnaclFd(ld_tool_name_.c_str(),
                                                &ld_file_info_);
  return (llc_file_info_.handle != PP_kInvalidFileHandle &&
          ld_file_info_.handle != PP_kInvalidFileHandle);
}

}  // namespace plugin
