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
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace plugin {

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
  for (std::map<nacl::string, nacl::DescWrapper*>::iterator
           i = resource_wrappers_.begin(), e = resource_wrappers_.end();
       i != e;
       ++i) {
    delete i->second;
  }
  resource_wrappers_.clear();
}

// static
int32_t PnaclResources::GetPnaclFD(Plugin* plugin, const char* filename) {
  PP_FileHandle file_handle =
      plugin->nacl_interface()->GetReadonlyPnaclFd(filename);
  if (file_handle == PP_kInvalidFileHandle)
    return -1;

#if NACL_WINDOWS
  //////// Now try the posix view.
  int32_t posix_desc = _open_osfhandle(reinterpret_cast<intptr_t>(file_handle),
                                       _O_RDONLY | _O_BINARY);
  if (posix_desc == -1) {
    PLUGIN_PRINTF((
        "PnaclResources::GetPnaclFD failed to convert HANDLE to posix\n"));
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(file_handle);
  }
  return posix_desc;
#else
  return file_handle;
#endif
}

nacl::DescWrapper* PnaclResources::WrapperForUrl(const nacl::string& url) {
  CHECK(resource_wrappers_.find(url) != resource_wrappers_.end());
  return resource_wrappers_[url];
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
  llc_tool_name_ = llc_tool_name.AsString();
  ld_tool_name_ = ld_tool_name.AsString();
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, resource_info_read_cb, PP_OK);
}

nacl::string PnaclResources::GetFullUrl(
    const nacl::string& partial_url, const nacl::string& sandbox_arch) const {
  return PnaclUrls::GetBaseUrl() + sandbox_arch + "/" + partial_url;
}

void PnaclResources::StartLoad(
    const pp::CompletionCallback& all_loaded_callback) {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  std::vector<nacl::string> resource_urls;
  resource_urls.push_back(llc_tool_name_);
  resource_urls.push_back(ld_tool_name_);

  PLUGIN_PRINTF(("PnaclResources::StartLoad -- local install of PNaCl.\n"));
  // Do a blocking load of each of the resources.
  int32_t result = PP_OK;
  for (size_t i = 0; i < resource_urls.size(); ++i) {
    nacl::string full_url = GetFullUrl(
        resource_urls[i], plugin_->nacl_interface()->GetSandboxArch());
    nacl::string filename = PnaclUrls::PnaclComponentURLToFilename(full_url);

    int32_t fd = PnaclResources::GetPnaclFD(plugin_, filename.c_str());
    if (fd < 0) {
      // File-open failed. Assume this means that the file is
      // not actually installed. This shouldn't actually occur since
      // ReadResourceInfo() should happen first, and error out.
      coordinator_->ReportNonPpapiError(
          PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        nacl::string("The Portable Native Client (pnacl) component is not "
                     "installed. Please consult chrome://components for more "
                     "information."));
      result = PP_ERROR_FILENOTFOUND;
      break;
    } else {
      resource_wrappers_[resource_urls[i]] =
          plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
    }
  }
  // We're done!  Queue the callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, all_loaded_callback, result);
}

}  // namespace plugin
