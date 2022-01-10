// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include <stddef.h>

#include <sstream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

ANGLEImplementation MakeANGLEImplementation(
    const GLImplementation gl_impl,
    const ANGLEImplementation angle_impl) {
  if (gl_impl == kGLImplementationEGLANGLE) {
    if (angle_impl == ANGLEImplementation::kNone) {
      return ANGLEImplementation::kDefault;
    } else {
      return angle_impl;
    }
  } else {
    return ANGLEImplementation::kNone;
  }
}

GLImplementationParts::GLImplementationParts(
    const ANGLEImplementation angle_impl)
    : gl(kGLImplementationEGLANGLE),
      angle(MakeANGLEImplementation(kGLImplementationEGLANGLE, angle_impl)) {}

GLImplementationParts::GLImplementationParts(const GLImplementation gl_impl)
    : gl(gl_impl),
      angle(MakeANGLEImplementation(gl_impl, ANGLEImplementation::kDefault)) {}

bool GLImplementationParts::IsValid() const {
  if (angle == ANGLEImplementation::kNone) {
    return (gl != kGLImplementationEGLANGLE);
  } else {
    return (gl == kGLImplementationEGLANGLE);
  }
}

bool GLImplementationParts::IsAllowed(
    const std::vector<GLImplementationParts>& allowed_impls) const {
  // Given a vector of GLImplementationParts, this function checks if "this"
  // GLImplementation is found in the list, with a special case where if the
  // list contains ANGLE/kDefault, "this" may be any ANGLE implementation.
  for (const GLImplementationParts& impl_iter : allowed_impls) {
    if (gl == kGLImplementationEGLANGLE &&
        impl_iter.gl == kGLImplementationEGLANGLE) {
      if (impl_iter.angle == ANGLEImplementation::kDefault) {
        return true;
      } else if (angle == impl_iter.angle) {
        return true;
      }
    } else if (gl == impl_iter.gl) {
      return true;
    }
  }
  return false;
}

std::string GLImplementationParts::ToString() const {
  std::stringstream s;
  s << "(gl=";
  switch (gl) {
    case GLImplementation::kGLImplementationNone:
      s << "none";
      break;
    case GLImplementation::kGLImplementationDesktopGL:
      s << "desktop-gl";
      break;
    case GLImplementation::kGLImplementationDesktopGLCoreProfile:
      s << "desktop-gl-core-profile";
      break;
    case GLImplementation::kGLImplementationSwiftShaderGL:
      s << "swiftshader-gl";
      break;
    case GLImplementation::kGLImplementationAppleGL:
      s << "apple-gl";
      break;
    case GLImplementation::kGLImplementationEGLGLES2:
      s << "egl-gles2";
      break;
    case GLImplementation::kGLImplementationMockGL:
      s << "mock-gl";
      break;
    case GLImplementation::kGLImplementationStubGL:
      s << "stub-gl";
      break;
    case GLImplementation::kGLImplementationDisabled:
      s << "disabled";
      break;
    case GLImplementation::kGLImplementationEGLANGLE:
      s << "egl-angle";
      break;
  }
  s << ",angle=";
  switch (angle) {
    case ANGLEImplementation::kNone:
      s << "none";
      break;
    case ANGLEImplementation::kD3D9:
      s << "d3d9";
      break;
    case ANGLEImplementation::kD3D11:
      s << "d3d11";
      break;
    case ANGLEImplementation::kOpenGL:
      s << "opengl";
      break;
    case ANGLEImplementation::kOpenGLES:
      s << "opengles";
      break;
    case ANGLEImplementation::kNull:
      s << "null";
      break;
    case ANGLEImplementation::kVulkan:
      s << "vulkan";
      break;
    case ANGLEImplementation::kSwiftShader:
      s << "swiftshader";
      break;
    case ANGLEImplementation::kMetal:
      s << "metal";
      break;
    case ANGLEImplementation::kDefault:
      s << "default";
      break;
  }
  s << ")";
  return s.str();
}

namespace {

const struct {
  const char* gl_name;
  const char* angle_name;
  GLImplementationParts implementation;
} kGLImplementationNamePairs[] = {
    {kGLImplementationDesktopName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationDesktopGL)},
    {kGLImplementationSwiftShaderName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationSwiftShaderGL)},
#if defined(OS_APPLE)
    {kGLImplementationAppleName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationAppleGL)},
#endif
    {kGLImplementationEGLName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationEGLGLES2)},
    {kGLImplementationANGLEName, kANGLEImplementationNoneName,
     GLImplementationParts(ANGLEImplementation::kDefault)},
    {kGLImplementationANGLEName, kANGLEImplementationDefaultName,
     GLImplementationParts(ANGLEImplementation::kDefault)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D9Name,
     GLImplementationParts(ANGLEImplementation::kD3D9)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11Name,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11on12Name,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11NULLName,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLEGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLNULLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESEGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESNULLName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationVulkanName,
     GLImplementationParts(ANGLEImplementation::kVulkan)},
    {kGLImplementationANGLEName, kANGLEImplementationVulkanNULLName,
     GLImplementationParts(ANGLEImplementation::kVulkan)},
    {kGLImplementationANGLEName, kANGLEImplementationMetalName,
     GLImplementationParts(ANGLEImplementation::kMetal)},
    {kGLImplementationANGLEName, kANGLEImplementationMetalNULLName,
     GLImplementationParts(ANGLEImplementation::kMetal)},
    {kGLImplementationANGLEName, kANGLEImplementationSwiftShaderName,
     GLImplementationParts(ANGLEImplementation::kSwiftShader)},
    {kGLImplementationANGLEName, kANGLEImplementationSwiftShaderForWebGLName,
     GLImplementationParts(ANGLEImplementation::kSwiftShader)},
    {kGLImplementationANGLEName, kANGLEImplementationNullName,
     GLImplementationParts(ANGLEImplementation::kNull)},
    {kGLImplementationMockName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationMockGL)},
    {kGLImplementationStubName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationStubGL)},
    {kGLImplementationDisabledName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationDisabled)}};

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementationParts g_gl_implementation =
    GLImplementationParts(kGLImplementationNone);
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* due_to_fallback) {
  if (g_libraries) {
    // We do not call base::UnloadNativeLibrary() for these libraries as
    // unloading libGL without closing X display is not allowed. See
    // https://crbug.com/250813 for details.
    // However, if we fallback to a software renderer (e.g., SwiftShader),
    // then the above concern becomes irrelevant.
    // During fallback from ANGLE to SwiftShader ANGLE library needs to
    // be unloaded, otherwise software SwiftShader loading will fail. See
    // https://crbug.com/760063 for details.
    // During fallback from VMware mesa to SwiftShader mesa libraries need
    // to be unloaded. See https://crbug.com/852537 for details.
    if (due_to_fallback && *static_cast<bool*>(due_to_fallback)) {
      for (auto* library : *g_libraries)
        base::UnloadNativeLibrary(library);
    }
    delete g_libraries;
    g_libraries = nullptr;
  }
}

gfx::ExtensionSet GetGLExtensionsFromCurrentContext(
    GLApi* api,
    GLenum extensions_enum,
    GLenum num_extensions_enum) {
  if (WillUseGLGetStringForExtensions(api)) {
    const char* extensions =
        reinterpret_cast<const char*>(api->glGetStringFn(extensions_enum));
    return extensions ? gfx::MakeExtensionSet(extensions) : gfx::ExtensionSet();
  }

  GLint num_extensions = 0;
  api->glGetIntegervFn(num_extensions_enum, &num_extensions);

  std::vector<base::StringPiece> exts(num_extensions);
  for (GLint i = 0; i < num_extensions; ++i) {
    const char* extension =
        reinterpret_cast<const char*>(api->glGetStringiFn(extensions_enum, i));
    DCHECK(extension != NULL);
    exts[i] = extension;
  }
  return gfx::ExtensionSet(exts);
}

}  // namespace

base::ThreadLocalPointer<CurrentGL>* g_current_gl_context_tls = NULL;

#if defined(USE_EGL)
EGLApi* g_current_egl_context;
#endif

#if defined(USE_GLX)
GLXApi* g_current_glx_context;
#endif

GLImplementationParts GetNamedGLImplementation(const std::string& gl_name,
                                               const std::string& angle_name) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (gl_name == name_pair.gl_name && angle_name == name_pair.angle_name)
      return name_pair.implementation;
  }

  return GLImplementationParts(kGLImplementationNone);
}

GLImplementationParts GetLegacySoftwareGLImplementation() {
  return GLImplementationParts(kGLImplementationSwiftShaderGL);
}

GLImplementationParts GetSoftwareGLImplementation() {
  return GLImplementationParts(ANGLEImplementation::kSwiftShader);
}

bool IsSoftwareGLImplementation(GLImplementationParts implementation) {
  return (implementation == GetLegacySoftwareGLImplementation()) ||
         (implementation == GetSoftwareGLImplementation());
}

void SetSoftwareGLCommandLineSwitches(base::CommandLine* command_line,
                                      bool legacy_software_gl) {
  if (legacy_software_gl) {
    command_line->AppendSwitchASCII(
        switches::kUseGL,
        gl::GetGLImplementationGLName(gl::GetLegacySoftwareGLImplementation()));
  } else {
    GLImplementationParts implementation = GetSoftwareGLImplementation();
    command_line->AppendSwitchASCII(
        switches::kUseGL, gl::GetGLImplementationGLName(implementation));
    command_line->AppendSwitchASCII(
        switches::kUseANGLE, gl::GetGLImplementationANGLEName(implementation));
  }
}

void SetSoftwareWebGLCommandLineSwitches(base::CommandLine* command_line,
                                         bool legacy_software_gl) {
  if (legacy_software_gl) {
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    kGLImplementationSwiftShaderForWebGLName);
  } else {
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    kGLImplementationANGLEName);
    command_line->AppendSwitchASCII(
        switches::kUseANGLE, kANGLEImplementationSwiftShaderForWebGLName);
  }
}

const char* GetGLImplementationGLName(GLImplementationParts implementation) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (implementation.gl == name_pair.implementation.gl &&
        implementation.angle == name_pair.implementation.angle)
      return name_pair.gl_name;
  }

  return "unknown";
}

const char* GetGLImplementationANGLEName(GLImplementationParts implementation) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (implementation.gl == name_pair.implementation.gl &&
        implementation.angle == name_pair.implementation.angle)
      return name_pair.angle_name;
  }

  return "not defined";
}

void SetGLImplementationParts(const GLImplementationParts& implementation) {
  DCHECK(implementation.IsValid());
  g_gl_implementation = GLImplementationParts(implementation);
}

const GLImplementationParts& GetGLImplementationParts() {
  return g_gl_implementation;
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = GLImplementationParts(implementation);
  DCHECK(g_gl_implementation.IsValid());
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation.gl;
}

void SetANGLEImplementation(ANGLEImplementation implementation) {
  g_gl_implementation = GLImplementationParts(implementation);
  DCHECK(g_gl_implementation.IsValid());
}

ANGLEImplementation GetANGLEImplementation() {
  return g_gl_implementation.angle;
}

bool HasDesktopGLFeatures() {
  return kGLImplementationDesktopGL == g_gl_implementation.gl ||
         kGLImplementationDesktopGLCoreProfile == g_gl_implementation.gl ||
         kGLImplementationAppleGL == g_gl_implementation.gl;
}

void AddGLNativeLibrary(base::NativeLibrary library) {
  DCHECK(library);

  if (!g_libraries) {
    g_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(CleanupNativeLibraries, NULL);
  }

  g_libraries->push_back(library);
}

void UnloadGLNativeLibraries(bool due_to_fallback) {
  CleanupNativeLibraries(&due_to_fallback);
}

void SetGLGetProcAddressProc(GLGetProcAddressProc proc) {
  DCHECK(proc);
  g_get_proc_address = proc;
}

NO_SANITIZE("cfi-icall")
GLFunctionPointerType GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation.gl != kGLImplementationNone);

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
    return base::Contains(disabled_extensions, ext);
  };
  base::EraseIf(extension_vec, is_disabled);

  return base::JoinString(extension_vec, " ");
}

DisableNullDrawGLBindings::DisableNullDrawGLBindings() {
  initial_enabled_ = SetNullDrawGLBindingsEnabled(false);
}

DisableNullDrawGLBindings::~DisableNullDrawGLBindings() {
  SetNullDrawGLBindingsEnabled(initial_enabled_);
}

GLWindowSystemBindingInfo::GLWindowSystemBindingInfo() {}
GLWindowSystemBindingInfo::~GLWindowSystemBindingInfo() {}

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

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext() {
  return GetRequestableGLExtensionsFromCurrentContext(g_current_gl_context);
}

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext(GLApi* api) {
  return GetGLExtensionsFromCurrentContext(api, GL_REQUESTABLE_EXTENSIONS_ANGLE,
                                           GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE);
}

bool WillUseGLGetStringForExtensions() {
  return WillUseGLGetStringForExtensions(g_current_gl_context);
}

bool WillUseGLGetStringForExtensions(GLApi* api) {
  const char* version_str =
      reinterpret_cast<const char*>(api->glGetStringFn(GL_VERSION));
  const char* renderer_str =
      reinterpret_cast<const char*>(api->glGetStringFn(GL_RENDERER));
  gfx::ExtensionSet extensions;
  GLVersionInfo version_info(version_str, renderer_str, extensions);
  return version_info.is_es || version_info.major_version < 3;
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

#if BUILDFLAG(USE_OPENGL_APITRACE)
void TerminateFrame() {
  GetGLProcAddress("glFrameTerminatorGREMEDY")();
}
#endif

}  // namespace gl
