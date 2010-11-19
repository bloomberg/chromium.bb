# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'SRPCDIR' : '<(DEPTH)/srpc/trusted',
    'SRPC_OUT': [
      '<(SRPCDIR)/srpcgen/ppp_rpc_client.cc',
      '<(SRPCDIR)/srpcgen/ppb_rpc_server.cc',
      '<(SRPCDIR)/srpcgen/upcall_server.cc',
    ],
  },
  'targets': [
    {
      'target_name': 'nacl_ppapi_browser',
      'type': 'static_library',
      'sources': [
        'utility.cc',
        'browser_core.cc',
        'browser_completion_callback_invoker.cc',
        'browser_globals.cc',
        'browser_graphics_2d.cc',
        'browser_image_data.cc',
        'browser_instance.cc',
        'browser_ppp.cc',
        'browser_upcall.cc',
        'object.cc',
        'object_proxy.cc',
        'object_serialize.cc',
        'objectstub_rpc_impl.cc',
        '<@(SRPC_OUT)',
      ],
      'include_dirs': [
        '<(SRPCDIR)',
      ],
    },
    {
      'target_name': 'nacl_ppapi_plugin',
      'type': 'static_library',
      'sources': [
        'utility.cc',
        'plugin_instance.cc',
        'plugin_var.cc',
      ],
      'include_dirs': [
        '<(SRPCDIR)',
      ],
    },
  ],
}
