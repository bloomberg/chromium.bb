// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUNDLE_DATA_H_
#define TOOLS_GN_BUNDLE_DATA_H_

#include <string>
#include <vector>

#include "tools/gn/bundle_file_rule.h"
#include "tools/gn/unique_vector.h"

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
  using UniqueTargets = UniqueVector<const Target*>;
  using SourceFiles = std::vector<SourceFile>;
  using OutputFiles = std::vector<OutputFile>;
  using BundleFileRules = std::vector<BundleFileRule>;

  BundleData();
  ~BundleData();

  // Adds a bundle_data target to the recursive collection of all bundle_data
  // that the target depends on.
  void AddBundleData(const Target* target);

  // Called upon resolution of the target owning this instance of BundleData.
  // |owning_target| is the owning target.
  void OnTargetResolved(Target* owning_target);

  // Returns the list of inputs.
  void GetSourceFiles(SourceFiles* sources) const;

  // Returns the list of outputs.
  void GetOutputFiles(const Settings* settings,
                      OutputFiles* outputs) const;

  // Returns the list of outputs as SourceFile.
  void GetOutputsAsSourceFiles(
      const Settings* settings,
      SourceFiles* outputs_as_source) const;

  // Returns the path to the compiled asset catalog. Only valid if
  // asset_catalog_sources() is not empty.
  SourceFile GetCompiledAssetCatalogPath() const;

  // Returns the path to the top-level directory of the bundle. This is
  // based on root_dir(), but since that can be Bundle.app/Contents/ or
  // any other subpath, this is just the most top-level directory (e.g.,
  // just Bundle.app/).
  //
  // Note that this is a SourceFile instead of a SourceDir. This is because
  // the output of a create_bundle rule is a single logical unit, even though
  // it is really a directory containing many outputs. This allows other
  // targets to treat the bundle as a single unit, rather than a collection
  // of its contents.
  SourceFile GetBundleRootDirOutput(const Settings* settings) const;

  // Returns the list of inputs for the compilation of the asset catalog.
  SourceFiles& asset_catalog_sources() { return asset_catalog_sources_; }
  const SourceFiles& asset_catalog_sources() const {
    return asset_catalog_sources_;
  }

  BundleFileRules& file_rules() { return file_rules_; }
  const BundleFileRules& file_rules() const { return file_rules_; }

  std::string& root_dir() { return root_dir_; }
  const std::string& root_dir() const { return root_dir_; }

  std::string& resources_dir() { return resources_dir_; }
  const std::string& resources_dir() const { return resources_dir_; }

  std::string& executable_dir() { return executable_dir_; }
  const std::string& executable_dir() const { return executable_dir_; }

  std::string& plugins_dir() { return plugins_dir_; }
  const std::string& plugins_dir() const { return plugins_dir_; }

  // Recursive collection of all bundle_data that the target depends on.
  const UniqueTargets& bundle_deps() const { return bundle_deps_; }

 private:
  SourceFiles asset_catalog_sources_;
  BundleFileRules file_rules_;
  UniqueTargets bundle_deps_;

  // All those values are subdirectories relative to root_build_dir, and apart
  // from root_dir, they are either equal to root_dir_ or subdirectories of it.
  std::string root_dir_;
  std::string resources_dir_;
  std::string executable_dir_;
  std::string plugins_dir_;
};

#endif  // TOOLS_GN_BUNDLE_DATA_H_
