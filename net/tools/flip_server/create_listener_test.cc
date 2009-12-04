// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <iostream>

#include "net/tools/flip_server/create_listener.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct TestCase {
  string host;
  string port;
  bool valid;
  TestCase(const string& host, const string& port, bool valid) :
    host(host), port(port), valid(valid) {
  }
};

int main(int argc, char ** argv) {
  vector<TestCase> tests;
  tests.push_back(TestCase(""         , "8090"   , true ));
  tests.push_back(TestCase("invalid"  , "80"     , false)); // bad host spec.
  tests.push_back(TestCase("127.0.0.1", "invalid", false)); // bad port spec.
  tests.push_back(TestCase("127.0.0.2", "80"     , false)); // priviledged port.
  tests.push_back(TestCase("127.0.0.2", "8080"   , true ));
  tests.push_back(TestCase("127.0.0.2", "8080"   , false)); // already bound.
  tests.push_back(TestCase(""         , ""       , false)); // bad port spec.

  // create sockets and bind on all indicated interface/port combinations.
  for (unsigned int i = 0; i < tests.size(); ++i) {
    cerr << "test " << i << "...";
    const TestCase& test = tests[i];

    int socket = -2;
    CreateListeningSocket(test.host,
                          test.port,
                          true,
                          5,
                          &socket,
                          true,
                          &cerr);
    assert(socket != -2);
    if (test.valid) {
      assert(socket != -1);
    }
    cerr << "...done\n";
    // it would be good to invoke a seperate process (perhaps "nc"?) to
    // talk to this process in order to verify that the listen worked.
  }

  return EXIT_SUCCESS;
}

