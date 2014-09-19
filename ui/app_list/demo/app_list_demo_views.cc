// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views_content_client/views_content_client.h"

#if defined(OS_WIN)
#include "content/public/app/startup_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace {

class AppListDemoService;

// Number of dummy apps to populate in the app list.
const int kInitialItems = 20;

// Extends the test AppListViewDelegate to quit the run loop when the launcher
// window is closed, and to close the window if it is simply dismissed.
class DemoAppListViewDelegate : public app_list::test::AppListTestViewDelegate {
 public:
  explicit DemoAppListViewDelegate(content::BrowserContext* browser_context)
      : view_(NULL), browser_context_(browser_context) {}
  virtual ~DemoAppListViewDelegate() {}

  app_list::AppListView* InitView(gfx::NativeWindow window_context);

  // Overridden from AppListViewDelegate:
  virtual void Dismiss() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) OVERRIDE;

 private:
  app_list::AppListView* view_;  // Weak. Owns this.
  content::BrowserContext* browser_context_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(DemoAppListViewDelegate);
};

app_list::AppListView* DemoAppListViewDelegate::InitView(
    gfx::NativeWindow window_context) {
  gfx::NativeView container = NULL;
  // On Ash, the app list is placed into an aura::Window container. For the demo
  // use the root window context as the parent. This only works on Aura since an
  // aura::Window is also a NativeView.
#if defined(USE_AURA)
  container = window_context;
#endif

  view_ = new app_list::AppListView(this);
  view_->InitAsBubbleAtFixedLocation(container,
                                     0,
                                     gfx::Point(300, 300),
                                     views::BubbleBorder::FLOAT,
                                     false /* border_accepts_events */);

  // Populate some apps.
  GetTestModel()->PopulateApps(kInitialItems);
  app_list::AppListItemList* item_list = GetTestModel()->top_level_item_list();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Image test_image = rb.GetImageNamed(IDR_DEFAULT_FAVICON_32);
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    app_list::AppListItem* item = item_list->item_at(i);
    // Alternate images with shadows and images without.
    item->SetIcon(*test_image.ToImageSkia(), i & 1);
  }
  return view_;
}

void DemoAppListViewDelegate::Dismiss() {
  view_->GetWidget()->Close();
}

void DemoAppListViewDelegate::ViewClosing() {
  base::MessageLoop* message_loop = base::MessageLoopForUI::current();
  message_loop->DeleteSoon(FROM_HERE, this);
  message_loop->QuitWhenIdle();
}

views::View* DemoAppListViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(browser_context_)));
  web_contents_->GetController().LoadURL(GURL("http://www.google.com/"),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                         std::string());
  views::WebView* web_view = new views::WebView(
      web_contents_->GetBrowserContext());
  web_view->SetPreferredSize(size);
  web_view->SetWebContents(web_contents_.get());
  return web_view;
}

void ShowAppList(content::BrowserContext* browser_context,
                 gfx::NativeWindow window_context) {
  DemoAppListViewDelegate* delegate =
      new DemoAppListViewDelegate(browser_context);
  app_list::AppListView* view = delegate->InitView(window_context);
  view->GetWidget()->Show();
  view->GetWidget()->Activate();
}

}  // namespace

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ui::ViewsContentClient views_content_client(instance, &sandbox_info);
#else
int main(int argc, const char** argv) {
  ui::ViewsContentClient views_content_client(argc, argv);
#endif

  views_content_client.set_task(base::Bind(&ShowAppList));
  return views_content_client.RunMain();
}
