// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/ninja_writer.h"
#include "tools/gn/output_file.h"
#include "tools/gn/path_output.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace commands {

namespace {

// Suppress output on success.
const char kSwitchQuiet[] = "q";

// Skip actually executing GYP. This is for when you're working on the GN
// build and don't want to wait for GYP to regenerate. All GN files are
// regenerated, but the GYP ones are not.
const char kSwitchNoGyp[] = "no-gyp";

// Where to have GYP write its outputs.
const char kDirOut[] = "out.gn";
const char kSourceDirOut[] = "//out.gn/";

// We'll do the GN build to here.
const char kBuildSourceDir[] = "//out.gn/Debug/";

// File that GYP will write dependency information to.
const char kGypDepsSourceFileName[] = "//out.gn/gyp_deps.txt";

void TargetResolvedCallback(base::subtle::Atomic32* write_counter,
                            const Target* target) {
  base::subtle::NoBarrier_AtomicIncrement(write_counter, 1);
  NinjaTargetWriter::RunAndWriteFile(target);
}

bool SimpleNinjaParse(const std::string& data,
                      std::set<std::string>* subninjas,
                      size_t* first_subninja_offset) {
  const size_t kSubninjaPrefixLen = 10;
  const char kSubninjaPrefix[kSubninjaPrefixLen + 1] = "\nsubninja ";

  *first_subninja_offset = std::string::npos;
  size_t next_subninja = 0;
  while ((next_subninja = data.find(kSubninjaPrefix, next_subninja)) !=
         std::string::npos) {
    if (*first_subninja_offset == std::string::npos)
      *first_subninja_offset = next_subninja;

    size_t line_end = data.find('\n', next_subninja + 1);
    if (line_end == std::string::npos)
      return false;

    std::string filename = data.substr(
        next_subninja + kSubninjaPrefixLen,
        line_end - next_subninja - kSubninjaPrefixLen);
    subninjas->insert(filename);

    next_subninja = line_end;
  }
  return *first_subninja_offset != std::string::npos;
}

bool FixupBuildNinja(const BuildSettings* build_settings,
                     const base::FilePath& buildfile) {
  std::string contents;
  if (!base::ReadFileToString(buildfile, &contents)) {
    Err(Location(), "Could not load " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }

  std::set<std::string> subninjas;
  size_t first_subninja_offset = 0;
  if (!SimpleNinjaParse(contents, &subninjas, &first_subninja_offset)) {
    Err(Location(), "Could not parse " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }

  // Write toolchain files.
  std::vector<const Settings*> all_settings;
  if (!NinjaWriter::RunAndWriteToolchainFiles(
          build_settings, subninjas, &all_settings))
    return false;

  // Copy first part of buildfile to the output.
  std::ofstream file;
  file.open(FilePathToUTF8(buildfile).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail()) {
    Err(Location(), "Could not write " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }
  file.write(contents.data(), first_subninja_offset);

  // Add refs for our toolchains to the original build.ninja.
  NinjaHelper helper(build_settings);
  PathOutput path_output(build_settings->build_dir(), ESCAPE_NINJA, true);
  file << "\n# GN-added toolchain files.\n";
  for (size_t i = 0; i < all_settings.size(); i++) {
    file << "subninja ";
    path_output.WriteFile(file,
                          helper.GetNinjaFileForToolchain(all_settings[i]));
    file << std::endl;
  }
  file << "\n# GYP-written subninjas.";

  // Write remaining old subninjas from original file.
  file.write(&contents[first_subninja_offset],
             contents.size() - first_subninja_offset);
  return true;
}

bool RunGyp(const BuildSettings* build_settings) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kSwitchQuiet))
    OutputString("Running GYP...\n");

  const base::FilePath& python_path = build_settings->python_path();

  // Construct the command line. Note that AppendArgPath and AppendSwitchPath
  // don't preserve the relative ordering, and we need the python file to be
  // first, so we have to convert switch values to strings before appending.
  //
  // Note that GYP will get confused if this path is quoted, so don't quote it
  // and hope that there are no spaces!
  CommandLine cmdline(python_path);
  cmdline.AppendArgPath(
      build_settings->GetFullPath(SourceFile("//build/gyp_chromium.py")));

  // Override the default output directory so we can coexist in parallel
  // with a normal Ninja GYP build.
  cmdline.AppendArg("-G");
  cmdline.AppendArg(std::string("output_dir=") + kDirOut);

  // Force the Ninja generator.
  cmdline.AppendArg("-f");
  cmdline.AppendArg("ninja");

  // Write deps for libraries so we can pick them up.
  cmdline.AppendArg("-G");
  cmdline.AppendArg("link_deps_file=" + FilePathToUTF8(
      build_settings->GetFullPath(SourceFile(kGypDepsSourceFileName))));

  std::string output;
  if (!base::GetAppOutput(cmdline, &output)) {
    Err(Location(), "GYP execution failed.", output).PrintToStdout();
    return false;
  }
  return true;
}

}  // namespace

// Converts a GYP qualified target which looks like:
// "/home/you/src/third_party/icu/icu.gyp:icui18n#target" to a GN label like
// "//third_party/icu:icui18n". On failure returns an empty label and sets the
// error.
Label GypQualifiedTargetToLabel(const std::string& source_root_prefix,
                                const base::StringPiece& target,
                                Err* err) {
  // Prefix should end in canonical path separator.
  const char kSep = static_cast<char>(base::FilePath::kSeparators[0]);
  DCHECK(source_root_prefix[source_root_prefix.size() - 1] == kSep);

  if (!target.starts_with(source_root_prefix)) {
    *err = Err(Location(), "GYP deps parsing failed.",
        "The line was \"" + target.as_string() + "\" and it should have "
        "started with \"" + source_root_prefix + "\"");
    return Label();
  }

  size_t begin = source_root_prefix.size();
  size_t colon = target.find(':', begin);
  if (colon == std::string::npos) {
    *err = Err(Location(), "Expected :", target.as_string());
    return Label();
  }

  size_t octothorpe = target.find('#', colon);
  if (octothorpe == std::string::npos) {
    *err = Err(Location(), "Expected #", target.as_string());
    return Label();
  }

  // This will look like "third_party/icu/icu.gyp"
  base::StringPiece gyp_file = target.substr(begin, colon - begin);

  // Strip the file name from the end to get "third_party/icu".
  size_t last_sep = gyp_file.find_last_of(kSep);
  if (last_sep == std::string::npos) {
    *err = Err(Location(), "Expected path separator.", target.as_string());
    return Label();
  }
  base::StringPiece path = gyp_file.substr(0, last_sep);
  SourceDir dir("//" + path.as_string());

  base::StringPiece name = target.substr(colon + 1, octothorpe - colon - 1);

  return Label(dir, name);
}

// Parses the link deps file, filling the given map. Returns true on sucess.
// On failure fills the error and returns false.
//
// Example format for each line:
//   /home/you/src/third_party/icu/icu.gyp:icui18n#target lib/libi18n.so
bool ParseLinkDepsFile(const BuildSettings* build_settings,
                       const std::string& contents,
                       BuildSettings::AdditionalLibsMap* deps,
                       Err* err) {
  std::string source_root_prefix = FilePathToUTF8(build_settings->root_path());
  source_root_prefix.push_back(base::FilePath::kSeparators[0]);

  size_t cur = 0;
  while (cur < contents.size()) {
    // The source file is everything up to the space.
    size_t space = contents.find(' ', cur);
    if (space == std::string::npos)
      break;
    Label source(GypQualifiedTargetToLabel(
        source_root_prefix,
        base::StringPiece(&contents[cur], space - cur),
        err));
    if (source.is_null())
      return false;

    // The library file is everything between the space and EOL.
    cur = space + 1;
    size_t eol = contents.find('\n', cur);
    if (eol == std::string::npos) {
      *err = Err(Location(), "Expected newline at end of link deps file.");
      return false;
    }
    OutputFile lib(contents.substr(cur, eol - cur));

    deps->insert(std::make_pair(source, lib));
    cur = eol + 1;
  }
  return true;
}

const char kGyp[] = "gyp";
const char kGyp_HelpShort[] =
    "gyp: Run GYP and then GN.";
const char kGyp_Help[] =
    "gyp: Run GYP and then GN.\n"
    "\n"
    "  Generate a hybrid GYP/GN build where some targets are generated by\n"
    "  each of the tools. As long as target names and locations match between\n"
    "  the two tools, they can depend on each other.\n"
    "\n"
    "  When GN is run in this mode, it will not write out any targets\n"
    "  annotated with \"external = true\". Otherwise, GYP targets with the\n"
    "  same name and location will be overwritten.\n"
    "\n"
    "  References to the GN ninja files will be inserted into the\n"
    "  GYP-generated build.ninja file.\n"
    "\n"
    "Option:\n"
    "  --no-gyp\n"
    "      Don't actually run GYP or modify build.ninja. This is used when\n"
    "      working on the GN build when it is known that no GYP files have\n"
    "      changed and you want it to run faster.\n";

// Note: partially duplicated from command_gen.cc.
int RunGyp(const std::vector<std::string>& args) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  bool no_gyp = cmdline->HasSwitch(kSwitchNoGyp);

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return 1;

  setup->build_settings().SetBuildDir(SourceDir(kBuildSourceDir));
  setup->build_settings().set_using_external_generator(true);

  // Provide a way for buildfiles to know we're doing a GYP build.
  /*
  Scope::KeyValueMap variable_overrides;
  variable_overrides["is_gyp"] = Value(NULL, true);
  setup->build_settings().build_args().AddArgOverrides(variable_overrides);
  */

  base::FilePath link_deps_file =
      setup->build_settings().GetFullPath(SourceFile(kGypDepsSourceFileName));
  if (!no_gyp)
    base::DeleteFile(link_deps_file, false);

  base::TimeTicks begin_time = base::TimeTicks::Now();
  if (!no_gyp) {
    if (!RunGyp(&setup->build_settings()))
      return 1;
  }
  base::TimeTicks end_gyp_time = base::TimeTicks::Now();

  // Read in the GYP link dependencies.
  std::string link_deps_contents;
  if (!base::ReadFileToString(link_deps_file, &link_deps_contents)) {
    Err(Location(), "Couldn't load link deps file.",
        FilePathToUTF8(link_deps_file)).PrintToStdout();
    return 1;
  }
  Err err;
  if (!ParseLinkDepsFile(&setup->build_settings(),
                         link_deps_contents,
                         &setup->build_settings().external_link_deps(), &err)) {
    err.PrintToStdout();
    return 1;
  }

  if (!cmdline->HasSwitch(kSwitchQuiet))
    OutputString("Running GN...\n");

  // Cause the load to also generate the ninja files for each target. We wrap
  // the writing to maintain a counter.
  base::subtle::Atomic32 write_counter = 0;
  setup->build_settings().set_target_resolved_callback(
      base::Bind(&TargetResolvedCallback, &write_counter));

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return 1;

  // Integrate with the GYP build.
  if (!no_gyp) {
    base::FilePath ninja_buildfile(setup->build_settings().GetFullPath(
        SourceFile(setup->build_settings().build_dir().value() +
                   "build.ninja")));
    if (!FixupBuildNinja(&setup->build_settings(), ninja_buildfile))
      return 1;
  }

  // Timing info.
  base::TimeTicks end_time = base::TimeTicks::Now();
  if (!cmdline->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    std::string stats = "Wrote " +
        base::IntToString(static_cast<int>(write_counter)) +
        " targets from " +
        base::IntToString(
            setup->scheduler().input_file_manager()->GetInputFileCount()) +
        " files in " +
        base::IntToString((end_time - end_gyp_time).InMilliseconds()) + "ms " +
        "(GYP took " +
        base::IntToString((end_gyp_time - begin_time).InMilliseconds()) +
        "ms)\n";

    OutputString(stats);
  }

  return 0;
}

}  // namespace commands
