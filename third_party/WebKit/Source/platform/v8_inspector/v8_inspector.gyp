# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../inspector_protocol/inspector_protocol.gypi',
  ],
  'variables': {
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
  },
  'conditions': [
    ['v8_inspector!="true"',
      {
        'targets': [
          {
            # GN version: //third_party/WebKit/Source/platform:inspector_protocol_sources
            'target_name': 'protocol_sources',
            'type': 'none',
            'dependencies': ['protocol_version'],
            'actions': [
              {
                'action_name': 'generateV8InspectorProtocolBackendSources',
                'inputs': [
                  '<@(inspector_protocol_files)',
                  'js_protocol.json',
                  'inspector_protocol_config.json',
                ],
                'outputs': [
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Forward.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Console.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.cpp',
                  '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.h',
                  '<(blink_platform_output_dir)/v8_inspector/public/protocol/Debugger.h',
                  '<(blink_platform_output_dir)/v8_inspector/public/protocol/Runtime.h',
                  '<(blink_platform_output_dir)/v8_inspector/public/protocol/Schema.h',
                ],
                'action': [
                  'python',
                  '../inspector_protocol/CodeGenerator.py',
                  '--jinja_dir', '../../../',  # jinja is in chromium's third_party
                  '--output_base', '<(blink_platform_output_dir)',
                  '--config', 'inspector_protocol_config.json',
                ],
                'message': 'Generating protocol backend sources from json definitions.',
              },
            ]
          },
        ],
      },
    ],
  ],

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
      # GN version: //third_party/WebKit/Source/core/inspector:protocol_version
      'target_name': 'protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateV8InspectorProtocolVersion',
          'inputs': [
            '../inspector_protocol/CheckProtocolCompatibility.py',
            'js_protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/js_protocol.stamp',
          ],
          'action': [
            'python',
            '../inspector_protocol/CheckProtocolCompatibility.py',
            '--stamp',
            '<@(_outputs)',
            'js_protocol.json',
          ],
          'message': 'Validate v8_inspector protocol for backwards compatibility',
        },
      ]
    },
    {
      'target_name': 'protocol_sources_stl',
      'type': 'none',
      'dependencies': ['protocol_version'],
      'actions': [
        {
          'action_name': 'generateV8InspectorProtocolBackendSourcesSTL',
          'inputs': [
            '<@(inspector_protocol_files)',
            'js_protocol.json',
            'inspector_protocol_config_stl.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/v8_inspector/protocol/Forward.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Console.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.cpp',
            '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.h',
            '<(blink_platform_output_dir)/v8_inspector/public/protocol/Debugger.h',
            '<(blink_platform_output_dir)/v8_inspector/public/protocol/Runtime.h',
            '<(blink_platform_output_dir)/v8_inspector/public/protocol/Schema.h',
          ],
          'action': [
            'python',
            '../inspector_protocol/CodeGenerator.py',
            '--jinja_dir', '../../../',
            '--output_base', '<(blink_platform_output_dir)',
            '--config', 'inspector_protocol_config_stl.json',
          ],
          'message': 'Generating protocol backend sources from json definitions.',
        },
      ]
    },
    {
      'target_name': 'v8_inspector_stl',
      'type': '<(component)',
      'dependencies': [
        ':inspector_injected_script',
        ':inspector_debugger_script',
        ':protocol_sources_stl',
      ],
      'include_dirs': [
        '../..',
        '../../../../../v8/include',
        '../../../../../v8',
        '<(SHARED_INTERMEDIATE_DIR)/blink',
      ],
      'sources': [
        '<(blink_platform_output_dir)/v8_inspector/protocol/Forward.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Protocol.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Console.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.h',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.cpp',
        '<(blink_platform_output_dir)/v8_inspector/protocol/Schema.h',
        '<(blink_platform_output_dir)/v8_inspector/public/protocol/Debugger.h',
        '<(blink_platform_output_dir)/v8_inspector/public/protocol/Runtime.h',
        '<(blink_platform_output_dir)/v8_inspector/public/protocol/Schema.h',

        'Allocator.h',
        'Atomics.h',
        'InjectedScript.cpp',
        'InjectedScript.h',
        'InjectedScriptNative.cpp',
        'InjectedScriptNative.h',
        'InspectedContext.cpp',
        'InspectedContext.h',
        'JavaScriptCallFrame.cpp',
        'JavaScriptCallFrame.h',
        'ProtocolPlatformSTL.h',
        'RemoteObjectId.cpp',
        'RemoteObjectId.h',
        'ScriptBreakpoint.h',
        'SearchUtil.cpp',
        'SearchUtil.h',
        'String16.cpp',
        'String16.h',
        'StringUtil.cpp',
        'StringUtil.h',
        'V8Console.cpp',
        'V8Console.h',
        'V8ConsoleAgentImpl.cpp',
        'V8ConsoleAgentImpl.h',
        'V8ConsoleMessage.cpp',
        'V8ConsoleMessage.h',
        'V8Debugger.cpp',
        'V8Debugger.h',
        'V8DebuggerAgentImpl.cpp',
        'V8DebuggerAgentImpl.h',
        'V8InspectorImpl.cpp',
        'V8InspectorImpl.h',
        'V8DebuggerScript.cpp',
        'V8DebuggerScript.h',
        'V8FunctionCall.cpp',
        'V8FunctionCall.h',
        'V8HeapProfilerAgentImpl.cpp',
        'V8HeapProfilerAgentImpl.h',
        'V8InjectedScriptHost.cpp',
        'V8InjectedScriptHost.h',
        'V8InspectorSessionImpl.cpp',
        'V8InspectorSessionImpl.h',
        'V8InternalValueType.cpp',
        'V8InternalValueType.h',
        'V8ProfilerAgentImpl.cpp',
        'V8ProfilerAgentImpl.h',
        'V8Regex.cpp',
        'V8Regex.h',
        'V8RuntimeAgentImpl.cpp',
        'V8RuntimeAgentImpl.h',
        'V8SchemaAgentImpl.cpp',
        'V8SchemaAgentImpl.h',
        'V8StackTraceImpl.cpp',
        'V8StackTraceImpl.h',
        'V8ValueCopier.cpp',
        'V8ValueCopier.h',
        'public/StringBuffer.h',
        'public/StringView.h',
        'public/V8ContextInfo.h',
        'public/V8Inspector.h',
        'public/V8InspectorClient.h',
        'public/V8InspectorSession.h',
        'public/V8StackTrace.h',

        '<(blink_platform_output_dir)/v8_inspector/DebuggerScript.h',
        '<(blink_platform_output_dir)/v8_inspector/InjectedScriptSource.h',
      ],
    },
  ],  # targets
}
