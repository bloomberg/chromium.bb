// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_STRING_SOURCE_MAP_H_
#define EXTENSIONS_RENDERER_STRING_SOURCE_MAP_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "extensions/renderer/source_map.h"

namespace extensions {

// A testing implementation of the source map that takes strings for source
// contents.
class StringSourceMap : public SourceMap {
 public:
  StringSourceMap();
  ~StringSourceMap() override;

  // Adds a new string to be used in the source map.
  void RegisterModule(const std::string& name,
                      const std::string& source,
                      bool gzipped = false);

  // SourceMap:
  v8::Local<v8::String> GetSource(v8::Isolate* isolate,
                                  const std::string& name) const override;
  bool Contains(const std::string& name) const override;

 private:
  std::map<std::string, std::string> sources_;

  DISALLOW_COPY_AND_ASSIGN(StringSourceMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_STRING_SOURCE_MAP_H_
