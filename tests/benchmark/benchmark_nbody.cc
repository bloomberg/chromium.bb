// Copyright 2014 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "native_client/tests/benchmark/framework.h"


extern "C" {
  int main_nbody(int argc, const char* argv[]);
}

namespace {

// Wrap NBody in benchmark harness
class BenchmarkNBody : public Benchmark {
 public:
  virtual int Run() {
    const char *args[] = {"nbody", "500000"};
    return main_nbody(2, args);
  }
  virtual const std::string Name() { return "NBody"; }
  virtual const std::string Notes() { return "c #1 version"; }
};

}  // namespace

// Register an instance to the list of benchmarks to be run.
RegisterBenchmark<BenchmarkNBody> benchmark_nbody;
