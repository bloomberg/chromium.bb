/*
  Copyright (c) 2011 The Native Client Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <new>
#include <map>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/plugin_nacl_file.h"
#include "native_client/src/untrusted/ppapi/nacl_file.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"

namespace {

struct UrlData {
  PP_Instance instance;
  char* url;
};

void UrlDidLoad(void* user_data, int32_t pp_error) {
  nacl::scoped_ptr<UrlData> url_data(reinterpret_cast<UrlData*>(user_data));
  nacl::scoped_array<char> url(url_data->url);
  if (pp_error == PP_OK) {
    int fd = ppapi_proxy::GetFileDesc(url_data->instance, url.get());
    // TODO(nfullagar, polina): use fd to implement NaClFile API.
    // In the meantime this codde snippet can be used to "test"
    // the validity of the file descriptor delivered by the proxy.
    char buffer[256];
    read(fd, buffer, sizeof(buffer));
    printf("---------------------------------------------\n");
    printf("%s", static_cast<char*>(buffer));
    printf("---------------------------------------------\n");
  }
}

}  // namespace

int32_t LoadUrl(PP_Instance instance, const char* url) {
  UrlData* url_data = new(std::nothrow) UrlData;
  if (url_data == NULL)
    return PP_ERROR_NOMEMORY;

  url_data->instance = instance;
  size_t url_string_len = strlen(url) + 1;
  url_data->url = new(std::nothrow) char[url_string_len];
  memcpy(url_data->url, url, url_string_len);

  int32_t pp_error = ppapi_proxy::StreamAsFile(
      instance,
      url,
      PP_MakeCompletionCallback(UrlDidLoad, url_data));
  return pp_error;
}
