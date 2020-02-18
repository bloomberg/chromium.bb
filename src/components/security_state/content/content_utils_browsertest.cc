// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "components/security_state/core/insecure_input_event_data.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using content::NavigateToURL;
using security_state::GetVisibleSecurityState;
using security_state::InsecureInputEventData;
using security_state::SSLStatusInputEventData;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("components/security_state/content/testdata");

class SecurityStateContentUtilsBrowserTest
    : public content::ContentBrowserTest {
 public:
  SecurityStateContentUtilsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateContentUtilsBrowserTest);
};

#if defined(OS_WIN)
#define MAYBE_VisibleSecurityStateInsecureFieldEdit \
  DISABLED_VisibleSecurityStateInsecureFieldEdit
#else
#define MAYBE_VisibleSecurityStateInsecureFieldEdit \
  VisibleSecurityStateInsecureFieldEdit
#endif
// Tests that the flags for nonsecure editing are set correctly.
IN_PROC_BROWSER_TEST_F(SecurityStateContentUtilsBrowserTest,
                       MAYBE_VisibleSecurityStateInsecureFieldEdit) {
  ASSERT_TRUE(https_server_.Start());
  EXPECT_TRUE(NavigateToURL(shell(), https_server_.GetURL("/hello.html")));

  content::WebContents* contents = shell()->web_contents();
  ASSERT_TRUE(contents);

  // First, ensure the flag is not set prematurely.
  content::SSLStatus& ssl_status =
      contents->GetController().GetVisibleEntry()->GetSSL();
  SSLStatusInputEventData* ssl_status_input_events =
      static_cast<SSLStatusInputEventData*>(ssl_status.user_data.get());
  InsecureInputEventData events;
  if (ssl_status_input_events)
    events = *ssl_status_input_events->input_events();
  EXPECT_FALSE(events.insecure_field_edited);

  std::unique_ptr<security_state::VisibleSecurityState> visible_state =
      GetVisibleSecurityState(contents);
  EXPECT_FALSE(visible_state->insecure_input_events.insecure_field_edited);

  // Simulate a field edit and update the SSLStatus' |user_data|.
  events.insecure_field_edited = true;
  ssl_status.user_data =
      std::make_unique<security_state::SSLStatusInputEventData>(events);

  // Verify the field edit was recorded properly in the |user_data|.
  ssl_status_input_events =
      static_cast<SSLStatusInputEventData*>(ssl_status.user_data.get());
  EXPECT_TRUE(ssl_status_input_events->input_events()->insecure_field_edited);
  // Verify the field edit was propagated to the VisibleSecurityState.
  visible_state = GetVisibleSecurityState(contents);
  EXPECT_TRUE(visible_state->insecure_input_events.insecure_field_edited);
}

}  // namespace
