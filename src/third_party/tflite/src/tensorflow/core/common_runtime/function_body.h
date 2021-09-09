/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_BODY_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_BODY_H_

#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/gtl/inlined_vector.h"

namespace tensorflow {

class Graph;
class Node;

// FunctionLibraryRuntime::GetFunctionBody returns a description of an
// instantiated function that is represented as a Graph with arg/ret
// nodes annotated.
struct FunctionBody {
  FunctionDef fdef;
  Graph* graph = nullptr;  // owned.
  DataTypeVector arg_types;
  DataTypeVector ret_types;
  // arg_nodes[i] contains the i'th function input. In other words,
  // GetNodeAttr(arg_nodes[i]->attrs(), "index") == i.
  gtl::InlinedVector<Node*, 4> arg_nodes;
  // ret_nodes[i] contains the i'th function output. In other words,
  // GetNodeAttr(ret_nodes[i]->attrs(), "index") == i.
  gtl::InlinedVector<Node*, 4> ret_nodes;
  gtl::InlinedVector<Node*, 4> control_ret_nodes;

  FunctionBody() {}
  FunctionBody(const FunctionDef& f, DataTypeSlice arg_types,
               DataTypeSlice ret_types, Graph* g);
  ~FunctionBody();
};

}  // end namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_BODY_H_
