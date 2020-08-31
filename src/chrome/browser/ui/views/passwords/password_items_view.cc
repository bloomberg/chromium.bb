// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_items_view.h"

#include <numeric>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace {

constexpr int kDeleteButtonTag = 1;
constexpr int kUndoButtonTag = 2;

// Column set identifiers for displaying or undoing removal of credentials.
// They both allocate space differently.
enum PasswordItemsViewColumnSetType { PASSWORD_COLUMN_SET, UNDO_COLUMN_SET };

void BuildColumnSet(views::GridLayout* layout,
                    PasswordItemsViewColumnSetType type_id) {
  DCHECK(!layout->GetColumnSet(type_id));
  views::ColumnSet* column_set = layout->AddColumnSet(type_id);
  // Passwords are split 60/40 (6:4) as the username is more important
  // than obscured password digits. Otherwise two columns are 50/50 (1:1).
  constexpr float kFirstColumnWeight = 60.0f;
  constexpr float kSecondColumnWeight = 40.0f;
  const int between_column_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        kFirstColumnWeight,
                        views::GridLayout::ColumnSize::kFixed, 0, 0);

  if (type_id == PASSWORD_COLUMN_SET) {
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 between_column_padding);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          kSecondColumnWeight,
                          views::GridLayout::ColumnSize::kFixed, 0, 0);
  }
  // All rows end with a trailing column for the undo/trash button.
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               between_column_padding);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

void StartRow(views::GridLayout* layout,
              PasswordItemsViewColumnSetType type_id) {
  if (!layout->GetColumnSet(type_id))
    BuildColumnSet(layout, type_id);
  layout->StartRow(views::GridLayout::kFixedSize, type_id);
}

std::unique_ptr<views::ImageButton> CreateDeleteButton(
    views::ButtonListener* listener,
    const base::string16& username) {
  std::unique_ptr<views::ImageButton> button(
      views::CreateVectorImageButtonWithNativeTheme(listener, kTrashCanIcon));
  button->SetFocusForPlatform();
  button->SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_DELETE, username));
  button->set_tag(kDeleteButtonTag);
  return button;
}

std::unique_ptr<views::LabelButton> CreateUndoButton(
    views::ButtonListener* listener,
    const base::string16& username) {
  auto undo_button = views::MdTextButton::Create(
      listener, l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO));
  undo_button->set_tag(kUndoButtonTag);
  undo_button->SetFocusForPlatform();
  undo_button->SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_UNDO_TOOLTIP, username));
  return undo_button;
}

std::unique_ptr<views::View> CreateManageButton(
    views::ButtonListener* listener) {
  return views::MdTextButton::Create(
      listener,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON));
}

}  // namespace

std::unique_ptr<views::Label> CreateUsernameLabel(
    const autofill::PasswordForm& form) {
  auto label = std::make_unique<views::Label>(GetDisplayUsername(form),
                                              CONTEXT_BODY_TEXT_LARGE,
                                              views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

std::unique_ptr<views::Label> CreatePasswordLabel(
    const autofill::PasswordForm& form,
    int federation_message_id,
    bool are_passwords_revealed) {
  base::string16 text =
      form.federation_origin.opaque()
          ? form.password_value
          : l10n_util::GetStringFUTF16(federation_message_id,
                                       GetDisplayFederation(form));
  int text_style = form.federation_origin.opaque()
                       ? STYLE_SECONDARY_MONOSPACED
                       : views::style::STYLE_SECONDARY;
  auto label =
      std::make_unique<views::Label>(text, CONTEXT_BODY_TEXT_LARGE, text_style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  if (form.federation_origin.opaque() && !are_passwords_revealed)
    label->SetObscured(true);
  if (!form.federation_origin.opaque())
    label->SetElideBehavior(gfx::ELIDE_HEAD);

  return label;
}

// An entry for each credential. Relays delete/undo actions associated with
// this password row to parent dialog.
class PasswordItemsView::PasswordRow : public views::ButtonListener {
 public:
  PasswordRow(PasswordItemsView* parent,
              const autofill::PasswordForm* password_form);

  void AddToLayout(views::GridLayout* layout);

 private:
  void AddUndoRow(views::GridLayout* layout);
  void AddPasswordRow(views::GridLayout* layout);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  PasswordItemsView* const parent_;
  const autofill::PasswordForm* const password_form_;
  bool deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordRow);
};

PasswordItemsView::PasswordRow::PasswordRow(
    PasswordItemsView* parent,
    const autofill::PasswordForm* password_form)
    : parent_(parent), password_form_(password_form) {}

void PasswordItemsView::PasswordRow::AddToLayout(views::GridLayout* layout) {
  if (deleted_) {
    AddUndoRow(layout);
  } else {
    AddPasswordRow(layout);
  }
}

void PasswordItemsView::PasswordRow::AddUndoRow(views::GridLayout* layout) {
  std::unique_ptr<views::Label> text = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED),
      CONTEXT_BODY_TEXT_LARGE);
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  std::unique_ptr<views::LabelButton> undo_button =
      CreateUndoButton(this, GetDisplayUsername(*password_form_));

  StartRow(layout, UNDO_COLUMN_SET);
  layout->AddView(std::move(text));
  layout->AddView(std::move(undo_button));
}

void PasswordItemsView::PasswordRow::AddPasswordRow(views::GridLayout* layout) {
  std::unique_ptr<views::Label> username_label =
      CreateUsernameLabel(*password_form_);
  std::unique_ptr<views::Label> password_label =
      CreatePasswordLabel(*password_form_, IDS_PASSWORDS_VIA_FEDERATION, false);
  std::unique_ptr<views::ImageButton> delete_button =
      CreateDeleteButton(this, GetDisplayUsername(*password_form_));
  StartRow(layout, PASSWORD_COLUMN_SET);
  layout->AddView(std::move(username_label));
  layout->AddView(std::move(password_label));
  layout->AddView(std::move(delete_button));
}

void PasswordItemsView::PasswordRow::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK(sender->tag() == kDeleteButtonTag || sender->tag() == kUndoButtonTag);
  deleted_ = sender->tag() == kDeleteButtonTag;
  parent_->NotifyPasswordFormAction(
      *password_form_,
      deleted_ ? PasswordBubbleControllerBase::PasswordAction::kRemovePassword
               : PasswordBubbleControllerBase::PasswordAction::kAddPassword);
}

PasswordItemsView::PasswordItemsView(content::WebContents* web_contents,
                                     views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetExtraView(CreateManageButton(this));

  if (controller_.local_credentials().empty()) {
    // A LayoutManager is required for GetHeightForWidth() even without content.
    SetLayoutManager(std::make_unique<views::FillLayout>());
  } else {
    for (auto& password_form : controller_.local_credentials()) {
      password_rows_.push_back(
          std::make_unique<PasswordRow>(this, &password_form));
    }

    RecreateLayout();
  }
}

PasswordItemsView::~PasswordItemsView() = default;

PasswordBubbleControllerBase* PasswordItemsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordItemsView::GetController() const {
  return &controller_;
}

void PasswordItemsView::RecreateLayout() {
  // This method should only be used when we have password rows, otherwise the
  // dialog should only show the no-passwords title and doesn't need to be
  // recreated.
  DCHECK(!controller_.local_credentials().empty());

  RemoveAllChildViews(true);

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);
  bool first_row = true;
  for (auto& row : password_rows_) {
    if (!first_row)
      grid_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 vertical_padding);

    row->AddToLayout(grid_layout);
    first_row = false;
  }

  PreferredSizeChanged();
  if (GetBubbleFrameView())
    SizeToContents();
}

void PasswordItemsView::NotifyPasswordFormAction(
    const autofill::PasswordForm& password_form,
    PasswordBubbleControllerBase::PasswordAction action) {
  RecreateLayout();
  // After the view is consistent, notify the model that the password needs to
  // be updated (either removed or put back into the store, as appropriate.
  controller_.OnPasswordAction(password_form, action);
}

bool PasswordItemsView::ShouldShowCloseButton() const {
  return true;
}

gfx::Size PasswordItemsView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void PasswordItemsView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  controller_.OnManageClicked(
      password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);
  CloseBubble();
}
