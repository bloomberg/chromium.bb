// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace {

bool DesktopCoreCommonCheck(
    bool is_es, unsigned major_version, unsigned minor_version) {
  return (!is_es &&
          ((major_version == 3 && minor_version >= 2) ||
           major_version > 3));
}

}

namespace gl {

GLVersionInfo::GLVersionInfo(const char* version_str,
                             const char* renderer_str,
                             const ExtensionSet& extensions)
    : is_es(false),
      is_angle(false),
      is_mesa(false),
      is_swiftshader(false),
      major_version(0),
      minor_version(0),
      is_es2(false),
      is_es3(false),
      is_desktop_core_profile(false),
      is_es3_capable(false) {
  Initialize(version_str, renderer_str, extensions);
}

void GLVersionInfo::Initialize(const char* version_str,
                               const char* renderer_str,
                               const ExtensionSet& extensions) {
  if (version_str) {
    ParseVersionString(version_str, &major_version, &minor_version,
                       &is_es, &is_es2, &is_es3);
  }
  if (renderer_str) {
    is_angle = base::StartsWith(renderer_str, "ANGLE",
                                base::CompareCase::SENSITIVE);
    is_mesa = base::StartsWith(renderer_str, "Mesa",
                               base::CompareCase::SENSITIVE);
    is_swiftshader = base::StartsWith(renderer_str, "Google SwiftShader",
                                      base::CompareCase::SENSITIVE);
  }
  is_desktop_core_profile =
      DesktopCoreCommonCheck(is_es, major_version, minor_version) &&
      !HasExtension(extensions, "GL_ARB_compatibility");
  is_es3_capable = IsES3Capable(extensions);
}

bool GLVersionInfo::IsES3Capable(const ExtensionSet& extensions) const {
  // Version ES3 capable without extensions needed.
  if (IsAtLeastGLES(3, 0) || IsAtLeastGL(4, 2)) {
    return true;
  }

  // Don't try supporting ES3 on ES2, or desktop before 3.3.
  if (is_es || !IsAtLeastGL(3, 3)) {
    return false;
  }

  bool has_transform_feedback =
      (IsAtLeastGL(4, 0) ||
       HasExtension(extensions, "GL_ARB_transform_feedback2"));

  // This code used to require the GL_ARB_gpu_shader5 extension in order to
  // have support for dynamic indexing of sampler arrays, which was
  // optionally supported in ESSL 1.00. However, since this is expressly
  // forbidden in ESSL 3.00, and some desktop drivers (specifically
  // Mesa/Gallium on AMD GPUs) don't support it, we no longer require it.

  // tex storage is available in core spec since GL 4.2.
  bool has_tex_storage = HasExtension(extensions, "GL_ARB_texture_storage");

  // TODO(cwallez) check for texture related extensions. See crbug.com/623577

  return (has_transform_feedback && has_tex_storage);
}

void GLVersionInfo::ParseVersionString(const char* version_str,
                                       unsigned* major_version,
                                       unsigned* minor_version,
                                       bool* is_es,
                                       bool* is_es2,
                                       bool* is_es3) {
  // Make sure the outputs are always initialized.
  *major_version = 0;
  *minor_version = 0;
  *is_es = false;
  *is_es2 = false;
  *is_es3 = false;
  if (!version_str)
    return;
  std::string lstr(base::ToLowerASCII(version_str));
  *is_es = (lstr.length() > 12) && (lstr.substr(0, 9) == "opengl es");
  if (*is_es)
    lstr = lstr.substr(10, 3);
  base::StringTokenizer tokenizer(lstr.begin(), lstr.end(), ". ");
  unsigned major, minor;
  if (tokenizer.GetNext() &&
      base::StringToUint(tokenizer.token_piece(), &major)) {
    *major_version = major;
    if (tokenizer.GetNext() &&
        base::StringToUint(tokenizer.token_piece(), &minor)) {
      *minor_version = minor;
    }
  }
  if (*is_es && *major_version == 2)
    *is_es2 = true;
  if (*is_es && *major_version == 3)
    *is_es3 = true;
  DCHECK(major_version != 0);
}

}  // namespace gl
