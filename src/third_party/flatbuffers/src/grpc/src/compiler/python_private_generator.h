/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef GRPC_INTERNAL_COMPILER_PYTHON_PRIVATE_GENERATOR_H
#define GRPC_INTERNAL_COMPILER_PYTHON_PRIVATE_GENERATOR_H

#include <iostream>
#include <vector>

#include "src/compiler/python_generator.h"
#include "src/compiler/schema_interface.h"

namespace grpc_python_generator {

// Tucks all generator state in an anonymous namespace away from
// PythonGrpcGenerator and the header file, mostly to encourage future changes
// to not require updates to the grpcio-tools C++ code part. Assumes that it is
// only ever used from a single thread.
struct PrivateGenerator {
  const GeneratorConfiguration& config;
  const grpc_generator::File* file;

  PrivateGenerator(const GeneratorConfiguration& config,
                   const grpc_generator::File* file);

  grpc::string GetGrpcServices();

 private:
  void PrintPreamble(grpc_generator::Printer* out);
  void PrintBetaPreamble(grpc_generator::Printer* out);
  void PrintGAServices(grpc_generator::Printer* out);
  void PrintBetaServices(grpc_generator::Printer* out);

  void PrintAddServicerToServer(
      const grpc::string& package_qualified_service_name,
      const grpc_generator::Service* service, grpc_generator::Printer* out);
  void PrintServicer(const grpc_generator::Service* service,
                     grpc_generator::Printer* out);
  void PrintStub(const grpc::string& package_qualified_service_name,
                 const grpc_generator::Service* service,
                 grpc_generator::Printer* out);

  void PrintBetaServicer(const grpc_generator::Service* service,
                         grpc_generator::Printer* out);
  void PrintBetaServerFactory(
      const grpc::string& package_qualified_service_name,
      const grpc_generator::Service* service, grpc_generator::Printer* out);
  void PrintBetaStub(const grpc_generator::Service* service,
                     grpc_generator::Printer* out);
  void PrintBetaStubFactory(const grpc::string& package_qualified_service_name,
                            const grpc_generator::Service* service,
                            grpc_generator::Printer* out);
};

}  // namespace grpc_python_generator

#endif  // GRPC_INTERNAL_COMPILER_PYTHON_PRIVATE_GENERATOR_H
