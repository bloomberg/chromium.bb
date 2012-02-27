// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FIND_DEV_H_
#define PPAPI_CPP_DEV_FIND_DEV_H_

#include <string>

#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Instance;

// This class allows you to associate the PPP_Find and PPB_Find C-based
// interfaces with an object. It associates itself with the given instance, and
// registers as the global handler for handling the PPP_Find interface that the
// browser calls.
//
// You would typically use this either via inheritance on your instance:
//   class MyInstance : public pp::Instance, public pp::Find_Dev {
//     class MyInstance() : pp::Find_Dev(this) {
//     }
//     ...
//   };
//
// or by composition:
//   class MyFinder : public pp::Find {
//     ...
//   };
//
//   class MyInstance : public pp::Instance {
//     MyInstance() : finder_(this) {
//     }
//
//     MyFinder finder_;
//   };
class Find_Dev {
 public:
  // The instance parameter must outlive this class.
  Find_Dev(Instance* instance);
  virtual ~Find_Dev();

  // PPP_Find_Dev functions exposed as virtual functions for you to
  // override.
  virtual bool StartFind(const std::string& text, bool case_sensitive) = 0;
  virtual void SelectFindResult(bool forward) = 0;
  virtual void StopFind() = 0;

  // PPB_Find_Def functions for you to call to report find results.
  void NumberOfFindResultsChanged(int32_t total, bool final_result);
  void SelectedFindResultChanged(int32_t index);

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FIND_DEV_H_
