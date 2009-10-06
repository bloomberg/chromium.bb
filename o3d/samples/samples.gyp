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
      'target_name': 'samples',
      'type': 'none',
      'dependencies': [
        'install_samples',
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
