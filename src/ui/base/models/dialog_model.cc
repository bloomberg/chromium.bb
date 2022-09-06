// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"
#include <memory>

#include "base/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"

namespace ui {

DialogModel::Builder::Builder(std::unique_ptr<DialogModelDelegate> delegate)
    : model_(std::make_unique<DialogModel>(base::PassKey<Builder>(),
                                           std::move(delegate))) {}

DialogModel::Builder::Builder() : Builder(nullptr) {}

DialogModel::Builder::~Builder() {
  DCHECK(!model_) << "Model should've been built.";
}

std::unique_ptr<DialogModel> DialogModel::Builder::Build() {
  DCHECK(model_);
  return std::move(model_);
}

DialogModel::Builder& DialogModel::Builder::AddOkButton(
    base::OnceClosure callback,
    std::u16string label,
    const DialogModelButton::Params& params) {
  DCHECK(!model_->accept_action_callback_);
  model_->accept_action_callback_ = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogAccepted() instead.
  model_->ok_button_.emplace(
      model_->GetPassKey(), model_.get(),
      base::BindRepeating([](const Event&) { NOTREACHED(); }), std::move(label),
      params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddCancelButton(
    base::OnceClosure callback,
    std::u16string label,
    const DialogModelButton::Params& params) {
  DCHECK(!model_->cancel_action_callback_);
  model_->cancel_action_callback_ = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogCanceled() instead.
  model_->cancel_button_.emplace(
      model_->GetPassKey(), model_.get(),
      base::BindRepeating([](const Event&) { NOTREACHED(); }), std::move(label),
      params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraButton(
    base::RepeatingCallback<void(const Event&)> callback,
    std::u16string label,
    const DialogModelButton::Params& params) {
  DCHECK(!model_->extra_button_);
  DCHECK(!model_->extra_link_);
  model_->extra_button_.emplace(model_->GetPassKey(), model_.get(),
                                std::move(callback), std::move(label), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraLink(
    ui::DialogModelLabel::Link link) {
  DCHECK(!model_->extra_button_);
  DCHECK(!model_->extra_link_);
  model_->extra_link_.emplace(std::move(link));
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetInitiallyFocusedField(
    ElementIdentifier id) {
  // This must be called with a non-null id
  DCHECK(id);
  // This can only be called once.
  DCHECK(!model_->initially_focused_field_);
  model_->initially_focused_field_ = id;
  return *this;
}

DialogModel::DialogModel(base::PassKey<Builder>,
                         std::unique_ptr<DialogModelDelegate> delegate)
    : delegate_(std::move(delegate)) {
  if (delegate_)
    delegate_->set_dialog_model(this);
}

DialogModel::~DialogModel() = default;

void DialogModel::AddBodyText(const DialogModelLabel& label,
                              ElementIdentifier id) {
  AddField(
      std::make_unique<DialogModelBodyText>(GetPassKey(), this, label, id));
}

void DialogModel::AddCheckbox(ElementIdentifier id,
                              const DialogModelLabel& label,
                              const DialogModelCheckbox::Params& params) {
  AddField(std::make_unique<DialogModelCheckbox>(GetPassKey(), this, id, label,
                                                 params));
}

void DialogModel::AddCombobox(ElementIdentifier id,
                              std::u16string label,
                              std::unique_ptr<ui::ComboboxModel> combobox_model,
                              const DialogModelCombobox::Params& params) {
  AddField(std::make_unique<DialogModelCombobox>(
      GetPassKey(), this, id, std::move(label), std::move(combobox_model),
      params));
}

void DialogModel::AddSeparator() {
  AddField(std::make_unique<DialogModelSeparator>(GetPassKey(), this));
}

void DialogModel::AddMenuItem(ImageModel icon,
                              std::u16string label,
                              base::RepeatingCallback<void(int)> callback,
                              const DialogModelMenuItem::Params& params) {
  AddField(std::make_unique<DialogModelMenuItem>(
      GetPassKey(), this, std::move(icon), std::move(label),
      std::move(callback), params));
}

void DialogModel::AddTextfield(ElementIdentifier id,
                               std::u16string label,
                               std::u16string text,
                               const DialogModelTextfield::Params& params) {
  AddField(std::make_unique<DialogModelTextfield>(
      GetPassKey(), this, id, std::move(label), std::move(text), params));
}

void DialogModel::AddCustomField(
    std::unique_ptr<DialogModelCustomField::Field> field,
    ElementIdentifier id) {
  AddField(std::make_unique<DialogModelCustomField>(GetPassKey(), this, id,
                                                    std::move(field)));
}

bool DialogModel::HasField(ElementIdentifier id) const {
  return base::ranges::any_of(fields_,
                              [id](auto& field) { return field->id_ == id; });
}

DialogModelField* DialogModel::GetFieldByUniqueId(ElementIdentifier id) {
  // Assert that there is one and only one.
  DCHECK_EQ(1, static_cast<int>(base::ranges::count_if(
                   fields_, [id](auto& field) { return field->id_ == id; })));

  for (auto& field : fields_) {
    if (field->id_ == id)
      return field.get();
  }
  return nullptr;
}

DialogModelCheckbox* DialogModel::GetCheckboxByUniqueId(ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCheckbox();
}

DialogModelCombobox* DialogModel::GetComboboxByUniqueId(ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCombobox();
}

DialogModelTextfield* DialogModel::GetTextfieldByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsTextfield();
}

void DialogModel::OnDialogAcceptAction(base::PassKey<DialogModelHost>) {
  if (accept_action_callback_)
    std::move(accept_action_callback_).Run();
}

void DialogModel::OnDialogCancelAction(base::PassKey<DialogModelHost>) {
  if (cancel_action_callback_)
    std::move(cancel_action_callback_).Run();
}

void DialogModel::OnDialogCloseAction(base::PassKey<DialogModelHost>) {
  if (close_action_callback_)
    std::move(close_action_callback_).Run();
}

void DialogModel::OnDialogDestroying(base::PassKey<DialogModelHost>) {
  if (dialog_destroying_callback_)
    std::move(dialog_destroying_callback_).Run();
}

void DialogModel::AddField(std::unique_ptr<DialogModelField> field) {
  fields_.push_back(std::move(field));
  if (host_)
    host_->OnFieldAdded(fields_.back().get());
}

}  // namespace ui
