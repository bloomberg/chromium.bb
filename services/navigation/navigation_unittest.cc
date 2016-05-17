// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_test.h"

namespace navigation {

class NavigationTest : public shell::test::ShellTest,
                       public mojom::ViewClient {
 public:
  NavigationTest()
      : shell::test::ShellTest("exe:navigation_unittests"),
        binding_(this) {}
  ~NavigationTest() override {}

 protected:
   void SetUp() override {
     shell::test::ShellTest::SetUp();
     window_manager_connection_ = connector()->Connect("mojo:test_wm");
   }

  mojom::ViewClientPtr GetViewClient() {
    return binding_.CreateInterfacePtrAndBind();
  }

  void QuitOnLoadingStateChange(base::RunLoop* loop) {
    loop_ = loop;
  }

 private:
  // mojom::ViewClient:
  void LoadingStateChanged(bool is_loading) override {
    // Should see loading start, then stop.
    if (++load_count_ == 2 && loop_)
      loop_->Quit();
  }
  void NavigationStateChanged(const GURL& url,
                              const mojo::String& title,
                              bool can_go_back,
                              bool can_go_forward) override {}
  void LoadProgressChanged(double progress) override {}
  void ViewCreated(mojom::ViewPtr,
                   mojom::ViewClientRequest,
                   bool,
                   mojo::RectPtr,
                   bool) override {}
  void Close() override {}

  int load_count_ = 0;
  mojo::Binding<mojom::ViewClient> binding_;
  base::RunLoop* loop_ = nullptr;
  std::unique_ptr<shell::Connection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(NavigationTest);
};

TEST_F(NavigationTest, Navigate) {
  mojom::ViewFactoryPtr view_factory;
  connector()->ConnectToInterface("exe:navigation", &view_factory);

  mojom::ViewPtr view;
  view_factory->CreateView(GetViewClient(), GetProxy(&view));
  view->NavigateTo(GURL("about:blank"));

  base::RunLoop loop;
  QuitOnLoadingStateChange(&loop);
  loop.Run();
}

}  // namespace navigation
