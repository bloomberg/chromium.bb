// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/test/plugin_geturl_test.h"

#include <stdio.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

// url for "self".  The %22%22 is to make a statement for javascript to
// evaluate and return.
#define SELF_URL "javascript:window.location+\"\""

// The identifier for the self url stream.
#define SELF_URL_STREAM_ID 1

// The identifier for the fetched url stream.
#define FETCHED_URL_STREAM_ID 2

// url for testing GetURL with a bogus URL.
#define BOGUS_URL "bogoproto:///x:/asdf.xysdhffieasdf.asdhj/"

// url for testing redirect notifications sent to plugins.
#define REDIRECT_SRC_URL \
    "http://mock.http/npapi/plugin_read_page_redirect_src.html"

// The notification id for the redirect notification url.
#define REDIRECT_SRC_URL_NOTIFICATION_ID 4

// The identifier for the bogus url stream.
#define BOGUS_URL_STREAM_ID 3

// The maximum chunk size of stream data.
#define STREAM_CHUNK 197

namespace NPAPIClient {

PluginGetURLTest::PluginGetURLTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions),
    tests_started_(false),
    tests_in_progress_(0),
    test_file_(NULL),
    expect_404_response_(false),
    npn_evaluate_context_(false),
    handle_url_redirects_(false),
    received_url_redirect_notification_(false) {
}

PluginGetURLTest::~PluginGetURLTest() {}

NPError PluginGetURLTest::New(uint16 mode, int16 argc, const char* argn[],
                              const char* argv[], NPSavedData* saved) {
  const char* page_not_found_url = GetArgValue("page_not_found_url", argc,
                                               argn, argv);
  if (page_not_found_url) {
    page_not_found_url_ = page_not_found_url;
    expect_404_response_ = true;
  }

  const char* fail_write_url = GetArgValue("fail_write_url", argc,
                                           argn, argv);
  if (fail_write_url) {
    fail_write_url_ = fail_write_url;
  }

  const char* referrer_target_url = GetArgValue("ref_target", argc,
                                                argn, argv);
  if (referrer_target_url) {
    referrer_target_url_ = referrer_target_url;
  }

  if (!base::strcasecmp(GetArgValue("name", argc, argn, argv),
                        "geturlredirectnotify")) {
    handle_url_redirects_ = true;
  }
  return PluginTest::New(mode, argc, argn, argv, saved);
}

NPError PluginGetURLTest::SetWindow(NPWindow* pNPWindow) {
  if (pNPWindow->window == NULL)
    return NPERR_NO_ERROR;

  if (!tests_started_) {
    tests_started_ = true;

    tests_in_progress_++;

    if (expect_404_response_) {
      HostFunctions()->geturl(id(), page_not_found_url_.c_str(), NULL);
      return NPERR_NO_ERROR;
    } else if (!fail_write_url_.empty()) {
      HostFunctions()->geturl(id(), fail_write_url_.c_str(), NULL);
      return NPERR_NO_ERROR;
    } else if (!referrer_target_url_.empty()) {
      HostFunctions()->pushpopupsenabledstate(id(), true);
      HostFunctions()->geturl(id(), referrer_target_url_.c_str(), "_blank");
      HostFunctions()->poppopupsenabledstate(id());
      return NPERR_NO_ERROR;
    } else if (handle_url_redirects_) {
      HostFunctions()->geturlnotify(
          id(), REDIRECT_SRC_URL, NULL,
          reinterpret_cast<void*>(REDIRECT_SRC_URL_NOTIFICATION_ID));
      return NPERR_NO_ERROR;
    }

    std::string url = SELF_URL;
    HostFunctions()->geturlnotify(id(), url.c_str(), NULL,
                                  reinterpret_cast<void*>(SELF_URL_STREAM_ID));

    tests_in_progress_++;
    std::string bogus_url = BOGUS_URL;
    HostFunctions()->geturlnotify(id(), bogus_url.c_str(), NULL,
                                  reinterpret_cast<void*>(BOGUS_URL_STREAM_ID));
  }
  return NPERR_NO_ERROR;
}

NPError PluginGetURLTest::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

  if (test_completed()) {
    return PluginTest::NewStream(type, stream, seekable, stype);
  }

  if (!referrer_target_url_.empty()) {
    return NPERR_NO_ERROR;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);

  if (expect_404_response_) {
    NPObject *window_obj = NULL;
    HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window_obj);
    if (!window_obj) {
      SetError("Failed to get NPObject for plugin instance2");
      SignalTestCompleted();
      return NPERR_NO_ERROR;
    }

    std::string script = "javascript:alert('Hi there from plugin');";
    NPString script_string;
    script_string.UTF8Characters = script.c_str();
    script_string.UTF8Length = static_cast<unsigned int>(script.length());
    NPVariant result_var;

    npn_evaluate_context_ = true;
    HostFunctions()->evaluate(id(), window_obj, &script_string, &result_var);
    npn_evaluate_context_ = false;
    return NPERR_NO_ERROR;
  }

  if (!fail_write_url_.empty()) {
    return NPERR_NO_ERROR;
  }


  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);

  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      break;
    case FETCHED_URL_STREAM_ID:
      {
        std::string filename = self_url_;
        if (filename.find("file:///", 0) != 0) {
          SetError("Test expects a file-url.");
          break;
        }

        // TODO(evanm): use the net:: functions to convert file:// URLs to
        // on-disk file paths.  But it probably doesn't actually matter in
        // this test.

#if defined(OS_WIN)
        filename = filename.substr(8);  // remove "file:///"
        // Assume an ASCII path on Windows.
        FilePath path = FilePath(ASCIIToWide(filename));
#else
        filename = filename.substr(7);  // remove "file://"
        FilePath path = FilePath(filename);
#endif

        test_file_ = file_util::OpenFile(path, "r");
        if (!test_file_) {
          SetError("Could not open source file");
        }
      }
      break;
    case BOGUS_URL_STREAM_ID:
      SetError("Unexpected NewStream for BOGUS_URL");
      break;
    case REDIRECT_SRC_URL_NOTIFICATION_ID:
      SetError("Should not redirect to URL when plugin denied it.");
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

int32 PluginGetURLTest::WriteReady(NPStream *stream) {
  if (test_completed()) {
    return PluginTest::WriteReady(stream);
  }

  if (!referrer_target_url_.empty()) {
    return STREAM_CHUNK;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);
  if (stream_id == BOGUS_URL_STREAM_ID)
    SetError("Received WriteReady for BOGUS_URL");

  return STREAM_CHUNK;
}

int32 PluginGetURLTest::Write(NPStream *stream, int32 offset, int32 len,
                              void *buffer) {
  if (test_completed()) {
    return PluginTest::Write(stream, offset, len, buffer);
  }

  if (!fail_write_url_.empty()) {
    SignalTestCompleted();
    return -1;
  }

  if (!referrer_target_url_.empty()) {
    return len;
  }

  if (stream == NULL) {
    SetError("Write got null stream");
    return -1;
  }
  if (len < 0 || len > STREAM_CHUNK) {
    SetError("Write got bogus stream chunk size");
    return -1;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        int32 bytes = fread(read_buffer, 1, len, test_file_);
        // Technically, fread could return fewer than len
        // bytes.  But this is not likely.
        if (bytes != len)
          SetError("Did not read correct bytelength from source file");
        if (memcmp(read_buffer, buffer, len))
          SetError("Content mismatch between data and source!");
      }
      break;
    case BOGUS_URL_STREAM_ID:
      SetError("Unexpected write callback for BOGUS_URL");
      break;
    default:
      SetError("Unexpected write callback");
      break;
  }
  // Pretend that we took all the data.
  return len;
}


NPError PluginGetURLTest::DestroyStream(NPStream *stream, NPError reason) {
  if (test_completed()) {
    return PluginTest::DestroyStream(stream, reason);
  }

  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);

  if (expect_404_response_) {
    if (npn_evaluate_context_) {
      SetError("Received destroyStream in the context of NPN_Evaluate.");
    }

    SignalTestCompleted();
    return NPERR_NO_ERROR;
  }

  if (!referrer_target_url_.empty()) {
    return NPERR_NO_ERROR;
  }

  unsigned long stream_id =
      reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        size_t bytes = fread(read_buffer, 1, sizeof(read_buffer), test_file_);
        if (bytes != 0)
          SetError("Data and source mismatch on length");
        file_util::CloseFile(test_file_);
      }
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

void PluginGetURLTest::StreamAsFile(NPStream* stream, const char* fname) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id =
      reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
}

void PluginGetURLTest::URLNotify(const char* url, NPReason reason, void* data) {
  if (!tests_in_progress_) {
    SetError("URLNotify received after tests completed");
    return;
  }

  if (!url) {
    SetError("URLNotify received NULL url");
    return;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(data), cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(data);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      if (strcmp(url, SELF_URL) != 0)
        SetError("URLNotify reported incorrect url for SELF_URL");

      // We have our stream url.  Go fetch it.
      HostFunctions()->geturlnotify(id(), self_url_.c_str(), NULL,
                            reinterpret_cast<void*>(FETCHED_URL_STREAM_ID));
      break;
    case FETCHED_URL_STREAM_ID:
      if (!url || strcmp(url, self_url_.c_str()) != 0)
        SetError("URLNotify reported incorrect url for FETCHED_URL");
      tests_in_progress_--;
      break;
    case BOGUS_URL_STREAM_ID:
      if (reason != NPRES_NETWORK_ERR) {
        std::string err = "BOGUS_URL received unexpected URLNotify status: ";
        err.append(base::IntToString(reason));
        SetError(err);
      }
      tests_in_progress_--;
      break;
    case REDIRECT_SRC_URL_NOTIFICATION_ID: {
      if (!received_url_redirect_notification_) {
        SetError("Failed to receive URLRedirect notification");
      }
      tests_in_progress_--;
      break;
    }
    default:
      SetError("Unexpected NewStream callback");
      break;
  }

  if (tests_in_progress_ == 0)
      SignalTestCompleted();
}

void PluginGetURLTest::URLRedirectNotify(const char* url,
                                         int32_t status,
                                         void* notify_data) {
  if (!base::strcasecmp(url, "http://mock.http/npapi/plugin_read_page.html")) {
    received_url_redirect_notification_ = true;
    // Disallow redirect notification.
    HostFunctions()->urlredirectresponse(id(), notify_data, false);
  }
}

} // namespace NPAPIClient
