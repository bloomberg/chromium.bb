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
    # TODO(kbr): change these back to using the directory copying
    # syntax ("o3d-webgl/"), and roll forward gyp in DEPS, once the
    # bug in the MSVS gyp generator causing it to crash is fixed.
    {
      # TODO(petersont): tie in the copying of these to the doc
      # generation process, compile the sources, etc.
      'target_name': 'install_o3d_webgl',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/samples/o3d-webgl',
          'files': [
            'o3d-webgl/archive_request.js',
            'o3d-webgl/base.js',
            'o3d-webgl/bitmap.js',
            'o3d-webgl/bounding_box.js',
            'o3d-webgl/buffer.js',
            'o3d-webgl/clear_buffer.js',
            'o3d-webgl/client.js',
            'o3d-webgl/draw_context.js',
            'o3d-webgl/draw_element.js',
            'o3d-webgl/draw_list.js',
            'o3d-webgl/draw_pass.js',
            'o3d-webgl/effect.js',
            'o3d-webgl/element.js',
            'o3d-webgl/event.js',
            'o3d-webgl/field.js',
            'o3d-webgl/file_request.js',
            'o3d-webgl/material.js',
            'o3d-webgl/named_object.js',
            'o3d-webgl/named_object_base.js',
            'o3d-webgl/object_base.js',
            'o3d-webgl/pack.js',
            'o3d-webgl/param.js',
            'o3d-webgl/param_object.js',
            'o3d-webgl/primitive.js',
            'o3d-webgl/raw_data.js',
            'o3d-webgl/ray_intersection_info.js',
            'o3d-webgl/render_node.js',
            'o3d-webgl/render_surface.js',
            'o3d-webgl/render_surface_set.js',
            'o3d-webgl/sampler.js',
            'o3d-webgl/shape.js',
            'o3d-webgl/state.js',
            'o3d-webgl/state_set.js',
            'o3d-webgl/stream.js',
            'o3d-webgl/stream_bank.js',
            'o3d-webgl/texture.js',
            'o3d-webgl/transform.js',
            'o3d-webgl/tree_traversal.js',
            'o3d-webgl/types.js',
            'o3d-webgl/viewport.js',
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
          'destination': '<(PRODUCT_DIR)/samples/o3d-webgl-samples',
          'files': [
            'o3d-webgl-samples/culling.html',
            'o3d-webgl-samples/hellocube-colors.html',
            'o3d-webgl-samples/hellocube-textures.html',
            'o3d-webgl-samples/hellocube.html',
            'o3d-webgl-samples/helloworld.html',
            'o3d-webgl-samples/pool.html',
            'o3d-webgl-samples/primitives.html',
            'o3d-webgl-samples/shadow-map.html',
          ]
        },
        {
          'destination': '<(PRODUCT_DIR)/samples/o3d-webgl-samples/simpleviewer',
          'files': [
            'o3d-webgl-samples/simpleviewer/simpleviewer.html',
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
