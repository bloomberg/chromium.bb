// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_GETURL_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_GETURL_TEST_H_

#include <stdio.h>

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// The PluginGetURLTest test functionality of the NPN_GetURL
// and NPN_GetURLNotify methods.
//
// This test first discovers it's URL by sending a GetURL request
// for 'javascript:top.location'.  After receiving that, the
// test will request the url itself (again via GetURL).
class PluginGetURLTest : public PluginTest {
 public:
  // Constructor.
  PluginGetURLTest(NPP id, NPNetscapeFuncs *host_functions);
  virtual ~PluginGetURLTest();

  //
  // NPAPI functions
  //
  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved);
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
  virtual int32   WriteReady(NPStream *stream);
  virtual int32   Write(NPStream *stream, int32 offset, int32 len,
                        void *buffer);
  virtual NPError DestroyStream(NPStream *stream, NPError reason);
  virtual void    StreamAsFile(NPStream* stream, const char* fname);
  virtual void    URLNotify(const char* url, NPReason reason, void* data);
  virtual void    URLRedirectNotify(const char* url, int32_t status,
                                    void* notify_data);

 private:
  bool tests_started_;
  int tests_in_progress_;
  std::string self_url_;
  FILE* test_file_;
  bool expect_404_response_;
  // This flag is set to true in the context of the NPN_Evaluate call.
  bool npn_evaluate_context_;
  // The following two flags handle URL redirect notifications received by
  // plugins.
  bool handle_url_redirects_;
  bool received_url_redirect_notification_;
  std::string page_not_found_url_;
  std::string fail_write_url_;
  std::string referrer_target_url_;
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_GETURL_TEST_H_
