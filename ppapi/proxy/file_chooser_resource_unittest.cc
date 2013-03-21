// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/file_chooser_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest FileChooserResourceTest;

void* GetFileRefDataBuffer(void* user_data,
                           uint32_t element_count,
                           uint32_t element_size) {
  EXPECT_TRUE(element_size == sizeof(PP_Resource));
  std::vector<PP_Resource>* output =
      static_cast<std::vector<PP_Resource>*>(user_data);
  output->resize(element_count);
  if (element_count > 0)
    return &(*output)[0];
  return NULL;
}

void DoNothingCallback(void* user_data, int32_t result) {
}

// Calls PopulateAcceptTypes and verifies that the resulting array contains
// the given values. The values may be NULL if there aren't expected to be
// that many results.
bool CheckParseAcceptType(const std::string& input,
                          const char* expected1,
                          const char* expected2) {
  std::vector<std::string> output;
  FileChooserResource::PopulateAcceptTypes(input, &output);

  const size_t kCount = 2;
  const char* expected[kCount] = { expected1, expected2 };

  for (size_t i = 0; i < kCount; i++) {
    if (!expected[i])
      return i == output.size();
    if (output.size() <= i)
      return false;
    if (output[i] != expected[i])
      return false;
  }

  return output.size() == kCount;
}

}  // namespace

// Does a full test of Show() and reply functionality in the plugin side using
// the public C interfaces.
TEST_F(FileChooserResourceTest, Show) {
  const PPB_FileChooser_Dev_0_6* chooser_iface =
      thunk::GetPPB_FileChooser_Dev_0_6_Thunk();
  ScopedPPResource res(ScopedPPResource::PassRef(),
      chooser_iface->Create(pp_instance(), PP_FILECHOOSERMODE_OPEN,
                            PP_MakeUndefined()));

  std::vector<PP_Resource> dest;
  PP_ArrayOutput output;
  output.GetDataBuffer = &GetFileRefDataBuffer;
  output.user_data = &dest;

  int32_t result = chooser_iface->Show(
      res, output, PP_MakeCompletionCallback(&DoNothingCallback, NULL));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  // Should have sent a "show" message.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileChooser_Show::ID, &params, &msg));

  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(PP_OK);

  // Synthesize a response with one file ref in it. Note that it must have a
  // host resource value set or deserialization will fail. Since there isn't
  // actually a host, this can be whatever we want.
  std::vector<PPB_FileRef_CreateInfo> create_info_array;
  PPB_FileRef_CreateInfo create_info;
  create_info.resource.SetHostResource(pp_instance(), 123);
  create_info.path = "foo/bar";
  create_info.name = "baz";
  create_info_array.push_back(create_info);
  ASSERT_TRUE(plugin_dispatcher()->OnMessageReceived(
      PpapiPluginMsg_ResourceReply(reply_params,
          PpapiPluginMsg_FileChooser_ShowReply(create_info_array))));

  // Should have populated our vector.
  ASSERT_EQ(1u, dest.size());
  ScopedPPResource dest_deletor(dest[0]);  // Ensure it's cleaned up.

  const PPB_FileRef_1_0* file_ref_iface = thunk::GetPPB_FileRef_1_0_Thunk();
  EXPECT_EQ(PP_FILESYSTEMTYPE_EXTERNAL,
            file_ref_iface->GetFileSystemType(dest[0]));

  PP_Var name_var(file_ref_iface->GetName(dest[0]));
  {
    ProxyAutoLock lock;
    ScopedPPVar release_name_var(ScopedPPVar::PassRef(), name_var);
    EXPECT_VAR_IS_STRING(create_info.name, name_var);
  }
  // Path should be undefined since it's external filesystem.
  PP_Var path_var(file_ref_iface->GetPath(dest[0]));
  {
    ProxyAutoLock lock;
    ScopedPPVar release_path_var(ScopedPPVar::PassRef(), path_var);
    EXPECT_EQ(PP_VARTYPE_UNDEFINED, path_var.type);
  }
}

TEST_F(FileChooserResourceTest, PopulateAcceptTypes) {
  EXPECT_TRUE(CheckParseAcceptType(std::string(), NULL, NULL));
  EXPECT_TRUE(CheckParseAcceptType("/", NULL, NULL));
  EXPECT_TRUE(CheckParseAcceptType(".", NULL, NULL));
  EXPECT_TRUE(CheckParseAcceptType(",, , ", NULL, NULL));

  EXPECT_TRUE(CheckParseAcceptType("app/txt", "app/txt", NULL));
  EXPECT_TRUE(CheckParseAcceptType("app/txt,app/pdf", "app/txt", "app/pdf"));
  EXPECT_TRUE(CheckParseAcceptType(" app/txt , app/pdf ",
                                   "app/txt", "app/pdf"));

  // No dot or slash ones should be skipped.
  EXPECT_TRUE(CheckParseAcceptType("foo", NULL, NULL));
  EXPECT_TRUE(CheckParseAcceptType("foo,.txt", ".txt", NULL));
}

}  // namespace proxy
}  // namespace ppapi
