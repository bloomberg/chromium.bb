// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/simple_web_view_dialog.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using ::content::WebContents;
using ::views::GridLayout;

const int kLocationBarHeight = 35;

// Margin between screen edge and SimpleWebViewDialog border.
const int kExternalMargin = 60;

// Margin between WebView and SimpleWebViewDialog border.
const int kInnerMargin = 2;

const SkColor kDialogColor = SK_ColorWHITE;

class ToolbarRowView : public views::View {
 public:
  METADATA_HEADER(ToolbarRowView);
  ToolbarRowView() {
    SetBackground(views::CreateSolidBackground(kDialogColor));
  }

  ToolbarRowView(const ToolbarRowView&) = delete;
  ToolbarRowView& operator=(const ToolbarRowView&) = delete;
  ~ToolbarRowView() override = default;

  void Init(std::unique_ptr<views::View> back,
            std::unique_ptr<views::View> forward,
            std::unique_ptr<views::View> reload,
            std::unique_ptr<views::View> location_bar) {
    GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());

    const int related_horizontal_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
    // Back button.
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Forward button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Reload button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Location bar.
    column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                          GridLayout::ColumnSize::kFixed, kLocationBarHeight,
                          0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);

    layout->StartRow(0, 0);
    layout->AddView(std::move(back));
    layout->AddView(std::move(forward));
    layout->AddView(std::move(reload));
    layout->AddView(std::move(location_bar));
  }
};

BEGIN_METADATA(ToolbarRowView, views::View)
END_METADATA

}  // namespace

// Stub implementation of ContentSettingBubbleModelDelegate.
class StubBubbleModelDelegate : public ContentSettingBubbleModelDelegate {
 public:
  StubBubbleModelDelegate() = default;
  StubBubbleModelDelegate(const StubBubbleModelDelegate&) = delete;
  StubBubbleModelDelegate& operator=(const StubBubbleModelDelegate&) = delete;
  ~StubBubbleModelDelegate() override = default;

 private:
  // ContentSettingBubbleModelDelegate implementation:
  void ShowCollectedCookiesDialog(content::WebContents* web_contents) override {
  }
  void ShowContentSettingsPage(ContentSettingsType type) override {}
  void ShowMediaSettingsPage() override {}
  void ShowLearnMorePage(ContentSettingsType type) override {}
};

// SimpleWebViewDialog class ---------------------------------------------------

SimpleWebViewDialog::SimpleWebViewDialog(Profile* profile)
    : profile_(profile),
      bubble_model_delegate_(new StubBubbleModelDelegate) {
  command_updater_ = std::make_unique<CommandUpdaterImpl>(this);
  command_updater_->UpdateCommandEnabled(IDC_BACK, true);
  command_updater_->UpdateCommandEnabled(IDC_FORWARD, true);
  command_updater_->UpdateCommandEnabled(IDC_STOP, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, true);
}

SimpleWebViewDialog::~SimpleWebViewDialog() {
  for (auto& observer : observers_)
    observer.OnHostDestroying();

  if (web_view_ && web_view_->web_contents()) {
    web_view_->web_contents()->SetDelegate(nullptr);
    web_modal::WebContentsModalDialogManager::FromWebContents(
        web_view_->web_contents())
        ->SetDelegate(nullptr);
  }
}

void SimpleWebViewDialog::StartLoad(const GURL& url) {
  if (!web_view_container_)
    web_view_container_ = std::make_unique<views::WebView>(profile_);
  web_view_ = web_view_container_.get();
  web_view_->GetWebContents()->SetDelegate(this);
  web_view_->LoadInitialURL(url);

  WebContents* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);

  // Create the password manager that is needed for the proxy.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));

  // Set this as the web modal delegate so that web dialog can appear.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

void SimpleWebViewDialog::Init() {
  // Create the security state model that the location bar model needs.
  if (web_view_->GetWebContents())
    SecurityStateTabHelper::CreateForWebContents(web_view_->GetWebContents());
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      this, content::kMaxURLDisplayChars);

  SetBackground(views::CreateSolidBackground(kDialogColor));

  // Back/Forward buttons.
  auto back = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](CommandUpdater* updater) { updater->ExecuteCommand(IDC_BACK); },
      command_updater_.get()));
  back->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                 ui::EF_MIDDLE_MOUSE_BUTTON);
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
  back->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  back->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back->SetID(VIEW_ID_BACK_BUTTON);
  back_ = back.get();

  auto forward = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](CommandUpdater* updater) { updater->ExecuteCommand(IDC_FORWARD); },
      command_updater_.get()));
  forward->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  forward->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  forward->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward->SetID(VIEW_ID_FORWARD_BUTTON);
  forward_ = forward.get();

  // Location bar.
  auto location_bar = std::make_unique<LocationBarView>(
      nullptr, profile_, command_updater_.get(), this, true);
  location_bar_ = location_bar.get();

  // Reload button.
  auto reload = std::make_unique<ReloadButton>(command_updater_.get());
  reload->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                   ui::EF_MIDDLE_MOUSE_BUTTON);
  reload->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload->SetID(VIEW_ID_RELOAD_BUTTON);
  reload_ = reload.get();

  // Use separate view to setup custom background.
  auto toolbar_row = std::make_unique<ToolbarRowView>();
  toolbar_row->Init(std::move(back), std::move(forward), std::move(reload),
                    std::move(location_bar));
  // Add the views as child views before the grid layout is installed. This
  // ensures ownership is more clear.
  ToolbarRowView* toolbar_row_ptr = AddChildView(std::move(toolbar_row));
  // Transfer ownership of the `web_view_` from the `web_view_container_`
  // created in StartLoad() to `this`.
  AddChildView(std::move(web_view_container_));

  // Layout.
  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::ColumnSize::kFixed, 0, 0);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, kInnerMargin);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::ColumnSize::kFixed, 0, 0);
  column_set->AddPaddingColumn(0, kInnerMargin);

  // Setup layout rows.
  layout->StartRow(0, 0);
  layout->AddExistingView(toolbar_row_ptr);

  layout->AddPaddingRow(0, kInnerMargin);

  layout->StartRow(1, 1);
  layout->AddExistingView(web_view_);
  layout->AddPaddingRow(0, kInnerMargin);

  LoadImages();

  location_bar_->Init();
  UpdateReload(web_view_->web_contents()->IsLoading(), true);

  gfx::Rect bounds(CalculateScreenBounds(gfx::Size()));
  bounds.Inset(kExternalMargin, kExternalMargin);
  layout->set_minimum_size(bounds.size());

  Layout();
}

content::WebContents* SimpleWebViewDialog::OpenURL(
    const content::OpenURLParams& params) {
  // As there are no Browsers right now, this could not actually ever work.
  NOTIMPLEMENTED();
  return nullptr;
}

void SimpleWebViewDialog::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (location_bar_) {
    location_bar_->Update(nullptr);
    UpdateButtons();
  }
}

void SimpleWebViewDialog::LoadingStateChanged(WebContents* source,
                                              bool should_show_loading_ui) {
  bool is_loading = source->IsLoading();
  UpdateReload(is_loading && should_show_loading_ui, false);
  command_updater_->UpdateCommandEnabled(IDC_STOP, is_loading);
}

WebContents* SimpleWebViewDialog::GetWebContents() {
  return nullptr;
}

LocationBarModel* SimpleWebViewDialog::GetLocationBarModel() {
  return location_bar_model_.get();
}

const LocationBarModel* SimpleWebViewDialog::GetLocationBarModel() const {
  return location_bar_model_.get();
}

ContentSettingBubbleModelDelegate*
SimpleWebViewDialog::GetContentSettingBubbleModelDelegate() {
  return bubble_model_delegate_.get();
}

content::WebContents* SimpleWebViewDialog::GetActiveWebContents() const {
  return web_view_->web_contents();
}

void SimpleWebViewDialog::ExecuteCommandWithDisposition(int id,
                                                        WindowOpenDisposition) {
  WebContents* web_contents = web_view_->web_contents();
  switch (id) {
    case IDC_BACK:
      if (web_contents->GetController().CanGoBack()) {
        location_bar_->Revert();
        web_contents->GetController().GoBack();
      }
      break;
    case IDC_FORWARD:
      if (web_contents->GetController().CanGoForward()) {
        location_bar_->Revert();
        web_contents->GetController().GoForward();
      }
      break;
    case IDC_STOP:
      web_contents->Stop();
      break;
    case IDC_RELOAD:
    // Always reload bypassing cache.
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE:
      location_bar_->Revert();
      web_contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           true);
      break;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<views::WidgetDelegate>
SimpleWebViewDialog::MakeWidgetDelegate() {
  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetInitiallyFocusedView(web_view_);
  delegate->SetOwnedByWidget(true);
  return delegate;
}

void SimpleWebViewDialog::LoadImages() {
  const ui::ThemeProvider* tp = GetThemeProvider();

  back_->SetImage(views::Button::STATE_NORMAL, tp->GetImageSkiaNamed(IDR_BACK));
  back_->SetImage(views::Button::STATE_HOVERED,
                  tp->GetImageSkiaNamed(IDR_BACK_H));
  back_->SetImage(views::Button::STATE_PRESSED,
                  tp->GetImageSkiaNamed(IDR_BACK_P));
  back_->SetImage(views::Button::STATE_DISABLED,
                  tp->GetImageSkiaNamed(IDR_BACK_D));

  forward_->SetImage(views::Button::STATE_NORMAL,
                     tp->GetImageSkiaNamed(IDR_FORWARD));
  forward_->SetImage(views::Button::STATE_HOVERED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_H));
  forward_->SetImage(views::Button::STATE_PRESSED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_P));
  forward_->SetImage(views::Button::STATE_DISABLED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_D));
}

void SimpleWebViewDialog::UpdateButtons() {
  content::NavigationController& navigation_controller =
      web_view_->web_contents()->GetController();
  back_->SetEnabled(navigation_controller.CanGoBack());
  forward_->SetEnabled(navigation_controller.CanGoForward());
}

void SimpleWebViewDialog::UpdateReload(bool is_loading, bool force) {
  if (reload_) {
    reload_->ChangeMode(
        is_loading ? ReloadButton::Mode::kStop : ReloadButton::Mode::kReload,
        force);
  }
}

web_modal::WebContentsModalDialogHost*
SimpleWebViewDialog::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView SimpleWebViewDialog::GetHostView() const {
  return GetWidget()->GetNativeWindow();
}

gfx::Point SimpleWebViewDialog::GetDialogPosition(const gfx::Size& size) {
  const gfx::Size& host_size = this->size();
  return gfx::Point(host_size.width() / 2 - size.width() / 2,
                    host_size.height() / 2 - size.height() / 2);
}

gfx::Size SimpleWebViewDialog::GetMaximumDialogSize() {
  return size();
}

void SimpleWebViewDialog::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.AddObserver(observer);
}

void SimpleWebViewDialog::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(SimpleWebViewDialog, views::View)
END_METADATA

}  // namespace ash
