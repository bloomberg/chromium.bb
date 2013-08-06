// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_target_writer.h"

#include <fstream>
#include <sstream>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/file_template.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/location.h"
#include "tools/gn/path_output.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

namespace {

static const char kCustomTargetSourceKey[] = "{{source}}";
static const char kCustomTargetSourceNamePartKey[] = "{{source_name_part}}";

struct DefineWriter {
  void operator()(const std::string& s, std::ostream& out) const {
    out << " -D" << s;
  }
};

struct IncludeWriter {
  IncludeWriter(PathOutput& path_output,
                const NinjaHelper& h)
      : helper(h),
        path_output_(path_output),
        old_inhibit_quoting_(path_output.inhibit_quoting()) {
    // Inhibit quoting since we'll put quotes around the whole thing ourselves.
    // Since we're writing in NINJA escaping mode, this won't actually do
    // anything, but I think we may need to change to shell-and-then-ninja
    // escaping for this in the future.
    path_output_.set_inhibit_quoting(true);
  }
  ~IncludeWriter() {
    path_output_.set_inhibit_quoting(old_inhibit_quoting_);
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    out << " \"-I";
    // It's important not to include the trailing slash on directories or on
    // Windows it will be a backslash and the compiler might think we're
    // escaping the quote!
    path_output_.WriteDir(out, d, PathOutput::DIR_NO_LAST_SLASH);
    out << "\"";
  }

  const NinjaHelper& helper;
  PathOutput& path_output_;
  bool old_inhibit_quoting_;  // So we can put the PathOutput back.
};

}  // namespace

NinjaTargetWriter::NinjaTargetWriter(const Target* target, std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   ESCAPE_NINJA, true),
      helper_(settings_->build_settings()) {
}

NinjaTargetWriter::~NinjaTargetWriter() {
}

void NinjaTargetWriter::Run() {
  // TODO(brettw) have a better way to do the environment setup on Windows.
  if (target_->settings()->IsWin())
    out_ << "arch = environment.x86\n";

  if (target_->output_type() == Target::COPY_FILES) {
    WriteCopyRules();
  } else if (target_->output_type() == Target::CUSTOM) {
    WriteCustomRules();
  } else {
    WriteCompilerVars();

    std::vector<OutputFile> obj_files;
    WriteSources(&obj_files);

    WriteLinkerStuff(obj_files);
  }
}

// static
void NinjaTargetWriter::RunAndWriteFile(const Target* target) {
  if (target->output_type() == Target::NONE)
    return;

  const Settings* settings = target->settings();
  NinjaHelper helper(settings->build_settings());

  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForTarget(target).GetSourceFile(
          settings->build_settings())));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Writing", FilePathToUTF8(ninja_file));

  file_util::CreateDirectory(ninja_file.DirName());

  // It's rediculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  std::stringstream file;
  if (file.fail()) {
    g_scheduler->FailWithError(
        Err(Location(), "Error writing ninja file.",
            "Unable to open \"" + FilePathToUTF8(ninja_file) + "\"\n"
            "for writing."));
    return;
  }

  NinjaTargetWriter gen(target, file);
  gen.Run();

  std::string contents = file.str();
  file_util::WriteFile(ninja_file, contents.c_str(), contents.size());
}

void NinjaTargetWriter::WriteCopyRules() {
  // The dest dir should be inside the output dir so we can just remove the
  // prefix and get ninja-relative paths.
  const std::string& output_dir =
      settings_->build_settings()->build_dir().value();
  const std::string& dest_dir = target_->destdir().value();
  DCHECK(StartsWithASCII(dest_dir, output_dir, true));
  std::string relative_dest_dir(&dest_dir[output_dir.size()],
                                dest_dir.size() - output_dir.size());

  const Target::FileList& sources = target_->sources();
  std::vector<OutputFile> dest_files;
  dest_files.reserve(sources.size());

  // Write out rules for each file copied.
  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];

    // The files should have the same name but in the dest dir.
    base::StringPiece name_part = FindFilename(&input_file.value());
    OutputFile dest_file(relative_dest_dir);
    AppendStringPiece(&dest_file.value(), name_part);

    dest_files.push_back(dest_file);

    out_ << "build ";
    path_output_.WriteFile(out_, dest_file);
    out_ << ": copy ";
    path_output_.WriteFile(out_, input_file);
    out_ << std::endl;
  }

  // Write out the rule for the target to copy all of them.
  out_ << std::endl << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": stamp";
  for (size_t i = 0; i < dest_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, dest_files[i]);
  }
  out_ << std::endl;

  // TODO(brettw) need some kind of stamp file for depending on this, as well
  // as order_only=prebuild.
}

void NinjaTargetWriter::WriteCustomRules() {
  // Make a unique name for this rule.
  std::string target_label = target_->label().GetUserVisibleName(true);
  std::string custom_rule_name(target_label);
  ReplaceChars(custom_rule_name, ":/()", "_", &custom_rule_name);
  custom_rule_name.append("_rule");

  // Run the script from the dir of the BUILD file. This has no trailing
  // slash.
  const SourceDir& script_cd = target_->label().dir();
  std::string script_cd_to_root = InvertDir(script_cd);
  if (script_cd_to_root.empty()) {
    script_cd_to_root = ".";
  } else {
    // Remove trailing slash
    DCHECK(script_cd_to_root[script_cd_to_root.size() - 1] == '/');
    script_cd_to_root.resize(script_cd_to_root.size() - 1);
  }

  std::string script_relative_to_cd =
      script_cd_to_root + target_->script().value();

  bool no_sources = target_->sources().empty();

  // Use a unique name for the response file when there are multiple build
  // steps so that they don't stomp on each other.
  std::string rspfile = custom_rule_name;
  if (!no_sources)
    rspfile += ".$unique_name";
  rspfile += ".rsp";

  // First write the custom rule.
  out_ << "rule " << custom_rule_name << std::endl;
  out_ << "  command = $pythonpath gyp-win-tool action-wrapper $arch "
       << rspfile << " ";
  path_output_.WriteDir(out_, script_cd, PathOutput::DIR_NO_LAST_SLASH);
  out_ << std::endl;
  out_ << "  description = CUSTOM " << target_label << std::endl;
  out_ << "  restat = 1" << std::endl;
  out_ << "  rspfile = " << rspfile << std::endl;

  // The build command goes in the rsp file.
  out_ << "  rspfile_content = $pythonpath " << script_relative_to_cd;
  for (size_t i = 0; i < target_->script_args().size(); i++) {
    const std::string& arg = target_->script_args()[i];
    out_ << " ";
    WriteCustomArg(arg);
  }
  out_ << std::endl << std::endl;

  // Precompute the common dependencies for each step. This includes the
  // script itself (changing the script should force a rebuild) and any data
  // files.
  //
  // TODO(brettW) this needs to be re-thought. "data" is supposed to be runtime
  // data (i.e. for tests and such) rather than compile-time dependencies for
  // each target. If we really need this, we need to have a different way to
  // express it.
  //
  // One idea: add an "inputs" variable to specify this kind of thing. We
  // should probably make it an error to specify data but no inputs for a
  // script as a way to catch people doing the wrong way.
  std::ostringstream common_deps_stream;
  path_output_.WriteFile(common_deps_stream, target_->script());
  const Target::FileList& datas = target_->data();
  for (size_t i = 0; i < datas.size(); i++) {
    common_deps_stream << " ";
    path_output_.WriteFile(common_deps_stream, datas[i]);
  }
  const std::string& common_deps = common_deps_stream.str();

  // Collects all output files for writing below.
  std::vector<OutputFile> output_files;

  if (no_sources) {
    // No sources, write a rule that invokes the script once with the
    // outputs as outputs, and the data as inputs.
    out_ << "build";
    const Target::FileList& outputs = target_->outputs();
    for (size_t i = 0; i < outputs.size(); i++) {
      OutputFile output_path(
          RemovePrefix(outputs[i].value(),
                       settings_->build_settings()->build_dir().value()));
      output_files.push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }
    out_ << ": " << custom_rule_name << " " << common_deps << std::endl;
  } else {
    // Write separate rules for each input source file.
    WriteCustomSourceRules(custom_rule_name, common_deps, script_cd,
                           script_cd_to_root, &output_files);
  }
  out_ << std::endl;

  // Last write a stamp rule to collect all outputs.
  out_ << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": stamp";
  for (size_t i = 0; i < output_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, output_files[i]);
  }
  out_ << std::endl;
}

void NinjaTargetWriter::WriteCustomArg(const std::string& arg) {
  // This can be optimized if it's called a lot.
  EscapeOptions options;
  options.mode = ESCAPE_NINJA;
  std::string output_str = EscapeString(arg, options);

  // Do this substitution after escaping our our $ will be escaped (which we
  // don't want).
  ReplaceSubstringsAfterOffset(&output_str, 0, FileTemplate::kSource,
                               "${source}");
  ReplaceSubstringsAfterOffset(&output_str, 0, FileTemplate::kSourceNamePart,
                               "${source_name_part}");
  out_ << output_str;
}

void NinjaTargetWriter::WriteCustomSourceRules(
    const std::string& custom_rule_name,
    const std::string& common_deps,
    const SourceDir& script_cd,
    const std::string& script_cd_to_root,
    std::vector<OutputFile>* output_files) {
  // Construct the template for generating the output files from each source.
  const Target::FileList& outputs = target_->outputs();
  std::vector<std::string> output_template_args;
  for (size_t i = 0; i < outputs.size(); i++) {
    // All outputs should be in the output dir.
    output_template_args.push_back(
        RemovePrefix(outputs[i].value(),
                     settings_->build_settings()->build_dir().value()));
  }
  FileTemplate output_template(output_template_args);

  // Prevent re-allocating each time by initializing outside the loop.
  std::vector<std::string> output_template_result;

  // Path output formatter for wrigin source paths passed to the script.
  PathOutput script_source_path_output(script_cd, ESCAPE_SHELL, true);

  const Target::FileList& sources = target_->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    // Write outputs for this source file computed by the template.
    out_ << "build";
    output_template.ApplyString(sources[i].value(), &output_template_result);
    for (size_t out_i = 0; out_i < output_template_result.size(); out_i++) {
      OutputFile output_path(output_template_result[out_i]);
      output_files->push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }

    out_ << ": " << custom_rule_name
         << " " << common_deps
         << " ";
    path_output_.WriteFile(out_, sources[i]);
    out_ << std::endl;

    out_ << "  unique_name = " << i << std::endl;

    // The source file here should be relative to the script directory since
    // this is the variable passed to the script. Here we slightly abuse the
    // OutputFile object by putting a non-output-relative path in it to signal
    // that the PathWriter should not prepend directories.
    out_ << "  source = ";
    script_source_path_output.WriteFile(out_, sources[i]);
    out_ << std::endl;

    out_ << "  source_name_part = "
         << FindFilenameNoExtension(&sources[i].value()).as_string()
         << std::endl;
  }
}

void NinjaTargetWriter::WriteCompilerVars() {
  // Defines.
  out_ << "defines =";
  RecursiveTargetConfigToStream(target_, &ConfigValues::defines,
                                DefineWriter(), out_);
  out_ << std::endl;

  // Includes.
  out_ << "includes =";
  RecursiveTargetConfigToStream(target_, &ConfigValues::includes,
                                IncludeWriter(path_output_, helper_), out_);

  out_ << std::endl;

  // C flags and friends.
#define WRITE_FLAGS(name) \
    out_ << #name " ="; \
    RecursiveTargetConfigStringsToStream(target_, &ConfigValues::name, out_); \
    out_ << std::endl;

  WRITE_FLAGS(cflags)
  WRITE_FLAGS(cflags_c)
  WRITE_FLAGS(cflags_cc)
  WRITE_FLAGS(cflags_objc)
  WRITE_FLAGS(cflags_objcc)

#undef WRITE_FLAGS

  out_ << std::endl;
}

void NinjaTargetWriter::WriteSources(
    std::vector<OutputFile>* object_files) {
  const Target::FileList& sources = target_->sources();
  object_files->reserve(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];

    SourceFileType input_file_type = GetSourceFileType(input_file,
                                                       settings_->target_os());
    if (input_file_type == SOURCE_UNKNOWN)
      continue;  // Skip unknown file types.
    const char* command = GetCommandForSourceType(input_file_type);
    if (!command)
      continue;  // Skip files not needing compilation.

    OutputFile output_file = helper_.GetOutputFileForSource(
        target_, input_file, input_file_type);
    object_files->push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": " << command << " ";
    path_output_.WriteFile(out_, input_file);
    out_ << std::endl;
  }
  out_ << std::endl;
}

void NinjaTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files) {
  // Manifest file on Windows.
  // TODO(brettw) this seems not to be necessary for static libs, skip in
  // that case?
  OutputFile windows_manifest;
  if (settings_->IsWin()) {
    windows_manifest.value().assign(helper_.GetTargetOutputDir(target_));
    windows_manifest.value().append(target_->label().name());
    windows_manifest.value().append(".intermediate.manifest");
    out_ << "manifests = ";
    path_output_.WriteFile(out_, windows_manifest);
    out_ << std::endl;
  }

  // Linker flags, append manifest flag on Windows to reference our file.
  out_ << "ldflags =";
  RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags, out_);
  // HACK ERASEME BRETTW FIXME
  if (settings_->IsWin()) {
    out_ << " /MANIFEST /ManifestFile:";
    path_output_.WriteFile(out_, windows_manifest);
    out_ << " /DEBUG /MACHINE:X86 /LIBPATH:\"C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x86\" /DELAYLOAD:dbghelp.dll /DELAYLOAD:dwmapi.dll /DELAYLOAD:shell32.dll /DELAYLOAD:uxtheme.dll /safeseh /dynamicbase /ignore:4199 /ignore:4221 /nxcompat /SUBSYSTEM:CONSOLE /INCREMENTAL /FIXED:NO /DYNAMICBASE:NO wininet.lib dnsapi.lib version.lib msimg32.lib ws2_32.lib usp10.lib psapi.lib dbghelp.lib winmm.lib shlwapi.lib kernel32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib user32.lib uuid.lib odbc32.lib odbccp32.lib delayimp.lib /NXCOMPAT";
  }
  out_ << std::endl;

  // Libraries to link.
  out_ << "libs =";
  if (settings_->IsMac()) {
    // TODO(brettw) fix this.
    out_ << " -framework AppKit -framework ApplicationServices -framework Carbon -framework CoreFoundation -framework Foundation -framework IOKit -framework Security";
  }
  out_ << std::endl;

  // The external output file is the one that other libs depend on.
  OutputFile external_output_file = helper_.GetTargetOutputFile(target_);

  // The internal output file is the "main thing" we think we're making. In
  // the case of shared libraries, this is the shared library and the external
  // output file is the import library. In other cases, the internal one and
  // the external one are the same.
  OutputFile internal_output_file;
  if (target_->output_type() == Target::SHARED_LIBRARY) {
    if (settings_->IsWin()) {
      internal_output_file = OutputFile(target_->label().name() + ".dll");
    } else {
      internal_output_file = external_output_file;
    }
  } else {
    internal_output_file = external_output_file;
  }

  // In Python see "self.ninja.build(output, command, input,"
  WriteLinkCommand(external_output_file, internal_output_file, object_files);

  if (target_->output_type() == Target::SHARED_LIBRARY) {
    out_ << "  soname = ";
    path_output_.WriteFile(out_, internal_output_file);
    out_ << std::endl;

    out_ << "  lib = ";
    path_output_.WriteFile(out_, internal_output_file);
    out_ << std::endl;

    out_ << "  dll = ";
    path_output_.WriteFile(out_, internal_output_file);
    out_ << std::endl;

    if (settings_->IsWin()) {
      out_ << "  implibflag = /IMPLIB:";
      path_output_.WriteFile(out_, external_output_file);
      out_ << std::endl;
    }

    // TODO(brettw) postbuild steps.
    if (settings_->IsMac())
      out_ << "  postbuilds = $ && (export BUILT_PRODUCTS_DIR=/Users/brettw/prj/src/out/gn; export CONFIGURATION=Debug; export DYLIB_INSTALL_NAME_BASE=@rpath; export EXECUTABLE_NAME=libbase.dylib; export EXECUTABLE_PATH=libbase.dylib; export FULL_PRODUCT_NAME=libbase.dylib; export LD_DYLIB_INSTALL_NAME=@rpath/libbase.dylib; export MACH_O_TYPE=mh_dylib; export PRODUCT_NAME=base; export PRODUCT_TYPE=com.apple.product-type.library.dynamic; export SDKROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk; export SRCROOT=/Users/brettw/prj/src/out/gn/../../base; export SOURCE_ROOT=\"$${SRCROOT}\"; export TARGET_BUILD_DIR=/Users/brettw/prj/src/out/gn; export TEMP_DIR=\"$${TMPDIR}\"; (cd ../../base && ../build/mac/strip_from_xcode); G=$$?; ((exit $$G) || rm -rf libbase.dylib) && exit $$G)";
  }

  out_ << std::endl;
}

void NinjaTargetWriter::WriteLinkCommand(
    const OutputFile& external_output_file,
    const OutputFile& internal_output_file,
    const std::vector<OutputFile>& object_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, internal_output_file);
  if (external_output_file != internal_output_file) {
    out_ << " ";
    path_output_.WriteFile(out_, external_output_file);
  }
  out_ << ": " << GetCommandForTargetType();

  // Object files.
  for (size_t i = 0; i < object_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, object_files[i]);
  }

  // Library inputs (deps and inherited static libraries).
  //
  // Static libraries since they're just a collection of the object files so
  // don't need libraries linked with them, but we still need to go through
  // the list and find non-linkable data deps in the "deps" section. We'll
  // collect all non-linkable deps and put it in the order-only deps below.
  std::vector<const Target*> extra_data_deps;
  const std::vector<const Target*>& deps = target_->deps();
  const std::set<const Target*>& inherited = target_->inherited_libraries();
  for (size_t i = 0; i < deps.size(); i++) {
    if (inherited.find(deps[i]) != inherited.end())
      continue;
    if (target_->output_type() != Target::STATIC_LIBRARY &&
        deps[i]->IsLinkable()) {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(deps[i]));
    } else {
      extra_data_deps.push_back(deps[i]);
    }
  }
  for (std::set<const Target*>::const_iterator i = inherited.begin();
       i != inherited.end(); ++i) {
    if (target_->output_type() == Target::STATIC_LIBRARY) {
      extra_data_deps.push_back(*i);
    } else {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(*i));
    }
  }

  // Append data dependencies as order-only dependencies.
  const std::vector<const Target*>& datadeps = target_->datadeps();
  const std::vector<SourceFile>& data = target_->data();
  if (!extra_data_deps.empty() || !datadeps.empty() || !data.empty()) {
    out_ << " ||";

    // Non-linkable deps in the deps section above.
    for (size_t i = 0; i < extra_data_deps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_,
                             helper_.GetTargetOutputFile(extra_data_deps[i]));
    }

    // Data deps.
    for (size_t i = 0; i < datadeps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(datadeps[i]));
    }

    // Data files.
    const std::vector<SourceFile>& data = target_->data();
    for (size_t i = 0; i < data.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, data[i]);
    }
  }

  out_ << std::endl;
}

const char* NinjaTargetWriter::GetCommandForSourceType(
    SourceFileType type) const {
  if (type == SOURCE_C)
    return "cc";
  if (type == SOURCE_CC)
    return "cxx";

  // TODO(brettw) asm files.

  if (settings_->IsMac()) {
    if (type == SOURCE_M)
      return "objc";
    if (type == SOURCE_MM)
      return "objcxx";
  }

  if (settings_->IsWin()) {
    if (type == SOURCE_RC)
      return "rc";
  }

  // TODO(brettw) stuff about "S" files on non-Windows.
  return NULL;
}

const char* NinjaTargetWriter::GetCommandForTargetType() const {
  if (target_->output_type() == Target::NONE) {
    NOTREACHED();
    return "";
  }

  if (target_->output_type() == Target::STATIC_LIBRARY) {
    // TODO(brettw) stuff about standalong static libraryes on Unix in
    // WriteTarget in the Python one, and lots of postbuild steps.
    return "alink";
  }

  if (target_->output_type() == Target::SHARED_LIBRARY)
    return "solink";

  return "link";
}
