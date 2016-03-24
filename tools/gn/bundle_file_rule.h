// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUNDLE_FILE_RULE_H_
#define TOOLS_GN_BUNDLE_FILE_RULE_H_

#include <vector>

#include "tools/gn/source_file.h"
#include "tools/gn/substitution_pattern.h"

class BundleData;
class Settings;
class SourceFile;
class OutputFile;

// BundleFileRule contains the information found in a "bundle_data" target.
class BundleFileRule {
 public:
  BundleFileRule(const std::vector<SourceFile> sources,
                 const SubstitutionPattern& pattern);
  BundleFileRule(const BundleFileRule& other);
  ~BundleFileRule();

  // Applies the substitution pattern to a source file, returning the result
  // as either a SourceFile or an OutputFile.
  SourceFile ApplyPatternToSource(const Settings* settings,
                                  const BundleData& bundle_data,
                                  const SourceFile& source_file) const;
  OutputFile ApplyPatternToSourceAsOutputFile(
      const Settings* settings,
      const BundleData& bundle_data,
      const SourceFile& source_file) const;

  // Returns the list of SourceFiles.
  const std::vector<SourceFile>& sources() const { return sources_; }

 private:
  std::vector<SourceFile> sources_;
  SubstitutionPattern pattern_;
};

#endif  // TOOLS_GN_BUNDLE_FILE_RULE_H_
