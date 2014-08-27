// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_writer.h"

#include "tools/gn/builder.h"
#include "tools/gn/loader.h"
#include "tools/gn/location.h"
#include "tools/gn/ninja_build_writer.h"
#include "tools/gn/ninja_toolchain_writer.h"
#include "tools/gn/settings.h"

NinjaWriter::NinjaWriter(const BuildSettings* build_settings,
                         Builder* builder)
    : build_settings_(build_settings),
      builder_(builder) {
}

NinjaWriter::~NinjaWriter() {
}

// static
bool NinjaWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                   Builder* builder) {
  NinjaWriter writer(build_settings, builder);

  std::vector<const Settings*> all_settings;
  std::vector<const Target*> default_targets;
  if (!writer.WriteToolchains(&all_settings, &default_targets))
    return false;
  return writer.WriteRootBuildfiles(all_settings, default_targets);
}

// static
bool NinjaWriter::RunAndWriteToolchainFiles(
    const BuildSettings* build_settings,
    Builder* builder,
    std::vector<const Settings*>* all_settings) {
  NinjaWriter writer(build_settings, builder);
  std::vector<const Target*> default_targets;
  return writer.WriteToolchains(all_settings, &default_targets);
}

bool NinjaWriter::WriteToolchains(std::vector<const Settings*>* all_settings,
                                  std::vector<const Target*>* default_targets) {
  // Categorize all targets by toolchain.
  typedef std::map<Label, std::vector<const Target*> > CategorizedMap;
  CategorizedMap categorized;

  std::vector<const BuilderRecord*> all_records = builder_->GetAllRecords();
  for (size_t i = 0; i < all_records.size(); i++) {
    if (all_records[i]->type() == BuilderRecord::ITEM_TARGET &&
        all_records[i]->should_generate()) {
      categorized[all_records[i]->label().GetToolchainLabel()].push_back(
          all_records[i]->item()->AsTarget());
      }
  }
  if (categorized.empty()) {
    Err(Location(), "No targets.",
        "I could not find any targets to write, so I'm doing nothing.")
        .PrintToStdout();
    return false;
  }

  Label default_label = builder_->loader()->GetDefaultToolchain();

  // Write out the toolchain buildfiles, and also accumulate the set of
  // all settings and find the list of targets in the default toolchain.
  for (CategorizedMap::const_iterator i = categorized.begin();
       i != categorized.end(); ++i) {
    const Settings* settings =
        builder_->loader()->GetToolchainSettings(i->first);
    const Toolchain* toolchain = builder_->GetToolchain(i->first);

    all_settings->push_back(settings);
    if (!NinjaToolchainWriter::RunAndWriteFile(settings, toolchain,
                                               i->second)) {
      Err(Location(),
          "Couldn't open toolchain buildfile(s) for writing").PrintToStdout();
      return false;
    }
  }

  *default_targets = categorized[default_label];
  return true;
}

bool NinjaWriter::WriteRootBuildfiles(
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_targets) {
  // All Settings objects should have the same default toolchain, and there
  // should always be at least one settings object in the build.
  CHECK(!all_settings.empty());
  const Toolchain* default_toolchain =
      builder_->GetToolchain(all_settings[0]->default_toolchain_label());

  // Write the root buildfile.
  if (!NinjaBuildWriter::RunAndWriteFile(build_settings_, all_settings,
                                         default_toolchain, default_targets)) {
    Err(Location(),
        "Couldn't open toolchain buildfile(s) for writing").PrintToStdout();
    return false;
  }
  return true;
}
