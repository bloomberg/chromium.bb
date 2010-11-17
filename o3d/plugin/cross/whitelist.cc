/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "plugin/cross/whitelist.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"

namespace o3d {

#if !defined(O3D_PLUGIN_DOMAIN_WHITELIST) && \
    !defined(O3D_PLUGIN_ENABLE_FULLSCREEN_MSG)
#error "No whitelist and no fullscreen message is a security vulnerability"
#endif

#ifdef O3D_PLUGIN_DOMAIN_WHITELIST

static const char *const kDomainWhitelist[] = {
  // This macro contains the comma-separated string literals for the whitelist
  O3D_PLUGIN_DOMAIN_WHITELIST
};

static const char kHttpProtocol[] = "http://";
static const char kHttpsProtocol[] = "https://";

// For testing purposes assume local files valid too.
static const char kLocalFileUrlProtocol[] = "file://";

static std::string GetURL(NPP instance) {
  std::string url;
  // get URL for the loading page - first approach from
  // http://developer.mozilla.org/en/docs/Getting_the_page_URL_in_NPAPI_plugin
  // Get the window object.
  // note: on some browsers, this will increment the window ref count.
  // on others, it won't.
  // this is a bug in.... something, but no one agrees what.
  // http://lists.apple.com/archives/webkitsdk-dev/2005/Aug/msg00044.html
  NPObject *window_obj = NULL;
  NPError err = NPN_GetValue(instance, NPNVWindowNPObject,
                             &window_obj);
  if (NPERR_NO_ERROR != err) {
    LOG(ERROR) << "getvalue failed (err = " << err << ")";
    goto exit0;
  }
  {
    // Create a "location" identifier.
    NPIdentifier identifier = NPN_GetStringIdentifier("location");
    // Declare a local variant value for the location.
    NPVariant location_variant_value;
    // Get the location property from the window object
    // (which is another object).
    bool success = NPN_GetProperty(instance, window_obj, identifier,
                                   &location_variant_value);
    if (!success) {
      LOG(ERROR) << "getproperty failed (location)";
      goto exit0;
    }
    if (!NPVARIANT_IS_OBJECT(location_variant_value)) {
      LOG(ERROR) << "location property has wrong type: "
                 << location_variant_value.type;
      goto exit1;
    }
    {
      // Get a pointer to the "location" object.
      NPObject *location_obj = location_variant_value.value.objectValue;
      // Create a "href" identifier.
      identifier = NPN_GetStringIdentifier("href");
      // Declare a local variant value for the href.
      NPVariant href_variant_value;
      // Get the location property from the location object.
      success = NPN_GetProperty(instance, location_obj, identifier,
                                &href_variant_value);
      if (!success) {
        LOG(ERROR) << "getproperty failed (href)";
        goto exit1;
      }
      if (!NPVARIANT_IS_STRING(href_variant_value)) {
        LOG(ERROR) << "href property has wrong type: "
                   << href_variant_value.type;
        goto exit2;
      }
      // let's just grab the NPUTF8 from the variant and make a std::string
      // from it.
      url = std::string(
          static_cast<const char *>(
              href_variant_value.value.stringValue.UTF8Characters),
          static_cast<size_t>(
              href_variant_value.value.stringValue.UTF8Length));

     exit2:
      NPN_ReleaseVariantValue(&href_variant_value);
    }
   exit1:
    NPN_ReleaseVariantValue(&location_variant_value);
  }
 exit0:
  return url;
}

static std::string ParseUrlHost(const std::string &in_url) {
  size_t host_start;
  if (in_url.find(kHttpProtocol) == 0) {
    host_start = sizeof(kHttpProtocol) - 1;
  } else if (in_url.find(kHttpsProtocol) == 0) {
    host_start = sizeof(kHttpsProtocol) - 1;
  } else {
    // Do not allow usage on non http/https pages.
    return "";
  }
  size_t path_start = in_url.find("/", host_start);
  if (path_start == std::string::npos) {
    path_start = in_url.size();
  }
  const std::string host_and_port(
      in_url.substr(host_start, path_start - host_start));
  size_t colon_pos = host_and_port.find(":");
  if (colon_pos == std::string::npos) {
    colon_pos = host_and_port.size();
  }
  return host_and_port.substr(0, colon_pos);
}

static bool IsDomainWhitelisted(const std::string &in_url) {
  if (in_url.find(kLocalFileUrlProtocol) == 0) {
    // Starts with file://, so it's a local file. Allow access for testing
    // purposes.
    return true;
  } else {
    std::string host(ParseUrlHost(in_url));

    // convert the host to a lower-cased version so we
    // don't have to worry about case mismatches.
    for (size_t i = 0; i < host.length(); ++i) {
      host[i] = tolower(host[i]);
    }

    for (size_t i = 0; i < arraysize(kDomainWhitelist); ++i) {
      size_t pos = host.rfind(kDomainWhitelist[i]);
      if (pos != std::string::npos &&
          ((pos + strlen(kDomainWhitelist[i]) == host.length())))
        return true;
    }

    return false;
  }
}

#endif  // O3D_PLUGIN_DOMAIN_WHITELIST

bool IsDomainAuthorized(NPP instance) {
#ifdef O3D_PLUGIN_DOMAIN_WHITELIST
  bool authorized = IsDomainWhitelisted(GetURL(instance));
  if (!authorized) {
    LOG(ERROR) << "Unauthorized domain";
  }
  return authorized;
#else
  // No whitelist; allow usage on any website. (This is the default.)
  return true;
#endif
}

}  // namespace o3d
