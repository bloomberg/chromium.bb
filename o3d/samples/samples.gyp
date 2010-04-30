# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'install_samples',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/samples',
          'files': [
            '2d.html',
            'animated-scene.html',
            'animation.html',
            'bitmap-draw-image.html',
            'billboards.html',
            'canvas-texturedraw.html',
            'canvas.html',
            'convolution.html',
            'culling.html',
            'customcamera.html',
            'displayfps.html',
            'error-texture.html',
            'generate-texture.html',
            'hellocube-colors.html',
            'hellocube-textures.html',
            'hellocube.html',
            'helloworld.html',
            'hud-2d-overlay.html',
            'instance-override.html',
            'instancing.html',
            'juggler.html',
            'julia.html',
            'multiple-clients.html',
            'multiple-views.html',
            'old-school-shadows.html',
            'particles.html',
            'phongshading.html',
            'picking.html',
            'primitives.html',
            'procedural-texture.html',
            'render-mode.html',
            'render-targets.html',
            'rotatemodel.html',
            'scatter-chart.html',
            'shader-test.html',
            'simple.html',
            'simpletexture.html',
            'skinning.html',
            'sobel.html',
            'stencil_example.html',
            'texturesamplers.html',
            'tutorial-primitive.html',
            'vertex-shader.html',
            'vertex-shader-animation.html',
            'zsorting.html'
          ]
        },
      ],
    },
    {
      # TODO(petersont): tie in the copying of this to the doc
      # generation process, compile the sources, etc.
      'target_name': 'install_webgl_js',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/samples/o3djs',
          'files': [
            'o3djs/webgl.js',
          ]
        },
      ],
    },
    {
      # TODO(petersont): tie in the copying of these to the doc
      # generation process, compile the sources, etc.
      'target_name': 'install_o3d_webgl',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/samples',
          'files': [
            'o3d-webgl/',
          ]
        },
      ],
    },
    {
      # TODO(petersont): consider picking and choosing the files taken
      # from this directory. Note that some of the samples are copied
      # via the list above, and some are copied by virtue of being in
      # the MANIFEST file in this directory (see samples_gen.py).
      'target_name': 'install_o3d_webgl_samples',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/samples',
          'files': [
            'o3d-webgl-samples/',
          ]
        },
      ],
    },
    {
      'target_name': 'samples',
      'type': 'none',
      'dependencies': [
        'install_samples',
        'install_webgl_js',
        'install_o3d_webgl',
        'install_o3d_webgl_samples',
        '<!(python samples_gen.py):build_samples',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
