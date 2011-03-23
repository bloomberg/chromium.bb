// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

// This is a simple C++ Pepper plugin that demonstrates HandleMessage and
// PostMessage.

// This object represents one time the page says <embed>.
class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~MyInstance() {}
  virtual void HandleMessage(const pp::Var& message_data);
};

// HandleMessage gets invoked when postMessage is called on the DOM element
// associated with this plugin instance.
// In this case, if we are given a string, we'll post a message back to
// JavaScript indicating whether or not that string is a palindrome.
void MyInstance::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string()) {
    std::string string_copy(message_data.AsString());
    std::istringstream str_stream(string_copy);
    std::string id;
    std::string input_string;
    // Tokenize the string to get the id and the input_string.  If we find both,
    // post a message back to JavaScript indicating whether the given string is
    // a palindrome.
    if (std::getline(str_stream, id, ',') &&
        std::getline(str_stream, input_string, ',')) {
      std::string reversed_string(input_string);
      std::reverse(reversed_string.begin(), reversed_string.end());
      bool is_palindrome(input_string == reversed_string);
      // Create a result string of the form "<id>,<result>", where <id> is the
      // id we were given, and <result> is true if the given string was a
      // palindrome, false otherwise.
      std::string result(id);
      result += ",";
      result += is_palindrome ? "true" : "false";
      // Send this result back to JS.
      PostMessage(pp::Var(result));
    }
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

