// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/Extensions3DUtil.h"

#include <memory>
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

namespace {

void SplitStringHelper(const String& str, HashSet<String>& set) {
  Vector<String> substrings;
  str.Split(' ', substrings);
  for (size_t i = 0; i < substrings.size(); ++i)
    set.insert(substrings[i]);
}

}  // anonymous namespace

std::unique_ptr<Extensions3DUtil> Extensions3DUtil::Create(
    gpu::gles2::GLES2Interface* gl) {
  std::unique_ptr<Extensions3DUtil> out =
      WTF::WrapUnique(new Extensions3DUtil(gl));
  out->InitializeExtensions();
  return out;
}

Extensions3DUtil::Extensions3DUtil(gpu::gles2::GLES2Interface* gl)
    : gl_(gl), is_valid_(true) {}

Extensions3DUtil::~Extensions3DUtil() {}

void Extensions3DUtil::InitializeExtensions() {
  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // If the context is lost don't initialize the extension strings.
    // This will cause supportsExtension, ensureExtensionEnabled, and
    // isExtensionEnabled to always return false.
    is_valid_ = false;
    return;
  }

  String extensions_string(gl_->GetString(GL_EXTENSIONS));
  SplitStringHelper(extensions_string, enabled_extensions_);

  String requestable_extensions_string(gl_->GetRequestableExtensionsCHROMIUM());
  SplitStringHelper(requestable_extensions_string, requestable_extensions_);
}

bool Extensions3DUtil::SupportsExtension(const String& name) {
  return enabled_extensions_.Contains(name) ||
         requestable_extensions_.Contains(name);
}

bool Extensions3DUtil::EnsureExtensionEnabled(const String& name) {
  if (enabled_extensions_.Contains(name))
    return true;

  if (requestable_extensions_.Contains(name)) {
    gl_->RequestExtensionCHROMIUM(name.Ascii().Data());
    enabled_extensions_.Clear();
    requestable_extensions_.Clear();
    InitializeExtensions();
  }
  return enabled_extensions_.Contains(name);
}

bool Extensions3DUtil::IsExtensionEnabled(const String& name) {
  return enabled_extensions_.Contains(name);
}

bool Extensions3DUtil::CanUseCopyTextureCHROMIUM(GLenum dest_target,
                                                 GLenum dest_format,
                                                 GLenum dest_type,
                                                 GLint level) {
  // TODO(zmo): restriction of (RGB || RGBA)/UNSIGNED_BYTE/(Level 0) should be
  // lifted when GLES2Interface::CopyTextureCHROMIUM(...) are fully functional.
  if (dest_target == GL_TEXTURE_2D &&
      (dest_format == GL_RGB || dest_format == GL_RGBA) &&
      dest_type == GL_UNSIGNED_BYTE && !level)
    return true;
  return false;
}

}  // namespace blink
