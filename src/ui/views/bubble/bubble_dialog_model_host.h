// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {

class Label;
class StyledLabel;

// BubbleDialogModelHost is a views implementation of ui::DialogModelHost which
// hosts a ui::DialogModel as a BubbleDialogDelegate. This exposes such as
// SetAnchorView(), SetArrow() and SetHighlightedButton(). For methods that are
// reflected in ui::DialogModelHost (such as ::Close()), prefer using the
// ui::DialogModelHost to avoid platform-specific code (GetWidget()->Close())
// where unnecessary. For those methods, note that this can be retrieved as a
// ui::DialogModelHost through DialogModel::host(). This helps minimize
// platform-specific code from platform-agnostic model-delegate code.
class VIEWS_EXPORT BubbleDialogModelHost : public BubbleDialogDelegate,
                                           public ui::DialogModelHost {
 public:
  enum class FieldType { kText, kControl, kMenuItem };

  // TODO(pbos): Reconsider whether this should be generic outside of
  // BubbleDialogModelHost.
  // TODO(pbos): Consider making this interface appropriate for all fields (not
  // just custom ones). If so rename this ViewFactory (not CustomViewFactory).
  // Interface for adding custom views to a DialogModel. This factory interface
  // allows constructing views to be hosted in BubbleDialogModelHost.
  class CustomViewFactory : public ui::DialogModelCustomField::Factory {
   public:
    virtual std::unique_ptr<View> CreateView() = 0;
    virtual FieldType GetFieldType() const = 0;
  };

  // Constructs a BubbleDialogModelHost, which for most purposes is to used as a
  // BubbleDialogDelegate. The BubbleDialogDelegate is nominally handed to
  // BubbleDialogDelegate::CreateBubble() which returns a Widget that has taken
  // ownership of the bubble. Widget::Show() finally shows the bubble.
  BubbleDialogModelHost(std::unique_ptr<ui::DialogModel> model,
                        View* anchor_view,
                        BubbleBorder::Arrow arrow);
  ~BubbleDialogModelHost() override;

  static std::unique_ptr<BubbleDialogModelHost> CreateModal(
      std::unique_ptr<ui::DialogModel> model,
      ui::ModalType modal_type);

  // BubbleDialogDelegate:
  // TODO(pbos): Populate initparams with initial view instead of overriding
  // GetInitiallyFocusedView().
  View* GetInitiallyFocusedView() override;
  void OnWidgetInitialized() override;

  // ui::DialogModelHost:
  void Close() override;
  void OnFieldAdded(ui::DialogModelField* field) override;

 private:
  class ContentsView;
  // TODO(pbos): Consider externalizing this functionality into a different
  // format that could feasibly be adopted by LayoutManagers. This is used for
  // BoxLayouts (but could be others) to agree on columns' preferred width as a
  // replacement for using GridLayout.
  class LayoutConsensusView;
  class LayoutConsensusGroup {
   public:
    LayoutConsensusGroup();
    ~LayoutConsensusGroup();

    void AddView(LayoutConsensusView* view);
    void RemoveView(LayoutConsensusView* view);

    void InvalidateChildren();

    // Get the union of all preferred sizes within the group.
    gfx::Size GetMaxPreferredSize() const;

    // Get the union of all minimum sizes within the group.
    gfx::Size GetMaxMinimumSize() const;

   private:
    base::flat_set<View*> children_;
  };

  struct DialogModelHostField {
    ui::DialogModelField* dialog_model_field;

    // View representing the entire field.
    View* field_view;

    // Child view to |field_view|, if any, that's used for focus. For instance,
    // a textfield row would be a container that contains both a
    // views::Textfield and a descriptive label. In this case |focusable_view|
    // would refer to the views::Textfield which is also what would gain focus.
    View* focusable_view;
  };

  void OnWindowClosing();

  void AddInitialFields();
  void AddOrUpdateBodyText(ui::DialogModelBodyText* model_field);
  void AddOrUpdateCheckbox(ui::DialogModelCheckbox* model_field);
  void AddOrUpdateCombobox(ui::DialogModelCombobox* model_field);
  void AddOrUpdateTextfield(ui::DialogModelTextfield* model_field);

  void UpdateSpacingAndMargins();

  void AddViewForLabelAndField(ui::DialogModelField* model_field,
                               const std::u16string& label_text,
                               std::unique_ptr<views::View> field,
                               const gfx::FontList& field_font);

  static bool DialogModelLabelRequiresStyledLabel(
      const ui::DialogModelLabel& dialog_label);
  std::unique_ptr<View> CreateViewForLabel(
      const ui::DialogModelLabel& dialog_label);
  std::unique_ptr<StyledLabel> CreateStyledLabelForDialogModelLabel(
      const ui::DialogModelLabel& dialog_label);
  std::unique_ptr<Label> CreateLabelForDialogModelLabel(
      const ui::DialogModelLabel& dialog_label);

  void AddDialogModelHostField(std::unique_ptr<View> view,
                               const DialogModelHostField& field_view_info);
  void AddDialogModelHostFieldForExistingView(
      const DialogModelHostField& field_view_info);

  DialogModelHostField FindDialogModelHostField(
      ui::DialogModelField* model_field);
  DialogModelHostField FindDialogModelHostField(View* view);

  static View* GetTargetView(const DialogModelHostField& field_view_info);

  bool IsModalDialog() const;

  std::unique_ptr<ui::DialogModel> model_;
  const raw_ptr<ContentsView> contents_view_;

  std::vector<DialogModelHostField> fields_;
  std::vector<base::CallbackListSubscription> property_changed_subscriptions_;

  LayoutConsensusGroup textfield_first_column_group_;
  LayoutConsensusGroup textfield_second_column_group_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DIALOG_MODEL_HOST_H_
