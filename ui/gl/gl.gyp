# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'gl',
      'type': '<(component)',
      'product_name': 'gl_wrapper',  # Avoid colliding with OS X's libGL.dylib
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/ui.gyp:ui',
      ],
      'variables': {
        'gl_binding_output_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/gl',
      },
      'defines': [
        'GL_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/swiftshader/include',
        '<(DEPTH)/third_party/mesa/MesaLib/include',
        '<(gl_binding_output_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/mesa/MesaLib/include',
          '<(gl_binding_output_dir)',
        ],
      },
     'sources': [
        'gl_bindings.h',
        'gl_bindings_skia_in_process.cc',
        'gl_bindings_skia_in_process.h',
        'gl_context.cc',
        'gl_context.h',
        'gl_context_android.cc',
        'gl_context_linux.cc',
        'gl_context_mac.mm',
        'gl_context_osmesa.cc',
        'gl_context_osmesa.h',
        'gl_context_stub.cc',
        'gl_context_stub.h',
        'gl_context_win.cc',
        'gl_export.h',
        'gl_fence.cc',
        'gl_fence.h',
        'gl_implementation.cc',
        'gl_implementation.h',
        'gl_implementation_android.cc',
        'gl_implementation_linux.cc',
        'gl_implementation_mac.cc',
        'gl_implementation_win.cc',
        'gl_interface.cc',
        'gl_interface.h',
        'gl_share_group.cc',
        'gl_share_group.h',
        'gl_surface.cc',
        'gl_surface.h',
        'gl_surface_android.cc',
        'gl_surface_android.h',
        'gl_surface_linux.cc',
        'gl_surface_mac.cc',
        'gl_surface_stub.cc',
        'gl_surface_stub.h',
        'gl_surface_win.cc',
        'gl_surface_osmesa.cc',
        'gl_surface_osmesa.h',
        'gl_switches.cc',
        'gl_switches.h',
        'scoped_make_current.cc',
        'scoped_make_current.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
        '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
        '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
      ],
      # hard_dependency is necessary for this target because it has actions
      # that generate header files included by dependent targets. The header
      # files must be generated before the dependents are compiled. The usual
      # semantics are to allow the two targets to build concurrently.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate_gl_bindings',
          'inputs': [
            'generate_bindings.py',
            '<(DEPTH)/third_party/khronos/GLES2/gl2ext.h',
            '<(DEPTH)/third_party/khronos/EGL/eglext.h',
            '<(DEPTH)/third_party/mesa/MesaLib/include/GL/glext.h',
            '<(DEPTH)/third_party/mesa/MesaLib/include/GL/glxext.h',
            '<(DEPTH)/third_party/mesa/MesaLib/include/GL/wglext.h',
          ],
          'outputs': [
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_gl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_mock.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
          ],
          'action': [
            'python',
            'generate_bindings.py',
            '<(gl_binding_output_dir)',
          ],
        },
      ],
      'conditions': [
        ['OS != "mac"', {
          'sources': [
            'egl_util.cc',
            'egl_util.h',
            'gl_context_egl.cc',
            'gl_context_egl.h',
            'gl_surface_egl.cc',
            'gl_surface_egl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_egl.h',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/angle/include',
          ],
        }],
        ['use_x11 == 1', {
          'sources': [
            'gl_context_glx.cc',
            'gl_context_glx.h',
            'gl_surface_glx.cc',
            'gl_surface_glx.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_glx.h',
          ],
          'all_dependent_settings': {
            'defines': [
              'GL_GLEXT_PROTOTYPES',
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'gl_context_wgl.cc',
            'gl_context_wgl.h',
            'gl_surface_wgl.cc',
            'gl_surface_wgl.h',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_wgl.h',
          ],
          'include_dirs': [
            '$(DXSDK_DIR)/include',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'gl_context_cgl.cc',
            'gl_context_cgl.h',
            'gl_surface_cgl.cc',
            'gl_surface_cgl.h',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="mac" and use_aura == 1', {
          'sources': [
            'gl_context_nsview.mm',
            'gl_context_nsview.h',
            'gl_surface_nsview.mm',
            'gl_surface_nsview.h',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            'android_native_window.cc',
            'android_native_window.h',
          ],
          'sources!': [
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.cc',
            '<(gl_binding_output_dir)/gl_bindings_autogen_osmesa.h',
            'system_monitor_posix.cc',
          ],
          'defines': [
            'GL_GLEXT_PROTOTYPES',
            'EGL_EGLEXT_PROTOTYPES',
          ],
        }],
      ],
    },
  ],
}
