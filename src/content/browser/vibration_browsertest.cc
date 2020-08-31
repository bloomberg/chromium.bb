// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/browser_interface_binders.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"

namespace content {

namespace {

class VibrationTest : public ContentBrowserTest,
                      public device::mojom::VibrationManager {
 public:
  VibrationTest() {
    OverrideVibrationManagerBinderForTesting(base::BindRepeating(
        &VibrationTest::BindVibrationManager, base::Unretained(this)));
  }

  ~VibrationTest() override {
    OverrideVibrationManagerBinderForTesting(base::NullCallback());
  }

  void BindVibrationManager(
      mojo::PendingReceiver<device::mojom::VibrationManager> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 protected:
  bool TriggerVibrate(int duration, base::OnceClosure vibrate_done) {
    vibrate_done_ = std::move(vibrate_done);

    bool result;
    RenderFrameHost* frame = shell()->web_contents()->GetMainFrame();
    std::string script = "domAutomationController.send(navigator.vibrate(" +
                         base::NumberToString(duration) + "))";
    EXPECT_TRUE(ExecuteScriptAndExtractBool(frame, script, &result));
    return result;
  }

  int64_t vibrate_milliseconds() { return vibrate_milliseconds_; }

 private:
  // device::mojom::VibrationManager:
  void Vibrate(int64_t milliseconds, VibrateCallback callback) override {
    vibrate_milliseconds_ = milliseconds;
    std::move(callback).Run();
    std::move(vibrate_done_).Run();
  }
  void Cancel(CancelCallback callback) override { std::move(callback).Run(); }

  int64_t vibrate_milliseconds_ = -1;
  base::OnceClosure vibrate_done_;
  mojo::Receiver<device::mojom::VibrationManager> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(VibrationTest);
};

IN_PROC_BROWSER_TEST_F(VibrationTest, Vibrate) {
  ASSERT_EQ(-1, vibrate_milliseconds());

  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  base::RunLoop run_loop;
  ASSERT_TRUE(TriggerVibrate(1234, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1234, vibrate_milliseconds());
}

}  //  namespace

}  //  namespace content
