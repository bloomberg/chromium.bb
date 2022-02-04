// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

content::WebContents* GetWebContents(Browser* browser, int tab) {
  return browser->tab_strip_model()->GetWebContentsAt(tab);
}

content::DesktopMediaID GetDesktopMediaIDForScreen() {
  return content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                 content::DesktopMediaID::kNullId);
}

content::DesktopMediaID GetDesktopMediaIDForTab(Browser* browser, int tab) {
  content::RenderFrameHost* main_frame =
      GetWebContents(browser, tab)->GetMainFrame();
  return content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID()));
}

infobars::ContentInfoBarManager* GetInfoBarManager(Browser* browser, int tab) {
  return infobars::ContentInfoBarManager::FromWebContents(
      GetWebContents(browser, tab));
}

infobars::ContentInfoBarManager* GetInfoBarManager(
    content::WebContents* contents) {
  return infobars::ContentInfoBarManager::FromWebContents(contents);
}

ConfirmInfoBarDelegate* GetDelegate(Browser* browser, int tab) {
  return static_cast<ConfirmInfoBarDelegate*>(
      GetInfoBarManager(browser, tab)->infobar_at(0)->delegate());
}

class InfobarUIChangeObserver : public TabStripModelObserver {
 public:
  explicit InfobarUIChangeObserver(Browser* browser) : browser_{browser} {
    for (int tab = 0; tab < browser_->tab_strip_model()->count(); ++tab) {
      auto* contents = browser_->tab_strip_model()->GetWebContentsAt(tab);
      observers_[contents] =
          std::make_unique<InfoBarChangeObserver>(base::BindOnce(
              &InfobarUIChangeObserver::EraseObserver, base::Unretained(this)));
      GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
    }
    browser_->tab_strip_model()->AddObserver(this);
  }

  ~InfobarUIChangeObserver() override {
    for (auto& observer_iter : observers_) {
      auto* contents = observer_iter.first;
      auto* observer = observer_iter.second.get();

      GetInfoBarManager(contents)->RemoveObserver(observer);
    }
    browser_->tab_strip_model()->RemoveObserver(this);
    observers_.clear();
  }

  void ExpectCalls(size_t expected_changes) {
    run_loop_ = std::make_unique<base::RunLoop>();
    barrier_closure_ =
        base::BarrierClosure(expected_changes, run_loop_->QuitClosure());
    for (auto& observer : observers_) {
      observer.second->SetCallback(barrier_closure_);
    }
  }

  void Wait() { run_loop_->Run(); }

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents_with_index : change.GetInsert()->contents) {
        auto* contents = contents_with_index.contents;
        if (observers_.find(contents) == observers_.end()) {
          observers_[contents] = std::make_unique<InfoBarChangeObserver>(
              base::BindOnce(&InfobarUIChangeObserver::EraseObserver,
                             base::Unretained(this)));
          GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
          if (!barrier_closure_.is_null()) {
            observers_[contents]->SetCallback(barrier_closure_);
          }
        }
      }
    }
  }
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    if (observers_.find(contents) == observers_.end()) {
      observers_[contents] =
          std::make_unique<InfoBarChangeObserver>(base::BindOnce(
              &InfobarUIChangeObserver::EraseObserver, base::Unretained(this)));
      GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
      if (!barrier_closure_.is_null()) {
        observers_[contents]->SetCallback(barrier_closure_);
      }
    }
  }

 private:
  class InfoBarChangeObserver;

 public:
  void EraseObserver(InfoBarChangeObserver* observer) {
    auto iter = std::find_if(observers_.begin(), observers_.end(),
                             [observer](const auto& observer_iter) {
                               return observer_iter.second.get() == observer;
                             });
    observers_.erase(iter);
  }

 private:
  class InfoBarChangeObserver : public infobars::InfoBarManager::Observer {
   public:
    using ShutdownCallback = base::OnceCallback<void(InfoBarChangeObserver*)>;

    explicit InfoBarChangeObserver(ShutdownCallback shutdown_callback)
        : shutdown_callback_{std::move(shutdown_callback)} {}

    ~InfoBarChangeObserver() override = default;

    void SetCallback(base::RepeatingClosure change_closure) {
      DCHECK(!change_closure.is_null());
      change_closure_ = change_closure;
    }

    void OnInfoBarAdded(infobars::InfoBar* infobar) override {
      DCHECK(!change_closure_.is_null());
      change_closure_.Run();
    }

    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
      DCHECK(!change_closure_.is_null());
      change_closure_.Run();
    }

    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override {
      NOTREACHED();
    }

    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
      manager->RemoveObserver(this);
      DCHECK(!shutdown_callback_.is_null());
      std::move(shutdown_callback_).Run(this);
    }

   private:
    base::RepeatingClosure change_closure_;
    ShutdownCallback shutdown_callback_;
  };

  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<content::WebContents*, std::unique_ptr<InfoBarChangeObserver>>
      observers_;
  Browser* browser_;
  base::RepeatingClosure barrier_closure_;
};

}  // namespace

// Top-level integration test for WebRTC. Uses an actual desktop capture
// extension to capture the whole screen or a tab.
class WebRtcDesktopCaptureBrowserTest : public WebRtcTestBase {
 public:
  using MediaIDCallback = base::OnceCallback<content::DesktopMediaID()>;

  WebRtcDesktopCaptureBrowserTest() {
    extensions::DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Flags use to automatically select the right dekstop source and get
    // around security restrictions.
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
    command_line->AppendSwitch(switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  void InitializeTabSharingForFirstTab(MediaIDCallback media_id_callback,
                                       InfobarUIChangeObserver* observer) {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoadDesktopCaptureExtension();
    auto* first_tab = OpenTestPageInNewTab(kMainWebrtcTestHtmlPage);
    OpenTestPageInNewTab(kMainWebrtcTestHtmlPage);

    FakeDesktopMediaPickerFactory::TestFlags test_flags{
        .expect_screens = true,
        .expect_windows = true,
        .expect_tabs = true,
        .selected_source = std::move(media_id_callback).Run(),
    };
    picker_factory_.SetTestFlags(&test_flags, /*tests_count=*/1);

    std::string stream_id = GetDesktopMediaStream(first_tab);
    EXPECT_NE(stream_id, "");

    LOG(INFO) << "Opened desktop media stream, got id " << stream_id;

    const std::string constraints =
        "{audio: false, video: {mandatory: {chromeMediaSource: 'desktop',"
        "chromeMediaSourceId: '" +
        stream_id + "'}}}";

    // Should create 3 infobars if a tab (webcontents) is shared!
    if (observer)
      observer->ExpectCalls(3);

    EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
        first_tab, constraints));

    if (observer)
      observer->Wait();
  }

  void DetectVideoAndHangUp(content::WebContents* first_tab,
                            content::WebContents* second_tab) {
    StartDetectingVideo(first_tab, "remote-view");
    StartDetectingVideo(second_tab, "remote-view");
#if !BUILDFLAG(IS_MAC)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(first_tab);
    WaitForVideoToPlay(second_tab);
#endif
    HangUp(first_tab);
    HangUp(second_tab);
  }

  void RunP2PScreenshareWhileSharing(MediaIDCallback media_id_callback) {
    InitializeTabSharingForFirstTab(std::move(media_id_callback), nullptr);
    auto* first_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
    auto* second_tab = browser()->tab_strip_model()->GetWebContentsAt(2);
    GetUserMediaAndAccept(second_tab);

    SetupPeerconnectionWithLocalStream(first_tab);
    SetupPeerconnectionWithLocalStream(second_tab);
    NegotiateCall(first_tab, second_tab);
    VerifyStatsGeneratedCallback(second_tab);
    DetectVideoAndHangUp(first_tab, second_tab);
  }

  FakeDesktopMediaPickerFactory picker_factory_;
};

// TODO(crbug.com/796889): Enable on Mac when thread check crash is fixed.
// TODO(sprang): Figure out why test times out on Win 10 and ChromeOS.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
// TODO(crbug.com/1225911): Test is flaky on Linux.
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       DISABLED_RunP2PScreenshareWhileSharingScreen) {
  RunP2PScreenshareWhileSharing(base::BindOnce(GetDesktopMediaIDForScreen));
}

// TODO(crbug.com/1282292): Test is flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_RunP2PScreenshareWhileSharingTab \
  DISABLED_RunP2PScreenshareWhileSharingTab
#else
#define MAYBE_RunP2PScreenshareWhileSharingTab RunP2PScreenshareWhileSharingTab
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       MAYBE_RunP2PScreenshareWhileSharingTab) {
  RunP2PScreenshareWhileSharing(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2));
}

IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       SwitchSharedTabBackAndForth) {
  InfobarUIChangeObserver observer(browser());

  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2),
      &observer);

  // Should delete 3 infobars and create 3 new!
  observer.ExpectCalls(6);
  // Switch shared tab from 2 to 0.
  GetDelegate(browser(), 0)->Cancel();
  observer.Wait();

  // Should delete 3 infobars and create 3 new!
  observer.ExpectCalls(6);
  // Switch shared tab from 0 to 2.
  GetDelegate(browser(), 2)->Cancel();
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       CloseAndReopenNonSharedTab) {
  InfobarUIChangeObserver observer(browser());

  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2),
      &observer);

  // Should delete 1 infobar.
  observer.ExpectCalls(1);
  // Close non-shared and non-sharing, i.e., unrelated tab
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  observer.Wait();

  // Should create 1 infobar.
  observer.ExpectCalls(1);
  // Restore tab
  chrome::RestoreTab(browser());
  observer.Wait();
}
