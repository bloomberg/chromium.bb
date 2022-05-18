// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// The padding values of the SavedDeskItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// The corner radius for the SavedDeskItemView.
constexpr int kCornerRadius = 16;

// The margin for the delete button.
constexpr int kDeleteButtonMargin = 8;

// The distance from the bottom of the launch button to the bottom of `this`.
constexpr int kLaunchButtonDistanceFromBottomDp = 14;

// The preferred width of the container that houses the template name textfield
// and managed status indicator and the time label.
constexpr int kTemplateNameAndTimePreferredWidth =
    SavedDeskItemView::kPreferredSize.width() - kHorizontalPaddingDp * 2;

// The height of the view which contains the time of the template.
constexpr int kTimeViewHeight = 24;

// The spacing between the textfield and the managed status icon.
constexpr int kManagedStatusIndicatorSpacing = 8;
constexpr int kManagedStatusIndicatorSize = 20;

// There is a gap between the background of the name view and the name view's
// actual text.
constexpr auto kTemplateNameInsets = gfx::Insets::VH(0, 2);

std::u16string GetTimeStr(base::Time timestamp) {
  std::u16string date, time, time_str;

  // Returns empty if `timestamp` is out of relative date range, which is
  // yesterday and today as of now. Please see `ui/base/l10n/time_format.h` for
  // more details.
  date = ui::TimeFormat::RelativeDate(timestamp, nullptr);
  if (date.empty()) {
    // Syntax `yMMMdjmm` is used by the File App if it's not a relative date.
    // Please note, this might be slightly different for different locales.
    // Examples:
    //  `en-US` - `Jan 1, 2022, 10:30 AM`
    //  `zh-CN` - `2022年1月1日 10:30`
    time_str = base::TimeFormatWithPattern(timestamp, "yMMMdjmm");
  } else {
    // If it's a relative date, just append `jmm` to it.
    // Please note, this might be slightly different for different locales.
    // Examples:
    //  `en-US` - `Today 10:30 AM`
    //  `zh-CN` - `今天 10:30`
    time_str = date + u" " + base::TimeFormatWithPattern(timestamp, "jmm");
  }

  return time_str;
}

}  // namespace

SavedDeskItemView::SavedDeskItemView(const DeskTemplate* desk_template)
    : desk_template_(desk_template->Clone()) {
  auto launch_template_callback = base::BindRepeating(
      &SavedDeskItemView::OnGridItemPressed, weak_ptr_factory_.GetWeakPtr());

  const std::u16string template_name = desk_template_->template_name();
  DCHECK(!template_name.empty());
  auto* color_provider = AshColorProvider::Get();
  const bool is_admin_managed =
      desk_template_->source() == DeskTemplateSource::kPolicy;

  views::Builder<SavedDeskItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(template_name)
      .SetCallback(std::move(launch_template_callback))
      .SetBackground(views::CreateRoundedRectBackground(
          color_provider->GetBaseLayerColor(
              AshColorProvider::BaseLayerType::kTransparent80),
          kCornerRadius))
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetInteriorMargin(
                  gfx::Insets::VH(kVerticalPaddingDp, kHorizontalPaddingDp))
              // TODO(richui): Consider splitting some of the children into
              // different files and/or classes.
              .AddChildren(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetBetweenChildSpacing(kManagedStatusIndicatorSpacing)
                      .SetPreferredSize(gfx::Size(
                          kTemplateNameAndTimePreferredWidth,
                          SavedDeskNameView::kSavedDeskNameViewHeight))
                      .AddChildren(
                          views::Builder<SavedDeskNameView>()
                              .CopyAddressTo(&name_view_)
                              .SetController(this)
                              .SetText(template_name)
                              .SetAccessibleName(template_name)
                              .SetReadOnly(!desk_template_->IsModifiable())
                              // Use the focus behavior specified by the
                              // subclass of `SavedDeskNameView` unless the
                              // template is not modifiable.
                              .SetFocusBehavior(desk_template_->IsModifiable()
                                                    ? GetFocusBehavior()
                                                    : FocusBehavior::NEVER),
                          views::Builder<views::ImageView>()
                              .SetPreferredSize(
                                  gfx::Size(kManagedStatusIndicatorSize,
                                            kManagedStatusIndicatorSize))
                              .SetImage(gfx::CreateVectorIcon(
                                  chromeos::kEnterpriseIcon,
                                  kManagedStatusIndicatorSize,
                                  color_provider->GetContentLayerColor(
                                      AshColorProvider::ContentLayerType::
                                          kIconColorSecondary)))
                              .SetVisible(is_admin_managed)),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetText(
                          is_admin_managed
                              ? l10n_util::GetStringUTF16(
                                    IDS_ASH_DESKS_TEMPLATES_MANAGEMENT_STATUS_DESCRIPTION)
                              : GetTimeStr(desk_template_->created_time()))
                      .SetPreferredSize(gfx::Size(
                          kTemplateNameAndTimePreferredWidth, kTimeViewHeight)),
                  // View which acts as a spacer, taking up all the available
                  // space between the date and the icons container.
                  views::Builder<views::View>().SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToZero,
                          views::MaximumFlexSizeRule::kUnbounded)),
                  views::Builder<SavedDeskIconContainer>()
                      .CopyAddressTo(&icon_container_view_)
                      .PopulateIconContainerFromTemplate(desk_template_.get())
                      .SetVisible(true)),
          views::Builder<views::View>()
              .CopyAddressTo(&hover_container_)
              .SetUseDefaultFillLayout(true)
              .SetVisible(false))
      .BuildChildren();

  // We need to ensure that the layer is non-opaque when animating.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const int button_text_id =
      desk_template_->type() == DeskTemplateType::kTemplate
          ? IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON
          : IDS_ASH_DESKS_TEMPLATES_OPEN_DESK_BUTTON;
  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SavedDeskItemView::OnGridItemPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(button_text_id), PillButton::Type::kIconless,
      /*icon=*/nullptr));

  // Users cannot delete admin templates.
  if (!is_admin_managed) {
    delete_button_ =
        hover_container_->AddChildView(std::make_unique<CloseButton>(
            base::BindRepeating(&SavedDeskItemView::OnDeleteButtonPressed,
                                weak_ptr_factory_.GetWeakPtr()),
            CloseButton::Type::kMedium));
    delete_button_->SetVectorIcon(kDeleteIcon);
    delete_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON));
    delete_button_->SetBackgroundColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  }

  // Use a border to create spacing between `name_view_`s background (set in
  // `DesksTextfield`) and the actual text. Shift the parent by the same amount
  // so that the text stays aligned with the text in `time_view`. We shift the
  // parent here and not `name_view_` itself otherwise its bounds will be
  // outside the parent bounds and the background will get clipped.
  name_view_->SetBorder(views::CreateEmptyBorder(kTemplateNameInsets));
  name_view_->parent()->SetProperty(views::kMarginsKey, -kTemplateNameInsets);
  name_view_observation_.Observe(name_view_);

  // Add a shadow with system UI shadow type.
  shadow_ = std::make_unique<ViewShadow>(
      this,
      SystemShadow::GetElevationFromType(SystemShadow::Type::kElevation12));
  shadow_->shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
  shadow_->SetRoundedCornerRadius(kCornerRadius);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<SavedDeskItemView*>(view)->IsViewHighlighted();
  });

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

SavedDeskItemView::~SavedDeskItemView() {
  name_view_observation_.Reset();
}

void SavedDeskItemView::UpdateHoverButtonsVisibility(
    const gfx::Point& screen_location,
    bool is_touch) {
  gfx::Point location_in_view = screen_location;
  ConvertPointFromScreen(this, &location_in_view);

  // For switch access, setting the hover buttons to visible allows users to
  // navigate to it.
  const bool visible =
      !is_template_name_being_modified_ &&
      ((is_touch && HitTestPoint(location_in_view)) ||
       (!is_touch && IsMouseHovered()) ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());
  hover_container_->SetVisible(visible);
  icon_container_view_->SetVisible(!visible);
}

bool SavedDeskItemView::IsNameBeingModified() const {
  return name_view_->HasFocus();
}

void SavedDeskItemView::MaybeRemoveNameNumber() {
  // When there are existing matched Desk name and Template name (ie.
  // "Desk 1"), creating a new template from "Desk 1" will get auto generated
  // template name from backend as "Desk 1 (1)", to prevent template
  // duplication, we show the template view name to be "Desk 1" by removing name
  // number, save template under such name will call out template replace
  // dialog.
  if (FindOtherTemplateWithName(
          DesksController::Get()->active_desk()->name())) {
    // Replace the name number.
    name_view_->SetTemporaryName(DesksController::Get()->active_desk()->name());
    name_view_->SetViewName(DesksController::Get()->active_desk()->name());
  }
}

void SavedDeskItemView::ReplaceTemplate(const std::string& uuid) {
  // Make sure we delete the template we are replacing first, so that we don't
  // get template name collisions.
  SavedDeskPresenter::Get()->DeleteEntry(uuid);
  UpdateTemplateName();
  RecordReplaceTemplateHistogram();
}

void SavedDeskItemView::RevertTemplateName() {
  views::FocusManager* focus_manager = GetFocusManager();
  focus_manager->SetFocusedView(name_view_);
  const auto temporary_name = name_view_->temporary_name();
  name_view_->SetViewName(
      temporary_name.value_or(desk_template_->template_name()));
  name_view_->SelectAll(true);

  name_view_->OnContentsChanged();
}

void SavedDeskItemView::UpdateTemplate(const DeskTemplate& updated_template) {
  desk_template_ = updated_template.Clone();

  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);

  auto new_name = desk_template_->template_name();
  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  SetAccessibleName(new_name);

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`
  name_view_->OnContentsChanged();
}

void SavedDeskItemView::Layout() {
  views::View::Layout();

  if (delete_button_) {
    const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
    DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
    delete_button_->SetBoundsRect(
        gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                  kDeleteButtonMargin, delete_button_size.width(),
                  delete_button_size.height()));
  }

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize();
  launch_button_->SetBoundsRect(
      gfx::Rect({(width() - launch_button_preferred_size.width()) / 2,
                 height() - launch_button_preferred_size.height() -
                     kLaunchButtonDistanceFromBottomDp},
                launch_button_preferred_size));
}

void SavedDeskItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  GetBackground()->SetNativeControlColor(color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));

  time_view_->SetBackgroundColor(SK_ColorTRANSPARENT);
  time_view_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));

  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

void SavedDeskItemView::OnViewFocused(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about focus on
  // `name_view_`.
  if (observed_view == this)
    return;

  DCHECK_EQ(observed_view, name_view_);
  is_template_name_being_modified_ = true;

  // Assume we should commit the name change unless `HandleKeyEvent` detects the
  // user pressed the escape key.
  should_commit_name_changes_ = true;
  name_view_->UpdateViewAppearance();

  // Hide the hover container when we are modifying the template name.
  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);

  // Set the Overview highlight to move focus with the `name_view_`.
  auto* highlight_controller = Shell::Get()
                                   ->overview_controller()
                                   ->overview_session()
                                   ->highlight_controller();
  if (highlight_controller->IsFocusHighlightVisible())
    highlight_controller->MoveHighlightToView(name_view_);

  if (!defer_select_all_)
    name_view_->SelectAll(false);
}

void SavedDeskItemView::OnViewBlurred(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about blur on
  // `name_view_`.
  if (observed_view == this)
    return;

  // If we exit overview while the `name_view_` is still focused, the shutdown
  // sequence will reset the presenter before `OnViewBlurred` gets called. This
  // checks and makes sure that we don't call the presenter while trying to
  // shutdown the overview session.
  // `overview_session` may also be null as `OnViewBlurred` may be called after
  // the owning widget is no longer owned by the session for overview exit
  // animation. See https://crbug.com/1281422.
  // TODO(richui): Revisit this once the behavior of the template name when
  // exiting overview is determined.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  if (!overview_session || overview_session->is_shutting_down())
    return;

  DCHECK_EQ(observed_view, name_view_);
  is_template_name_being_modified_ = false;
  defer_select_all_ = false;
  name_view_->UpdateViewAppearance();

  // Collapse the whitespace for the text first before comparing it or trying to
  // commit the name in order to prevent duplicate name issues.
  const std::u16string user_entered_name =
      base::CollapseWhitespace(name_view_->GetText(),
                               /*trim_sequences_with_line_breaks=*/false);
  name_view_->SetText(user_entered_name);

  // When committing the name, do not allow an empty template name. Also, don't
  // commit the name changes if the view was blurred from the user pressing the
  // escape key (identified by `should_commit_name_changes_`). Revert back to
  // the original name.
  if (!should_commit_name_changes_ || user_entered_name.empty() ||
      desk_template_->template_name() == user_entered_name) {
    OnTemplateNameChanged(desk_template_->template_name());
    // Saving a desk template always puts it in the top left corner of the desk
    // templates grid. This may mean that the grid is no longer sorted
    // alphabetically by template name. Ensure that the grid is sorted.
    for (auto& overview_grid : overview_session->grid_list()) {
      if (SavedDeskLibraryView* library_view =
              overview_grid->GetSavedDeskLibraryView()) {
        for (auto* grid_view : library_view->grid_views()) {
          grid_view->SortTemplateGridItems(
              /*last_saved_template_uuid=*/base::GUID());
        }
      }
    }
    return;
  }

  // Check if template name exist, replace existing template if confirmed by
  // user. Use a post task to avoid activating a widget while another widget is
  // still being activated. In this case, we don't want to show the dialog and
  // activate its associated widget until after the desks bar widget is finished
  // activating. See https://crbug.com/1301759.
  auto* template_to_replace = FindOtherTemplateWithName(name_view_->GetText());
  if (template_to_replace) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SavedDeskItemView::MaybeShowReplaceDialog,
                       weak_ptr_factory_.GetWeakPtr(), template_to_replace));
    return;
  }

  UpdateTemplateName();
}

void SavedDeskItemView::MaybeShowReplaceDialog(
    SavedDeskItemView* template_to_replace) {
  // Show replace template dialog. If accepted, replace old template and commit
  // name change.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  SavedDeskDialogController::Get()->ShowReplaceDialog(
      root_window, name_view_->GetText(),
      base::BindOnce(
          &SavedDeskItemView::ReplaceTemplate, weak_ptr_factory_.GetWeakPtr(),
          template_to_replace->desk_template_->uuid().AsLowercaseString()),
      base::BindOnce(&SavedDeskItemView::RevertTemplateName,
                     weak_ptr_factory_.GetWeakPtr()));
}

views::Button::KeyClickAction SavedDeskItemView::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  // Prevents any key events from activating a button click while the template
  // name is being modified.
  if (is_template_name_being_modified_)
    return KeyClickAction::kNone;

  return Button::GetKeyClickActionForEvent(event);
}

void SavedDeskItemView::UpdateTemplateName() {
  desk_template_->set_template_name(name_view_->GetText());
  OnTemplateNameChanged(desk_template_->template_name());

  SavedDeskPresenter::Get()->SaveOrUpdateDeskTemplate(
      /*is_update=*/true, GetWidget()->GetNativeWindow()->GetRootWindow(),
      desk_template_->Clone());
}

void SavedDeskItemView::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  DCHECK_EQ(sender, name_view_);

  // To avoid potential security and memory issues, we don't allow template
  // names to have an unbounded length. Therefore we trim if needed at
  // `kMaxLength` UTF-16 boundary. Note that we don't care about code point
  // boundaries in this case.
  if (new_contents.size() > DesksTextfield::kMaxLength) {
    std::u16string trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DesksTextfield::kMaxLength);
    name_view_->SetText(trimmed_new_contents);
  }

  name_view_->OnContentsChanged();

  auto* focus_manager = GetWidget()->GetFocusManager();
  if (focus_manager->GetFocusedView() != name_view_) {
    // The text editor isn't currently the active view, so we'll assume that it
    // was updated from a drag and drop operation.
    UpdateTemplateName();
  }
}

bool SavedDeskItemView::HandleKeyEvent(views::Textfield* sender,
                                       const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, name_view_);
  DCHECK(is_template_name_being_modified_);

  // Pressing enter or escape should blur the focus away from `name_view_` so
  // that editing the template's name ends. Pressing tab should do the same, but
  // is handled in `OverviewSession`.
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }

  // If the escape key was pressed, `should_commit_name_changes_` is set to
  // false so that `OnViewBlurred` knows that it should not change the name of
  // the template.
  if (key_event.key_code() == ui::VKEY_ESCAPE)
    should_commit_name_changes_ = false;

  SavedDeskNameView::CommitChanges(GetWidget());

  return true;
}

bool SavedDeskItemView::HandleMouseEvent(views::Textfield* sender,
                                         const ui::MouseEvent& mouse_event) {
  DCHECK_EQ(sender, name_view_);

  switch (mouse_event.type()) {
    case ui::ET_MOUSE_PRESSED:
      // If this is the first mouse press on the `name_view_`, then it's not
      // focused yet. `OnViewFocused()` should not select all text, since it
      // will be undone by the mouse release event. Instead we defer it until we
      // get the mouse release event.
      if (!is_template_name_being_modified_)
        defer_select_all_ = true;
      break;

    case ui::ET_MOUSE_RELEASED:
      if (defer_select_all_) {
        defer_select_all_ = false;
        // The user may have already clicked and dragged to select some range
        // other than all the text. In this case, don't mess with an existing
        // selection.
        if (!name_view_->HasSelection()) {
          name_view_->SelectAll(false);
        }
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

views::View* SavedDeskItemView::TargetForRect(views::View* root,
                                              const gfx::Rect& rect) {
  gfx::RectF name_view_bounds(name_view_->GetMirroredBounds());
  views::View::ConvertRectToTarget(name_view_->parent(), this,
                                   &name_view_bounds);

  // With the design of the template card having the textfield within a
  // clickable button, as well as having the grid view be a `PreTargetHandler`,
  // we needed to make `this` a `ViewTargeterDelegate` for the view event
  // targeter in order to allow the `name_view_` to be specifically targeted and
  // focused. Use the centerpoint for `rect` as parts of `rect` may be outside
  // the `name_view_bounds` for touch events.
  if (root == this &&
      gfx::ToRoundedRect(name_view_bounds).Contains(rect.CenterPoint())) {
    return name_view_;
  }
  return views::ViewTargeterDelegate::TargetForRect(root, rect);
}

SavedDeskItemView* SavedDeskItemView::FindOtherTemplateWithName(
    const std::u16string& name) const {
  const auto templates_grid_view_items =
      static_cast<const SavedDeskGridView*>(parent())->grid_items();

  auto iter = std::find_if(
      templates_grid_view_items.begin(), templates_grid_view_items.end(),
      [this, name](const SavedDeskItemView* d) {
        // Name duplication is allowed if one of the templates is an admin
        // template.
        return (d != this && d->desk_template()->template_name() == name &&
                d->desk_template()->source() != DeskTemplateSource::kPolicy);
      });
  return iter == templates_grid_view_items.end() ? nullptr : *iter;
}

void SavedDeskItemView::OnDeleteTemplate() {
  SavedDeskPresenter::Get()->DeleteEntry(
      desk_template_->uuid().AsLowercaseString());
}

void SavedDeskItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* dialog_controller = SavedDeskDialogController::Get();
  dialog_controller->ShowDeleteDialog(
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      name_view_->GetAccessibleName(),
      base::BindOnce(&SavedDeskItemView::OnDeleteTemplate,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskItemView::OnGridItemPressed(const ui::Event& event) {
  MaybeLaunchTemplate(event.IsShiftDown());
}

void SavedDeskItemView::MaybeLaunchTemplate(bool should_delay) {
  if (is_template_name_being_modified_) {
    SavedDeskNameView::CommitChanges(GetWidget());
    return;
  }

  // Make shift-click on the launch button launch apps with a delay. This allows
  // developers to simulate delayed launch behaviors with ARC apps.
  // TODO(crbug.com/1281685): Remove before feature launch.
  base::TimeDelta delay;
#if !defined(OFFICIAL_BUILD)
  if (should_delay)
    delay = base::Seconds(3);
#endif

  SavedDeskPresenter::Get()->LaunchDeskTemplate(
      desk_template_->uuid().AsLowercaseString(), delay,
      GetWidget()->GetNativeWindow()->GetRootWindow());
}

void SavedDeskItemView::OnTemplateNameChanged(const std::u16string& new_name) {
  if (is_template_name_being_modified_)
    return;

  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  name_view_->ResetTemporaryName();
  SetAccessibleName(new_name);

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`.
  name_view_->OnContentsChanged();
}

views::View* SavedDeskItemView::GetView() {
  return this;
}

void SavedDeskItemView::MaybeActivateHighlightedView() {
  MaybeLaunchTemplate(/*should_delay=*/false);
}

void SavedDeskItemView::MaybeCloseHighlightedView(bool primary_action) {
  if (primary_action)
    OnDeleteButtonPressed();
}

void SavedDeskItemView::MaybeSwapHighlightedView(bool right) {}

void SavedDeskItemView::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SavedDeskItemView::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(SavedDeskItemView, views::Button)
END_METADATA

}  // namespace ash
