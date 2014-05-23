// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace commands {

const char kCheck[] = "check";
const char kCheck_HelpShort[] =
    "check: Check header dependencies.";
const char kCheck_Help[] =
    "gn check: Check header dependencies.\n"
    "\n"
    "  \"gn check\" is the same thing as \"gn gen\" with the \"--check\" flag\n"
    "  except that this command does not write out any build files. It's\n"
    "  intended to be an easy way to manually trigger include file checking.\n"
    "\n"
    "  See \"gn help\" for the common command-line switches.\n";

int RunCheck(const std::vector<std::string>& args) {
  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup();
  // TODO(brettw) bug 343726: Use a temporary directory instead of this
  // default one to avoid messing up any build that's in there.
  if (!setup->DoSetup("//out/Default"))
    return 1;
  setup->set_check_public_headers(true);

  if (!setup->Run())
    return 1;

  OutputString("Header dependency check OK\n", DECORATION_GREEN);
  return 0;
}

}  // namespace commands
