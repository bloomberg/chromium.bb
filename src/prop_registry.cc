// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/prop_registry.h"

#include <set>
#include <string>

#include <base/string_util.h>

#include "gestures/include/gestures.h"

using std::set;
using std::string;

namespace gestures {

void PropRegistry::Register(Property* prop) {
  props_.insert(prop);
  if (prop_provider_)
    prop->CreateProp();
}

void PropRegistry::Unregister(Property* prop) {
  if (props_.erase(prop) != 1)
    Err("Unregister failed?");
  if (prop_provider_)
    prop->DestroyProp();
}

void PropRegistry::SetPropProvider(GesturesPropProvider* prop_provider,
                                   void* data) {
  if (prop_provider_ == prop_provider)
    return;
  if (prop_provider_) {
    for (std::set<Property*>::iterator it = props_.begin(), e= props_.end();
         it != e; ++it)
      (*it)->DestroyProp();
  }
  prop_provider_ = prop_provider;
  prop_provider_data_ = data;
  if (prop_provider_) {
    Err("creating props");
    for (std::set<Property*>::iterator it = props_.begin(), e= props_.end();
         it != e; ++it) {
      (*it)->CreateProp();
      Err("created one");
    }
  }
}

void Property::CreateProp() {
  if (gprop_)
    Err("Property already created");
  CreatePropImpl();
  if (delegate_) {
    parent_->PropProvider()->register_handlers_fn(
        parent_->PropProviderData(),
        gprop_,
        this,
        &StaticHandleGesturesPropWillRead,
        &StaticHandleGesturesPropWritten);
  }
}

void Property::DestroyProp() {
  if (!gprop_) {
    Err("gprop_ already freed!");
    return;
  }
  parent_->PropProvider()->free_fn(parent_->PropProviderData(), gprop_);
  gprop_ = NULL;
}

void BoolProperty::CreatePropImpl() {
  gprop_ = parent_->PropProvider()->create_bool_fn(
      parent_->PropProviderData(),
      name(),
      &val_,
      val_);
}

std::string BoolProperty::Value() {
  return StringPrintf("%s", val_ ? "true" : "false");
}

bool BoolProperty::SetValue(::Value* value) {
  if (value->GetType() != Value::TYPE_BOOLEAN) {
    return false;
  }
  FundamentalValue* type_value = static_cast<FundamentalValue*>(value);
  bool val = false;
  if (!type_value->GetAsBoolean(&val))
    return false;
  val_ = static_cast<GesturesPropBool>(val);
  return true;
}

void BoolProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->BoolWasWritten(this);
}

void DoubleProperty::CreatePropImpl() {
  gprop_ = parent_->PropProvider()->create_real_fn(
      parent_->PropProviderData(),
      name(),
      &val_,
      val_);
}

std::string DoubleProperty::Value() {
  return StringPrintf("%f", val_);
}

bool DoubleProperty::SetValue(::Value* value) {
  if (value->GetType() != Value::TYPE_DOUBLE) {
    return false;
  }
  FundamentalValue* type_value = static_cast<FundamentalValue*>(value);
  return type_value->GetAsDouble(&val_);
}

void DoubleProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->DoubleWasWritten(this);
}

void IntProperty::CreatePropImpl() {
  gprop_ = parent_->PropProvider()->create_int_fn(
      parent_->PropProviderData(),
      name(),
      &val_,
      val_);
}

std::string IntProperty::Value() {
  return StringPrintf("%d", val_);
}

bool IntProperty::SetValue(::Value* value) {
  if (value->GetType() != Value::TYPE_INTEGER) {
    return false;
  }
  FundamentalValue* type_value = static_cast<FundamentalValue*>(value);
  return type_value->GetAsInteger(&val_);
}

void IntProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->IntWasWritten(this);
}

void ShortProperty::CreatePropImpl() {
  gprop_ = parent_->PropProvider()->create_short_fn(
      parent_->PropProviderData(),
      name(),
      &val_,
      val_);
}

std::string ShortProperty::Value() {
  return StringPrintf("%d", val_);
}

bool ShortProperty::SetValue(::Value* value) {
  if (value->GetType() != Value::TYPE_INTEGER) {
    return false;
  }
  FundamentalValue* type_value = static_cast<FundamentalValue*>(value);
  int val;
  if (!type_value->GetAsInteger(&val))
    return false;
  val_ = static_cast<short>(val);
  return true;
}

void ShortProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->ShortWasWritten(this);
}

void StringProperty::CreatePropImpl() {
  gprop_ = parent_->PropProvider()->create_string_fn(
      parent_->PropProviderData(),
      name(),
      &val_,
      val_);
}

std::string StringProperty::Value() {
  return StringPrintf("\"%s\"", val_);
}

bool StringProperty::SetValue(::Value* value) {
  if (value->GetType() != Value::TYPE_STRING) {
    return false;
  }
  FundamentalValue* type_value = static_cast<FundamentalValue*>(value);
  if (!type_value->GetAsString(&parsed_val_))
    return false;
  val_ = parsed_val_.c_str();
  return true;
}

void StringProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->StringWasWritten(this);
}

}  // namespace gestures
