// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include <stddef.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

namespace {

const struct {
  const char* name;
  GLImplementation implementation;
} kGLImplementationNamePairs[] = {
    {kGLImplementationDesktopName, kGLImplementationDesktopGL},
    {kGLImplementationOSMesaName, kGLImplementationOSMesaGL},
    {kGLImplementationSwiftShaderName, kGLImplementationSwiftShaderGL},
#if defined(OS_MACOSX)
    {kGLImplementationAppleName, kGLImplementationAppleGL},
#endif
    {kGLImplementationEGLName, kGLImplementationEGLGLES2},
    {kGLImplementationMockName, kGLImplementationMockGL}};

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementation g_gl_implementation = kGLImplementationNone;
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* unused) {
  if (g_libraries) {
    // We do not call base::UnloadNativeLibrary() for these libraries as
    // unloading libGL without closing X display is not allowed. See
    // crbug.com/250813 for details.
    delete g_libraries;
    g_libraries = NULL;
  }
}

}  // namespace

base::ThreadLocalPointer<CurrentGL>* g_current_gl_context_tls = NULL;
OSMESAApi* g_current_osmesa_context;

#if defined(USE_EGL)
EGLApi* g_current_egl_context;
#endif

#if defined(OS_WIN)
WGLApi* g_current_wgl_context;
#endif

#if defined(USE_GLX)
GLXApi* g_current_glx_context;
#endif

GLImplementation GetNamedGLImplementation(const std::string& name) {
  for (size_t i = 0; i < arraysize(kGLImplementationNamePairs); ++i) {
    if (name == kGLImplementationNamePairs[i].name)
      return kGLImplementationNamePairs[i].implementation;
  }

  return kGLImplementationNone;
}

GLImplementation GetSoftwareGLImplementation() {
#if defined(OS_WIN)
  return kGLImplementationSwiftShaderGL;
#else
  return kGLImplementationOSMesaGL;
#endif
}

const char* GetGLImplementationName(GLImplementation implementation) {
  for (size_t i = 0; i < arraysize(kGLImplementationNamePairs); ++i) {
    if (implementation == kGLImplementationNamePairs[i].implementation)
      return kGLImplementationNamePairs[i].name;
  }

  return "unknown";
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = implementation;
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation;
}

bool HasDesktopGLFeatures() {
  return kGLImplementationDesktopGL == g_gl_implementation ||
         kGLImplementationDesktopGLCoreProfile == g_gl_implementation ||
         kGLImplementationOSMesaGL == g_gl_implementation ||
         kGLImplementationAppleGL == g_gl_implementation;
}

void AddGLNativeLibrary(base::NativeLibrary library) {
  DCHECK(library);

  if (!g_libraries) {
    g_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(CleanupNativeLibraries, NULL);
  }

  g_libraries->push_back(library);
}

void UnloadGLNativeLibraries() {
  CleanupNativeLibraries(NULL);
}

void SetGLGetProcAddressProc(GLGetProcAddressProc proc) {
  DCHECK(proc);
  g_get_proc_address = proc;
}

GLFunctionPointerType GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

  if (g_libraries) {
    for (size_t i = 0; i < g_libraries->size(); ++i) {
      GLFunctionPointerType proc = reinterpret_cast<GLFunctionPointerType>(
          base::GetFunctionPointerFromNativeLibrary((*g_libraries)[i], name));
      if (proc)
        return proc;
    }
  }
  if (g_get_proc_address) {
    GLFunctionPointerType proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  return NULL;
}

void InitializeNullDrawGLBindings() {
  SetNullDrawGLBindingsEnabled(true);
}

bool HasInitializedNullDrawGLBindings() {
  return GetNullDrawBindingsEnabled();
}

std::string FilterGLExtensionList(
    const char* extensions,
    const std::vector<std::string>& disabled_extensions) {
  if (extensions == NULL)
    return "";

  std::vector<base::StringPiece> extension_vec = base::SplitStringPiece(
      extensions, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  auto is_disabled = [&disabled_extensions](const base::StringPiece& ext) {
    return base::ContainsValue(disabled_extensions, ext);
  };
  extension_vec.erase(
      std::remove_if(extension_vec.begin(), extension_vec.end(), is_disabled),
      extension_vec.end());

  return base::JoinString(extension_vec, " ");
}

DisableNullDrawGLBindings::DisableNullDrawGLBindings() {
  initial_enabled_ = SetNullDrawGLBindingsEnabled(false);
}

DisableNullDrawGLBindings::~DisableNullDrawGLBindings() {
  SetNullDrawGLBindingsEnabled(initial_enabled_);
}

GLWindowSystemBindingInfo::GLWindowSystemBindingInfo()
    : direct_rendering(true) {}

std::string GetGLExtensionsFromCurrentContext() {
  return GetGLExtensionsFromCurrentContext(g_current_gl_context);
}

std::string GetGLExtensionsFromCurrentContext(GLApi* api) {
  if (WillUseGLGetStringForExtensions(api)) {
    const char* extensions =
        reinterpret_cast<const char*>(api->glGetStringFn(GL_EXTENSIONS));
    return extensions ? std::string(extensions) : std::string();
  }

  GLint num_extensions = 0;
  api->glGetIntegervFn(GL_NUM_EXTENSIONS, &num_extensions);

  std::vector<base::StringPiece> exts(num_extensions);
  for (GLint i = 0; i < num_extensions; ++i) {
    const char* extension =
        reinterpret_cast<const char*>(api->glGetStringiFn(GL_EXTENSIONS, i));
    DCHECK(extension != NULL);
    exts[i] = extension;
  }
  return base::JoinString(exts, " ");
}

bool WillUseGLGetStringForExtensions() {
  return WillUseGLGetStringForExtensions(g_current_gl_context);
}

bool WillUseGLGetStringForExtensions(GLApi* api) {
  const char* version_str =
      reinterpret_cast<const char*>(api->glGetStringFn(GL_VERSION));
  unsigned major_version, minor_version;
  bool is_es, is_es2, is_es3;
  GLVersionInfo::ParseVersionString(version_str, &major_version, &minor_version,
                                    &is_es, &is_es2, &is_es3);
  return is_es || major_version < 3;
}

std::unique_ptr<GLVersionInfo> GetVersionInfoFromContext(GLApi* api) {
  std::string extensions = GetGLExtensionsFromCurrentContext(api);
  return base::MakeUnique<GLVersionInfo>(
      reinterpret_cast<const char*>(api->glGetStringFn(GL_VERSION)),
      reinterpret_cast<const char*>(api->glGetStringFn(GL_RENDERER)),
      extensions.c_str());
}

base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath::CharType* filename) {
  return LoadLibraryAndPrintError(base::FilePath(filename));
}

base::NativeLibrary LoadLibraryAndPrintError(const base::FilePath& filename) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library = base::LoadNativeLibrary(filename, &error);
  if (!library) {
    LOG(ERROR) << "Failed to load " << filename.MaybeAsASCII() << ": "
               << error.ToString();
    return NULL;
  }
  return library;
}

}  // namespace gl
