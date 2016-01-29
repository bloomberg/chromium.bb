// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VISUAL_STUDIO_WRITER_H_
#define TOOLS_GN_VISUAL_STUDIO_WRITER_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace base {
class FilePath;
}

class Builder;
class BuildSettings;
class Err;
class Target;

class VisualStudioWriter {
 public:
  // On failure will populate |err| and will return false.
  static bool RunAndWriteFiles(const BuildSettings* build_settings,
                               Builder* builder,
                               Err* err);

 private:
  FRIEND_TEST_ALL_PREFIXES(VisualStudioWriterTest, ResolveSolutionFolders);
  FRIEND_TEST_ALL_PREFIXES(VisualStudioWriterTest,
                           ResolveSolutionFolders_AbsPath);

  // Solution project or folder.
  struct SolutionEntry {
    SolutionEntry(const std::string& name,
                  const std::string& path,
                  const std::string& guid);

    ~SolutionEntry();

    // Entry name. For projects must be unique in the solution.
    std::string name;
    // Absolute project file or folder directory path.
    std::string path;
    // Absolute label dir path (only for projects).
    std::string label_dir_path;
    // GUID-like string.
    std::string guid;
    // Pointer to parent folder. nullptr if entry has no parent.
    SolutionEntry* parent_folder;
  };

  using SolutionEntries = std::vector<SolutionEntry*>;

  explicit VisualStudioWriter(const BuildSettings* build_settings);
  ~VisualStudioWriter();

  bool WriteProjectFiles(const Target* target, Err* err);
  bool WriteProjectFileContents(std::ostream& out,
                                const SolutionEntry& solution_project,
                                const Target* target,
                                Err* err);
  void WriteFiltersFileContents(std::ostream& out, const Target* target);
  bool WriteSolutionFile(Err* err);
  void WriteSolutionFileContents(std::ostream& out,
                                 const base::FilePath& solution_dir_path);

  // Resolves all solution folders (parent folders for projects) into |folders_|
  // and updates |root_folder_dir_|. Also sets |parent_folder| for |projects_|.
  void ResolveSolutionFolders();

  const BuildSettings* build_settings_;

  // Indicates if project files are generated for Debug mode configuration.
  bool is_debug_config_;

  // Platform for projects configuration (Win32, x64).
  std::string config_platform_;

  // All projects contained by solution.
  SolutionEntries projects_;

  // Absolute root solution folder path.
  std::string root_folder_path_;

  // Folders for all solution projects.
  SolutionEntries folders_;

  // Semicolon-separated Windows SDK include directories.
  std::string windows_kits_include_dirs_;

  DISALLOW_COPY_AND_ASSIGN(VisualStudioWriter);
};

#endif  // TOOLS_GN_VISUAL_STUDIO_WRITER_H_
