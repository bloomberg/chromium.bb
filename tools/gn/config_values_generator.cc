// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/config_values_generator.h"

#include "tools/gn/config_values.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

namespace {

void GetStringList(
    Scope* scope,
    const char* var_name,
    ConfigValues* config_values,
    std::vector<std::string>& (ConfigValues::* accessor)(),
    Err* err) {
  const Value* value = scope->GetValue(var_name, true);
  if (!value)
    return;  // No value, empty input and succeed.

  std::vector<std::string> result;
  ExtractListOfStringValues(*value, &result, err);
  (config_values->*accessor)().swap(result);
}

void GetDirList(
    Scope* scope,
    const char* var_name,
    ConfigValues* config_values,
    const SourceDir input_dir,
    std::vector<SourceDir>& (ConfigValues::* accessor)(),
    Err* err) {
  const Value* value = scope->GetValue(var_name, true);
  if (!value)
    return;  // No value, empty input and succeed.

  std::vector<SourceDir> result;
  ExtractListOfRelativeDirs(scope->settings()->build_settings(),
                            *value, input_dir, &result, err);
  (config_values->*accessor)().swap(result);
}

}  // namespace

ConfigValuesGenerator::ConfigValuesGenerator(
    ConfigValues* dest_values,
    Scope* scope,
    const SourceDir& input_dir,
    Err* err)
    : config_values_(dest_values),
      scope_(scope),
      input_dir_(input_dir),
      err_(err) {
}

ConfigValuesGenerator::~ConfigValuesGenerator() {
}

void ConfigValuesGenerator::Run() {
#define FILL_STRING_CONFIG_VALUE(name) \
    GetStringList(scope_, #name, config_values_, &ConfigValues::name, err_);
#define FILL_DIR_CONFIG_VALUE(name) \
    GetDirList(scope_, #name, config_values_, input_dir_, \
               &ConfigValues::name, err_);

  FILL_STRING_CONFIG_VALUE(cflags)
  FILL_STRING_CONFIG_VALUE(cflags_c)
  FILL_STRING_CONFIG_VALUE(cflags_cc)
  FILL_STRING_CONFIG_VALUE(cflags_objc)
  FILL_STRING_CONFIG_VALUE(cflags_objcc)
  FILL_STRING_CONFIG_VALUE(defines)
  FILL_DIR_CONFIG_VALUE(   include_dirs)
  FILL_STRING_CONFIG_VALUE(ldflags)
  FILL_DIR_CONFIG_VALUE(   lib_dirs)
  FILL_STRING_CONFIG_VALUE(libs)

#undef FILL_STRING_CONFIG_VALUE
#undef FILL_DIR_CONFIG_VALUE
}
