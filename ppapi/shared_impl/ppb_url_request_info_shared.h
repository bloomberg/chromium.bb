// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_URL_REQUEST_INFO_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_URL_REQUEST_INFO_SHARED_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT PPB_URLRequestInfo_Data {
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

  PPB_URLRequestInfo_Data();
  ~PPB_URLRequestInfo_Data();

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
  // encoding.
  bool has_custom_content_transfer_encoding;
  std::string custom_content_transfer_encoding;

  int32_t prefetch_buffer_upper_threshold;
  int32_t prefetch_buffer_lower_threshold;

  std::vector<BodyItem> body;

  // If you add more stuff here, be sure to modify the serialization rules in
  // ppapi_messages.h
};

class PPAPI_SHARED_EXPORT PPB_URLRequestInfo_Shared
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_URLRequestInfo_API {
 public:
  PPB_URLRequestInfo_Shared(ResourceObjectType type,
                            PP_Instance instance,
                            const PPB_URLRequestInfo_Data& data);
  ~PPB_URLRequestInfo_Shared();

  // Resource overrides.
  virtual thunk::PPB_URLRequestInfo_API* AsPPB_URLRequestInfo_API() OVERRIDE;

  // PPB_URLRequestInfo_API implementation.
  virtual PP_Bool SetProperty(PP_URLRequestProperty property,
                              PP_Var var) OVERRIDE;
  virtual PP_Bool AppendDataToBody(const void* data, uint32_t len) OVERRIDE;
  virtual PP_Bool AppendFileToBody(
      PP_Resource file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      PP_Time expected_last_modified_time) OVERRIDE;
  virtual const PPB_URLRequestInfo_Data& GetData() const OVERRIDE;

 protected:
  // Constructor used by the webkit implementation.
  PPB_URLRequestInfo_Shared(PP_Instance instance,
                     const PPB_URLRequestInfo_Data& data);

  bool SetUndefinedProperty(PP_URLRequestProperty property);
  bool SetBooleanProperty(PP_URLRequestProperty property, bool value);
  bool SetIntegerProperty(PP_URLRequestProperty property, int32_t value);
  bool SetStringProperty(PP_URLRequestProperty property,
                         const std::string& value);

  const PPB_URLRequestInfo_Data& data() const { return data_; }
  PPB_URLRequestInfo_Data& data() { return data_; }

 private:
  PPB_URLRequestInfo_Data data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_URLRequestInfo_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_URL_REQUEST_INFO_SHARED_H_
