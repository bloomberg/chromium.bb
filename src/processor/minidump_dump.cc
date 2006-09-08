// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// minidump_dump.cc: Print the contents of a minidump file in somewhat
// readable text.
//
// Author: Mark Mentovai

#include <stdlib.h>
#include <stdio.h>

#include <string>

#include "processor/minidump.h"


using std::string;
using namespace google_airbag;


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    exit(1);
  }

  Minidump minidump(argv[1]);
  if (!minidump.Read()) {
    printf("minidump.Read() failed\n");
    exit(1);
  }
  minidump.Print();

  int error = 0;

  MinidumpThreadList* threadList = minidump.GetThreadList();
  if (!threadList) {
    error |= 1 << 2;
    printf("minidump.GetThreadList() failed\n");
  } else {
    threadList->Print();
  }

  MinidumpModuleList* moduleList = minidump.GetModuleList();
  if (!moduleList) {
    error |= 1 << 3;
    printf("minidump.GetModuleList() failed\n");
  } else {
    moduleList->Print();
  }

  MinidumpMemoryList* memoryList = minidump.GetMemoryList();
  if (!memoryList) {
    error |= 1 << 4;
    printf("minidump.GetMemoryList() failed\n");
  } else {
    memoryList->Print();
  }

  MinidumpException* exception = minidump.GetException();
  if (!exception) {
    error |= 1 << 5;
    printf("minidump.GetException() failed\n");
  } else {
    exception->Print();
  }

  MinidumpSystemInfo* systemInfo = minidump.GetSystemInfo();
  if (!systemInfo) {
    error |= 1 << 6;
    printf("minidump.GetSystemInfo() failed\n");
  } else {
    systemInfo->Print();
  }

  MinidumpMiscInfo* miscInfo = minidump.GetMiscInfo();
  if (!miscInfo) {
    error |= 1 << 7;
    printf("minidump.GetMiscInfo() failed\n");
  } else {
    miscInfo->Print();
  }

  // Use return instead of exit to allow destructors to run.
  return(error);
}
