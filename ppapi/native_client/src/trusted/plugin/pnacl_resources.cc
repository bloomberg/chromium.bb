// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

nacl::string GetFullUrl(const nacl::string& partial_url) {
  return PnaclUrls::GetBaseUrl() + GetNaClInterface()->GetSandboxArch() + "/" +
         partial_url;
}

}  // namespace

static const char kPnaclBaseUrl[] = "chrome://pnacl-translator/";

nacl::string PnaclUrls::GetBaseUrl() {
  return nacl::string(kPnaclBaseUrl);
}

// Determine if a URL is for a pnacl-component file, or if it is some other
// type of URL (e.g., http://, https://, chrome-extension://).
// The URL could be one of the other variants for shared libraries
// served from the web.
bool PnaclUrls::IsPnaclComponent(const nacl::string& full_url) {
  return full_url.find(kPnaclBaseUrl, 0) == 0;
}

// Convert a URL to a filename accepted by GetReadonlyPnaclFd.
// Must be kept in sync with chrome/browser/nacl_host/nacl_file_host.
nacl::string PnaclUrls::PnaclComponentURLToFilename(
    const nacl::string& full_url) {
  // strip component scheme.
  nacl::string r = full_url.substr(nacl::string(kPnaclBaseUrl).length());

  // Use white-listed-chars.
  size_t replace_pos;
  static const char* white_list = "abcdefghijklmnopqrstuvwxyz0123456789_";
  replace_pos = r.find_first_not_of(white_list);
  while(replace_pos != nacl::string::npos) {
    r = r.replace(replace_pos, 1, "_");
    replace_pos = r.find_first_not_of(white_list);
  }
  return r;
}

nacl::string PnaclUrls::GetResourceInfoUrl() {
  return "pnacl.json";
}

//////////////////////////////////////////////////////////////////////

PnaclResources::~PnaclResources() {
  if (llc_file_handle_ != PP_kInvalidFileHandle)
    CloseFileHandle(llc_file_handle_);
  if (ld_file_handle_ != PP_kInvalidFileHandle)
    CloseFileHandle(ld_file_handle_);
}

void PnaclResources::ReadResourceInfo(
    const nacl::string& resource_info_url,
    const pp::CompletionCallback& resource_info_read_cb) {
  PLUGIN_PRINTF(("PnaclResources::ReadResourceInfo\n"));

  nacl::string full_url = PnaclUrls::GetBaseUrl() + resource_info_url;
  PLUGIN_PRINTF(("Resolved resources info url: %s\n", full_url.c_str()));
  nacl::string resource_info_filename =
    PnaclUrls::PnaclComponentURLToFilename(full_url);

  PLUGIN_PRINTF(("Pnacl-converted resources info url: %s\n",
                 resource_info_filename.c_str()));

  PP_Var pp_llc_tool_name_var;
  PP_Var pp_ld_tool_name_var;
  if (!plugin_->nacl_interface()->GetPnaclResourceInfo(
          plugin_->pp_instance(),
          resource_info_filename.c_str(),
          &pp_llc_tool_name_var,
          &pp_ld_tool_name_var)) {
    coordinator_->ExitWithError();
  }

  pp::Var llc_tool_name(pp::PASS_REF, pp_llc_tool_name_var);
  pp::Var ld_tool_name(pp::PASS_REF, pp_ld_tool_name_var);
  llc_tool_name_ = GetFullUrl(llc_tool_name.AsString());
  ld_tool_name_ = GetFullUrl(ld_tool_name.AsString());
  pp::Module::Get()->core()->CallOnMainThread(0, resource_info_read_cb, PP_OK);
}

PP_FileHandle PnaclResources::TakeLlcFileHandle() {
  PP_FileHandle to_return = llc_file_handle_;
  llc_file_handle_ = PP_kInvalidFileHandle;
  return to_return;
}

PP_FileHandle PnaclResources::TakeLdFileHandle() {
  PP_FileHandle to_return = ld_file_handle_;
  ld_file_handle_ = PP_kInvalidFileHandle;
  return to_return;
}

void PnaclResources::StartLoad(
    const pp::CompletionCallback& all_loaded_callback) {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  // Do a blocking load of each of the resources.
  nacl::string llc_filename =
      PnaclUrls::PnaclComponentURLToFilename(llc_tool_name_);
  llc_file_handle_ =
      plugin_->nacl_interface()->GetReadonlyPnaclFd(llc_filename.c_str());

  nacl::string ld_filename =
      PnaclUrls::PnaclComponentURLToFilename(ld_tool_name_);
  ld_file_handle_ =
      plugin_->nacl_interface()->GetReadonlyPnaclFd(ld_filename.c_str());

  int32_t result = PP_OK;
  if (llc_file_handle_ == PP_kInvalidFileHandle ||
      ld_file_handle_ == PP_kInvalidFileHandle) {
    // File-open failed. Assume this means that the file is
    // not actually installed. This shouldn't actually occur since
    // ReadResourceInfo() fail first.
    coordinator_->ReportNonPpapiError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
      nacl::string("The Portable Native Client (pnacl) component is not "
                   "installed. Please consult chrome://components for more "
                   "information."));
    result = PP_ERROR_FILENOTFOUND;
  }
  pp::Module::Get()->core()->CallOnMainThread(0, all_loaded_callback, result);
}

}  // namespace plugin
