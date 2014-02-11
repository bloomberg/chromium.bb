// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Checks that are implemented:
// [currently none]

#include "Config.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;
using std::string;

namespace {

struct BlinkGCPluginOptions {
  BlinkGCPluginOptions() {
  }
};

// Main class containing checks for various invariants of the Blink
// garbage collection infrastructure.
class BlinkGCPluginConsumer : public ASTConsumer {
 public:
  BlinkGCPluginConsumer(CompilerInstance& instance,
                        const BlinkGCPluginOptions& options) {
  }

  virtual void HandleTranslationUnit(ASTContext& context) {
    // FIXME: implement consistency checks.
  }
};


class BlinkGCPluginAction : public PluginASTAction {
 public:
  BlinkGCPluginAction() {
  }

 protected:
  // Overridden from PluginASTAction:
  virtual ASTConsumer* CreateASTConsumer(CompilerInstance& instance,
                                         llvm::StringRef ref) {
    return new BlinkGCPluginConsumer(instance, options_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "enable-oilpan") {
        // TODO: Remove this once all transition types are eliminated.
        Config::set_oilpan_enabled(true);
      } else {
        parsed = false;
        llvm::errs() << "Unknown blink-gc-plugin argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  BlinkGCPluginOptions options_;
};

}  // namespace

bool Config::oilpan_enabled_ = false;

static FrontendPluginRegistry::Add<BlinkGCPluginAction>
X("blink-gc-plugin", "Check Blink GC invariants");
