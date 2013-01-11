// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/pp_errors.h"

namespace plugin {

static const char kExtensionOrigin[] =
    "chrome-extension://gcodniebolpnpaiggndmcmmfpldlknih/";
static const char kPnaclComponentID[] =
    "pnacl-component://";
const char PnaclUrls::kLlcUrl[] = "llc.nexe";
const char PnaclUrls::kLdUrl[] = "ld.nexe";

bool PnaclUrls::UsePnaclExtension(const Plugin* plugin) {
  // TODO(jvoung): remove extension stuff if we aren't using it.
  return false;
}

nacl::string PnaclUrls::GetBaseUrl(bool use_extension) {
  if (use_extension) {
    return nacl::string(kExtensionOrigin) + GetSandboxISA() + "/";
  } else {
    return nacl::string(kPnaclComponentID) + GetSandboxISA() + "/";
  }
}

bool PnaclUrls::IsPnaclComponent(const nacl::string& full_url) {
  return full_url.find(kPnaclComponentID, 0) == 0;
}

// Convert a URL to a filename accepted by GetReadonlyPnaclFd.
// Must be kept in sync with pnacl_file_host.cc.
nacl::string PnaclUrls::PnaclComponentURLToFilename(
    const nacl::string& full_url) {
  // strip componentID.
  nacl::string r = full_url.substr(nacl::string(kPnaclComponentID).length());

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

void PnaclResources::StartLoad() {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  CHECK(resource_urls_.size() > 0);
  if (PnaclUrls::UsePnaclExtension(plugin_)) {
    PLUGIN_PRINTF(("PnaclResources::StartLoad -- PNaCl chrome extension.\n"));
    // Do a URL fetch.
    // Create a counter (barrier) callback to track when all of the resources
    // are loaded.
    uint32_t resource_count = static_cast<uint32_t>(resource_urls_.size());
    delayed_callback_.reset(
        new DelayedCallback(all_loaded_callback_, resource_count));

    // Schedule the downloads.
    for (size_t i = 0; i < resource_urls_.size(); ++i) {
      nacl::string full_url;
      ErrorInfo error_info;
      if (!manifest_->ResolveURL(resource_urls_[i], &full_url, &error_info)) {
        coordinator_->ReportNonPpapiError(
            ERROR_PNACL_RESOURCE_FETCH,
            nacl::string("failed to resolve ") +
            resource_urls_[i] + ": " +
            error_info.message() + ".");
        break;
      }
      pp::CompletionCallback ready_callback =
          callback_factory_.NewCallback(
              &PnaclResources::ResourceReady,
              resource_urls_[i],
              full_url);
      if (!plugin_->StreamAsFile(full_url,
                                 ready_callback.pp_completion_callback())) {
        coordinator_->ReportNonPpapiError(
            ERROR_PNACL_RESOURCE_FETCH,
            nacl::string("failed to download ") +
            resource_urls_[i] + ".");
        break;
      }
    }
  } else {
    PLUGIN_PRINTF(("PnaclResources::StartLoad -- local install of PNaCl.\n"));
    // Do a blocking load of each of the resources.
    int32_t result = PP_OK;
    for (size_t i = 0; i < resource_urls_.size(); ++i) {
      const nacl::string& url = resource_urls_[i];
      nacl::string full_url;
      ErrorInfo error_info;
      if (!manifest_->ResolveURL(resource_urls_[i], &full_url, &error_info)) {
        coordinator_->ReportNonPpapiError(
            ERROR_PNACL_RESOURCE_FETCH,
            nacl::string("failed to resolve ") +
            url + ": " +
            error_info.message() + ".");
        break;
      }
      nacl::string filename = PnaclUrls::PnaclComponentURLToFilename(full_url);

      int32_t fd = PnaclResources::GetPnaclFD(plugin_, filename.c_str());
      if (fd < 0) {
        coordinator_->ReportNonPpapiError(
            ERROR_PNACL_RESOURCE_FETCH,
            nacl::string("PnaclLocalResources::StartLoad failed for: ") +
            filename);
        result = PP_ERROR_FILENOTFOUND;
        break;
      } else {
        resource_wrappers_[url] =
            plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
      }
    }
    // We're done!  Queue the callback.
    pp::Core* core = pp::Module::Get()->core();
    core->CallOnMainThread(0, all_loaded_callback_, result);
  }
}

void PnaclResources::ResourceReady(int32_t pp_error,
                                   const nacl::string& url,
                                   const nacl::string& full_url) {
  PLUGIN_PRINTF(("PnaclResources::ResourceReady (pp_error=%"
                 NACL_PRId32", url=%s)\n", pp_error, url.c_str()));
  // pp_error is checked by GetLoadedFileDesc.
  int32_t fd = coordinator_->GetLoadedFileDesc(pp_error,
                                               full_url,
                                               "resource " + url);
  if (fd < 0) {
    coordinator_->ReportPpapiError(ERROR_PNACL_RESOURCE_FETCH,
                                   pp_error,
                                   "PnaclResources::ResourceReady failed.");
  } else {
    resource_wrappers_[url] =
        plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY);
    delayed_callback_->RunIfTime();
  }
}

}  // namespace plugin
