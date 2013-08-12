// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_
#define PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class Resource;

struct PPAPI_SHARED_EXPORT URLRequestInfoData {
  struct PPAPI_SHARED_EXPORT BodyItem {
    BodyItem();
    explicit BodyItem(const std::string& data);
    BodyItem(Resource* file_ref,
             int64_t start_offset,
             int64_t number_of_bytes,
             PP_Time expected_last_modified_time);

    // Set if the input is a file, false means the |data| is valid.
    bool is_file;

    std::string data;

    // Is is_file is set, these variables are set. Note that the resource
    // may still be NULL in some cases, such as deserialization errors.
    //
    // This is a bit tricky. In the plugin side of the proxy, both the file ref
    // and the file_ref_host_resource will be set and valid. The scoped_refptr
    // ensures that the resource is alive for as long as the BodyItem is.
    //
    // When we deserialize this in the renderer, only the
    // file_ref_host_resource's are serialized over IPC. The file_refs won't be
    // valid until the host resources are converted to Resource pointers in the
    // PPB_URLRequestInfo_Impl.
    scoped_refptr<Resource> file_ref;
    HostResource file_ref_host_resource;

    int64_t start_offset;
    int64_t number_of_bytes;
    PP_Time expected_last_modified_time;

    // If you add more stuff here, be sure to modify the serialization rules in
    // ppapi_messages.h
  };

  URLRequestInfoData();
  ~URLRequestInfoData();

  std::string url;
  std::string method;
  std::string headers;

  bool stream_to_file;
  bool follow_redirects;
  bool record_download_progress;
  bool record_upload_progress;

  // |has_custom_referrer_url| is set to false if a custom referrer hasn't been
  // set (or has been set to an Undefined Var) and the default referrer should
  // be used. (Setting the custom referrer to an empty string indicates that no
  // referrer header should be generated.)
  bool has_custom_referrer_url;
  std::string custom_referrer_url;

  bool allow_cross_origin_requests;
  bool allow_credentials;

  // Similar to the custom referrer (above), but for custom content transfer
  // encoding and custom user agent, respectively.
  bool has_custom_content_transfer_encoding;
  std::string custom_content_transfer_encoding;
  bool has_custom_user_agent;
  std::string custom_user_agent;

  int32_t prefetch_buffer_upper_threshold;
  int32_t prefetch_buffer_lower_threshold;

  std::vector<BodyItem> body;

  // If you add more stuff here, be sure to modify the serialization rules in
  // ppapi_messages.h
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_
