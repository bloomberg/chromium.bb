// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/node_holder.h"
#include "base/no_destructor.h"

namespace cc {

NodeHolder::NodeHolder() : is_empty(true) {}

NodeHolder::NodeHolder(scoped_refptr<TextHolder> holder)
    : text_holder(holder), type(Type::kTextHolder), is_empty(false) {}

NodeHolder::NodeHolder(int id) : id(id), type(Type::kID), is_empty(false) {}

NodeHolder::NodeHolder(const NodeHolder& other) {
  text_holder = other.text_holder;
  id = other.id;
  type = other.type;
  is_empty = other.is_empty;
}

NodeHolder::~NodeHolder() = default;

// static
const NodeHolder& NodeHolder::EmptyNodeHolder() {
  static const base::NoDestructor<NodeHolder> s;
  return *s;
}

bool operator==(const NodeHolder& l, const NodeHolder& r) {
  if (l.is_empty != r.is_empty) {
    return false;
  } else if (l.is_empty) {
    return true;
  } else {
    switch (l.type) {
      case NodeHolder::Type::kTextHolder:
        return l.text_holder == r.text_holder;
      case NodeHolder::Type::kID:
        return l.id == r.id;
    }
  }
}

bool operator!=(const NodeHolder& l, const NodeHolder& r) {
  return !(l == r);
}

}  // namespace cc
