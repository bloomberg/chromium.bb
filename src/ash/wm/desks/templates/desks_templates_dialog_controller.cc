// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

DesksTemplatesDialogController* g_instance = nullptr;

std::u16string GetStringWithQuotes(const std::u16string& str) {
  return u"\"" + str + u"\"";
}

}  // namespace

// The client view of the dialog. Contains a label which is a description, and
// optionally a couple images of unsupported apps. This dialog will block the
// entire system.
class DesksTemplatesDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DesksTemplatesDialog);

  DesksTemplatesDialog() {
    SetModalType(ui::MODAL_TYPE_SYSTEM);
    SetShowCloseButton(false);
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

    auto* layout_provider = views::LayoutProvider::Get();
    set_fixed_width(layout_provider->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    // Add the description for the dialog.
    AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&description_label_)
            .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL))
            .SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary))
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build());
  }
  DesksTemplatesDialog(const DesksTemplatesDialog&) = delete;
  DesksTemplatesDialog& operator=(const DesksTemplatesDialog&) = delete;
  ~DesksTemplatesDialog() override = default;

  void SetTitleText(int message_id) {
    SetTitle(l10n_util::GetStringUTF16(message_id));
  }

  void SetConfirmButtonText(int message_id) {
    SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(message_id));
  }

  void SetDescriptionText(const std::u16string& text) {
    DCHECK(description_label_);
    description_label_->SetText(text);
  }

  void SetDescriptionAccessibleName(const std::u16string& accessible_name) {
    DCHECK(description_label_);
    description_label_->SetAccessibleName(accessible_name);
  }

 private:
  views::Label* description_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesDialog,
                   views::DialogDelegateView)
VIEW_BUILDER_PROPERTY(int, TitleText)
VIEW_BUILDER_PROPERTY(int, ConfirmButtonText)
VIEW_BUILDER_PROPERTY(std::u16string, DescriptionText)
VIEW_BUILDER_PROPERTY(std::u16string, DescriptionAccessibleName)
END_VIEW_BUILDER

BEGIN_METADATA(DesksTemplatesDialog, views::DialogDelegateView)
END_METADATA

}  // namespace ash

// Must be in global namespace and defined before usage.
DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesDialog)

namespace ash {

//-----------------------------------------------------------------------------
// DesksTemplatesDialogController:

DesksTemplatesDialogController::DesksTemplatesDialogController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

DesksTemplatesDialogController::~DesksTemplatesDialogController() {
  if (dialog_widget_ && !dialog_widget_->IsClosed())
    dialog_widget_->CloseNow();

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
DesksTemplatesDialogController* DesksTemplatesDialogController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void DesksTemplatesDialogController::ShowUnsupportedAppsDialog(
    aura::Window* root_window,
    const std::vector<aura::Window*>& unsupported_apps,
    DesksController::GetDeskTemplateCallback callback,
    std::unique_ptr<DeskTemplate> desk_template) {
  // We can only bind `callback` once but the user has three possible paths
  // (accept, cancel, or close) so store `callback` and `desk_template` for
  // future usage once we know the user's decision.
  unsupported_apps_callback_ = std::move(callback);
  unsupported_apps_template_ = std::move(desk_template);

  DesksTemplatesIconContainer* icon_container = nullptr;
  auto dialog =
      views::Builder<DesksTemplatesDialog>()
          .SetTitleText(IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_TITLE)
          .SetConfirmButtonText(
              IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_CONFIRM_BUTTON)
          .SetDescriptionText(l10n_util::GetStringUTF16(
              IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_DESCRIPTION))
          .SetCancelCallback(
              base::BindOnce(&DesksTemplatesDialogController::
                                 OnUserCanceledUnsupportedAppsDialog,
                             weak_ptr_factory_.GetWeakPtr()))
          .SetCloseCallback(
              base::BindOnce(&DesksTemplatesDialogController::
                                 OnUserCanceledUnsupportedAppsDialog,
                             weak_ptr_factory_.GetWeakPtr()))
          .SetAcceptCallback(
              base::BindOnce(&DesksTemplatesDialogController::
                                 OnUserAcceptedUnsupportedAppsDialog,
                             weak_ptr_factory_.GetWeakPtr()))
          .AddChildren(
              views::Builder<views::Label>()
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                             gfx::Font::Weight::MEDIUM))
                  .SetEnabledColor(
                      AshColorProvider::Get()->GetContentLayerColor(
                          AshColorProvider::ContentLayerType::
                              kTextColorPrimary))
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_HEADER)),
              views::Builder<DesksTemplatesIconContainer>().CopyAddressTo(
                  &icon_container))
          .Build();
  icon_container->PopulateIconContainerFromWindows(unsupported_apps);
  CreateDialogWidget(std::move(dialog), root_window);
  RecordUnsupportedAppDialogShowHistogram();
}

void DesksTemplatesDialogController::ShowReplaceDialog(
    aura::Window* root_window,
    const std::u16string& template_name) {
  auto dialog =
      views::Builder<DesksTemplatesDialog>()
          .SetTitleText(IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_TITLE)
          .SetConfirmButtonText(
              IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_CONFIRM_BUTTON)
          .SetDescriptionText(l10n_util::GetStringFUTF16(
              IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_DESCRIPTION,
              GetStringWithQuotes(template_name)))
          .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
              IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_DESCRIPTION,
              template_name))
          .SetAcceptCallback(base::BindOnce(&RecordReplaceTemplateHistogram))
          .Build();
  CreateDialogWidget(std::move(dialog), root_window);
}

void DesksTemplatesDialogController::ShowDeleteDialog(
    aura::Window* root_window,
    const std::u16string& template_name,
    base::OnceClosure on_accept_callback) {
  auto dialog =
      views::Builder<DesksTemplatesDialog>()
          .SetTitleText(IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_TITLE)
          .SetButtonLabel(
              ui::DIALOG_BUTTON_OK,
              l10n_util::GetStringUTF16(
                  IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON))
          .SetDescriptionText(l10n_util::GetStringFUTF16(
              IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
              GetStringWithQuotes(template_name)))
          .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
              IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION, template_name))
          .SetAcceptCallback(std::move(on_accept_callback))
          .Build();

  CreateDialogWidget(std::move(dialog), root_window);
}

void DesksTemplatesDialogController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  for (auto& overview_grid :
       Shell::Get()->overview_controller()->overview_session()->grid_list()) {
    views::Widget* templates_grid_widget =
        overview_grid->desks_templates_grid_widget();
    if (templates_grid_widget) {
      auto* templates_grid_view = static_cast<DesksTemplatesGridView*>(
          templates_grid_widget->GetContentsView());
      for (DesksTemplatesItemView* template_item :
           templates_grid_view->grid_items()) {
        // Update the button visibility when a dialog is closed.
        template_item->UpdateHoverButtonsVisibility(
            aura::Env::GetInstance()->last_mouse_location(),
            /*is_touch=*/false);
      }
    }
  }
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
}

void DesksTemplatesDialogController::CreateDialogWidget(
    std::unique_ptr<DesksTemplatesDialog> dialog,
    aura::Window* root_window) {
  if (dialog_widget_)
    dialog_widget_->CloseNow();

  // The dialog will show on the display associated with `root_window`, and will
  // block all input since it is system modal.
  DCHECK(root_window->IsRootWindow());
  dialog_widget_ = views::DialogDelegate::CreateDialogWidget(
      std::move(dialog),
      /*context=*/root_window, /*parent=*/nullptr);
  dialog_widget_->Show();
  dialog_widget_observation_.Observe(dialog_widget_);
}

void DesksTemplatesDialogController::OnUserAcceptedUnsupportedAppsDialog() {
  DCHECK(!unsupported_apps_callback_.is_null());
  DCHECK(unsupported_apps_template_);
  std::move(unsupported_apps_callback_)
      .Run(std::move(unsupported_apps_template_));
}

void DesksTemplatesDialogController::OnUserCanceledUnsupportedAppsDialog() {
  DCHECK(!unsupported_apps_callback_.is_null());
  std::move(unsupported_apps_callback_).Run(nullptr);
  unsupported_apps_template_.reset();
}

}  // namespace ash
