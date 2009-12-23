// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// module_unittest.cc: Unit tests for google_breakpad::Module

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>

#include "breakpad_googletest_includes.h"
#include "common/linux/module.h"

using std::string;
using std::vector;
using google_breakpad::Module;

// Return a FILE * referring to a temporary file that will be deleted
// automatically when the stream is closed or the program exits.
FILE *checked_tmpfile() {
  FILE *f = tmpfile();
  if (!f) {
    fprintf(stderr, "error creating temporary file: %s\n", strerror(errno));
    exit(1);
  }
  return f;
}

// Read from STREAM until end of file, and return the contents as a
// string.
string checked_read(FILE *stream) {
  string contents;
  int c;
  while ((c = getc(stream)) != EOF)
    contents.push_back(c);
  if (ferror(stream)) {
    fprintf(stderr, "error reading temporary file contents: %s\n",
            strerror(errno));
    exit(1);
  }
  return contents;
}

// Apply 'fflush' to STREAM, and check for errors.
void checked_fflush(FILE *stream) {
  if (fflush(stream) == EOF) {
    fprintf(stderr, "error flushing temporary file stream: %s\n",
            strerror(errno));
    exit(1);
  }
}

// Apply 'fclose' to STREAM, and check for errors.
void checked_fclose(FILE *stream) {
  if (fclose(stream) == EOF) {
    fprintf(stderr, "error closing temporary file stream: %s\n",
            strerror(errno));
    exit(1);
  }
}

#define MODULE_NAME "name with spaces"
#define MODULE_OS "os-name"
#define MODULE_ARCH "architecture"
#define MODULE_ID "id-string"

TEST(Write, Header) {
  FILE *f = checked_tmpfile();
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);
  m.Write(f);
  checked_fflush(f);
  rewind(f);
  string contents = checked_read(f);
  checked_fclose(f);
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n",
               contents.c_str());
}

TEST(Write, OneLineFunc) {
  FILE *f = checked_tmpfile();
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  Module::File *file = m.FindFile("file_name.cc");
  Module::Function *function = new(Module::Function);
  function->name_ = "function_name";
  function->address_ = 0xe165bf8023b9d9abLL;
  function->size_ = 0x1e4bb0eb1cbf5b09LL;
  function->parameter_size_ = 0x772beee89114358aLL;
  Module::Line line = { 0xe165bf8023b9d9abLL, 0x1e4bb0eb1cbf5b09LL,
                        file, 67519080 };
  function->lines_.push_back(line);
  m.AddFunction(function);

  m.Write(f);
  checked_fflush(f);
  rewind(f);
  string contents = checked_read(f);
  checked_fclose(f);
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 file_name.cc\n"
               "FUNC e165bf8023b9d9ab 1e4bb0eb1cbf5b09 772beee89114358a"
               " function_name\n"
               "e165bf8023b9d9ab 1e4bb0eb1cbf5b09 67519080 0\n",
               contents.c_str());
}

TEST(Write, RelativeLoadAddress) {
  FILE *f = checked_tmpfile();
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  m.SetLoadAddress(0x2ab698b0b6407073LL);

  // Some source files.  We will expect to see them in lexicographic order.
  Module::File *file1 = m.FindFile("filename-b.cc");
  Module::File *file2 = m.FindFile("filename-a.cc");

  // A function.
  Module::Function *function = new(Module::Function);
  function->name_ = "A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)";
  function->address_ = 0xbec774ea5dd935f3LL;
  function->size_ = 0x2922088f98d3f6fcLL;
  function->parameter_size_ = 0xe5e9aa008bd5f0d0LL;

  // Some source lines.  The module should not sort these.
  Module::Line line1 = { 0xbec774ea5dd935f3LL, 0x1c2be6d6c5af2611LL,
                         file1, 41676901 };
  Module::Line line2 = { 0xdaf35bc123885c04LL, 0xcf621b8d324d0ebLL,
                         file2, 67519080 };
  function->lines_.push_back(line2);
  function->lines_.push_back(line1);

  m.AddFunction(function);

  m.Write(f);
  checked_fflush(f);
  rewind(f);
  string contents = checked_read(f);
  checked_fclose(f);
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename-a.cc\n"
               "FILE 1 filename-b.cc\n"
               "FUNC 9410dc39a798c580 2922088f98d3f6fc e5e9aa008bd5f0d0"
               " A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)\n"
               "b03cc3106d47eb91 cf621b8d324d0eb 67519080 0\n"
               "9410dc39a798c580 1c2be6d6c5af2611 41676901 1\n",
               contents.c_str());
}

TEST(Write, OmitUnusedFiles) {
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Create some source files.
  Module::File *file1 = m.FindFile("filename1");
  m.FindFile("filename2"); // not used by any line
  Module::File *file3 = m.FindFile("filename3");

  // Create a function.
  Module::Function *function = new(Module::Function);
  function->name_ = "function_name";
  function->address_ = 0x9b926d464f0b9384LL;
  function->size_ = 0x4f524a4ba795e6a6LL;
  function->parameter_size_ = 0xbbe8133a6641c9b7LL;

  // Source files that refer to some files, but not others.
  Module::Line line1 = { 0x595fa44ebacc1086LL, 0x1e1e0191b066c5b3LL,
                         file1, 137850127 };
  Module::Line line2 = { 0x401ce8c8a12d25e3LL, 0x895751c41b8d2ce2LL,
                         file3, 28113549 };
  function->lines_.push_back(line1);
  function->lines_.push_back(line2);
  m.AddFunction(function);

  m.AssignSourceIds();
  
  vector<Module::File *> vec;
  m.GetFiles(&vec);
  EXPECT_EQ((size_t) 3, vec.size());
  EXPECT_STREQ("filename1", vec[0]->name_.c_str());
  EXPECT_NE(-1, vec[0]->source_id_);
  // Expect filename2 not to be used.
  EXPECT_STREQ("filename2", vec[1]->name_.c_str());
  EXPECT_EQ(-1, vec[1]->source_id_);
  EXPECT_STREQ("filename3", vec[2]->name_.c_str());
  EXPECT_NE(-1, vec[2]->source_id_);

  FILE *f = checked_tmpfile();
  m.Write(f);
  checked_fflush(f);
  rewind(f);
  string contents = checked_read(f);
  checked_fclose(f);
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename1\n"
               "FILE 1 filename3\n"
               "FUNC 9b926d464f0b9384 4f524a4ba795e6a6 bbe8133a6641c9b7"
               " function_name\n"
               "595fa44ebacc1086 1e1e0191b066c5b3 137850127 0\n"
               "401ce8c8a12d25e3 895751c41b8d2ce2 28113549 1\n",
               contents.c_str());
}

TEST(Construct, AddFunctions) {
  FILE *f = checked_tmpfile();
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two functions.
  Module::Function *function1 = new(Module::Function);
  function1->name_ = "_without_form";
  function1->address_ = 0xd35024aa7ca7da5cLL;
  function1->size_ = 0x200b26e605f99071LL;
  function1->parameter_size_ = 0xf14ac4fed48c4a99LL;

  Module::Function *function2 = new(Module::Function);
  function2->name_ = "_and_void";
  function2->address_ = 0x2987743d0b35b13fLL;
  function2->size_ = 0xb369db048deb3010LL;
  function2->parameter_size_ = 0x938e556cb5a79988LL;

  // Put them in a vector.
  vector<Module::Function *> vec;
  vec.push_back(function1);
  vec.push_back(function2);

  m.AddFunctions(vec.begin(), vec.end());

  m.Write(f);
  checked_fflush(f);
  rewind(f);
  string contents = checked_read(f);
  checked_fclose(f);
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FUNC d35024aa7ca7da5c 200b26e605f99071 f14ac4fed48c4a99"
               " _without_form\n"
               "FUNC 2987743d0b35b13f b369db048deb3010 938e556cb5a79988"
               " _and_void\n",
               contents.c_str());

  // Check that m.GetFunctions returns the functions we expect.
  vec.clear();
  m.GetFunctions(&vec, vec.end());
  EXPECT_TRUE(vec.end() != find(vec.begin(), vec.end(), function1));
  EXPECT_TRUE(vec.end() != find(vec.begin(), vec.end(), function2));
  EXPECT_EQ((size_t) 2, vec.size());
}

TEST(Construct, UniqueFiles) {
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);
  Module::File *file1 = m.FindFile("foo");
  Module::File *file2 = m.FindFile(string("bar"));
  Module::File *file3 = m.FindFile(string("foo"));
  Module::File *file4 = m.FindFile("bar");
  EXPECT_NE(file1, file2);
  EXPECT_EQ(file1, file3);
  EXPECT_EQ(file2, file4);
  EXPECT_EQ(file1, m.FindExistingFile("foo"));
  EXPECT_TRUE(m.FindExistingFile("baz") == NULL);
}
