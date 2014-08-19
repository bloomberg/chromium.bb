// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target_generator.h"

#include "tools/gn/action_target_generator.h"
#include "tools/gn/binary_target_generator.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/copy_target_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/group_target_generator.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

TargetGenerator::TargetGenerator(Target* target,
                                 Scope* scope,
                                 const FunctionCallNode* function_call,
                                 Err* err)
    : target_(target),
      scope_(scope),
      function_call_(function_call),
      err_(err) {
}

TargetGenerator::~TargetGenerator() {
}

void TargetGenerator::Run() {
  // All target types use these.
  FillDependentConfigs();
  if (err_->has_error())
    return;

  FillData();
  if (err_->has_error())
    return;

  FillDependencies();
  if (err_->has_error())
    return;

  if (!Visibility::FillItemVisibility(target_, scope_, err_))
    return;

  // Do type-specific generation.
  DoRun();
}

// static
void TargetGenerator::GenerateTarget(Scope* scope,
                                     const FunctionCallNode* function_call,
                                     const std::vector<Value>& args,
                                     const std::string& output_type,
                                     Err* err) {
  // Name is the argument to the function.
  if (args.size() != 1u || args[0].type() != Value::STRING) {
    *err = Err(function_call,
        "Target generator requires one string argument.",
        "Otherwise I'm not sure what to call this target.");
    return;
  }

  // The location of the target is the directory name with no slash at the end.
  // FIXME(brettw) validate name.
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  Label label(scope->GetSourceDir(), args[0].string_value(),
              toolchain_label.dir(), toolchain_label.name());

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Defining target", label.GetUserVisibleName(true));

  scoped_ptr<Target> target(new Target(scope->settings(), label));
  target->set_defined_from(function_call);

  // Create and call out to the proper generator.
  if (output_type == functions::kCopy) {
    CopyTargetGenerator generator(target.get(), scope, function_call, err);
    generator.Run();
  } else if (output_type == functions::kAction) {
    ActionTargetGenerator generator(target.get(), scope, function_call,
                                    Target::ACTION, err);
    generator.Run();
  } else if (output_type == functions::kActionForEach) {
    ActionTargetGenerator generator(target.get(), scope, function_call,
                                    Target::ACTION_FOREACH, err);
    generator.Run();
  } else if (output_type == functions::kExecutable) {
    BinaryTargetGenerator generator(target.get(), scope, function_call,
                                    Target::EXECUTABLE, err);
    generator.Run();
  } else if (output_type == functions::kGroup) {
    GroupTargetGenerator generator(target.get(), scope, function_call, err);
    generator.Run();
  } else if (output_type == functions::kSharedLibrary) {
    BinaryTargetGenerator generator(target.get(), scope, function_call,
                                    Target::SHARED_LIBRARY, err);
    generator.Run();
  } else if (output_type == functions::kSourceSet) {
    BinaryTargetGenerator generator(target.get(), scope, function_call,
                                    Target::SOURCE_SET, err);
    generator.Run();
  } else if (output_type == functions::kStaticLibrary) {
    BinaryTargetGenerator generator(target.get(), scope, function_call,
                                    Target::STATIC_LIBRARY, err);
    generator.Run();
  } else {
    *err = Err(function_call, "Not a known output type",
               "I am very confused.");
  }

  if (err->has_error())
    return;

  // Save this target for the file.
  Scope::ItemVector* collector = scope->GetItemCollector();
  if (!collector) {
    *err = Err(function_call, "Can't define a target in this context.");
    return;
  }
  collector->push_back(new scoped_ptr<Item>(target.PassAs<Item>()));
}

const BuildSettings* TargetGenerator::GetBuildSettings() const {
  return scope_->settings()->build_settings();
}

void TargetGenerator::FillSources() {
  const Value* value = scope_->GetValue(variables::kSources, true);
  if (!value)
    return;

  Target::FileList dest_sources;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_sources, err_))
    return;
  target_->sources().swap(dest_sources);
}

void TargetGenerator::FillPublic() {
  const Value* value = scope_->GetValue(variables::kPublic, true);
  if (!value)
    return;

  // If the public headers are defined, don't default to public.
  target_->set_all_headers_public(false);

  Target::FileList dest_public;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_public, err_))
    return;
  target_->public_headers().swap(dest_public);
}

void TargetGenerator::FillInputs() {
  const Value* value = scope_->GetValue(variables::kInputs, true);
  if (!value) {
    // Older versions used "source_prereqs". Allow use of this variable until
    // all callers are updated.
    // TODO(brettw) remove this eventually.
    value = scope_->GetValue("source_prereqs", true);

    if (!value)
      return;
  }

  Target::FileList dest_inputs;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_inputs, err_))
    return;
  target_->inputs().swap(dest_inputs);
}

void TargetGenerator::FillConfigs() {
  FillGenericConfigs(variables::kConfigs, &target_->configs());
}

void TargetGenerator::FillDependentConfigs() {
  FillGenericConfigs(variables::kAllDependentConfigs,
                     &target_->all_dependent_configs());
  FillGenericConfigs(variables::kDirectDependentConfigs,
                     &target_->direct_dependent_configs());
}

void TargetGenerator::FillData() {
  const Value* value = scope_->GetValue(variables::kData, true);
  if (!value)
    return;

  Target::FileList dest_data;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &dest_data, err_))
    return;
  target_->data().swap(dest_data);
}

void TargetGenerator::FillDependencies() {
  FillGenericDeps(variables::kDeps, &target_->deps());
  if (err_->has_error())
    return;
  FillGenericDeps(variables::kDatadeps, &target_->datadeps());
  if (err_->has_error())
    return;

  // This is a list of dependent targets to have their configs fowarded, so
  // it goes here rather than in FillConfigs.
  FillForwardDependentConfigs();
  if (err_->has_error())
    return;
}

void TargetGenerator::FillOutputs(bool allow_substitutions) {
  const Value* value = scope_->GetValue(variables::kOutputs, true);
  if (!value)
    return;

  SubstitutionList& outputs = target_->action_values().outputs();
  if (!outputs.Parse(*value, err_))
    return;

  if (!allow_substitutions) {
    // Verify no substitutions were actually used.
    if (!outputs.required_types().empty()) {
      *err_ = Err(*value, "Source expansions not allowed here.",
          "The outputs of this target used source {{expansions}} but this "
          "targe type\ndoesn't support them. Just express the outputs "
          "literally.");
      return;
    }
  }

  // Check the substitutions used are valid for this purpose.
  if (!EnsureValidSourcesSubstitutions(outputs.required_types(),
                                       value->origin(), err_))
    return;

  // Validate that outputs are in the output dir.
  CHECK(outputs.list().size() == value->list_value().size());
  for (size_t i = 0; i < outputs.list().size(); i++) {
    if (!EnsureSubstitutionIsInOutputDir(outputs.list()[i],
                                         value->list_value()[i]))
      return;
  }
}

bool TargetGenerator::EnsureSubstitutionIsInOutputDir(
    const SubstitutionPattern& pattern,
    const Value& original_value) {
  if (pattern.ranges().empty()) {
    // Pattern is empty, error out (this prevents weirdness below).
    *err_ = Err(original_value, "This has an empty value in it.");
    return false;
  }

  if (pattern.ranges()[0].type == SUBSTITUTION_LITERAL) {
    // If the first thing is a literal, it must start with the output dir.
    if (!EnsureStringIsInOutputDir(
            GetBuildSettings()->build_dir(),
            pattern.ranges()[0].literal, original_value.origin(), err_))
      return false;
  } else {
    // Otherwise, the first subrange must be a pattern that expands to
    // something in the output directory.
    if (!SubstitutionIsInOutputDir(pattern.ranges()[0].type)) {
      *err_ = Err(original_value,
          "File is not inside output directory.",
          "The given file should be in the output directory. Normally you\n"
          "would specify\n\"$target_out_dir/foo\" or "
          "\"{{source_gen_dir}}/foo\".");
      return false;
    }
  }

  return true;
}

void TargetGenerator::FillGenericConfigs(const char* var_name,
                                         UniqueVector<LabelConfigPair>* dest) {
  const Value* value = scope_->GetValue(var_name, true);
  if (value) {
    ExtractListOfUniqueLabels(*value, scope_->GetSourceDir(),
                              ToolchainLabelForScope(scope_), dest, err_);
  }
}

void TargetGenerator::FillGenericDeps(const char* var_name,
                                      LabelTargetVector* dest) {
  const Value* value = scope_->GetValue(var_name, true);
  if (value) {
    ExtractListOfLabels(*value, scope_->GetSourceDir(),
                        ToolchainLabelForScope(scope_), dest, err_);
  }
}

void TargetGenerator::FillForwardDependentConfigs() {
  const Value* value = scope_->GetValue(
      variables::kForwardDependentConfigsFrom, true);
  if (value) {
    ExtractListOfUniqueLabels(*value, scope_->GetSourceDir(),
                              ToolchainLabelForScope(scope_),
                              &target_->forward_dependent_configs(), err_);
  }
}
