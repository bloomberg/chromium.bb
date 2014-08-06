// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/value_extractors.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/label.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/target.h"
#include "tools/gn/value.h"

namespace {

// Sets the error and returns false on failure.
template<typename T, class Converter>
bool ListValueExtractor(const Value& value,
                        std::vector<T>* dest,
                        Err* err,
                        const Converter& converter) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;
  const std::vector<Value>& input_list = value.list_value();
  dest->resize(input_list.size());
  for (size_t i = 0; i < input_list.size(); i++) {
    if (!converter(input_list[i], &(*dest)[i], err))
      return false;
  }
  return true;
}

// Like the above version but extracts to a UniqueVector and sets the error if
// there are duplicates.
template<typename T, class Converter>
bool ListValueUniqueExtractor(const Value& value,
                              UniqueVector<T>* dest,
                              Err* err,
                              const Converter& converter) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;
  const std::vector<Value>& input_list = value.list_value();

  for (size_t i = 0; i < input_list.size(); i++) {
    T new_one;
    if (!converter(input_list[i], &new_one, err))
      return false;
    if (!dest->push_back(new_one)) {
      // Already in the list, throw error.
      *err = Err(input_list[i], "Duplicate item in list");
      size_t previous_index = dest->IndexOf(new_one);
      err->AppendSubErr(Err(input_list[previous_index],
                            "This was the previous definition."));
      return false;
    }
  }
  return true;
}

// This extractor rejects files with system-absolute file paths. If we need
// that in the future, we'll have to add some flag to control this.
struct RelativeFileConverter {
  RelativeFileConverter(const BuildSettings* build_settings_in,
                        const SourceDir& current_dir_in)
      : build_settings(build_settings_in),
        current_dir(current_dir_in) {
  }
  bool operator()(const Value& v, SourceFile* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    *out = current_dir.ResolveRelativeFile(v.string_value(),
                                           build_settings->root_path_utf8());
    if (out->is_system_absolute()) {
      *err = Err(v, "System-absolute file path.",
          "You can't list a system-absolute file path here. Please include "
          "only files in\nthe source tree. Maybe you meant to begin with two "
          "slashes to indicate an\nabsolute path in the source tree?");
      return false;
    }
    return true;
  }
  const BuildSettings* build_settings;
  const SourceDir& current_dir;
};

struct RelativeDirConverter {
  RelativeDirConverter(const BuildSettings* build_settings_in,
                       const SourceDir& current_dir_in)
      : build_settings(build_settings_in),
        current_dir(current_dir_in) {
  }
  bool operator()(const Value& v, SourceDir* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    *out = current_dir.ResolveRelativeDir(v.string_value(),
                                          build_settings->root_path_utf8());
    return true;
  }
  const BuildSettings* build_settings;
  const SourceDir& current_dir;
};

// Fills the label part of a LabelPtrPair, leaving the pointer null.
template<typename T> struct LabelResolver {
  LabelResolver(const SourceDir& current_dir_in,
                const Label& current_toolchain_in)
      : current_dir(current_dir_in),
        current_toolchain(current_toolchain_in) {}
  bool operator()(const Value& v, LabelPtrPair<T>* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    out->label = Label::Resolve(current_dir, current_toolchain, v, err);
    out->origin = v.origin();
    return !err->has_error();
  }
  const SourceDir& current_dir;
  const Label& current_toolchain;
};

}  // namespace

bool ExtractListOfStringValues(const Value& value,
                               std::vector<std::string>* dest,
                               Err* err) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;
  const std::vector<Value>& input_list = value.list_value();
  dest->reserve(input_list.size());
  for (size_t i = 0; i < input_list.size(); i++) {
    if (!input_list[i].VerifyTypeIs(Value::STRING, err))
      return false;
    dest->push_back(input_list[i].string_value());
  }
  return true;
}

bool ExtractListOfRelativeFiles(const BuildSettings* build_settings,
                                const Value& value,
                                const SourceDir& current_dir,
                                std::vector<SourceFile>* files,
                                Err* err) {
  return ListValueExtractor(value, files, err,
                            RelativeFileConverter(build_settings, current_dir));
}

bool ExtractListOfRelativeDirs(const BuildSettings* build_settings,
                               const Value& value,
                               const SourceDir& current_dir,
                               std::vector<SourceDir>* dest,
                               Err* err) {
  return ListValueExtractor(value, dest, err,
                            RelativeDirConverter(build_settings, current_dir));
}

bool ExtractListOfLabels(const Value& value,
                         const SourceDir& current_dir,
                         const Label& current_toolchain,
                         LabelTargetVector* dest,
                         Err* err) {
  return ListValueExtractor(value, dest, err,
                            LabelResolver<Target>(current_dir,
                                                  current_toolchain));
}

bool ExtractListOfUniqueLabels(const Value& value,
                               const SourceDir& current_dir,
                               const Label& current_toolchain,
                               UniqueVector<LabelConfigPair>* dest,
                               Err* err) {
  return ListValueUniqueExtractor(value, dest, err,
                                  LabelResolver<Config>(current_dir,
                                                        current_toolchain));
}

bool ExtractListOfUniqueLabels(const Value& value,
                               const SourceDir& current_dir,
                               const Label& current_toolchain,
                               UniqueVector<LabelTargetPair>* dest,
                               Err* err) {
  return ListValueUniqueExtractor(value, dest, err,
                                  LabelResolver<Target>(current_dir,
                                                        current_toolchain));
}

bool ExtractRelativeFile(const BuildSettings* build_settings,
                         const Value& value,
                         const SourceDir& current_dir,
                         SourceFile* file,
                         Err* err) {
  RelativeFileConverter converter(build_settings, current_dir);
  return converter(value, file, err);
}
