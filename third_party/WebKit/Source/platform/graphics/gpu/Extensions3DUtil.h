// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Extensions3DUtil_h
#define Extensions3DUtil_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class PLATFORM_EXPORT Extensions3DUtil final {
  USING_FAST_MALLOC(Extensions3DUtil);
  WTF_MAKE_NONCOPYABLE(Extensions3DUtil);

 public:
  // Creates a new Extensions3DUtil. If the passed GLES2Interface has been
  // spontaneously lost, returns null.
  static std::unique_ptr<Extensions3DUtil> Create(gpu::gles2::GLES2Interface*);
  ~Extensions3DUtil();

  bool IsValid() { return is_valid_; }

  bool SupportsExtension(const String& name);
  bool EnsureExtensionEnabled(const String& name);
  bool IsExtensionEnabled(const String& name);

  static bool CanUseCopyTextureCHROMIUM(GLenum dest_target);

 private:
  Extensions3DUtil(gpu::gles2::GLES2Interface*);
  void InitializeExtensions();

  gpu::gles2::GLES2Interface* gl_;
  HashSet<String> enabled_extensions_;
  HashSet<String> requestable_extensions_;
  bool is_valid_;
};

}  // namespace blink

#endif  // Extensions3DUtil_h
