# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
    'jinja_module_files': [
      # jinja2/__init__.py contains version string, so sufficient for package
      '<(DEPTH)/third_party/jinja2/__init__.py',
      '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
    ],
  },
  'targets': [
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_injected_script
      'target_name': 'inspector_injected_script',
      'type': 'none',
      'actions': [
        {
          'action_name': 'ConvertFileToHeaderWithCharacterArray',
          'inputs': [
            'build/xxd.py',
            'InjectedScriptSource.js',
          ],
          'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/blink/platform/v8_inspector/InjectedScriptSource.h', ],
          'action': [
            'python', 'build/xxd.py', 'InjectedScriptSource_js', 'InjectedScriptSource.js', '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_debugger_script
      'target_name': 'inspector_debugger_script',
      'type': 'none',
      'actions': [
        {
          'action_name': 'ConvertFileToHeaderWithCharacterArray',
          'inputs': [
            'build/xxd.py',
            'DebuggerScript.js',
          ],
          'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/blink/platform/v8_inspector/DebuggerScript.h', ],
          'action': [
            'python', 'build/xxd.py', 'DebuggerScript_js', 'DebuggerScript.js', '<@(_outputs)'
          ],
        },
      ],
      # Since this target generates header files, it needs to be a hard dependency.
      'hard_dependency': 1,
    },
    {
      # GN version: //third_party/WebKit/Source/platform:inspector_protocol_sources
      'target_name': 'protocol_sources',
      'type': 'none',
      'dependencies': ['protocol_version'],
      'actions': [
        {
          'action_name': 'generateV8InspectorProtocolBackendSources',
          'inputs': [
            '<@(jinja_module_files)',
            # The python script in action below.
            '../inspector_protocol/CodeGenerator.py',
            # Source code templates.
            '../inspector_protocol/TypeBuilder_h.template',
            '../inspector_protocol/TypeBuilder_cpp.template',
            # Protocol definitions
            'js_protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',
          ],
          'action': [
            'python',
            '../inspector_protocol/CodeGenerator.py',
            '--protocol', 'js_protocol.json',
            '--string_type', 'String16',
            '--export_macro', 'PLATFORM_EXPORT',
            '--output_dir', '<(blink_platform_output_dir)/v8_inspector/protocol',
            '--output_package', 'platform/v8_inspector/protocol',
          ],
          'message': 'Generating protocol backend sources from json definitions.',
        },
      ]
    },
    {
      # GN version: //third_party/WebKit/Source/core/inspector:protocol_version
      'target_name': 'protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateV8InspectorProtocolVersion',
          'inputs': [
            '../inspector_protocol/generate-inspector-protocol-version',
            'js_protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/protocol.json',
          ],
          'action': [
            'python',
            '../inspector_protocol/generate-inspector-protocol-version',
            '--o',
            '<@(_outputs)',
            'js_protocol.json',
          ],
          'message': 'Validate v8_inspector protocol for backwards compatibility and generate version file',
        },
      ]
    },
  ],  # targets
}
