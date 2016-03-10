// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUNDLE_DATA_H_
#define TOOLS_GN_BUNDLE_DATA_H_

#include <string>
#include <vector>

#include "tools/gn/bundle_file_rule.h"

class OutputFile;
class SourceFile;
class Settings;
class Target;

// Returns true if |source| correspond to the path of a file in an asset
// catalog. If defined |asset_catalog| is set to its path.
//
// An asset catalog is an OS X bundle with the ".xcassets" extension. It
// contains one directory per assets each of them with the ".imageset"
// extension.
//
// All asset catalogs are compiled by Xcode into single Assets.car file as
// part of the creation of an application or framework bundle. BundleData
// emulates this with the "compile_xcassets" tool.
bool IsSourceFileFromAssetCatalog(const SourceFile& source,
                                  SourceFile* asset_catalog);

// BundleData holds the information required by "create_bundle" target.
class BundleData {
 public:
  BundleData();
  ~BundleData();

  // Extracts the information required from a "bundle_data" target.
  void AddFileRuleFromTarget(const Target* target);

  // Returns the list of inputs.
  void GetSourceFiles(std::vector<SourceFile>* sources) const;

  // Returns the list of outputs.
  void GetOutputFiles(const Settings* settings,
                      std::vector<OutputFile>* outputs) const;

  // Returns the list of outputs as SourceFile.
  void GetOutputsAsSourceFiles(
      const Settings* settings,
      std::vector<SourceFile>* outputs_as_source) const;

  // Returns the path to the compiled asset catalog. Only valid if
  // asset_catalog_sources() is not empty.
  SourceFile GetCompiledAssetCatalogPath() const;

  // Returns the list of inputs for the compilation of the asset catalog.
  std::vector<SourceFile>& asset_catalog_sources() {
    return asset_catalog_sources_;
  }
  const std::vector<SourceFile>& asset_catalog_sources() const {
    return asset_catalog_sources_;
  }

  std::vector<BundleFileRule>& file_rules() { return file_rules_; }
  const std::vector<BundleFileRule>& file_rules() const { return file_rules_; }

  std::string& root_dir() { return root_dir_; }
  const std::string& root_dir() const { return root_dir_; }

  std::string& resources_dir() { return resources_dir_; }
  const std::string& resources_dir() const { return resources_dir_; }

  std::string& executable_dir() { return executable_dir_; }
  const std::string& executable_dir() const { return executable_dir_; }

  std::string& plugins_dir() { return plugins_dir_; }
  const std::string& plugins_dir() const { return plugins_dir_; }

 private:
  std::vector<SourceFile> asset_catalog_sources_;
  std::vector<BundleFileRule> file_rules_;

  // All those values are subdirectories relative to root_build_dir, and apart
  // from root_dir, they are either equal to root_dir_ or subdirectories of it.
  std::string root_dir_;
  std::string resources_dir_;
  std::string executable_dir_;
  std::string plugins_dir_;
};

#endif  // TOOLS_GN_BUNDLE_DATA_H_
