# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'memory_watcher',
      'type': 'shared_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'BUILD_MEMORY_WATCHER',
      ],
      'include_dirs': [
        '../..',
        '<(DEPTH)/third_party/wtl/include',
      ],
      # 4748 "/GS can not protect parameters and local variables from local
      # buffer overrun because optimizations are disabled in function".
      # 4740 "flow in or out of inline asm code suppresses global optimization"
      # (result of __asm call x, __asm x:).
      # Nothing to be done about these warnings.
      'msvs_disabled_warnings': [ 4748, 4740 ],
      'sources': [
        'call_stack.cc',
        'call_stack.h',
        'dllmain.cc',
        'hotkey.h',
        'ia32_modrm_map.cc',
        'ia32_opcode_map.cc',
        'memory_hook.cc',
        'memory_hook.h',
        'memory_watcher.cc',
        'memory_watcher.h',
        'mini_disassembler.cc',
        'preamble_patcher.cc',
        'preamble_patcher.h',
        'preamble_patcher_with_stub.cc',
      ],
    },
  ],
}
