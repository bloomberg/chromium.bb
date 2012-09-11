// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/test/plugin_get_javascript_url_test.h"

#include "base/basictypes.h"
#include "base/logging.h"

// url for "self".
#define SELF_URL "javascript:window.location+\"\""
// The identifier for the self url stream.
#define SELF_URL_STREAM_ID 1

// The identifier for the fetched url stream.
#define FETCHED_URL_STREAM_ID 2

// The maximum chunk size of stream data.
#define STREAM_CHUNK 197

const int kNPNEvaluateTimerID = 100;
const int kNPNEvaluateTimerElapse = 50;


namespace NPAPIClient {

ExecuteGetJavascriptUrlTest::ExecuteGetJavascriptUrlTest(
    NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions),
    test_started_(false),
#ifdef OS_WIN
    window_(NULL),
#endif
    npn_evaluate_context_(false) {
}

NPError ExecuteGetJavascriptUrlTest::SetWindow(NPWindow* pNPWindow) {
#if !defined(OS_MACOSX)
  if (pNPWindow->window == NULL)
    return NPERR_NO_ERROR;
#endif

  if (!test_started_) {
    std::string url = SELF_URL;
    HostFunctions()->geturlnotify(id(), url.c_str(), "_top",
                                  reinterpret_cast<void*>(SELF_URL_STREAM_ID));
    test_started_ = true;

#ifdef OS_WIN
    HWND window_handle = reinterpret_cast<HWND>(pNPWindow->window);
    if (!::GetProp(window_handle, L"Plugin_Instance")) {
      // TODO: this propery leaks.
      ::SetProp(window_handle, L"Plugin_Instance", this);
      // We attempt to retreive the NPObject for the plugin instance identified
      // by the NPObjectLifetimeTestInstance2 class as it may not have been
      // instantiated yet.
      SetTimer(window_handle, kNPNEvaluateTimerID, kNPNEvaluateTimerElapse,
               TimerProc);
    }
    window_ = window_handle;
#endif
  }

  return NPERR_NO_ERROR;
}

#ifdef OS_WIN
void CALLBACK ExecuteGetJavascriptUrlTest::TimerProc(
    HWND window, UINT message, UINT timer_id, unsigned long elapsed_time) {
  ExecuteGetJavascriptUrlTest* this_instance =
      reinterpret_cast<ExecuteGetJavascriptUrlTest*>
          (::GetProp(window, L"Plugin_Instance"));
  CHECK(this_instance);

  ::RemoveProp(window, L"Plugin_Instance");

  NPObject *window_obj = NULL;
  this_instance->HostFunctions()->getvalue(this_instance->id(),
                                           NPNVWindowNPObject,
                                           &window_obj);
  if (!window_obj) {
    this_instance->SetError("Failed to get NPObject for plugin instance2");
    this_instance->SignalTestCompleted();
    return;
  }

  std::string script = "javascript:window.location";
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length = static_cast<unsigned int>(script.length());
  NPVariant result_var;

  this_instance->npn_evaluate_context_ = true;
  NPError result = this_instance->HostFunctions()->evaluate(
      this_instance->id(), window_obj, &script_string, &result_var);
  this_instance->npn_evaluate_context_ = false;
}
#endif

NPError ExecuteGetJavascriptUrlTest::NewStream(NPMIMEType type,
                                               NPStream* stream,
                                               NPBool seekable,
                                               uint16* stype) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

  if (npn_evaluate_context_) {
    SetError("NewStream received in context of NPN_Evaluate");
    return NPERR_NO_ERROR;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

int32 ExecuteGetJavascriptUrlTest::WriteReady(NPStream *stream) {
  if (npn_evaluate_context_) {
    SetError("WriteReady received in context of NPN_Evaluate");
    return NPERR_NO_ERROR;
  }
  return STREAM_CHUNK;
}

int32 ExecuteGetJavascriptUrlTest::Write(NPStream *stream, int32 offset,
                                         int32 len, void *buffer) {
  if (stream == NULL) {
    SetError("Write got null stream");
    return -1;
  }
  if (len < 0 || len > STREAM_CHUNK) {
    SetError("Write got bogus stream chunk size");
    return -1;
  }

  if (npn_evaluate_context_) {
    SetError("Write received in context of NPN_Evaluate");
    return len;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    default:
      SetError("Unexpected write callback");
      break;
  }
  // Pretend that we took all the data.
  return len;
}


NPError ExecuteGetJavascriptUrlTest::DestroyStream(NPStream *stream,
                                                   NPError reason) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

#ifdef OS_WIN
  KillTimer(window_, kNPNEvaluateTimerID);
#endif

  if (npn_evaluate_context_) {
    SetError("DestroyStream received in context of NPN_Evaluate");
    return NPERR_NO_ERROR;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

void ExecuteGetJavascriptUrlTest::URLNotify(const char* url, NPReason reason,
                                            void* data) {
  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(data),
                 cast_validity_check);

  if (npn_evaluate_context_) {
    SetError("URLNotify received in context of NPN_Evaluate");
    return;
  }

  unsigned long stream_id = reinterpret_cast<unsigned long>(data);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      if (strcmp(url, SELF_URL) != 0)
        SetError("URLNotify reported incorrect url for SELF_URL");
      if (self_url_.empty())
        SetError("Failed to obtain window location.");
      SignalTestCompleted();
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
}

} // namespace NPAPIClient
