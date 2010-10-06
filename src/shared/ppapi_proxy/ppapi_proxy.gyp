{
  'variables': {
    'SRPCGEN': '<(DEPTH)/native_client/tools/srpcgen.py',
    'GENDIR': '<(INTERMEDIATE_DIR)/gen/native_client/src/shared/ppapi_proxy',
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
      'ppp_specs': [
        'objectstub.srpc',
        'ppp.srpc',
        'ppp_instance.srpc',
      ],
      'ppp_rpc_outputs': [
        '<(GENDIR)/ppp_rpc.h',
        '<(GENDIR)/ppp_rpc_client.cc',
      ],
      'ppb_specs': [
        'objectstub.srpc',
        'ppb_core.srpc',
      ],
      'ppb_rpc_outputs': [
        '<(GENDIR)/ppb_rpc.h',
        '<(GENDIR)/ppb_rpc_server.cc',
      ],
      'ppb_upcall_specs': [
        'upcall.srpc',
      ],
      'ppb_upcall_outputs': [
        '<(GENDIR)/upcall.h',
        '<(GENDIR)/upcall_server.cc',
      ],
    },
    'target_conditions': [
      ['target_base=="nacl_ppapi_browser_base"', {
        'sources': [
          'utility.cc',

          'browser_core.cc',
          'browser_globals.cc',
          'browser_instance.cc',
          'browser_ppp.cc',
          'browser_upcall.cc',
          'object.cc',
          'object_proxy.cc',
          'object_serialize.cc',
          'objectstub_rpc_impl.cc',

          '<@(ppp_rpc_outputs)',
          '<@(ppb_rpc_outputs)',
          '<@(ppb_upcall_outputs)',
        ],
      }],
      ['target_base=="nacl_ppapi_plugin_base"', {
        'sources': [
          'utility.cc',

          'plugin_instance.cc',
          'plugin_var.cc',
        ],
      }],
    ],
    'actions': [
      {
        'action_name': 'ppp_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(ppp_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-c',
           'PppRpcs',
           'GEN_PPAPI_PROXY_PPP_RPC_H_',
           '<@(_outputs)',
           '<@(ppp_specs)'],
        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<@(ppp_rpc_outputs)',
        ],
        'message': 'Creating ppp_rpc.h and ppp_rpc_client.cc',
      },
      {
        'action_name': 'ppb_rpc_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(ppb_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-s',
           'PpbRpcs',
           'GEN_PPAPI_PROXY_PPB_RPC_H_',
           '<@(_outputs)',
           '<@(ppb_specs)'],
        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<@(ppb_rpc_outputs)',
        ],
        'message': 'Creating ppb_rpc.h and ppb_rpc_server.cc',
      },
      {
        'action_name': 'ppb_upcall_header',
        'inputs': [
          '<(SRPCGEN)',
          '<@(ppb_upcall_specs)',
        ],
        'action':
          ['<@(python_exe)', '<(SRPCGEN)',
           '-s',
           'PpbUpcalls',
           'GEN_PPAPI_PROXY_UPCALL_H_',
           '<@(_outputs)',
           '<@(ppb_upcall_specs)'],
        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,
        'outputs': [
          '<@(ppb_upcall_outputs)',
        ],
        'message': 'Creating upcall.h and upcall_server.cc',
      },
    ],
  },
  'conditions': [
    ['OS!="win"', {
      'targets': [
        {
          'target_name': 'nacl_ppapi_browser',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_ppapi_browser_base',
          },
          # Sources get set by inheritance from base
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
        },
        {
          'target_name': 'nacl_ppapi_plugin',
          'type': 'static_library',
          'variables': {
            'target_base': 'nacl_ppapi_plugin_base',
          },
          # Sources get set by inheritance from base
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
        },
      ],
    }],
#    ['OS=="win"', {
#      'targets': [
#        {
#          'target_name': 'nacl_ppapi_browser64',
#          'type': 'static_library',
#          'variables': {
#            'target_base': 'nacl_ppapi_browser_base',
#          },
#          # Sources get set by inheritance from base
#          'include_dirs': [
#            '<(INTERMEDIATE_DIR)',
#          ],
#          'configurations': {
#            'Common_Base': {
#              'msvs_target_platform': 'x64',
#            },
#          },
#        },
#        {
#          'target_name': 'nacl_ppapi_plugin64',
#          'type': 'static_library',
#          'variables': {
#            'target_base': 'nacl_ppapi_plugin_base',
#          },
#          # Sources get set by inheritance from base
#          'include_dirs': [
#            '<(INTERMEDIATE_DIR)',
#          ],
#          'configurations': {
#            'Common_Base': {
#              'msvs_target_platform': 'x64',
#            },
#          },
#        },
#      ],
#    }],
  ],
}
