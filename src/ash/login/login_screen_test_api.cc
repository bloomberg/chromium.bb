// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_test_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

LoginShelfView* GetLoginShelfView() {
  if (!Shell::HasInstance())
    return nullptr;

  return Shelf::ForWindow(Shell::GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view();
}

bool IsLoginShelfViewButtonShown(int button_view_id) {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view)
    return false;

  views::View* button_view = shelf_view->GetViewByID(button_view_id);

  return button_view && button_view->GetVisible();
}

}  // anonymous namespace

class ShelfTestUiUpdateDelegate : public LoginShelfView::TestUiUpdateDelegate {
 public:
  using Callback = mojom::LoginScreenTestApi::WaitForUiUpdateCallback;

  // Returns instance owned by LoginShelfView. Installs instance of
  // ShelfTestUiUpdateDelegate when needed.
  static ShelfTestUiUpdateDelegate* Get(LoginShelfView* shelf) {
    if (!shelf->test_ui_update_delegate()) {
      shelf->InstallTestUiUpdateDelegate(
          std::make_unique<ShelfTestUiUpdateDelegate>());
    }
    return static_cast<ShelfTestUiUpdateDelegate*>(
        shelf->test_ui_update_delegate());
  }

  ShelfTestUiUpdateDelegate() = default;
  ~ShelfTestUiUpdateDelegate() override {
    for (PendingCallback& entry : heap_)
      std::move(entry.callback).Run(false);
  }

  // Returns UI update count.
  int64_t ui_update_count() const { return ui_update_count_; }

  // Add a callback to be invoked when ui update count is greater than
  // |previous_update_count|. Note |callback| could be invoked synchronously
  // when the current ui update count is already greater than
  // |previous_update_count|.
  void AddCallback(int64_t previous_update_count, Callback callback) {
    if (previous_update_count < ui_update_count_) {
      std::move(callback).Run(true);
    } else {
      heap_.emplace_back(previous_update_count, std::move(callback));
      std::push_heap(heap_.begin(), heap_.end());
    }
  }

  // LoginShelfView::TestUiUpdateDelegate
  void OnUiUpdate() override {
    ++ui_update_count_;
    while (!heap_.empty() && heap_.front().old_count < ui_update_count_) {
      std::move(heap_.front().callback).Run(true);
      std::pop_heap(heap_.begin(), heap_.end());
      heap_.pop_back();
    }
  }

 private:
  struct PendingCallback {
    PendingCallback(int64_t old_count, Callback callback)
        : old_count(old_count), callback(std::move(callback)) {}

    bool operator<(const PendingCallback& right) const {
      // We need min_heap, therefore this returns true when another element on
      // the right is less than this count. (regular heap is max_heap).
      return old_count > right.old_count;
    }

    int64_t old_count = 0;
    Callback callback;
  };

  std::vector<PendingCallback> heap_;

  int64_t ui_update_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfTestUiUpdateDelegate);
};

// static
void LoginScreenTestApi::BindRequest(mojom::LoginScreenTestApiRequest request) {
  mojo::MakeStrongBinding(std::make_unique<LoginScreenTestApi>(),
                          std::move(request));
}

LoginScreenTestApi::LoginScreenTestApi() = default;

LoginScreenTestApi::~LoginScreenTestApi() = default;

void LoginScreenTestApi::IsLockShown(IsLockShownCallback callback) {
  std::move(callback).Run(
      LockScreen::HasInstance() && LockScreen::Get()->is_shown() &&
      LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLock);
}

void LoginScreenTestApi::IsLoginShelfShown(IsLoginShelfShownCallback callback) {
  LoginShelfView* view = GetLoginShelfView();
  std::move(callback).Run(view && view->GetVisible());
}

void LoginScreenTestApi::IsRestartButtonShown(
    IsRestartButtonShownCallback callback) {
  std::move(callback).Run(
      IsLoginShelfViewButtonShown(LoginShelfView::kRestart));
}

void LoginScreenTestApi::IsShutdownButtonShown(
    IsShutdownButtonShownCallback callback) {
  std::move(callback).Run(
      IsLoginShelfViewButtonShown(LoginShelfView::kShutdown));
}

void LoginScreenTestApi::IsAuthErrorBubbleShown(
    IsAuthErrorBubbleShownCallback callback) {
  ash::LockScreen::TestApi lock_screen_test(ash::LockScreen::Get());
  ash::LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  std::move(callback).Run(lock_contents_test.auth_error_bubble()->GetVisible());
}

void LoginScreenTestApi::IsGuestButtonShown(
    IsGuestButtonShownCallback callback) {
  std::move(callback).Run(
      IsLoginShelfViewButtonShown(LoginShelfView::kBrowseAsGuest));
}

void LoginScreenTestApi::IsAddUserButtonShown(
    IsAddUserButtonShownCallback callback) {
  std::move(callback).Run(
      IsLoginShelfViewButtonShown(LoginShelfView::kAddUser));
}

void LoginScreenTestApi::SubmitPassword(const AccountId& account_id,
                                        const std::string& password,
                                        SubmitPasswordCallback callback) {
  // It'd be better to generate keyevents dynamically and dispatch them instead
  // of reaching into the views structure, but at the time of writing I could
  // not find a good way to do this. If you know of a way feel free to change
  // this code.
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  LoginAuthUserView::TestApi auth_test(
      lock_contents_test.primary_big_view()->auth_user());
  LoginPasswordView::TestApi password_test(auth_test.password_view());

  // For the time being, only the primary user is supported. To support multiple
  // users this API needs to search all user views for the associated user and
  // potentially activate that user so it is showing its password field.
  CHECK_EQ(account_id,
           auth_test.user_view()->current_user().basic_user_info.account_id);

  password_test.SubmitPassword(password);

  std::move(callback).Run();
}

void LoginScreenTestApi::GetUiUpdateCount(GetUiUpdateCountCallback callback) {
  LoginShelfView* view = GetLoginShelfView();

  std::move(callback).Run(
      view ? ShelfTestUiUpdateDelegate::Get(view)->ui_update_count() : 0);
}

void LoginScreenTestApi::LaunchApp(const std::string& app_id,
                                   LaunchAppCallback callback) {
  LoginShelfView* view = GetLoginShelfView();

  std::move(callback).Run(view && view->LaunchAppForTesting(app_id));
}

void LoginScreenTestApi::ClickAddUserButton(
    ClickAddUserButtonCallback callback) {
  LoginShelfView* view = GetLoginShelfView();

  std::move(callback).Run(view && view->SimulateAddUserButtonForTesting());
}

void LoginScreenTestApi::ClickGuestButton(ClickGuestButtonCallback callback) {
  LoginShelfView* view = GetLoginShelfView();

  std::move(callback).Run(view && view->SimulateGuestButtonForTesting());
}

void LoginScreenTestApi::WaitForUiUpdate(int64_t previous_update_count,
                                         WaitForUiUpdateCallback callback) {
  LoginShelfView* view = GetLoginShelfView();
  if (view) {
    ShelfTestUiUpdateDelegate::Get(view)->AddCallback(previous_update_count,
                                                      std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace ash
