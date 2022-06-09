// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/password_items_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

int ComboboxIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::style::GetLineHeight(views::style::CONTEXT_MENU,
                                     views::style::STYLE_PRIMARY);
}

std::unique_ptr<views::View> CreateRow() {
  auto row = std::make_unique<views::View>();
  views::FlexLayout* row_layout =
      row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  row_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/0,
              /*horizontal=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  return row;
}

// Builds a credential row, adds the given elements to the layout.
// |destination_field| is nullptr if the destination field shouldn't be shown.
// |password_view_button| is an optional field.
void BuildCredentialRows(
    views::View* parent_view,
    std::unique_ptr<views::View> destination_field,
    std::unique_ptr<views::View> username_field,
    std::unique_ptr<views::View> password_field,
    std::unique_ptr<views::ToggleImageButton> password_view_button) {
  std::unique_ptr<views::Label> username_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  std::unique_ptr<views::Label> password_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  int labels_width = std::max({username_label->GetPreferredSize().width(),
                               password_label->GetPreferredSize().width()});
  int fields_height = std::max({username_field->GetPreferredSize().height(),
                                password_field->GetPreferredSize().height()});

  username_label->SetPreferredSize(gfx::Size(labels_width, fields_height));
  password_label->SetPreferredSize(gfx::Size(labels_width, fields_height));

  // Destination row.
  if (destination_field) {
    std::unique_ptr<views::View> destination_row = CreateRow();

    destination_field->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    destination_row->AddChildView(std::move(destination_field));

    parent_view->AddChildView(std::move(destination_row));
  }

  // Username row.
  std::unique_ptr<views::View> username_row = CreateRow();
  username_row->AddChildView(std::move(username_label));
  username_field->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  username_row->AddChildView(std::move(username_field));

  parent_view->AddChildView(std::move(username_row));

  // Password row.
  std::unique_ptr<views::View> password_row = CreateRow();
  password_row->AddChildView(std::move(password_label));
  password_field->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  password_row->AddChildView(std::move(password_field));

  // The eye icon is also added to the layout if it was passed.
  if (password_view_button) {
    password_row->AddChildView(std::move(password_view_button));
  }

  parent_view->AddChildView(std::move(password_row));
}

// Create a vector which contains only the values in |items| and no elements.
std::vector<std::u16string> ToValues(
    const password_manager::ValueElementVector& items) {
  std::vector<std::u16string> passwords;
  passwords.reserve(items.size());
  for (auto& pair : items)
    passwords.push_back(pair.first);
  return passwords;
}

std::unique_ptr<views::ToggleImageButton> CreatePasswordViewButton(
    views::Button::PressedCallback callback,
    bool are_passwords_revealed) {
  auto button = std::make_unique<views::ToggleImageButton>(std::move(callback));
  button->SetInstallFocusRingOnFocus(true);
  button->SetRequestFocusOnPress(true);
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetToggled(are_passwords_revealed);
  return button;
}

// Creates an EditableCombobox from |PasswordForm.all_possible_usernames| or
// even just |PasswordForm.username_value|.
std::unique_ptr<views::EditableCombobox> CreateUsernameEditableCombobox(
    const password_manager::PasswordForm& form) {
  std::vector<std::u16string> usernames = {form.username_value};
  for (const password_manager::ValueElementPair& other_possible_username_pair :
       form.all_possible_usernames) {
    if (other_possible_username_pair.first != form.username_value)
      usernames.push_back(other_possible_username_pair.first);
  }
  base::EraseIf(usernames, [](const std::u16string& username) {
    return username.empty();
  });
  bool display_arrow = !usernames.empty();
  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>(usernames.begin(),
                                                     usernames.end())),
      /*filter_on_edit=*/false, /*show_on_empty=*/true,
      views::EditableCombobox::Type::kRegular, views::style::CONTEXT_BUTTON,
      views::style::STYLE_PRIMARY, display_arrow);
  combobox->SetText(form.username_value);
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL));
  // In case of long username, ensure that the beginning of value is visible.
  combobox->SelectRange(gfx::Range(0));
  return combobox;
}

// Creates an EditableCombobox from |PasswordForm.all_possible_passwords| or
// even just |PasswordForm.password_value|.
std::unique_ptr<views::EditableCombobox> CreatePasswordEditableCombobox(
    const password_manager::PasswordForm& form,
    bool are_passwords_revealed) {
  DCHECK(!form.IsFederatedCredential());
  std::vector<std::u16string> passwords =
      form.all_possible_passwords.empty()
          ? std::vector<std::u16string>(/*n=*/1, form.password_value)
          : ToValues(form.all_possible_passwords);
  base::EraseIf(passwords, [](const std::u16string& password) {
    return password.empty();
  });
  bool display_arrow = !passwords.empty();
  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>(passwords.begin(),
                                                     passwords.end())),
      /*filter_on_edit=*/false, /*show_on_empty=*/true,
      views::EditableCombobox::Type::kPassword, views::style::CONTEXT_BUTTON,
      STYLE_PRIMARY_MONOSPACED, display_arrow);
  combobox->SetText(form.password_value);
  combobox->RevealPasswords(are_passwords_revealed);
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL));
  return combobox;
}

std::unique_ptr<views::Combobox> CreateDestinationCombobox(
    std::string primary_account_email,
    ui::ImageModel primary_account_avatar,
    bool is_using_account_store) {
  ui::ImageModel computer_image = ui::ImageModel::FromVectorIcon(
      kComputerWithCircleBackgroundIcon, ui::kColorIcon, ComboboxIconSize());

  ui::SimpleComboboxModel::Item account_destination(
      /*text=*/l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_ACCOUNT),
      /*dropdown_secondary_text=*/
      base::UTF8ToUTF16(primary_account_email),
      /*icon=*/primary_account_avatar);

  ui::SimpleComboboxModel::Item device_destination(
      /*text=*/l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_DEVICE),
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/computer_image);

  auto combobox = std::make_unique<views::Combobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>{
              std::move(account_destination), std::move(device_destination)}));
  if (is_using_account_store)
    combobox->SetSelectedRow(0);
  else
    combobox->SetSelectedRow(1);

  combobox->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_ACCESSIBLE_NAME));
  return combobox;
}

}  // namespace

// TODO(crbug.com/1077706): come up with a more general solution for this.
// This layout auto-resizes the host view to always adapt to changes in the size
// of the child views.
class PasswordSaveUpdateView::AutoResizingLayout : public views::FillLayout {
 public:
  AutoResizingLayout() = default;

 private:
  PasswordSaveUpdateView* bubble_view() {
    return static_cast<PasswordSaveUpdateView*>(host_view());
  }

  void OnLayoutChanged() override {
    FillLayout::OnLayoutChanged();
    if (bubble_view()->GetWidget())
      bubble_view()->SizeToContents();
  }
};

PasswordSaveUpdateView::PasswordSaveUpdateView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason,
    FeaturePromoControllerViews* promo_controller)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/reason == USER_GESTURE),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction),
      is_update_bubble_(controller_.state() ==
                        password_manager::ui::PENDING_PASSWORD_UPDATE_STATE),
      are_passwords_revealed_(
          controller_.are_passwords_revealed_when_bubble_is_opened()),
      promo_controller_(promo_controller) {
  DCHECK(controller_.state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         controller_.state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  std::unique_ptr<views::Combobox> destination_dropdown;
  if (controller_.ShouldShowPasswordStorePicker()) {
    destination_dropdown = CreateDestinationCombobox(
        controller_.GetPrimaryAccountEmail(),
        controller_.GetPrimaryAccountAvatar(ComboboxIconSize()),
        controller_.IsUsingAccountStore());
    destination_dropdown->SetCallback(base::BindRepeating(
        &PasswordSaveUpdateView::DestinationChanged, base::Unretained(this)));
    destination_dropdown_ = destination_dropdown.get();
  }
  const password_manager::PasswordForm& password_form =
      controller_.pending_password();
  if (password_form.IsFederatedCredential()) {
    // The credential to be saved doesn't contain password but just the identity
    // provider (e.g. "Sign in with Google"). Thus, the layout is different.
    views::FlexLayout* flex_layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets(
                /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                    DISTANCE_CONTROL_LIST_VERTICAL),
                /*horizontal=*/0));

    if (destination_dropdown)
      AddChildView(std::move(destination_dropdown));

    const auto titles = GetCredentialLabelsForAccountChooser(password_form);
    AddChildView(std::make_unique<CredentialsItemView>(
                     views::Button::PressedCallback(), titles.first,
                     titles.second, &password_form,
                     controller_.GetProfile()
                         ->GetDefaultStoragePartition()
                         ->GetURLLoaderFactoryForBrowserProcess()
                         .get()))
        ->SetEnabled(false);
  } else {
    std::unique_ptr<views::EditableCombobox> username_dropdown =
        CreateUsernameEditableCombobox(password_form);
    username_dropdown->SetCallback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));
    std::unique_ptr<views::EditableCombobox> password_dropdown =
        CreatePasswordEditableCombobox(password_form, are_passwords_revealed_);
    password_dropdown->SetCallback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));
    std::unique_ptr<views::ToggleImageButton> password_view_button =
        CreatePasswordViewButton(
            base::BindRepeating(
                &PasswordSaveUpdateView::TogglePasswordVisibility,
                base::Unretained(this)),
            are_passwords_revealed_);
    // Set up layout:
    SetLayoutManager(std::make_unique<AutoResizingLayout>());
    views::View* root_view = AddChildView(std::make_unique<views::View>());
    views::AnimatingLayoutManager* animating_layout =
        root_view->SetLayoutManager(
            std::make_unique<views::AnimatingLayoutManager>());
    animating_layout
        ->SetBoundsAnimationMode(views::AnimatingLayoutManager::
                                     BoundsAnimationMode::kAnimateMainAxis)
        .SetOrientation(views::LayoutOrientation::kVertical);
    views::FlexLayout* flex_layout = animating_layout->SetTargetLayoutManager(
        std::make_unique<views::FlexLayout>());
    flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets(
                /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                    DISTANCE_CONTROL_LIST_VERTICAL),
                /*horizontal=*/0));

    username_dropdown_ = username_dropdown.get();
    password_dropdown_ = password_dropdown.get();
    password_view_button_ = password_view_button.get();
    BuildCredentialRows(root_view, std::move(destination_dropdown),
                        std::move(username_dropdown),
                        std::move(password_dropdown),
                        std::move(password_view_button));

    // The |username_dropdown_| should observe the animating layout manager to
    // close the dropdown menu when the animation starts.
    animating_layout_for_username_dropdown_observation_ = std::make_unique<
        base::ScopedObservation<views::AnimatingLayoutManager,
                                views::AnimatingLayoutManager::Observer>>(
        username_dropdown_);
    animating_layout_for_username_dropdown_observation_->Observe(
        animating_layout);
    animating_layout_for_iph_observation_.Observe(animating_layout);

    // The account picker is only visible in Save bubbble, not Update bubble.
    if (destination_dropdown_)
      destination_dropdown_->SetVisible(!controller_.IsCurrentStateUpdate());

    // Only non-federated credentials bubble has a username field and can
    // change states between Save and Update. Therefore, we need to have the
    // `accessibility_alert_` to inform screen readers about thatchange.
    accessibility_alert_ =
        root_view->AddChildView(std::make_unique<views::View>());
    AddChildView(accessibility_alert_.get());
  }

  {
    using Controller = SaveUpdateBubbleController;
    using ControllerNotifyFn = void (Controller::*)();
    auto button_clicked = [](PasswordSaveUpdateView* dialog,
                             ControllerNotifyFn func) {
      dialog->UpdateUsernameAndPasswordInModel();
      (dialog->controller_.*func)();
    };

    SetAcceptCallback(base::BindOnce(button_clicked, base::Unretained(this),
                                     &Controller::OnSaveClicked));
    SetCancelCallback(base::BindOnce(
        button_clicked, base::Unretained(this),
        is_update_bubble_ ? &Controller::OnNopeUpdateClicked
                          : &Controller::OnNeverForThisSiteClicked));
  }
  SetShowIcon(false);

  UpdateBubbleUIElements();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerAutofillPassword, web_contents, 10000);
}

PasswordSaveUpdateView::~PasswordSaveUpdateView() {
  CloseIPHBubbleIfOpen();
}

views::View* PasswordSaveUpdateView::GetUsernameTextfieldForTest() const {
  return username_dropdown_->GetTextfieldForTest();
}

PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController()
    const {
  return &controller_;
}

void PasswordSaveUpdateView::DestinationChanged() {
  bool is_account_store_selected =
      destination_dropdown_->GetSelectedIndex() == 0;
  controller_.OnToggleAccountStore(is_account_store_selected);
  // Saving in account and local stores have action button text for non-opted-in
  // users (Next vs. Save).
  UpdateBubbleUIElements();
  // If the user explicitly switched to "save on this device only",
  // record this with the IPH tracker (so it can decide not to show the
  // IPH again). It may be null in tests, so handle that case.
  if (!is_account_store_selected && promo_controller_) {
    promo_controller_->feature_engagement_tracker()->NotifyEvent(
        "passwords_account_storage_unselected");
  }
  // The IPH shown upon failure in reauth is used to informs the user that the
  // password will be stored on device. This is why it's important to close it
  // if the user changes the destination to account.
  if (failed_reauth_promo_id_)
    CloseIPHBubbleIfOpen();
}

views::View* PasswordSaveUpdateView::GetInitiallyFocusedView() {
  if (username_dropdown_ && username_dropdown_->GetText().empty())
    return username_dropdown_;
  View* initial_view = PasswordBubbleViewBase::GetInitiallyFocusedView();
  // |initial_view| will normally be the 'Save' button, but in case it's not
  // focusable, we return nullptr so the Widget doesn't give focus to the next
  // focusable View, which would be |username_dropdown_|, and which would
  // bring up the menu without a user interaction. We only allow initial focus
  // on |username_dropdown_| above, when the text is empty.
  return (initial_view && initial_view->IsFocusable()) ? initial_view : nullptr;
}

bool PasswordSaveUpdateView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         controller_.pending_password().IsFederatedCredential() ||
         !controller_.pending_password().password_value.empty();
}

ui::ImageModel PasswordSaveUpdateView::GetWindowIcon() {
  return ui::ImageModel();
}

void PasswordSaveUpdateView::AddedToWidget() {
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetAllowCharacterBreak(true);
  SetBubbleHeader(IDR_SAVE_PASSWORD, IDR_SAVE_PASSWORD_DARK);
  if (ShouldShowFailedReauthIPH())
    MaybeShowIPH(IPHType::kFailedReauth);
  else
    MaybeShowIPH(IPHType::kRegular);
}

void PasswordSaveUpdateView::OnThemeChanged() {
  PasswordBubbleViewBase::OnThemeChanged();
  if (password_view_button_) {
    const auto* color_provider = GetColorProvider();
    const SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
    const SkColor disabled_icon_color =
        color_provider->GetColor(ui::kColorIconDisabled);
    views::SetImageFromVectorIconWithColor(password_view_button_, kEyeIcon,
                                           GetDefaultSizeOfVectorIcon(kEyeIcon),
                                           icon_color);
    views::SetToggledImageFromVectorIconWithColor(
        password_view_button_, kEyeCrossedIcon,
        GetDefaultSizeOfVectorIcon(kEyeCrossedIcon), icon_color,
        disabled_icon_color);
  }
}

void PasswordSaveUpdateView::OnLayoutIsAnimatingChanged(
    views::AnimatingLayoutManager* source,
    bool is_animating) {
  if (!is_animating)
    MaybeShowIPH(IPHType::kRegular);
}

void PasswordSaveUpdateView::TogglePasswordVisibility() {
  if (!are_passwords_revealed_ && !controller_.RevealPasswords())
    return;

  are_passwords_revealed_ = !are_passwords_revealed_;
  password_view_button_->SetToggled(are_passwords_revealed_);
  DCHECK(password_dropdown_);
  password_dropdown_->RevealPasswords(are_passwords_revealed_);
}

void PasswordSaveUpdateView::UpdateUsernameAndPasswordInModel() {
  if (!username_dropdown_ && !password_dropdown_)
    return;
  std::u16string new_username = controller_.pending_password().username_value;
  std::u16string new_password = controller_.pending_password().password_value;
  if (username_dropdown_) {
    new_username = username_dropdown_->GetText();
    base::TrimString(new_username, u" ", &new_username);
  }
  if (password_dropdown_)
    new_password = password_dropdown_->GetText();
  controller_.OnCredentialEdited(std::move(new_username),
                                 std::move(new_password));
}

void PasswordSaveUpdateView::UpdateBubbleUIElements() {
  SetButtons((ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));
  std::u16string ok_button_text;
  if (controller_.IsAccountStorageOptInRequiredBeforeSave()) {
    ok_button_text = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_SAVE_BUBBLE_OPT_IN_BUTTON);
  } else {
    ok_button_text = l10n_util::GetStringUTF16(
        controller_.IsCurrentStateUpdate() ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                           : IDS_PASSWORD_MANAGER_SAVE_BUTTON);
  }
  SetButtonLabel(ui::DIALOG_BUTTON_OK, ok_button_text);
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          is_update_bubble_ ? IDS_PASSWORD_MANAGER_CANCEL_BUTTON
                            : IDS_PASSWORD_MANAGER_BUBBLE_BLOCKLIST_BUTTON));
  // If the title is going to change, we should announce it to the screen
  // readers.
  bool should_announce_save_update_change =
      GetWindowTitle() != controller_.GetTitle();

  SetTitle(controller_.GetTitle());

  // Nothing to do if the bubble isn't visible yet.
  if (!GetWidget())
    return;

  if (should_announce_save_update_change)
    AnnounceSaveUpdateChange();

  // Nothing else to do if the account picker hasn't been created.
  if (!destination_dropdown_)
    return;

  // If it's not a save bubble anymore, close the IPH because the account picker
  // will disappear. If it has become a save bubble, the IPH will get triggered
  // after the animation finishes.
  if (controller_.IsCurrentStateUpdate())
    CloseIPHBubbleIfOpen();

  destination_dropdown_->SetVisible(!controller_.IsCurrentStateUpdate());
}

bool PasswordSaveUpdateView::ShouldShowFailedReauthIPH() {
  // If the reauth failed, we should have automatically switched to local mdoe,
  // and we should show the reauth failed IPH unconditionally as long as the
  // user didn't change the save location.
  return controller_.DidAuthForAccountStoreOptInFail() &&
         !controller_.IsUsingAccountStore();
}

void PasswordSaveUpdateView::MaybeShowIPH(IPHType type) {
  DCHECK_NE(IPHType::kNone, type);

  // IPH is shown only where the destination dropdown is shown (i.e. only for
  // Save bubble).
  if (!destination_dropdown_ || controller_.IsCurrentStateUpdate())
    return;

  // The promo controller may not exist in tests.
  if (!promo_controller_)
    return;

  // Make sure the Save/Update bubble doesn't get closed when the IPH bubble is
  // opened.
  bool close_save_bubble_on_deactivate_original_value = close_on_deactivate();
  set_close_on_deactivate(false);

  constexpr FeaturePromoSpecification::BubbleArrow kPromoArrow =
      FeaturePromoSpecification::BubbleArrow::kRightCenter;

  if (type == IPHType::kRegular) {
    FeaturePromoSpecification promo_spec =
        FeaturePromoSpecification::CreateForLegacyPromo(
            &feature_engagement::kIPHPasswordsAccountStorageFeature,
            ui::ElementIdentifier(),
            IDS_PASSWORD_MANAGER_IPH_BODY_SAVE_TO_ACCOUNT);
    promo_spec.SetBubbleTitleText(
        IDS_PASSWORD_MANAGER_IPH_TITLE_SAVE_TO_ACCOUNT);
    promo_spec.SetBubbleArrow(kPromoArrow);

    if (promo_controller_->MaybeShowPromoFromSpecification(
            promo_spec, destination_dropdown_)) {
      // If the regular promo was shown, the failed reauth promo is
      // definitely finished. If not, we can't be confident it hasn't
      // finished.
      failed_reauth_promo_id_ = absl::nullopt;
    }
  } else {
    FeaturePromoSpecification promo_spec =
        FeaturePromoSpecification::CreateForLegacyPromo(
            /* feature =*/nullptr, ui::ElementIdentifier(),
            IDS_PASSWORD_MANAGER_IPH_BODY_SAVE_REAUTH_FAIL);
    promo_spec.SetBubbleArrow(kPromoArrow);

    failed_reauth_promo_id_ =
        promo_controller_->ShowCriticalPromo(promo_spec, destination_dropdown_);
  }

  set_close_on_deactivate(close_save_bubble_on_deactivate_original_value);
}

void PasswordSaveUpdateView::CloseIPHBubbleIfOpen() {
  // The promo controller may not exist in tests.
  if (!promo_controller_)
    return;

  if (!failed_reauth_promo_id_) {
    promo_controller_->CloseBubble(
        feature_engagement::kIPHPasswordsAccountStorageFeature);
    return;
  }

  // |failed_reauth_promo_id_| may have a value if it closed on its
  // own. This is fine; CloseBubbleForCriticalPromo() handles expired
  // IDs, and we reset ours when showing a normal IPH bubble.
  promo_controller_->CloseBubbleForCriticalPromo(
      failed_reauth_promo_id_.value());
  failed_reauth_promo_id_ = absl::nullopt;
}

void PasswordSaveUpdateView::AnnounceSaveUpdateChange() {
  // Federated credentials bubbles don't change the state between Update and
  // Save, and hence they don't have an `accessibility_alert_` view created.
  if (!accessibility_alert_)
    return;

  std::u16string accessibility_alert_text = GetWindowTitle();
  if (destination_dropdown_ && !controller_.IsCurrentStateUpdate()) {
    // For Save bubbles, if the `destination_dropdown_` exists (for account
    // store users), we use the labels in the `destination_dropdown_` instead.
    accessibility_alert_text = destination_dropdown_->GetTextForRow(
        destination_dropdown_->GetSelectedIndex());
  }

  views::ViewAccessibility& ax = accessibility_alert_->GetViewAccessibility();
  ax.OverrideRole(ax::mojom::Role::kAlert);
  ax.OverrideName(accessibility_alert_text);
  accessibility_alert_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                 true);
}

void PasswordSaveUpdateView::OnContentChanged() {
  bool is_update_state_before = controller_.IsCurrentStateUpdate();
  bool is_ok_button_enabled_before =
      IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK);
  UpdateUsernameAndPasswordInModel();
  // Maybe the buttons should be updated.
  if (is_update_state_before != controller_.IsCurrentStateUpdate() ||
      is_ok_button_enabled_before !=
          IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK)) {
    UpdateBubbleUIElements();
    DialogModelChanged();
  }
}
