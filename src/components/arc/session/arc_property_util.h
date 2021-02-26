// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_PROPERTY_UTIL_H_
#define COMPONENTS_ARC_SESSION_ARC_PROPERTY_UTIL_H_

#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace base {
class FilePath;
}  // namespace base

namespace arc {

// A class that reads and parses |kArcBuildProperties| command line flags and
// holds the result as a dictionary. This is a drop-in replacement of the
// brillo::CrosConfigInterface classes (which are not available in Chromium).
class CrosConfig {
 public:
  CrosConfig();
  virtual ~CrosConfig();
  CrosConfig(const CrosConfig&) = delete;
  CrosConfig& operator=(const CrosConfig&) = delete;

  // Find the |property| in the dictionary and assigns the result to |val_out|.
  // Returns true when the property is found. The function always returns false
  // when |path| is not |kCrosConfigPropertiesPath|.
  virtual bool GetString(const std::string& path,
                         const std::string& property,
                         std::string* val_out);

 private:
  base::Optional<base::Value> info_;
};

// Expands the contents of a template Android property file.  Strings like
// {property} will be looked up in |config| and replaced with their values.
// Returns true if all {} strings were successfully expanded, or false if any
// properties were not found.
bool ExpandPropertyContentsForTesting(const std::string& content,
                                      CrosConfig* config,
                                      std::string* expanded_content);

// Truncates the value side of an Android key=val property line, including
// handling the special case of build fingerprint.
bool TruncateAndroidPropertyForTesting(const std::string& line,
                                       std::string* truncated);

// Expands properties (i.e. {property-name}) in |input| with the dictionary
// |config| provides, and writes the results to |output|. Returns true if the
// output file is successfully written.
bool ExpandPropertyFileForTesting(const base::FilePath& input,
                                  const base::FilePath& output,
                                  CrosConfig* config);

// Calls ExpandPropertyFile for {build,default,vendor_build}.prop files in
// |source_path|. Expanded files are written in |dest_path|. Returns true on
// success. When |single_file| is true, only one file (|dest_path| itself) is
// written. All expanded properties are included in the single file.
// When |add_native_bridge_64_bit_support| is true, add / modify some properties
// related to supported CPU ABIs.
bool ExpandPropertyFiles(const base::FilePath& source_path,
                         const base::FilePath& dest_path,
                         bool single_file,
                         bool add_native_bridge_64bit_support);

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_PROPERTY_UTIL_H_
