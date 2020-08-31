// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_platform_data_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "ui/wm/test/wm_test_helper.h"
#else  // !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace content {

struct ShellPlatformDelegate::ShellData {
  gfx::Size content_size;
  // Self-owned Widget, destroyed through CloseNow().
  views::Widget* window_widget = nullptr;
};

struct ShellPlatformDelegate::PlatformData {
#if defined(OS_CHROMEOS)
  std::unique_ptr<wm::WMTestHelper> wm_test_helper;
#else
  std::unique_ptr<wm::WMState> wm_state;
#endif

  // Only used in headless mode. Uses |wm_state| which must outlive this.
  std::unique_ptr<ShellPlatformDataAura> aura;

  // TODO(danakj): This looks unused?
  std::unique_ptr<views::ViewsDelegate> views_delegate;
};

namespace {

// Maintain the UI controls and web view for content shell
class ShellWindowDelegateView : public views::WidgetDelegateView,
                                public views::TextfieldController,
                                public views::ButtonListener {
 public:
  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  ShellWindowDelegateView(Shell* shell) : shell_(shell) {}

  ~ShellWindowDelegateView() override {}

  // Update the state of UI controls
  void SetAddressBarURL(const GURL& url) {
    url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
  }

  void SetWebContents(WebContents* web_contents, const gfx::Size& size) {
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    // If there was a previous WebView in this Shell it should be removed and
    // deleted.
    if (web_view_) {
      contents_view_->RemoveChildView(web_view_);
      delete web_view_;
    }
    auto web_view =
        std::make_unique<views::WebView>(web_contents->GetBrowserContext());
    web_view->SetWebContents(web_contents);
    web_view->SetPreferredSize(size);
    web_contents->Focus();
    web_view_ = contents_view_->AddChildView(std::move(web_view));
    Layout();

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);

    // Resizing a widget on chromeos doesn't automatically resize the root, need
    // to explicitly do that.
#if defined(OS_CHROMEOS)
    GetWidget()->GetNativeWindow()->GetHost()->SetBoundsInPixels(bounds);
#endif
  }

  void SetWindowTitle(const base::string16& title) { title_ = title; }
  void EnableUIControl(UIControl control, bool is_enabled) {
    if (control == BACK_BUTTON) {
      back_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
    } else if (control == FORWARD_BUTTON) {
      forward_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                           : views::Button::STATE_DISABLED);
    } else if (control == STOP_BUTTON) {
      stop_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
    }
  }

 private:
  // Initialize the UI control contained in shell window
  void InitShellWindow() {
    SetBackground(CreateThemedSolidBackground(
        this, ui::NativeTheme::kColorId_WindowBackground));

    auto contents_view = std::make_unique<views::View>();
    auto toolbar_view = std::make_unique<views::View>();

    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());

    using ColumnSize = views::GridLayout::ColumnSize;
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    if (!Shell::ShouldHideToolbar())
      column_set->AddPaddingColumn(0, 2);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          ColumnSize::kUsePreferred, 0, 0);
    if (!Shell::ShouldHideToolbar())
      column_set->AddPaddingColumn(0, 2);

    // Add toolbar buttons and URL text field
    if (!Shell::ShouldHideToolbar()) {
      layout->AddPaddingRow(0, 2);
      layout->StartRow(0, 0);
      views::GridLayout* toolbar_layout =
          toolbar_view->SetLayoutManager(std::make_unique<views::GridLayout>());

      views::ColumnSet* toolbar_column_set = toolbar_layout->AddColumnSet(0);
      // Back button
      auto back_button =
          views::MdTextButton::Create(this, base::ASCIIToUTF16("Back"));
      gfx::Size back_button_size = back_button->GetPreferredSize();
      toolbar_column_set->AddColumn(
          views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
          ColumnSize::kFixed, back_button_size.width(),
          back_button_size.width() / 2);
      // Forward button
      auto forward_button =
          views::MdTextButton::Create(this, base::ASCIIToUTF16("Forward"));
      gfx::Size forward_button_size = forward_button->GetPreferredSize();
      toolbar_column_set->AddColumn(
          views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
          ColumnSize::kFixed, forward_button_size.width(),
          forward_button_size.width() / 2);
      // Refresh button
      auto refresh_button =
          views::MdTextButton::Create(this, base::ASCIIToUTF16("Refresh"));
      gfx::Size refresh_button_size = refresh_button->GetPreferredSize();
      toolbar_column_set->AddColumn(
          views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
          ColumnSize::kFixed, refresh_button_size.width(),
          refresh_button_size.width() / 2);
      // Stop button
      auto stop_button =
          views::MdTextButton::Create(this, base::ASCIIToUTF16("Stop"));
      gfx::Size stop_button_size = stop_button->GetPreferredSize();
      toolbar_column_set->AddColumn(
          views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
          ColumnSize::kFixed, stop_button_size.width(),
          stop_button_size.width() / 2);
      toolbar_column_set->AddPaddingColumn(0, 2);
      // URL entry
      auto url_entry = std::make_unique<views::Textfield>();
      url_entry->SetAccessibleName(base::ASCIIToUTF16("Enter URL"));
      url_entry->set_controller(this);
      url_entry->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_URL);
      toolbar_column_set->AddColumn(views::GridLayout::FILL,
                                    views::GridLayout::FILL, 1,
                                    ColumnSize::kUsePreferred, 0, 0);
      toolbar_column_set->AddPaddingColumn(0, 2);

      // Fill up the first row
      toolbar_layout->StartRow(0, 0);
      back_button_ = toolbar_layout->AddView(std::move(back_button));
      forward_button_ = toolbar_layout->AddView(std::move(forward_button));
      refresh_button_ = toolbar_layout->AddView(std::move(refresh_button));
      stop_button_ = toolbar_layout->AddView(std::move(stop_button));
      url_entry_ = toolbar_layout->AddView(std::move(url_entry));

      toolbar_view_ = layout->AddView(std::move(toolbar_view));

      layout->AddPaddingRow(0, 5);
    }

    // Add web contents view as the second row
    {
      layout->StartRow(1, 0);
      contents_view_ = layout->AddView(std::move(contents_view));
    }

    if (!Shell::ShouldHideToolbar())
      layout->AddPaddingRow(0, 5);

    InitAccelerators();
  }
  void InitAccelerators() {
    static const ui::KeyboardCode keys[] = {ui::VKEY_F5, ui::VKEY_BROWSER_BACK,
                                            ui::VKEY_BROWSER_FORWARD};
    for (size_t i = 0; i < base::size(keys); ++i) {
      GetFocusManager()->RegisterAccelerator(
          ui::Accelerator(keys[i], ui::EF_NONE),
          ui::AcceleratorManager::kNormalPriority, this);
    }
  }
  // Overridden from TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {}
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::ET_KEY_PRESSED && sender == url_entry_ &&
        key_event.key_code() == ui::VKEY_RETURN) {
      std::string text = base::UTF16ToUTF8(url_entry_->GetText());
      GURL url(text);
      if (!url.has_scheme()) {
        url = GURL(std::string("http://") + std::string(text));
        url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
      }
      shell_->LoadURL(url);
      return true;
    }
    return false;
  }

  // Overridden from ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == back_button_)
      shell_->GoBackOrForward(-1);
    else if (sender == forward_button_)
      shell_->GoBackOrForward(1);
    else if (sender == refresh_button_)
      shell_->Reload();
    else if (sender == stop_button_)
      shell_->Stop();
  }

  // Overridden from WidgetDelegateView
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  base::string16 GetWindowTitle() const override { return title_; }
  void WindowClosing() override {
    if (shell_) {
      delete shell_;
      shell_ = nullptr;
    }
  }

  // Overridden from View
  gfx::Size GetMinimumSize() const override {
    // We want to be able to make the window smaller than its initial
    // (preferred) size.
    return gfx::Size();
  }
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      InitShellWindow();
    }
  }

  // Overridden from AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    switch (accelerator.key_code()) {
      case ui::VKEY_F5:
        shell_->Reload();
        return true;
      case ui::VKEY_BROWSER_BACK:
        shell_->GoBackOrForward(-1);
        return true;
      case ui::VKEY_BROWSER_FORWARD:
        shell_->GoBackOrForward(1);
        return true;
      default:
        return views::WidgetDelegateView::AcceleratorPressed(accelerator);
    }
  }

 private:
  // Hold a reference of Shell for deleting it when the window is closing
  Shell* shell_;

  // Window title
  base::string16 title_;

  // Toolbar view contains forward/backward/reload button and URL entry
  View* toolbar_view_ = nullptr;
  views::Button* back_button_ = nullptr;
  views::Button* forward_button_ = nullptr;
  views::Button* refresh_button_ = nullptr;
  views::Button* stop_button_ = nullptr;
  views::Textfield* url_entry_ = nullptr;

  // Contents view contains the web contents view
  View* contents_view_ = nullptr;
  views::WebView* web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowDelegateView);
};

}  // namespace

ShellPlatformDelegate::ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
#if defined(OS_WIN)
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif

  platform_ = std::make_unique<PlatformData>();

#if defined(OS_CHROMEOS)
  platform_->wm_test_helper =
      std::make_unique<wm::WMTestHelper>(default_window_size);
#else
  platform_->wm_state = std::make_unique<wm::WMState>();
  views::InstallDesktopScreenIfNecessary();
#endif

  platform_->views_delegate =
      std::make_unique<views::DesktopTestViewsDelegate>();
}

ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  shell_data.content_size = initial_size;

  if (shell->headless()) {
    if (!platform_->aura)
      platform_->aura = std::make_unique<ShellPlatformDataAura>(initial_size);
    else
      platform_->aura->ResizeWindow(initial_size);
    return;
  }

#if defined(OS_CHROMEOS)
  shell_data.window_widget = views::Widget::CreateWindowWithContext(
      new ShellWindowDelegateView(shell),
      platform_->wm_test_helper->GetDefaultParent(nullptr, gfx::Rect()),
      gfx::Rect(initial_size));
#else
  shell_data.window_widget = new views::Widget();
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(initial_size);
  params.delegate = new ShellWindowDelegateView(shell);
  params.wm_class_class = "chromium-content_shell";
  params.wm_class_name = params.wm_class_class;
  shell_data.window_widget->Init(std::move(params));
#endif

  // |window_widget| is made visible in PlatformSetContents(), so that the
  // platform-window size does not need to change due to layout again.
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return shell_data.window_widget->GetNativeWindow();
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell->headless()) {
    aura::Window* content = shell->web_contents()->GetNativeView();
    aura::Window* parent = platform_->aura->host()->window();
    if (!parent->Contains(content)) {
      parent->AddChild(content);
      // Move the cursor to a fixed position before tests run to avoid getting
      // an unpredictable result from mouse events.
      content->MoveCursorTo(gfx::Point());
      content->Show();
    }
    content->SetBounds(gfx::Rect(shell_data.content_size));
    RenderWidgetHostView* host_view =
        shell->web_contents()->GetRenderWidgetHostView();
    if (host_view)
      host_view->SetSize(shell_data.content_size);
  } else {
    views::WidgetDelegate* widget_delegate =
        shell_data.window_widget->widget_delegate();
    auto* delegate_view =
        static_cast<ShellWindowDelegateView*>(widget_delegate);
    delegate_view->SetWebContents(shell->web_contents(),
                                  shell_data.content_size);
    shell_data.window_widget->GetNativeWindow()->GetHost()->Show();
    shell_data.window_widget->Show();
  }
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->GetRenderWidgetHostView()->SetSize(content_size);
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  if (shell->headless() || Shell::ShouldHideToolbar())
    return;

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  auto* delegate_view = static_cast<ShellWindowDelegateView*>(
      shell_data.window_widget->widget_delegate());
  if (control == BACK_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::BACK_BUTTON,
                                   is_enabled);
  } else if (control == FORWARD_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::FORWARD_BUTTON,
                                   is_enabled);
  } else if (control == STOP_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::STOP_BUTTON,
                                   is_enabled);
  }
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  if (shell->headless() || Shell::ShouldHideToolbar())
    return;

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  auto* delegate_view = static_cast<ShellWindowDelegateView*>(
      shell_data.window_widget->widget_delegate());
  delegate_view->SetAddressBarURL(url);
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const base::string16& title) {
  if (shell->headless())
    return;

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  auto* delegate_view = static_cast<ShellWindowDelegateView*>(
      shell_data.window_widget->widget_delegate());
  delegate_view->SetWindowTitle(title);
  shell_data.window_widget->UpdateWindowTitle();
}

void ShellPlatformDelegate::RenderViewReady(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  if (shell->headless())
    return false;  // Shell destroys itself.

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  shell_data.window_widget->CloseNow();
  return true;  // The CloseNow() will do the destruction of Shell.
}

}  // namespace content
