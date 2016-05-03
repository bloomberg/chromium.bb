// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/xcode_object.h"

#include <iomanip>
#include <sstream>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"

// Helper methods -------------------------------------------------------------

namespace {
struct IndentRules {
  bool one_line;
  unsigned level;
};

std::vector<std::unique_ptr<PBXObject>> EmptyPBXObjectVector() {
  return std::vector<std::unique_ptr<PBXObject>>();
}

bool CharNeedEscaping(char c) {
  if (base::IsAsciiAlpha(c) || base::IsAsciiDigit(c))
    return false;
  if (c == '$' || c == '.' || c == '/' || c == '_')
    return false;
  return true;
}

bool StringNeedEscaping(const std::string& string) {
  if (string.empty())
    return true;
  if (string.find("___") != std::string::npos)
    return true;

  for (char c : string) {
    if (CharNeedEscaping(c))
      return true;
  }
  return false;
}

std::string EncodeString(const std::string& string) {
  if (!StringNeedEscaping(string))
    return string;

  std::stringstream buffer;
  buffer << '"';
  for (char c : string) {
    if (c <= 31) {
      switch (c) {
        case '\a':
          buffer << "\\a";
          break;
        case '\b':
          buffer << "\\b";
          break;
        case '\t':
          buffer << "\\t";
          break;
        case '\n':
        case '\r':
          buffer << "\\n";
          break;
        case '\v':
          buffer << "\\v";
          break;
        case '\f':
          buffer << "\\f";
          break;
        default:
          buffer << std::hex << std::setw(4) << std::left << "\\U"
                 << static_cast<unsigned>(c);
          break;
      }
    } else {
      if (c == '"' || c == '\\')
        buffer << '\\';
      buffer << c;
    }
  }
  buffer << '"';
  return buffer.str();
}

const char* GetSourceType(const base::FilePath::StringType& ext) {
  std::map<base::FilePath::StringType, const char*> extension_map = {
      {FILE_PATH_LITERAL(".a"), "archive.ar"},
      {FILE_PATH_LITERAL(".app"), "wrapper.application"},
      {FILE_PATH_LITERAL(".bdic"), "file"},
      {FILE_PATH_LITERAL(".bundle"), "wrapper.cfbundle"},
      {FILE_PATH_LITERAL(".c"), "sourcecode.c.c"},
      {FILE_PATH_LITERAL(".cc"), "sourcecode.cpp.cpp"},
      {FILE_PATH_LITERAL(".cpp"), "sourcecode.cpp.cpp"},
      {FILE_PATH_LITERAL(".css"), "text.css"},
      {FILE_PATH_LITERAL(".cxx"), "sourcecode.cpp.cpp"},
      {FILE_PATH_LITERAL(".dart"), "sourcecode"},
      {FILE_PATH_LITERAL(".dylib"), "compiled.mach-o.dylib"},
      {FILE_PATH_LITERAL(".framework"), "wrapper.framework"},
      {FILE_PATH_LITERAL(".h"), "sourcecode.c.h"},
      {FILE_PATH_LITERAL(".hxx"), "sourcecode.cpp.h"},
      {FILE_PATH_LITERAL(".icns"), "image.icns"},
      {FILE_PATH_LITERAL(".java"), "sourcecode.java"},
      {FILE_PATH_LITERAL(".js"), "sourcecode.javascript"},
      {FILE_PATH_LITERAL(".kext"), "wrapper.kext"},
      {FILE_PATH_LITERAL(".m"), "sourcecode.c.objc"},
      {FILE_PATH_LITERAL(".mm"), "sourcecode.cpp.objcpp"},
      {FILE_PATH_LITERAL(".nib"), "wrapper.nib"},
      {FILE_PATH_LITERAL(".o"), "compiled.mach-o.objfile"},
      {FILE_PATH_LITERAL(".pdf"), "image.pdf"},
      {FILE_PATH_LITERAL(".pl"), "text.script.perl"},
      {FILE_PATH_LITERAL(".plist"), "text.plist.xml"},
      {FILE_PATH_LITERAL(".pm"), "text.script.perl"},
      {FILE_PATH_LITERAL(".png"), "image.png"},
      {FILE_PATH_LITERAL(".py"), "text.script.python"},
      {FILE_PATH_LITERAL(".r"), "sourcecode.rez"},
      {FILE_PATH_LITERAL(".rez"), "sourcecode.rez"},
      {FILE_PATH_LITERAL(".s"), "sourcecode.asm"},
      {FILE_PATH_LITERAL(".storyboard"), "file.storyboard"},
      {FILE_PATH_LITERAL(".strings"), "text.plist.strings"},
      {FILE_PATH_LITERAL(".swift"), "sourcecode.swift"},
      {FILE_PATH_LITERAL(".ttf"), "file"},
      {FILE_PATH_LITERAL(".xcassets"), "folder.assetcatalog"},
      {FILE_PATH_LITERAL(".xcconfig"), "text.xcconfig"},
      {FILE_PATH_LITERAL(".xcdatamodel"), "wrapper.xcdatamodel"},
      {FILE_PATH_LITERAL(".xcdatamodeld"), "wrapper.xcdatamodeld"},
      {FILE_PATH_LITERAL(".xib"), "file.xib"},
      {FILE_PATH_LITERAL(".y"), "sourcecode.yacc"},
  };

  const auto& iter = extension_map.find(ext);
  if (iter != extension_map.end()) {
    return iter->second;
  }

  return "text";
}

bool HasExplicitFileType(const base::FilePath::StringType& ext) {
  return ext == FILE_PATH_LITERAL(".dart");
}

bool IsSourceFileForIndexing(const base::FilePath::StringType& ext) {
  return ext == FILE_PATH_LITERAL(".c") || ext == FILE_PATH_LITERAL(".cc") ||
         ext == FILE_PATH_LITERAL(".cpp") || ext == FILE_PATH_LITERAL(".cxx") ||
         ext == FILE_PATH_LITERAL(".m") || ext == FILE_PATH_LITERAL(".mm");
}

void PrintValue(std::ostream& out, IndentRules rules, unsigned value) {
  out << value;
}

void PrintValue(std::ostream& out, IndentRules rules, const char* value) {
  out << EncodeString(value);
}

void PrintValue(std::ostream& out,
                IndentRules rules,
                const std::string& value) {
  out << EncodeString(value);
}

void PrintValue(std::ostream& out, IndentRules rules, const PBXObject* value) {
  out << value->Reference();
}

template <typename ObjectClass>
void PrintValue(std::ostream& out,
                IndentRules rules,
                const std::unique_ptr<ObjectClass>& value) {
  PrintValue(out, rules, value.get());
}

template <typename ValueType>
void PrintValue(std::ostream& out,
                IndentRules rules,
                const std::vector<ValueType>& values) {
  IndentRules sub_rule{rules.one_line, rules.level + 1};
  out << "(" << (rules.one_line ? " " : "\n");
  for (const auto& value : values) {
    if (!sub_rule.one_line)
      out << std::string(sub_rule.level, '\t');

    PrintValue(out, sub_rule, value);
    out << "," << (rules.one_line ? " " : "\n");
  }

  if (!rules.one_line && rules.level)
    out << std::string(rules.level, '\t');
  out << ")";
}

template <typename ValueType>
void PrintValue(std::ostream& out,
                IndentRules rules,
                const std::map<std::string, ValueType>& values) {
  IndentRules sub_rule{rules.one_line, rules.level + 1};
  out << "{" << (rules.one_line ? " " : "\n");
  for (const auto& pair : values) {
    if (!sub_rule.one_line)
      out << std::string(sub_rule.level, '\t');

    out << pair.first << " = ";
    PrintValue(out, sub_rule, pair.second);
    out << ";" << (rules.one_line ? " " : "\n");
  }

  if (!rules.one_line && rules.level)
    out << std::string(rules.level, '\t');
  out << "}";
}

template <typename ValueType>
void PrintProperty(std::ostream& out,
                   IndentRules rules,
                   const char* name,
                   ValueType&& value) {
  if (!rules.one_line && rules.level)
    out << std::string(rules.level, '\t');

  out << name << " = ";
  PrintValue(out, rules, std::forward<ValueType>(value));
  out << ";" << (rules.one_line ? " " : "\n");
}
}  // namespace

// PBXObjectClass -------------------------------------------------------------

const char* ToString(PBXObjectClass cls) {
  switch (cls) {
    case PBXAggregateTargetClass:
      return "PBXAggregateTarget";
    case PBXBuildFileClass:
      return "PBXBuildFile";
    case PBXFileReferenceClass:
      return "PBXFileReference";
    case PBXFrameworksBuildPhaseClass:
      return "PBXFrameworksBuildPhase";
    case PBXGroupClass:
      return "PBXGroup";
    case PBXNativeTargetClass:
      return "PBXNativeTarget";
    case PBXProjectClass:
      return "PBXProject";
    case PBXShellScriptBuildPhaseClass:
      return "PBXShellScriptBuildPhase";
    case PBXSourcesBuildPhaseClass:
      return "PBXSourcesBuildPhase";
    case XCBuildConfigurationClass:
      return "XCBuildConfiguration";
    case XCConfigurationListClass:
      return "XCConfigurationList";
  }
  NOTREACHED();
  return nullptr;
}

// PBXObjectVisitor -----------------------------------------------------------

PBXObjectVisitor::PBXObjectVisitor() {}

PBXObjectVisitor::~PBXObjectVisitor() {}

// PBXObject ------------------------------------------------------------------

PBXObject::PBXObject() {}

PBXObject::~PBXObject() {}

void PBXObject::SetId(const std::string& id) {
  DCHECK(id_.empty());
  DCHECK(!id.empty());
  id_.assign(id);
}

std::string PBXObject::Reference() const {
  std::string comment = Comment();
  if (comment.empty())
    return id_;

  return id_ + " /* " + comment + " */";
}

std::string PBXObject::Comment() const {
  return Name();
}

void PBXObject::Visit(PBXObjectVisitor& visitor) {
  visitor.Visit(this);
}

// PBXBuildPhase --------------------------------------------------------------

PBXBuildPhase::PBXBuildPhase() {}

PBXBuildPhase::~PBXBuildPhase() {}

// PBXTarget ------------------------------------------------------------------

PBXTarget::PBXTarget(const std::string& name,
                     const std::string& shell_script,
                     const std::string& config_name,
                     const PBXAttributes& attributes)
    : configurations_(new XCConfigurationList(config_name, attributes, this)),
      name_(name) {
  if (!shell_script.empty()) {
    build_phases_.push_back(
        base::WrapUnique(new PBXShellScriptBuildPhase(name, shell_script)));
  }
}

PBXTarget::~PBXTarget() {}

std::string PBXTarget::Name() const {
  return name_;
}

void PBXTarget::Visit(PBXObjectVisitor& visitor) {
  PBXObject::Visit(visitor);
  configurations_->Visit(visitor);
  for (const auto& build_phase : build_phases_) {
    build_phase->Visit(visitor);
  }
}

// PBXAggregateTarget ---------------------------------------------------------

PBXAggregateTarget::PBXAggregateTarget(const std::string& name,
                                       const std::string& shell_script,
                                       const std::string& config_name,
                                       const PBXAttributes& attributes)
    : PBXTarget(name, shell_script, config_name, attributes) {}

PBXAggregateTarget::~PBXAggregateTarget() {}

PBXObjectClass PBXAggregateTarget::Class() const {
  return PBXAggregateTargetClass;
}

void PBXAggregateTarget::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildConfigurationList", configurations_);
  PrintProperty(out, rules, "buildPhases", build_phases_);
  PrintProperty(out, rules, "dependencies", EmptyPBXObjectVector());
  PrintProperty(out, rules, "name", name_);
  PrintProperty(out, rules, "productName", name_);
  out << indent_str << "};\n";
}

// PBXBuildFile ---------------------------------------------------------------

PBXBuildFile::PBXBuildFile(const PBXFileReference* file_reference,
                           const PBXSourcesBuildPhase* build_phase)
    : file_reference_(file_reference), build_phase_(build_phase) {
  DCHECK(file_reference_);
  DCHECK(build_phase_);
}

PBXBuildFile::~PBXBuildFile() {}

PBXObjectClass PBXBuildFile::Class() const {
  return PBXBuildFileClass;
}

std::string PBXBuildFile::Name() const {
  return file_reference_->Name() + " in " + build_phase_->Name();
}

void PBXBuildFile::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {true, 0};
  out << indent_str << Reference() << " = {";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "fileRef", file_reference_);
  out << "};\n";
}

// PBXFileReference -----------------------------------------------------------

PBXFileReference::PBXFileReference(const std::string& name,
                                   const std::string& path,
                                   const std::string& type)
    : name_(name), path_(path), type_(type) {}

PBXFileReference::~PBXFileReference() {}

PBXObjectClass PBXFileReference::Class() const {
  return PBXFileReferenceClass;
}

std::string PBXFileReference::Name() const {
  return path_;
}

void PBXFileReference::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {true, 0};
  out << indent_str << Reference() << " = {";
  PrintProperty(out, rules, "isa", ToString(Class()));

  if (!type_.empty()) {
    PrintProperty(out, rules, "explicitFileType", type_);
    PrintProperty(out, rules, "includeInIndex", 0u);
  } else {
    const base::FilePath::StringType ext =
        base::FilePath::FromUTF8Unsafe(path_).Extension();

    if (HasExplicitFileType(ext))
      PrintProperty(out, rules, "explicitFileType", GetSourceType(ext));
    else
      PrintProperty(out, rules, "lastKnownFileType", GetSourceType(ext));
  }

  if (name_ != path_ && !name_.empty())
    PrintProperty(out, rules, "name", name_);

  PrintProperty(out, rules, "path", path_);
  PrintProperty(out, rules, "sourceTree",
                type_.empty() ? "<group>" : "BUILT_PRODUCTS_DIR");
  out << "};\n";
}

// PBXFrameworksBuildPhase ----------------------------------------------------

PBXFrameworksBuildPhase::PBXFrameworksBuildPhase() {}

PBXFrameworksBuildPhase::~PBXFrameworksBuildPhase() {}

PBXObjectClass PBXFrameworksBuildPhase::Class() const {
  return PBXFrameworksBuildPhaseClass;
}

std::string PBXFrameworksBuildPhase::Name() const {
  return "Frameworks";
}

void PBXFrameworksBuildPhase::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildActionMask", 0x7fffffffu);
  PrintProperty(out, rules, "files", EmptyPBXObjectVector());
  PrintProperty(out, rules, "runOnlyForDeploymentPostprocessing", 0u);
  out << indent_str << "};\n";
}

// PBXGroup -------------------------------------------------------------------

PBXGroup::PBXGroup(const std::string& path, const std::string& name)
    : name_(name), path_(path) {}

PBXGroup::~PBXGroup() {}

PBXObject* PBXGroup::AddChild(std::unique_ptr<PBXObject> child) {
  DCHECK(child);
  children_.push_back(std::move(child));
  return children_.back().get();
}

PBXFileReference* PBXGroup::AddSourceFile(const std::string& source_path) {
  DCHECK(!source_path.empty());
  std::string::size_type sep = source_path.find("/");
  if (sep == std::string::npos) {
    children_.push_back(base::WrapUnique(
        new PBXFileReference(std::string(), source_path, std::string())));
    return static_cast<PBXFileReference*>(children_.back().get());
  }

  PBXGroup* group = nullptr;
  base::StringPiece component(source_path.data(), sep);
  for (const auto& child : children_) {
    if (child->Class() != PBXGroupClass)
      continue;

    PBXGroup* child_as_group = static_cast<PBXGroup*>(child.get());
    if (child_as_group->path_ == component) {
      group = child_as_group;
      break;
    }
  }

  if (!group) {
    children_.push_back(base::WrapUnique(new PBXGroup(component.as_string())));
    group = static_cast<PBXGroup*>(children_.back().get());
  }

  DCHECK(group);
  DCHECK(group->path_ == component);
  return group->AddSourceFile(source_path.substr(sep + 1));
}

PBXObjectClass PBXGroup::Class() const {
  return PBXGroupClass;
}

std::string PBXGroup::Name() const {
  if (!name_.empty())
    return name_;
  if (!path_.empty())
    return path_;
  return std::string();
}

void PBXGroup::Visit(PBXObjectVisitor& visitor) {
  PBXObject::Visit(visitor);
  for (const auto& child : children_) {
    child->Visit(visitor);
  }
}

void PBXGroup::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "children", children_);
  if (!name_.empty())
    PrintProperty(out, rules, "name", name_);
  if (!path_.empty())
    PrintProperty(out, rules, "path", path_);
  PrintProperty(out, rules, "sourceTree", "<group>");
  out << indent_str << "};\n";
}

// PBXNativeTarget ------------------------------------------------------------

PBXNativeTarget::PBXNativeTarget(const std::string& name,
                                 const std::string& shell_script,
                                 const std::string& config_name,
                                 const PBXAttributes& attributes,
                                 const std::string& product_type,
                                 const PBXFileReference* product_reference)
    : PBXTarget(name, shell_script, config_name, attributes),
      product_reference_(product_reference),
      product_type_(product_type) {
  DCHECK(product_reference_);
  build_phases_.push_back(base::WrapUnique(new PBXSourcesBuildPhase));
  source_build_phase_ =
      static_cast<PBXSourcesBuildPhase*>(build_phases_.back().get());

  build_phases_.push_back(base::WrapUnique(new PBXFrameworksBuildPhase));
}

PBXNativeTarget::~PBXNativeTarget() {}

void PBXNativeTarget::AddFileForIndexing(
    const PBXFileReference* file_reference) {
  DCHECK(file_reference);
  source_build_phase_->AddBuildFile(
      base::WrapUnique(new PBXBuildFile(file_reference, source_build_phase_)));
}

PBXObjectClass PBXNativeTarget::Class() const {
  return PBXNativeTargetClass;
}

void PBXNativeTarget::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildConfigurationList", configurations_);
  PrintProperty(out, rules, "buildPhases", build_phases_);
  PrintProperty(out, rules, "buildRules", EmptyPBXObjectVector());
  PrintProperty(out, rules, "dependencies", EmptyPBXObjectVector());
  PrintProperty(out, rules, "name", name_);
  PrintProperty(out, rules, "productName", name_);
  PrintProperty(out, rules, "productReference", product_reference_);
  PrintProperty(out, rules, "productType", product_type_);
  out << indent_str << "};\n";
}

// PBXProject -----------------------------------------------------------------

PBXProject::PBXProject(const std::string& name,
                       const std::string& config_name,
                       const std::string& source_path,
                       const PBXAttributes& attributes)
    : name_(name), config_name_(config_name), target_for_indexing_(nullptr) {
  attributes_["BuildIndependentTargetsInParallel"] = "YES";

  main_group_.reset(new PBXGroup);
  sources_ = static_cast<PBXGroup*>(main_group_->AddChild(
      base::WrapUnique(new PBXGroup(source_path, "Source"))));
  products_ = static_cast<PBXGroup*>(main_group_->AddChild(
      base::WrapUnique(new PBXGroup(std::string(), "Product"))));
  main_group_->AddChild(base::WrapUnique(new PBXGroup(std::string(), "Build")));

  configurations_.reset(new XCConfigurationList(config_name, attributes, this));
}

PBXProject::~PBXProject() {}

void PBXProject::AddSourceFile(const std::string& source_path) {
  PBXFileReference* file_reference = sources_->AddSourceFile(source_path);
  const base::FilePath::StringType ext =
      base::FilePath::FromUTF8Unsafe(source_path).Extension();
  if (!IsSourceFileForIndexing(ext))
    return;

  if (!target_for_indexing_) {
    PBXAttributes attributes;
    attributes["EXECUTABLE_PREFIX"] = "";
    attributes["HEADER_SEARCH_PATHS"] = sources_->path();
    attributes["PRODUCT_NAME"] = name_;

    PBXFileReference* product_reference = static_cast<PBXFileReference*>(
        products_->AddChild(base::WrapUnique(new PBXFileReference(
            std::string(), name_, "compiled.mach-o.executable"))));

    const char product_type[] = "com.apple.product-type.tool";
    targets_.push_back(base::WrapUnique(
        new PBXNativeTarget(name_, std::string(), config_name_, attributes,
                            product_type, product_reference)));
    target_for_indexing_ = static_cast<PBXNativeTarget*>(targets_.back().get());
  }

  DCHECK(target_for_indexing_);
  target_for_indexing_->AddFileForIndexing(file_reference);
}

void PBXProject::AddAggregateTarget(const std::string& name,
                                    const std::string& shell_script) {
  PBXAttributes attributes;
  attributes["CODE_SIGNING_REQUIRED"] = "NO";
  attributes["CONFIGURATION_BUILD_DIR"] = ".";
  attributes["PRODUCT_NAME"] = name;

  targets_.push_back(base::WrapUnique(
      new PBXAggregateTarget(name, shell_script, config_name_, attributes)));
}

void PBXProject::AddNativeTarget(const std::string& name,
                                 const std::string& type,
                                 const std::string& output_name,
                                 const std::string& output_type,
                                 const std::string& shell_script) {
  const base::FilePath::StringType ext =
      base::FilePath::FromUTF8Unsafe(output_name).Extension();

  PBXFileReference* product = static_cast<PBXFileReference*>(
      products_->AddChild(base::WrapUnique(new PBXFileReference(
          name, output_name, type.empty() ? GetSourceType(ext) : type))));

  PBXAttributes attributes;
  attributes["CODE_SIGNING_REQUIRED"] = "NO";
  attributes["CONFIGURATION_BUILD_DIR"] = ".";
  attributes["PRODUCT_NAME"] = name;

  targets_.push_back(base::WrapUnique(new PBXNativeTarget(
      name, shell_script, config_name_, attributes, output_type, product)));
}

void PBXProject::SetProjectDirPath(const std::string& project_dir_path) {
  DCHECK(!project_dir_path.empty());
  project_dir_path_.assign(project_dir_path);
}

void PBXProject::SetProjectRoot(const std::string& project_root) {
  DCHECK(!project_root.empty());
  project_root_.assign(project_root);
}

void PBXProject::AddTarget(std::unique_ptr<PBXTarget> target) {
  DCHECK(target);
  targets_.push_back(std::move(target));
}

PBXObjectClass PBXProject::Class() const {
  return PBXProjectClass;
}

std::string PBXProject::Name() const {
  return name_;
}

std::string PBXProject::Comment() const {
  return "Project object";
}

void PBXProject::Visit(PBXObjectVisitor& visitor) {
  PBXObject::Visit(visitor);
  configurations_->Visit(visitor);
  main_group_->Visit(visitor);
  for (const auto& target : targets_) {
    target->Visit(visitor);
  }
}

void PBXProject::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "attributes", attributes_);
  PrintProperty(out, rules, "buildConfigurationList", configurations_);
  PrintProperty(out, rules, "compatibilityVersion", "Xcode 3.2");
  PrintProperty(out, rules, "developmentRegion", "English");
  PrintProperty(out, rules, "hasScannedForEncodings", 1u);
  PrintProperty(out, rules, "knownRegions", std::vector<std::string>({"en"}));
  PrintProperty(out, rules, "mainGroup", main_group_);
  PrintProperty(out, rules, "projectDirPath", project_dir_path_);
  PrintProperty(out, rules, "projectRoot", project_root_);
  PrintProperty(out, rules, "targets", targets_);
  out << indent_str << "};\n";
}

// PBXShellScriptBuildPhase ---------------------------------------------------

PBXShellScriptBuildPhase::PBXShellScriptBuildPhase(
    const std::string& name,
    const std::string& shell_script)
    : name_("Action \"Compile and copy " + name + " via ninja\""),
      shell_script_(shell_script) {}

PBXShellScriptBuildPhase::~PBXShellScriptBuildPhase() {}

PBXObjectClass PBXShellScriptBuildPhase::Class() const {
  return PBXShellScriptBuildPhaseClass;
}

std::string PBXShellScriptBuildPhase::Name() const {
  return name_;
}

void PBXShellScriptBuildPhase::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildActionMask", 0x7fffffffu);
  PrintProperty(out, rules, "files", EmptyPBXObjectVector());
  PrintProperty(out, rules, "inputPaths", EmptyPBXObjectVector());
  PrintProperty(out, rules, "name", name_);
  PrintProperty(out, rules, "outputPaths", EmptyPBXObjectVector());
  PrintProperty(out, rules, "runOnlyForDeploymentPostprocessing", 0u);
  PrintProperty(out, rules, "shellPath", "/bin/sh");
  PrintProperty(out, rules, "shellScript", shell_script_);
  PrintProperty(out, rules, "showEnvVarsInLog", 0u);
  out << indent_str << "};\n";
}

// PBXSourcesBuildPhase -------------------------------------------------------

PBXSourcesBuildPhase::PBXSourcesBuildPhase() {}

PBXSourcesBuildPhase::~PBXSourcesBuildPhase() {}

void PBXSourcesBuildPhase::AddBuildFile(
    std::unique_ptr<PBXBuildFile> build_file) {
  files_.push_back(std::move(build_file));
}

PBXObjectClass PBXSourcesBuildPhase::Class() const {
  return PBXSourcesBuildPhaseClass;
}

std::string PBXSourcesBuildPhase::Name() const {
  return "Sources";
}

void PBXSourcesBuildPhase::Visit(PBXObjectVisitor& visitor) {
  PBXBuildPhase::Visit(visitor);
  for (const auto& file : files_) {
    file->Visit(visitor);
  }
}

void PBXSourcesBuildPhase::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildActionMask", 0x7fffffffu);
  PrintProperty(out, rules, "files", files_);
  PrintProperty(out, rules, "runOnlyForDeploymentPostprocessing", 0u);
  out << indent_str << "};\n";
}

// XCBuildConfiguration -------------------------------------------------------

XCBuildConfiguration::XCBuildConfiguration(const std::string& name,
                                           const PBXAttributes& attributes)
    : attributes_(attributes), name_(name) {}

XCBuildConfiguration::~XCBuildConfiguration() {}

PBXObjectClass XCBuildConfiguration::Class() const {
  return XCBuildConfigurationClass;
}

std::string XCBuildConfiguration::Name() const {
  return name_;
}

void XCBuildConfiguration::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildSettings", attributes_);
  PrintProperty(out, rules, "name", name_);
  out << indent_str << "};\n";
}

// XCConfigurationList --------------------------------------------------------

XCConfigurationList::XCConfigurationList(const std::string& name,
                                         const PBXAttributes& attributes,
                                         const PBXObject* owner_reference)
    : owner_reference_(owner_reference) {
  DCHECK(owner_reference_);
  configurations_.push_back(
      base::WrapUnique(new XCBuildConfiguration(name, attributes)));
}

XCConfigurationList::~XCConfigurationList() {}

PBXObjectClass XCConfigurationList::Class() const {
  return XCConfigurationListClass;
}

std::string XCConfigurationList::Name() const {
  std::stringstream buffer;
  buffer << "Build configuration list for "
         << ToString(owner_reference_->Class()) << " \""
         << owner_reference_->Name() << "\"";
  return buffer.str();
}

void XCConfigurationList::Visit(PBXObjectVisitor& visitor) {
  PBXObject::Visit(visitor);
  for (const auto& configuration : configurations_) {
    configuration->Visit(visitor);
  }
}

void XCConfigurationList::Print(std::ostream& out, unsigned indent) const {
  const std::string indent_str(indent, '\t');
  const IndentRules rules = {false, indent + 1};
  out << indent_str << Reference() << " = {\n";
  PrintProperty(out, rules, "isa", ToString(Class()));
  PrintProperty(out, rules, "buildConfigurations", configurations_);
  PrintProperty(out, rules, "defaultConfigurationIsVisible", 1u);
  PrintProperty(out, rules, "defaultConfigurationName",
                configurations_[0]->Name());
  out << indent_str << "};\n";
}
