// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "tools/cddl/codegen.h"
#include "tools/cddl/parse.h"
#include "tools/cddl/sema.h"

std::string ReadEntireFile(const std::string& filename) {
  std::ifstream input(filename);
  if (!input) {
    return {};
  }

  input.seekg(0, std::ios_base::end);
  size_t length = input.tellg();
  std::string input_data(length + 1, 0);

  input.seekg(0, std::ios_base::beg);
  input.read(const_cast<char*>(input_data.data()), length);
  input_data[length] = 0;

  return input_data;
}

struct CommandLineArguments {
  std::string header_filename;
  std::string cc_filename;
  std::string gen_dir;
  std::string cddl_filename;
};

CommandLineArguments ParseCommandLineArguments(int argc, char** argv) {
  --argc;
  ++argv;
  CommandLineArguments result;
  while (argc) {
    if (strcmp(*argv, "--header") == 0) {
      if (!result.header_filename.empty()) {
        return {};
      }
      if (!argc) {
        return {};
      }
      --argc;
      ++argv;
      result.header_filename = *argv;
    } else if (strcmp(*argv, "--cc") == 0) {
      if (!result.cc_filename.empty()) {
        return {};
      }
      if (!argc) {
        return {};
      }
      --argc;
      ++argv;
      result.cc_filename = *argv;
    } else if (strcmp(*argv, "--gen-dir") == 0) {
      if (!result.gen_dir.empty()) {
        return {};
      }
      if (!argc) {
        return {};
      }
      --argc;
      ++argv;
      result.gen_dir = *argv;
    } else if (!result.cddl_filename.empty()) {
      return {};
    } else {
      result.cddl_filename = *argv;
    }
    --argc;
    ++argv;
  }
  if (result.header_filename.empty() || result.cc_filename.empty() ||
      result.gen_dir.empty() || result.cddl_filename.empty()) {
    return {};
  }
  return result;
}

int main(int argc, char** argv) {
  CommandLineArguments args = ParseCommandLineArguments(argc, argv);
  if (args.cddl_filename.empty()) {
    std::cerr << "bad arguments" << std::endl;
    return 1;
  }

  size_t pos = args.cddl_filename.find_last_of('.');
  if (pos == std::string::npos) {
    return 1;
  }
  std::string header_filename =
      args.gen_dir + std::string("/") + args.header_filename;
  int header_fd = open(header_filename.c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                       S_IRUSR | S_IWUSR | S_IRGRP);
  if (header_fd == -1) {
    std::cerr << "failed to open " << args.header_filename << std::endl;
    return 1;
  }
  std::string cc_filename = args.gen_dir + std::string("/") + args.cc_filename;
  int cc_fd = open(cc_filename.c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                   S_IRUSR | S_IWUSR | S_IRGRP);
  if (cc_fd == -1) {
    std::cerr << "failed to open " << args.cc_filename << std::endl;
    return 1;
  }
  std::string data = ReadEntireFile(args.cddl_filename);
  if (data.empty()) {
    return 1;
  }

  ParseResult parse_result = ParseCddl(data);
  if (!parse_result.root) {
    return 1;
  }

  std::pair<bool, CddlSymbolTable> cddl_result =
      BuildSymbolTable(*parse_result.root);
  if (!cddl_result.first) {
    return 1;
  }
  std::pair<bool, CppSymbolTable> cpp_result =
      BuildCppTypes(cddl_result.second);
  if (!cpp_result.first) {
    return 1;
  }
  if (!WriteHeaderPrologue(header_fd, args.header_filename) ||
      !WriteTypeDefinitions(header_fd, cpp_result.second) ||
      !WriteFunctionDeclarations(header_fd, cpp_result.second) ||
      !WriteHeaderEpilogue(header_fd, args.header_filename) ||
      !WriteSourcePrologue(cc_fd, args.header_filename) ||
      !WriteEncoders(cc_fd, cpp_result.second) ||
      !WriteDecoders(cc_fd, cpp_result.second) || !WriteSourceEpilogue(cc_fd)) {
    return 1;
  }

  close(header_fd);
  close(cc_fd);

  return 0;
}
