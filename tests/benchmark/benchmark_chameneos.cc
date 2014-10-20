// Copyright 2014 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

// @EXEMPTION[include]
#include "./framework.h"

extern "C" {
  int main_chameneos(int argc, const char* argv[]);
}

namespace {

// Wrap Chameneos in benchmark harness
class BenchmarkChameneos : public Benchmark {
 public:
  virtual int Run() {
    const char *args[] = {"chameneos", "60000"};
    return main_chameneos(2, args);
  }
  virtual const std::string Name() { return "Chameneos"; }
  virtual const std::string Notes() { return "c #2 version"; }
};

}  // namespace

// Register an instance to the list of benchmarks to be run.
RegisterBenchmark<BenchmarkChameneos> benchmark_chameneos;
