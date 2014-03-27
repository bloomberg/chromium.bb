// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TEMPLATE_H_
#define TOOLS_GN_TEMPLATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class BlockNode;
class Err;
class FunctionCallNode;
class LocationRange;
class Scope;
class Value;

class Template {
 public:
  // Makes a new closure based on the given scope.
  Template(const Scope* scope, const FunctionCallNode* def);

  // Takes ownership of a previously-constructed closure.
  Template(scoped_ptr<Scope> closure, const FunctionCallNode* def);

  ~Template();

  // Makes a copy of this template.
  scoped_ptr<Template> Clone() const;

  // Invoke the template. The values correspond to the state of the code
  // invoking the template.
  Value Invoke(Scope* scope,
               const FunctionCallNode* invocation,
               const std::vector<Value>& args,
               BlockNode* block,
               Err* err) const;

  // Returns the location range where this template was defined.
  LocationRange GetDefinitionRange() const;

 private:
  Template();

  scoped_ptr<Scope> closure_;
  const FunctionCallNode* definition_;

  DISALLOW_COPY_AND_ASSIGN(Template);
};

#endif  // TOOLS_GN_TEMPLATE_H_
