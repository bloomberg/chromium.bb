// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_getter_registry.h"

#include "base/no_destructor.h"

namespace content {

using WebContentsGetter = WebContentsGetterRegistry::WebContentsGetter;

// static
WebContentsGetterRegistry* WebContentsGetterRegistry::GetInstance() {
  static base::NoDestructor<WebContentsGetterRegistry> instance;
  return instance.get();
}

void WebContentsGetterRegistry::Add(
    const base::UnguessableToken& id,
    const WebContentsGetter& web_contents_getter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = (map_.emplace(id, web_contents_getter)).second;
  CHECK(inserted);
}

void WebContentsGetterRegistry::Remove(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  map_.erase(id);
}

const WebContentsGetter& WebContentsGetterRegistry::Get(
    const base::UnguessableToken& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = map_.find(id);
  if (iter == map_.end())
    return GetNullGetter();
  return iter->second;
}

WebContentsGetterRegistry::WebContentsGetterRegistry() = default;

WebContentsGetterRegistry::~WebContentsGetterRegistry() = default;

// static
const WebContentsGetter& WebContentsGetterRegistry::GetNullGetter() {
  static const base::NoDestructor<WebContentsGetter> null_getter;
  return *null_getter;
}

}  // namespace content
