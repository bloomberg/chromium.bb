# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'adbf0d3106b26daa237b10b9bf72b1af7c31092d',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'a97bbc24c5bae15d3d2eaa2865c122e8d3be1b67',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '36c0c1596225e728bd49abb7ef56a3953e7ed468',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'fad68a75519fe8c2dddbb9b3b845fd1bb0e07d4c',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'ec9b96aae53e152f6812e66fa43757c1256369a0',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': 'c3601d4d914212a22e0ebed5329847b9ce89596f',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'ef9db7a8ec52f6c56158d83f5d57ef388c1abec1',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '846e0bb9e45f3eb4ebda1679e9b79cc13fcd0192',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
  },

  'spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@{spirv_cross_revision}',
  },

  'spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
  },

  'spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
  },

  'vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  'vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  'vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  'vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },
}
