// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FEATURE_ENTRY_MACROS_H_
#define COMPONENTS_FLAGS_UI_FEATURE_ENTRY_MACROS_H_


// Macros to simplify specifying the type of FeatureEntry. Please refer to
// the comments on FeatureEntry::Type in feature_entry.h, which explain the
// different entry types and when they should be used.
#define SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value)  \
  flags_ui::FeatureEntry::SINGLE_VALUE, {                               \
    .switches = { command_line_switch, switch_value, nullptr, nullptr } \
  }
#define SINGLE_VALUE_TYPE(command_line_switch) \
  SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define ORIGIN_LIST_VALUE_TYPE(command_line_switch, switch_value)       \
  flags_ui::FeatureEntry::ORIGIN_LIST_VALUE, {                          \
    .switches = { command_line_switch, switch_value, nullptr, nullptr } \
  }
#define SINGLE_DISABLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value) \
  flags_ui::FeatureEntry::SINGLE_DISABLE_VALUE, {                              \
    .switches = { command_line_switch, switch_value, nullptr, nullptr }        \
  }
#define SINGLE_DISABLE_VALUE_TYPE(command_line_switch) \
  SINGLE_DISABLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, enable_value,       \
                                            disable_switch, disable_value)     \
  flags_ui::FeatureEntry::ENABLE_DISABLE_VALUE, {                              \
    .switches = { enable_switch, enable_value, disable_switch, disable_value } \
  }
#define ENABLE_DISABLE_VALUE_TYPE(enable_switch, disable_switch) \
  ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, "", disable_switch, "")
#define MULTI_VALUE_TYPE(choices_list) \
  flags_ui::FeatureEntry::MULTI_VALUE, { .choices = choices_list }
#define FEATURE_VALUE_TYPE(feature_entry)      \
  flags_ui::FeatureEntry::FEATURE_VALUE, {     \
    .feature = { &feature_entry, {}, nullptr } \
  }
#define FEATURE_WITH_PARAMS_VALUE_TYPE(feature_entry, feature_variations, \
                                       feature_trial)                     \
  flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE, {                    \
    .feature = { &feature_entry, feature_variations, feature_trial }      \
  }

#endif  // COMPONENTS_FLAGS_UI_FEATURE_ENTRY_MACROS_H_
