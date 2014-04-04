// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/native_client/src/trusted/plugin/file_utils.h"
#include "ppapi/native_client/src/trusted/plugin/manifest.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace plugin {

static const char kPnaclBaseUrl[] = "chrome://pnacl-translator/";
const char PnaclUrls::kResourceInfoUrl[] = "pnacl.json";

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

  nacl::string full_url;
  ErrorInfo error_info;
  if (!manifest_->ResolveURL(resource_info_url, &full_url, &error_info)) {
    ReadResourceInfoError(nacl::string("failed to resolve ") +
                          resource_info_url + ": " +
                          error_info.message() + ".");
    return;
  }
  PLUGIN_PRINTF(("Resolved resources info url: %s\n", full_url.c_str()));
  nacl::string resource_info_filename =
    PnaclUrls::PnaclComponentURLToFilename(full_url);

  PLUGIN_PRINTF(("Pnacl-converted resources info url: %s\n",
                 resource_info_filename.c_str()));

  int32_t fd = GetPnaclFD(plugin_, resource_info_filename.c_str());
  if (fd < 0) {
    // File-open failed. Assume this means that the file is
    // not actually installed.
    ReadResourceInfoError(
        nacl::string("The Portable Native Client (pnacl) component is not "
                     "installed. Please consult chrome://components for more "
                     "information."));
    return;
  }

  nacl::string json_buffer;
  file_utils::StatusCode status = file_utils::SlurpFile(fd, json_buffer);
  if (status != file_utils::PLUGIN_FILE_SUCCESS) {
    ReadResourceInfoError(
        nacl::string("PnaclResources::ReadResourceInfo reading "
                     "failed for: ") + resource_info_filename);
    return;
  }

  // Finally, we have the resource info JSON data in json_buffer.
  PLUGIN_PRINTF(("Resource info JSON data:\n%s\n", json_buffer.c_str()));
  nacl::string error_message;
  if (!ParseResourceInfo(json_buffer, error_message)) {
    ReadResourceInfoError(nacl::string("Parsing resource info failed: ") +
                          error_message + "\n");
    return;
  }

  // Done. Queue the completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, resource_info_read_cb, PP_OK);
}

void PnaclResources::ReadResourceInfoError(const nacl::string& msg) {
  coordinator_->ReportNonPpapiError(PP_NACL_ERROR_PNACL_RESOURCE_FETCH, msg);
}

bool PnaclResources::ParseResourceInfo(const nacl::string& buf,
                                       nacl::string& errmsg) {
  // Expect the JSON file to contain a top-level object (dictionary).
  Json::Reader json_reader;
  Json::Value json_data;
  if (!json_reader.parse(buf, json_data)) {
    errmsg = nacl::string("JSON parse error: ") +
             json_reader.getFormatedErrorMessages();
    return false;
  }

  if (!json_data.isObject()) {
    errmsg = nacl::string("Malformed JSON dictionary");
    return false;
  }

  if (json_data.isMember("pnacl-llc-name")) {
    Json::Value json_name = json_data["pnacl-llc-name"];
    if (json_name.isString()) {
      llc_tool_name = json_name.asString();
      PLUGIN_PRINTF(("Set llc_tool_name=%s\n", llc_tool_name.c_str()));
    }
  }

  if (json_data.isMember("pnacl-ld-name")) {
    Json::Value json_name = json_data["pnacl-ld-name"];
    if (json_name.isString()) {
      ld_tool_name = json_name.asString();
      PLUGIN_PRINTF(("Set ld_tool_name=%s\n", ld_tool_name.c_str()));
    }
  }

  return true;
}

nacl::string PnaclResources::GetFullUrl(
    const nacl::string& partial_url, const nacl::string& sandbox_arch) const {
  nacl::string full_url;
  ErrorInfo error_info;
  const nacl::string& url_with_platform_prefix =
      sandbox_arch + "/" + partial_url;
  if (!manifest_->ResolveURL(url_with_platform_prefix,
                             &full_url,
                             &error_info)) {
    PLUGIN_PRINTF(("PnaclResources::GetFullUrl failed: %s.\n",
                   error_info.message().c_str()));
    return "";
  }
  return full_url;
}

void PnaclResources::StartLoad(
    const pp::CompletionCallback& all_loaded_callback) {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  std::vector<nacl::string> resource_urls;
  resource_urls.push_back(GetLlcUrl());
  resource_urls.push_back(GetLdUrl());

  PLUGIN_PRINTF(("PnaclResources::StartLoad -- local install of PNaCl.\n"));
  // Do a blocking load of each of the resources.
  int32_t result = PP_OK;
  for (size_t i = 0; i < resource_urls.size(); ++i) {
    nacl::string full_url = GetFullUrl(
        resource_urls[i], plugin_->nacl_interface()->GetSandboxArch());
    if (full_url == "") {
      coordinator_->ReportNonPpapiError(
          PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
          nacl::string("failed to resolve ") + resource_urls[i] + ".");
      break;
    }
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
