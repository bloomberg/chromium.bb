/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//
// Test for resource open before PPAPI initialization.
//

#include <stdio.h>
#include <string.h>

#include <string>
#include <sstream>

#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

std::string str;

void load_manifest(TYPE_nacl_irt_query *query_func) {
  struct nacl_irt_resource_open nacl_irt_resource_open;
  if (sizeof(nacl_irt_resource_open) !=
      (*query_func)(
          NACL_IRT_RESOURCE_OPEN_v0_1,
          &nacl_irt_resource_open,
          sizeof(nacl_irt_resource_open))) {
    str = "irt manifest api not found";
    return;
  }
  int desc;
  int error;
  error = nacl_irt_resource_open.open_resource("test_file", &desc);
  if (0 != error) {
    str = "Can't open file";
    printf("Can't open file, error=%d", error);
    return;
  }

  str = "File Contents:\n";

  FILE *iob = fdopen(desc, "r");
  char buffer[4096];
  while (fgets(buffer, sizeof buffer, iob) != NULL) {
    // NB: fgets does not discard the newline nor any carriage return
    // character before that.
    //
    // Note that CR LF is the default end-of-line style for Windows.
    // Furthermore, when the test_file (input data, which happens to
    // be the nmf file) is initially created in a change list, the
    // patch is sent to our try bots as text.  This means that when
    // the file arrives, it has CR LF endings instead of the original
    // LF line endings.  Since the expected or golden data is
    // (manually) encoded in the HTML file's JavaScript, there will be
    // a mismatch.  After submission, the svn property svn:eol-style
    // will be set to LF, so a clean check out should have LF and not
    // CR LF endings, and the tests will pass without CR removal.
    // However -- and there's always a however in long discourses --
    // if the nmf file is edited, say, because the test is being
    // modified, and the modification is being done on a Windows
    // machine, then it is likely that the editor used by the
    // programmer will convert the file to CR LF endings.  Which,
    // unfortunatly, implies that the test will mysteriously fail
    // again.
    //
    // To defend against such nonsense, we weaken the test slighty,
    // and just strip the CR if it is present.
    int len = strlen(buffer);
    if (len >= 2 && buffer[len-1] == '\n' && buffer[len-2] == '\r') {
      buffer[len-2] = '\n';
      buffer[len-1] = '\0';
    }
    str += buffer;
  }
  printf("file loaded: %s\n", str.c_str());
  fclose(iob);  // closed desc
  return;
}

class TestInstance : public pp::Instance {
 public:
  explicit TestInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~TestInstance() {}
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string()) {
      return;
    }
    if (var_message.AsString() != "hello") {
      return;
    }
    pp::Var reply = pp::Var(str);
    PostMessage(reply);
  }
};

class TestModule : public pp::Module {
 public:
  TestModule() : pp::Module() {}
  virtual ~TestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new TestInstance(instance);
  }
};

namespace pp {
Module* CreateModule() {
  return new TestModule();
}
}

int main() {
  load_manifest(&__nacl_irt_query);
  return PpapiPluginMain();
}

