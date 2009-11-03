# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # We only want the documentation targets to be defined if the JS
    # Compiler is available, so we use python to find out if it's
    # available.
    'jscomp_exists': '<!(python ../build/file_exists.py '
                     '../../o3d-internal/jscomp/JSCompiler_deploy.jar)',
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
  ],
  'conditions': [
    [ '"<(jscomp_exists)"=="True"',
      {
        # Define these here so we don't run the scripts unless we need to.
        'variables': {
          'input_js_files': [
            '<!@(python get_docs_files.py --js)',
          ],
          'input_idl_files': [
            '<!@(python get_docs_files.py --idl)',
          ],
        },
        'targets': [
          {
            'target_name': 'documentation',
            'type': 'none',
            'actions': [
              {
                'action_name': 'build_docs',
                'inputs': [
                  '<@(input_js_files)',
                  '<@(input_idl_files)',
                  'jsdoc-toolkit-templates/annotated.tmpl',
                  'jsdoc-toolkit-templates/class.tmpl',
                  'jsdoc-toolkit-templates/classtree.tmpl',
                  'jsdoc-toolkit-templates/dot.tmpl',
                  'jsdoc-toolkit-templates/filelist.tmpl',
                  'jsdoc-toolkit-templates/members.tmpl',
                  'jsdoc-toolkit-templates/namespaces.tmpl',
                  'jsdoc-toolkit-templates/publish.js',
                  'jsdoc-toolkit-templates/static/header.html',
                  'jsdoc-toolkit-templates/static/footer.html',
                  'jsdoc-toolkit-templates/static/stylesheet.css',
                  'jsdoc-toolkit-templates/static/tabs.css',
                  'jsdoc-toolkit-templates/static/tab_l.gif',
                  'jsdoc-toolkit-templates/static/tab_r.gif',
                  'jsdoc-toolkit-templates/static/tab_b.gif',
                  'externs/externs.js',
                  'externs/o3d-extra-externs.js',
                  'build_docs.py'
                ],
                'outputs': [
                  # There are really a whole lot more outputs than
                  # this, but to determine what they are would require
                  # having the entire docs script run every time, so
                  # we just depend on the ultimate compiled base.js
                  # file, which is rebuilt every time the docs build
                  # happens.
                  '<(PRODUCT_DIR)/docs/documentation/base.js',
                ],
                'action': [
                  'python',
                  'build_docs.py',
                  'java',
                  '../../third_party',
                  '<(PRODUCT_DIR)/docs',
                ],
              },
            ],
            'copies': [
              {
                'destination': '<(PRODUCT_DIR)/samples/o3djs',
                'files': [
                  '<(PRODUCT_DIR)/docs/documentation/base.js',
                ],
              },
            ],
          },
        ],
      },
      {
        'targets': [
          {
            # Empty target if the js compiler doesn't exist.
            'target_name': 'documentation',
            'type': 'none',
          },
        ],
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
