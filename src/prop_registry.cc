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

void StringProperty::HandleGesturesPropWritten() {
  if (delegate_)
    delegate_->StringWasWritten(this);
}

}  // namespace gestures
