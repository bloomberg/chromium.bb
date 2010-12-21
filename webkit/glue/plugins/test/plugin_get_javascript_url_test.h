// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_GET_JAVASCRIPT_URL_H
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_GET_JAVASCRIPT_URL_H

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// This class tests NPP_GetURLNotify for a javascript URL with _top
// as the target frame.
class ExecuteGetJavascriptUrlTest : public PluginTest {
 public:
  // Constructor.
  ExecuteGetJavascriptUrlTest(NPP id, NPNetscapeFuncs *host_functions);
  //
  // NPAPI functions
  //
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
  virtual int32   WriteReady(NPStream *stream);
  virtual int32   Write(NPStream *stream, int32 offset, int32 len,
                        void *buffer);
  virtual NPError DestroyStream(NPStream *stream, NPError reason);
  virtual void    URLNotify(const char* url, NPReason reason, void* data);

 private:
#if defined(OS_WIN)
  static void CALLBACK TimerProc(HWND window, UINT message, UINT timer_id,
                                 unsigned long elapsed_time);
#endif
  bool test_started_;
  // This flag is set to true in the context of the NPN_Evaluate call.
  bool npn_evaluate_context_;
  std::string self_url_;

#if defined(OS_WIN)
  HWND window_;
#endif
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_GET_JAVASCRIPT_URL_H
