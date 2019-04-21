# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from gpu_tests.gpu_test_expectations import GpuTestExpectations
from gpu_tests import webgl_test_util

# See the GpuTestExpectations class for documentation.

class WebGLConformanceExpectations(GpuTestExpectations):
  def __init__(self, is_asan=False):
    self.conformance_path = webgl_test_util.conformance_path
    super(WebGLConformanceExpectations, self).__init__(
      url_prefixes=webgl_test_util.url_prefixes_to_trim, is_asan=is_asan)

  def Fail(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Fail(self, pattern, condition, bug)

  def Flaky(self, pattern, condition=None, bug=None, max_num_retries=2):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Flaky(self, pattern, condition, bug=bug,
        max_num_retries=max_num_retries)

  def Skip(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Skip(self, pattern, condition, bug)

  def CheckPatternIsValid(self, pattern):
    # Look for basic wildcards.
    if not '*' in pattern and not 'WebglExtension_' in pattern:
      full_path = os.path.normpath(os.path.join(self.conformance_path, pattern))
      if not os.path.exists(full_path):
        raise Exception('The WebGL conformance test path specified in ' +
          'expectation does not exist: ' + full_path)

  def SetExpectations(self):
    # ===================================
    # Extension availability expectations
    # ===================================
    # It's expected that not all extensions will be available on all platforms.
    # Having a test listed here is not necessarily a problem.

    self.Fail('WebglExtension_EXT_color_buffer_float',
        ['win', 'mac'])
    self.Fail('WebglExtension_EXT_float_blend',
        ['win', 'passthrough', 'vulkan'])
    self.Fail('WebglExtension_WEBGL_multi_draw_instanced',
        ['passthrough', 'vulkan'], bug=2672) # angle bug ID
    # Skip these, rather than expect them to fail, to speed up test
    # execution. The browser is restarted even after expected test
    # failures.
    self.Skip('WebglExtension_WEBGL_compressed_texture_astc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc1',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_s3tc_srgb',
        ['win', 'mac', 'linux', 'android'])
    self.Skip('WebglExtension_EXT_disjoint_timer_query',
        ['android'], bug=808744)
    self.Skip('WebglExtension_KHR_parallel_shader_compile',
        ['no_passthrough'], bug=849576)

    # Extensions not available under D3D9
    self.Fail('WebglExtension_EXT_float_blend',
        ['win', 'd3d9'])
    self.Fail('WebglExtension_EXT_sRGB',
        ['win', 'd3d9'])
    self.Fail('WebglExtension_EXT_disjoint_timer_query',
        ['win', 'd3d9'], bug=867718)
    self.Fail('WebglExtension_WEBGL_depth_texture',
        ['win', 'amd', 'd3d9'])
    self.Fail('WebglExtension_WEBGL_draw_buffers',
        ['win', 'd3d9'])

    # Android general
    self.Fail('WebglExtension_EXT_frag_depth',
        ['android'])
    self.Fail('WebglExtension_EXT_float_blend',
        ['android'])
    self.Fail('WebglExtension_EXT_shader_texture_lod',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_astc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_s3tc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_depth_texture',
        ['android'])
    self.Fail('WebglExtension_WEBGL_draw_buffers',
        ['android'])

    # ========================
    # Conformance expectations
    # ========================
    # Fails on all platforms

    self.Fail('conformance/extensions/oes-texture-float.html',
        bug=930993)
    # TODO(shrekshao): Remove this after applying the new draw buffer
    # validation. And then uncomment the failure expectation for
    # angle bug 1523 (L160)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        bug=927908)

    # Need to implement new error semantics
    # https://github.com/KhronosGroup/WebGL/pull/2607
    self.Fail('conformance/extensions/' +
        'angle-instanced-arrays-out-of-bounds.html', bug=849572)

    # Nvidia bugs fixed in latest driver
    # TODO(http://crbug.com/887241): Upgrade the drivers on the bots.
    self.Fail('conformance/glsl/bugs/vector-scalar-arithmetic-inside-loop.html',
        ['linux', 'nvidia'], bug=772651)
    self.Fail('conformance/glsl/bugs/vector-scalar-arithmetic-inside-loop.html',
        ['android', 'nvidia'], bug=905370)
    self.Fail('conformance/glsl/bugs/' +
        'vector-scalar-arithmetic-inside-loop-complex.html',
        ['nvidia'], bug=772651)
    self.Fail('conformance/glsl/bugs/assign-to-swizzled-twice-in-function.html',
        ['win', 'nvidia', 'vulkan'], bug=798117)
    self.Fail('conformance/glsl/bugs/assign-to-swizzled-twice-in-function.html',
        ['linux', 'nvidia'], bug=798117)
    self.Fail('conformance/glsl/bugs/assign-to-swizzled-twice-in-function.html',
        ['android', 'nvidia'], bug=798117)
    self.Fail('conformance/glsl/bugs/' +
        'in-parameter-passed-as-inout-argument-and-global.html',
        ['nvidia'], bug=792210)

    # This test needs to be rewritten to measure its expected
    # performance; it's currently too flaky even on release bots.
    self.Skip('conformance/rendering/texture-switch-performance.html',
        bug=735483)

    self.Fail('conformance/rendering/blending.html',
        ['passthrough'], bug=951628)

    # Passthrough command decoder / OpenGL
    self.Fail(
        'conformance/textures/canvas/tex-2d-alpha-alpha-unsigned_byte.html',
        ['passthrough', 'opengl'], bug=2952) # angle bug ID
    self.Fail('conformance/textures/canvas/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['passthrough', 'opengl'], bug=2952) # angle bug ID

    # Intel graphics driver issue. Passed on 25.20.100.6471
    self.Fail('conformance/glsl/constructors/glsl-construct-mat2.html',
        ['passthrough', 'opengl', 'intel'], bug=665521)

    # Passthrough command decoder / OpenGL / AMD
    # self.Fail('conformance/extensions/webgl-draw-buffers.html',
    #     ['passthrough', 'opengl', 'amd', 'linux'], bug=1523) # angle bug ID
    self.Fail('conformance/glsl/constructors/glsl-construct-mat2.html',
        ['passthrough', 'opengl', 'amd'], bug=665521)
    self.Fail('conformance/glsl/constructors/' +
        'glsl-construct-vec-mat-corner-cases.html',
        ['passthrough', 'opengl', 'amd'], bug=665521)
    self.Fail('conformance/glsl/constructors/' +
        'glsl-construct-vec-mat-index.html',
        ['passthrough', 'opengl', 'amd', 'linux'], bug=665521)
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['passthrough', 'opengl', 'amd', 'linux'], bug=665521)
    self.Skip('conformance/glsl/misc/shaders-with-invariance.html',
        ['passthrough', 'opengl', 'amd', 'linux'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['passthrough', 'opengl', 'amd', 'linux'], bug=665521)
    self.Fail('conformance/renderbuffers/renderbuffer-initialization.html',
        ['passthrough', 'opengl', 'amd'], bug=1635) # angle bug ID
    self.Fail('conformance/renderbuffers/framebuffer-state-restoration.html',
        ['passthrough', 'opengl', 'amd'], bug=665521)
    self.Fail('conformance/uniforms/out-of-bounds-uniform-array-access.html',
        ['passthrough', 'opengl', 'amd'], bug=665521)
    self.Fail('conformance/textures/misc/texture-attachment-formats.html',
        ['passthrough', 'opengl', 'amd'], bug=665521)

    # Win / AMD / Passthrough command decoder / D3D11
    self.Flaky('conformance/textures/misc/copytexsubimage2d-subrects.html',
        ['win', 'amd', 'passthrough', 'd3d11'], bug=685232)
    self.Flaky('conformance/textures/misc/texture-sub-image-cube-maps.html',
        ['win7', 'amd', 'passthrough', 'd3d11', 'debug'], bug=772037)
    self.Flaky('conformance/extensions/oes-texture-half-float.html',
        ['win7', 'amd', 'passthrough', 'd3d11', 'release'], bug=772037)

    # Win / NVIDIA / Passthrough command decoder / D3D11
    self.Flaky('conformance/extensions/oes-texture-half-float-with-video.html',
        ['win7', 'nvidia', 'passthrough', 'd3d11', 'debug'], bug=751849)
    self.Flaky('conformance/programs/program-test.html',
        ['win', 'nvidia', 'passthrough', 'd3d11'], bug=737016)

    # Win failures
    # TODO(kbr): re-enable suppression for same test below once fixed.
    self.Skip('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['win'], bug=2103) # angle bug ID
    # Note that the following test seems to pass, but it may still be flaky.
    self.Flaky('conformance/glsl/constructors/' +
              'glsl-construct-vec-mat-index.html',
              ['win'], bug=525188)
    self.Fail('deqp/data/gles2/shaders/functions.html',
        ['win'], bug=478572)
    self.Fail('conformance/textures/misc/texture-active-bind.html',
        ['win'], bug=931006)
    self.Fail('conformance/rendering/blending.html',
        ['win', 'no_passthrough'], bug=951628)

    # Win NVIDIA failures
    self.Flaky('conformance/textures/misc/texture-npot-video.html',
        ['win', 'nvidia', 'no_passthrough'], bug=626524)
    self.Flaky('conformance/textures/misc/texture-upload-size.html',
        ['win', 'nvidia'], bug=630860)
    self.Skip('conformance/rendering/out-of-bounds-index-buffers.html',
              ['win', 'nvidia', 'passthrough', 'vulkan'], bug=3018) # ANGLE bug

    # self.Fail('conformance/extensions/ext-sRGB.html',
    #     ['win', 'nvidia', 'no_passthrough'], bug=679696)

    # Win10 / NVIDIA Quadro P400 / D3D9 failures
    self.Flaky('conformance/canvas/canvas-test.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=829389)
    self.Fail('conformance/canvas/drawingbuffer-static-canvas-test.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=680754)
    self.Fail('conformance/canvas/' +
        'framebuffer-bindings-affected-by-to-data-url.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=680754)
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=750896)
    self.Flaky('conformance/glsl/misc/shader-with-non-reserved-words.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=829389)
    self.Flaky('conformance/glsl/variables/gl-frontfacing.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=825416)
    self.Fail('conformance/ogles/GL/atan/atan_001_to_008.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=737018)
    self.Fail('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=680754)
    self.Flaky('conformance/textures/image_bitmap_from_video/*',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=750896)
    self.Flaky('conformance/textures/video/*',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=750896)
    self.Flaky('conformance/textures/misc/texture-corner-case-videos.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d9'], bug=750896)

    self.Flaky('conformance/uniforms/uniform-samplers-test.html',
        ['win10', ('nvidia', 0x1cb3), 'passthrough', 'd3d9'], bug=829389)

    # Win10 / NVIDIA Quadro P400 failures
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['win10', ('nvidia', 0x1cb3)], bug=728670)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['win10', ('nvidia', 0x1cb3)], bug=728670)

    # Win7 / NVIDIA D3D9 failures
    self.Flaky('conformance/canvas/canvas-test.html',
        ['win7', 'nvidia', 'd3d9'], bug=690248)

    # Win / Intel
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['win', 'intel'], bug=825338)
    self.Flaky('conformance/glsl/misc/shader-with-non-reserved-words.html',
        ['win', 'intel'], bug=929009)

    # This is an OpenGL driver bug on Intel platform and it is fixed in
    # Intel Driver 25.20.100.6444.
    # Case no-over-optimization-on-uniform-array-09 always fail if run
    # case biuDepthRange_001_to_002 first.
    # Temporarily skip these two cases now because this issue blocks
    # WEBGL_video_texture implementation.
    self.Skip(
        'conformance/ogles/GL/biuDepthRange/biuDepthRange_001_to_002.html',
        ['win', 'intel', 'opengl', 'passthrough'], bug=907195)
    self.Skip(
        'conformance/uniforms/no-over-optimization-on-uniform-array-09.html',
        ['win', 'intel', 'opengl', 'passthrough'], bug=907195)

    # Win7 / Intel failures
    self.Fail('conformance/textures/misc/' +
              'copy-tex-image-and-sub-image-2d.html',
              ['win7', 'intel', 'no_passthrough'])

    # Win AMD failures
    # This test is probably flaky on all AMD, but only visible on the
    # new AMD (the whole test suite is flaky on the old config).
    # Mark as Fail since it often flakes in all 3 retries
    self.Fail('conformance/extensions/oes-texture-half-float.html',
              ['win', 'no_passthrough', ('amd', 0x6613)], bug=653533)

    # Win / AMD D3D11 failures
    self.Flaky('conformance/textures/webgl_canvas/*', ['win', 'amd'],
        bug=878780)

    # Win / AMD D3D9 failures
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['win', 'amd', 'd3d9'], bug=475095)
    self.Fail('conformance/rendering/more-than-65536-indices.html',
        ['win', 'amd', 'd3d9'], bug=475095)

    # Win / D3D9 failures
    # Skipping these two tests because they're causing assertion failures.
    self.Skip('conformance/extensions/oes-texture-float-with-canvas.html',
        ['win', 'd3d9', 'no_passthrough'], bug=896) # angle bug ID
    self.Skip('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['win', 'd3d9', 'no_passthrough'], bug=896) # angle bug ID
    self.Fail('conformance/glsl/bugs/floor-div-cos-should-not-truncate.html',
        ['win', 'd3d9'], bug=1179) # angle bug ID
    # The functions test have been persistently flaky on D3D9
    self.Flaky('conformance/glsl/functions/*',
        ['win', 'd3d9'], bug=415609)
    self.Flaky('conformance/glsl/matrices/glsl-mat4-to-mat3.html',
        ['win', 'd3d9'], bug=617148)
    self.Flaky('conformance/glsl/matrices/glsl-mat3-construction.html',
        ['win', 'd3d9'], bug=617148)
    self.Skip('conformance/glsl/misc/large-loop-compile.html',
        ['win', 'd3d9'], bug=674572)

    # WIN / OpenGL / NVIDIA failures
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['win', ('nvidia', 0x1cb3), 'opengl', 'passthrough'], bug=715001)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['win', ('nvidia', 0x1cb3), 'opengl'], bug=703779)

    # Mark ANGLE's OpenGL as flaky on Windows Nvidia
    self.Flaky('conformance/*', ['win', 'nvidia', 'opengl'], bug=582083)

    # Win / OpenGL / AMD failures
    self.Skip('conformance/attribs/gl-bindAttribLocation-aliasing.html',
        ['win', 'amd', 'opengl'], bug=649824)
    self.Skip('conformance/glsl/misc/shader-struct-scope.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Skip('conformance/glsl/misc/shaders-with-invariance.html',
        ['win', 'amd', 'opengl', 'no_passthrough'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['win', 'amd', 'opengl'], bug=1506) # angle bug ID
    # AMD RX 550 Failures
    self.Skip('conformance/glsl/bugs/gl-fragcoord-multisampling-bug.html',
        ['win', ('amd', 0x699f), 'opengl'], bug=950123)
    self.Skip('conformance/glsl/misc/fragcolor-fragdata-invariant.html',
        ['win', ('amd', 0x699f), 'opengl'], bug=950123)
    self.Skip('conformance/glsl/samplers/glsl-function-texture2dprojlod.html',
        ['win', ('amd', 0x699f), 'opengl'], bug=950123)
    self.Fail('conformance/reading/read-pixels-test.html',
        ['win', ('amd', 0x699f), 'opengl'], bug=951771)
    self.Skip('conformance/rendering/line-rendering-quality.html',
        ['win', ('amd', 0x699f), 'opengl'], bug=950123)
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['win', ('amd', 0x699f), 'opengl', 'passthrough'], bug=952887)

    # Mark ANGLE's OpenGL as flaky on Windows Amd
    self.Flaky('conformance/*', ['win', 'amd', 'opengl'], bug=582083)

    # Win / OpenGL / Intel HD 530 / 630 failures
    self.Flaky('conformance/glsl/variables/gl-pointcoord.html',
        ['win10', 'intel', 'opengl'], bug=854100)

    # Win / Intel / Passthrough command decoder
    self.Flaky('conformance/renderbuffers/framebuffer-state-restoration.html',
        ['win', 'intel', 'passthrough', 'd3d11'], bug=602688)

    # D3D9 / Passthrough command decoder
    self.Fail('conformance/textures/canvas/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/canvas/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/image_bitmap_from_canvas/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/image_bitmap_from_canvas/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/video/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/video/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/webgl_canvas/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID
    self.Fail('conformance/textures/webgl_canvas/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['win', 'd3d9', 'passthrough'], bug=2192) # ANGLE bug ID

    # Vulkan / Win / Passthough command decoder
    self.Fail('conformance/attribs/' +
        'gl-vertex-attrib-unconsumed-out-of-bounds.html',
        ['win', 'passthrough', 'vulkan'], bug=2708) # ANGLE bug ID
    self.Fail('conformance/canvas/canvas-test.html',
        ['win', 'passthrough', 'vulkan'], bug=2929) # ANGLE bug ID
    self.Fail('conformance/canvas/' +
        'draw-static-webgl-to-multiple-canvas-test.html',
        ['win', 'passthrough', 'vulkan'], bug=2918) # ANGLE bug ID
    self.Fail('conformance/canvas/draw-webgl-to-canvas-test.html',
        ['win', 'passthrough', 'vulkan'], bug=2918) # ANGLE bug ID
    self.Fail('conformance/context/' +
        'context-attribute-preserve-drawing-buffer.html',
        ['win', 'passthrough', 'vulkan'], bug=2913) # ANGLE bug ID
    self.Fail('conformance/misc/uninitialized-test.html',
        ['win', 'passthrough', 'vulkan'], bug=2987) # ANGLE bug ID
    self.Fail('conformance/more/functions/copyTexSubImage2D.html',
        ['win', 'passthrough', 'vulkan'], bug=2911) # ANGLE bug ID
    self.Fail('conformance/offscreencanvas/context-lost-restored-worker.html',
        ['win', 'passthrough', 'vulkan'], bug=2916) # ANGLE bug ID
    self.Fail('conformance/offscreencanvas/context-lost-restored.html',
        ['win', 'passthrough', 'vulkan'], bug=2916) # ANGLE bug ID
    self.Fail('conformance/ogles/GL/faceforward/faceforward_001_to_006.html',
        ['win', 'passthrough', 'vulkan'], bug=2909) # ANGLE bug ID
    self.Fail('conformance/renderbuffers/' +
        'depth-renderbuffer-initialization.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['win', 'passthrough', 'vulkan'], bug=2910) # ANGLE bug ID
    self.Fail('conformance/renderbuffers/framebuffer-state-restoration.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/renderbuffers/' +
        'stencil-renderbuffer-initialization.html',
        ['win', 'passthrough', 'vulkan'], bug=0) # ANGLE bug ID
    self.Fail('conformance/rendering/preservedrawingbuffer-leak.html',
        ['win', 'passthrough', 'vulkan'], bug=2919) # ANGLE bug ID
    self.Fail('conformance/textures/misc/copy-tex-image-and-sub-image-2d.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/textures/misc/' +
        'copytexsubimage2d-large-partial-copy-corruption.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/textures/misc/copytexsubimage2d-subrects.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/textures/misc/gl-pixelstorei.html',
        ['win', 'passthrough', 'vulkan'], bug=2920) # ANGLE bug ID
    self.Fail('conformance/textures/misc/' +
        'tex-image-and-sub-image-2d-with-array-buffer-view.html',
        ['win', 'passthrough', 'vulkan'], bug=2912) # ANGLE bug ID
    self.Fail('conformance/textures/misc/tex-image-webgl.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-attachment-formats.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-copying-feedback-loops.html',
        ['win', 'passthrough', 'vulkan'], bug=2914) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-hd-dpi.html',
        ['win', 'passthrough', 'vulkan'], bug=2913) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-mips.html',
        ['win', 'passthrough', 'vulkan'], bug=2722) # ANGLE bug ID
    # Note: the following test crashes so it's skipped.  http://anglebug.com/3352
    self.Skip('conformance/uniforms/out-of-bounds-uniform-array-access.html',
        ['win', 'passthrough', 'vulkan'], bug=2921) # ANGLE bug ID
    self.Fail('WebglExtension_ANGLE_instanced_arrays',
        ['win', 'passthrough', 'vulkan'], bug=2672) # ANGLE bug ID
    self.Fail('WebglExtension_EXT_blend_minmax',
        ['win', 'passthrough', 'vulkan'], bug=2897) # ANGLE bug ID
    self.Fail('WebglExtension_EXT_color_buffer_half_float',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_EXT_disjoint_timer_query',
        ['win', 'passthrough', 'vulkan'], bug=2885) # ANGLE bug ID
    self.Fail('WebglExtension_EXT_frag_depth',
        ['win', 'passthrough', 'vulkan'], bug=2887) # ANGLE bug ID
    self.Fail('WebglExtension_EXT_shader_texture_lod',
        ['win', 'passthrough', 'vulkan'], bug=2899) # ANGLE bug ID
    self.Fail('WebglExtension_OES_element_index_uint',
        ['win', 'passthrough', 'vulkan'], bug=2902) # ANGLE bug ID
    self.Fail('WebglExtension_OES_standard_derivatives',
        ['win', 'passthrough', 'vulkan'], bug=2903) # ANGLE bug ID
    self.Fail('WebglExtension_OES_texture_float',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_OES_texture_float_linear',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_OES_texture_half_float',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_OES_texture_half_float_linear',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_WEBGL_color_buffer_float',
        ['win', 'passthrough', 'vulkan'], bug=2898) # ANGLE bug ID
    self.Fail('WebglExtension_WEBGL_compressed_texture_s3tc',
        ['win', 'passthrough', 'vulkan'], bug=2904) # ANGLE bug ID
    self.Fail('WebglExtension_WEBGL_depth_texture',
        ['win', 'passthrough', 'vulkan'], bug=2905) # ANGLE bug ID
    self.Fail('WebglExtension_WEBGL_draw_buffers',
        ['win', 'passthrough', 'vulkan'], bug=2394) # ANGLE bug ID
    self.Skip('deqp/data/gles2/shaders/swizzles.html',
        ['win', 'passthrough', 'vulkan'], bug=3111) # ANGLE bug ID

    # Vulkan / Win / NVIDIA / Passthough command decoder
    self.Fail('conformance/canvas/to-data-url-test.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2918) # ANGLE bug ID
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2922) # ANGLE bug ID
    self.Flaky('conformance/rendering/gl-scissor-fbo-test.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2939) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-size.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2915) # ANGLE bug ID
    self.Fail('conformance/textures/misc/texture-size-cube-maps.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2930) # ANGLE bug ID
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2915) # ANGLE bug ID
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['win', 'passthrough', 'vulkan', 'nvidia'], bug=2926) # ANGLE bug ID

    # Vulkan / Win / Intel / Passthough command decoder
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['win', 'passthrough', 'vulkan', 'intel'], bug=2722) # ANGLE bug ID

    # Vulkan / Win / AMD / Passthough command decoder
    self.Fail('conformance/buffers/buffer-data-dynamic-delay.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2931) # ANGLE bug ID
    self.Fail('conformance/canvas/to-data-url-test.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2918) # ANGLE bug ID
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2922) # ANGLE bug ID
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2722) # ANGLE bug ID
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2926) # ANGLE bug ID
    self.Fail('deqp/data/gles2/shaders/linkage.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=2926) # ANGLE bug ID
    self.Fail('conformance/textures/' +
        'image_bitmap_from_canvas/tex-2d-luminance*.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=931016)
    self.Fail('conformance/textures/' +
        'image_bitmap_from_canvas/tex-2d-rgb*.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=931016)
    self.Fail('conformance/glsl/bugs/assign-to-swizzled-twice-in-function.html',
        ['win', 'passthrough', 'vulkan', 'amd'], bug=3343) # ANGLE bug ID

    # Mac failures
    self.Fail('conformance/glsl/misc/fragcolor-fragdata-invariant.html',
        ['mac'], bug=844311)
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['mac', 'no_passthrough'], bug=599272)
    self.Flaky('conformance/ogles/GL/abs/abs_001_to_006.html', ['mac'],
        bug=928926)

    # Mac AMD failures
    self.Fail('conformance/glsl/bugs/bool-type-cast-bug-int-float.html',
        ['mac', 'amd'], bug=905007)
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['mac', 'amd'], bug=642822)
    self.Flaky('conformance/glsl/misc/shader-with-non-reserved-words.html',
        ['mac', 'amd'], bug=929009)
    # TODO(kbr): uncomment the following exepectation after test has
    # been made more robust.
    # self.Fail('conformance/rendering/texture-switch-performance.html',
    #     ['mac', 'amd', 'release'], bug=735483)

    # Mac Intel
    self.Fail('conformance/rendering/canvas-alpha-bug.html',
        ['mac', ('intel', 0x0a2e)], bug=886970)
    self.Fail('conformance/rendering/rendering-stencil-large-viewport.html',
        ['mac', 'intel'], bug=782317)


    # Mac Retina NVidia failures
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Flaky('conformance/ogles/GL/exp2/exp2_001_to_008.html',
        ['mac', ('nvidia', 0xfe9)], bug=923080)
    self.Fail('conformance/programs/' +
        'gl-bind-attrib-location-long-names-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/programs/gl-bind-attrib-location-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['mac', ('nvidia', 0xfe9), 'no_passthrough'], bug=635081)
    self.Fail('conformance/textures/misc/tex-input-validation.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/glsl/bugs/init-array-with-loop.html',
        ['mac', ('nvidia', 0xfe9)], bug=784817)
    self.Fail('conformance/uniforms/uniform-samplers-test.html',
        ['mac', 'debug', ('nvidia', 0xfe9)], bug=871352)
    self.Fail('conformance/glsl/misc/shader-with-non-reserved-words.html',
        ['mac', 'debug', ('nvidia', 0xfe9)], bug=948218)

    # Already fixed with Mesa 17.1.6
    self.Fail('conformance/extensions/webgl-compressed-texture-astc.html',
        ['linux', 'intel'], bug=680675)

    # NVIDIA
    self.Flaky('conformance/extensions/oes-element-index-uint.html',
               ['linux', 'nvidia', 'no_passthrough'], bug=524144)
    self.Flaky('conformance/textures/image/' +
               'tex-2d-rgb-rgb-unsigned_byte.html',
               ['linux', 'nvidia'], bug=596622)

    # NVIDIA P400 OpenGL
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['linux', ('nvidia', 0x1cb3)], bug=715001)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['linux', ('nvidia', 0x1cb3), 'opengl'], bug=703779)
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['linux', ('nvidia', 0x1cb3)], bug=913969)
    self.Flaky('conformance/extensions/oes-texture-half-float-with-video.html',
        ['linux', ('nvidia', 0x1cb3)], bug=913969)

    # NVIDIA P400 OpenGL, Debug
    self.Flaky('conformance/canvas/draw-webgl-to-canvas-test.html',
        ['linux', 'debug', ('nvidia', 0x1cb3)], bug=918995)
    self.Flaky('conformance/extensions/webgl-depth-texture.html',
        ['linux', 'debug', ('nvidia', 0x1cb3)], bug=918995)
    self.Flaky('conformance/rendering/polygon-offset.html',
        ['linux', 'debug', ('nvidia', 0x1cb3)], bug=918995)

    # AMD
    self.Fail('conformance/glsl/misc/fragcolor-fragdata-invariant.html',
        ['linux', 'amd'], bug=844311)
    self.Flaky('conformance/more/functions/uniformi.html',
               ['linux', 'amd'], bug=550989)
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['linux', 'amd'], bug=642822)

    # AMD Radeon 6450 and/or R7 240
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['linux', 'amd', 'no_angle'], bug=479260)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', 'amd', 'no_passthrough'], bug=479952)

    # Linux passthrough AMD
    self.Fail('conformance/renderbuffers/' +
        'depth-renderbuffer-initialization.html',
        ['linux', 'passthrough', 'amd'], bug=794339)
    self.Fail('conformance/renderbuffers/' +
        'stencil-renderbuffer-initialization.html',
        ['linux', 'passthrough', 'amd'], bug=794339)
    self.Fail('conformance/rendering/' +
        'draw-webgl-to-canvas-2d-repeatedly.html',
        ['linux', 'amd', 'passthrough'], bug=906066)

    # The following two tests only fail on Linux/Intel with Mesa 18.0.5,
    # not on Mesa 17.1.4 with the same Intel HD 630 GPU.
    self.Fail('conformance/programs/program-test.html',
        ['linux', 'intel'], bug=928530)
    self.Fail('conformance/uniforms/uniform-default-values.html',
        ['linux', 'intel'], bug=928530)

    ####################
    # Android failures #
    ####################

    self.Fail('conformance/glsl/bugs/sampler-array-struct-function-arg.html',
              ['android'], bug=903903)
    self.Fail('conformance/glsl/bugs/sequence-operator-evaluation-order.html',
        ['android', 'qualcomm'], bug=478572)
    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])

    self.Fail('conformance/textures/misc/' +
        'copytexsubimage2d-large-partial-copy-corruption.html',
        ['android', 'no_passthrough', 'qualcomm'], bug=679697)

    self.Fail('conformance/rendering/blending.html',
        ['android', 'no_passthrough'], bug=951628)
    # The following WebView crashes are causing problems with further
    # tests in the suite, so skip them for now.
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgb-rgb-unsigned_byte.html',
        ['android', 'android-webview-instrumentation'], bug=352645)
    # Also flaky on nexus9 non-webview (bug=834933) but can't specify both.
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
        ['android'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_byte.html',
        ['android', 'android-webview-instrumentation'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['android', 'android-webview-instrumentation'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['android-webview-instrumentation'], bug=352645)
    self.Skip('conformance/textures/misc/texture-npot-video.html',
        ['android', 'android-webview-instrumentation', 'no_angle'],
              bug=352645)
    self.Flaky('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['android', 'release', 'no_angle'], bug=907512)

    # These video tests appear to be flaky.
    self.Flaky('conformance/textures/video/' +
        'tex-2d-alpha-alpha-unsigned_byte.html',
        ['android', 'no_angle'], bug=733599)
    self.Flaky('conformance/textures/video/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['android', 'no_angle'], bug=733599)
    self.Flaky('conformance/textures/video/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['android', 'no_angle'], bug=733599)
    self.Flaky('conformance/textures/video/' +
        'tex-2d-rgb-rgb-unsigned_byte.html',
        ['android', 'android-chromium', 'no_angle'], bug=834933)
    self.Flaky('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_byte.html',
        ['android', 'android-chromium', 'no_angle'], bug=834933)

    # This crashes in Android WebView on the Nexus 6, preventing the
    # suite from running further. Rather than add multiple
    # suppressions, skip it until it's passing at least in content
    # shell.
    self.Skip('conformance/extensions/oes-texture-float-with-video.html',
        ['android', 'qualcomm'], bug=499555)

    # Nexus 5
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib-update.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=899754)
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/extensions/ext-texture-filter-anisotropic.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/' +
        'array-of-struct-with-int-first-position.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/gl-fragcoord-multisampling-bug.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/qualcomm-loop-with-continue-crash.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=527761)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/struct-constructor-highp-bug.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=559342)
    self.Fail('conformance/glsl/matrices/glsl-mat4-to-mat3.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/' +
        'shader-with-vec4-vec3-vec4-conditional.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['android', ('qualcomm', 'Adreno (TM) 330'), 'no_passthrough'],
        bug=611943)
    self.Fail('conformance/glsl/misc/struct-equals.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('deqp/data/gles2/shaders/linkage.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=478572)
    self.Fail('WebglExtension_OES_texture_float_linear',
        ['android', ('qualcomm', 'Adreno (TM) 330'), 'no_passthrough'])
    self.Fail('conformance/more/functions/vertexAttribPointerBadArgs.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=678850)
    self.Fail('conformance/attribs/gl-vertexattribpointer.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=678850)
    self.Fail('conformance/glsl/bugs/' +
              'varying-arrays-should-not-be-reversed.html',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=709704)

    # Nexus 5X
    # The following test recently became so flaky that it had to be
    # upgraded to Fail.
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=882323)
    # The following test just started timing out randomly on the
    # android_optional_gpu_tests_rel tryserver with no apparent cause.
    self.Flaky('deqp/data/gles2/shaders/swizzles.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=793050)
    # This one is causing intermittent timeouts on the device, and it
    # looks like when that happens, the next test also always times
    # out. Skip it for now until it's fixed and running reliably.
    self.Skip('conformance/extensions/oes-texture-half-float-with-video.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')],
        bug=609883)
    # This test is skipped because it is crashing the GPU process.
    self.Skip('conformance/glsl/bugs/init-array-with-loop.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=784817)
    self.Flaky('conformance/glsl/bugs/loop-if-loop-gradient.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=920737)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Flaky('conformance/glsl/constructors/glsl-construct-ivec4.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    self.Flaky('conformance/glsl/constructors/glsl-construct-mat2.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    self.Flaky('conformance/glsl/constructors/' +
        'glsl-construct-vec-mat-corner-cases.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    self.Flaky('conformance/glsl/functions/glsl-function-dot.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    self.Flaky('conformance/glsl/functions/glsl-function-min-gentype.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    self.Flaky('conformance/glsl/implicit/add_ivec3_vec3.vert.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=912161)
    # This test is skipped because it is crashing the GPU process.
    self.Skip('conformance/glsl/misc/shader-with-non-reserved-words.html',
        ['android', ('qualcomm', 'Adreno (TM) 418'), 'no_passthrough'],
        bug=609883)
    self.Flaky('conformance/ogles/GL/all/all_001_to_004.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=793050)
    self.Flaky('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=793050)
    self.Flaky('conformance/ogles/GL/dot/dot_001_to_006.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=818041)
    self.Flaky('conformance/ogles/GL/swizzlers/swizzlers_041_to_048.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=793050)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['android', ('qualcomm', 'Adreno (TM) 418'), 'no_angle'], bug=818041)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['android', ('qualcomm', 'Adreno (TM) 418'), 'no_angle'], bug=793050)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgb-rgb-unsigned_byte.html',
        ['android', ('qualcomm', 'Adreno (TM) 418'), 'no_angle'], bug=716496)
    self.Flaky('conformance/textures/misc/texture-npot-video.html',
        ['android', 'android-chromium',
         ('qualcomm', 'Adreno (TM) 418'), 'no_angle'],
        bug=934545)
    self.Skip('conformance/uniforms/uniform-samplers-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 418')],
        bug=610951)
    self.Fail('WebglExtension_EXT_sRGB',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Flaky('conformance/limits/gl-max-texture-dimensions.html',
        ['android', ('qualcomm', 'Adreno (TM) 418'), 'passthrough'], bug=914631)

    # Nexus 6 (Adreno 420) and 6P (Adreno 430)
    self.Fail('conformance/context/' +
        'context-attributes-alpha-depth-stencil-antialias.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/context/context-size-change.html',
        ['android', ('qualcomm', 'Adreno (TM) 420'), 'no_angle'], bug=611945)
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/glsl/bugs/gl-fragcoord-multisampling-bug.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/glsl/bugs/qualcomm-crash.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['android',
         ('qualcomm', 'Adreno (TM) 420'),
         ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    # This test is skipped because running it causes a future test to fail.
    # The list of tests which may be that future test is very long. It is
    # almost (but not quite) every webgl conformance test.
    self.Skip('conformance/glsl/misc/shader-struct-scope.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=614550)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['android', ('qualcomm', 'Adreno (TM) 420'), 'no_passthrough'],
        bug=611945)
    # bindBufferBadArgs is causing the GPU thread to crash, taking
    # down the WebView shell, causing the next test to fail and
    # subsequent tests to be aborted.
    self.Skip('conformance/more/functions/bindBufferBadArgs.html',
        ['android', 'android-webview-instrumentation',
         ('qualcomm', 'Adreno (TM) 420')], bug=499874)
    self.Fail('conformance/reading/' +
        'fbo-remains-unchanged-after-read-pixels.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=899754)
    self.Fail('conformance/reading/read-pixels-pack-alignment.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=899754)
    self.Fail('conformance/reading/read-pixels-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=899754)
    self.Fail('conformance/rendering/clear-after-copyTexImage2D.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=737002)
    self.Fail('conformance/rendering/' +
        'color-mask-preserved-during-implicit-clears.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=911918)
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/rendering/gl-viewport-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/rendering/line-rendering-quality.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=847222)
    self.Fail('conformance/textures/misc/' +
        'copy-tex-image-and-sub-image-2d.html',
        ['android', ('qualcomm', 'Adreno (TM) 420'), 'no_passthrough'],
        bug=499555)
    self.Skip('conformance/uniforms/uniform-samplers-test.html',
        ['android', ('qualcomm', 'Adreno (TM) 430')],
        bug=663071)
    self.Fail('conformance/offscreencanvas/' +
        'context-attribute-preserve-drawing-buffer.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=693135)
    self.Fail('WebglExtension_EXT_sRGB',
        ['android',
         ('qualcomm', 'Adreno (TM) 420'), ('qualcomm', 'Adreno (TM) 430')])
    self.Fail('conformance/glsl/misc/uninitialized-local-global-variables.html',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=2046) # angle bug ID

    # Nexus 9
    self.Fail('deqp/data/gles2/shaders/functions.html',
        ['android', 'nvidia'], bug=478572)
    self.Fail('conformance/glsl/bugs/multiplication-assignment.html',
        ['android', 'nvidia'], bug=606096)
    self.Flaky('conformance/glsl/constructors/glsl-construct-ivec4.html',
        ['android', 'nvidia'], bug=912161)
    self.Flaky('conformance/glsl/constructors/glsl-construct-mat2.html',
        ['android', 'nvidia'], bug=912161)
    self.Flaky('conformance/extensions/oes-texture-half-float-with-video.html',
        ['android', 'nvidia'], bug=891456)
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['android', 'nvidia'], bug=891456)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgb-rgb-unsigned_short_5_6_5.html', ['android', 'nvidia'],
        bug=891456)

    # Flaky timeout on android_n5x_swarming_rel and
    # android-marshmallow-arm64-rel.
    self.Flaky('conformance/glsl/constructors/glsl-construct-mat3.html',
               ['android'], bug=845411)
    self.Flaky('conformance/glsl/bugs/sketchfab-lighting-shader-crash.html',
               ['android'], bug=845438)

    # Android ANGLE GLES
    # Video tests time out
    self.Skip('conformance/textures/image_bitmap_from_video/*',
        ['android', 'opengles'], bug=906724)
    self.Skip('conformance/textures/misc/' +
        'tex-video-using-tex-unit-non-zero.html',
        ['android', 'opengles'], bug=906724)
    self.Skip('conformance/textures/misc/texture-corner-case-videos.html',
        ['android', 'opengles'], bug=906724)
    self.Skip('conformance/textures/misc/texture-npot-video.html',
        ['android', 'opengles'], bug=906724)
    self.Skip('conformance/textures/misc/texture-upload-size.html',
        ['android', 'opengles'], bug=906724)
    self.Skip('conformance/textures/video/*',
        ['android', 'opengles'], bug=906724)

    # Canvas tests fail with missing fonts
    self.Fail('conformance/extensions/oes-texture-float-with-canvas.html',
        ['android', 'opengles'], bug=908866)
    self.Fail('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['android', 'opengles'], bug=908866)
    self.Fail('conformance/textures/canvas/*',
        ['android', 'opengles'], bug=908866)

    # Misc failures
    self.Fail('conformance/context/context-size-change.html',
        ['android', 'opengles'], bug=2988) #angle bug ID
    self.Fail('conformance/misc/uninitialized-test.html',
        ['android', 'opengles'], bug=2407) # angle bug ID
    self.Fail('conformance/renderbuffers/framebuffer-test.html',
        ['android', 'opengles'], bug=908912) # angle bug ID
    self.Fail('conformance/uniforms/out-of-bounds-uniform-array-access.html',
        ['android', 'opengles'], bug=2978)
    self.Fail('WebglExtension_WEBGL_compressed_texture_etc1',
        ['android', 'opengles'], bug=1552) # angle bug ID

    ############
    # ChromeOS #
    ############

    # ChromeOS: affecting all devices.
    self.Fail('conformance/extensions/webgl-depth-texture.html',
        ['chromeos', 'no_passthrough'], bug=382651)

    # ChromeOS: all Intel except for pinetrail (stumpy, parrot, peppy,...)
    # We will just include pinetrail here for now as we don't want to list
    # every single Intel device ID.
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/shaders-with-varyings.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['chromeos', 'intel', 'no_passthrough'], bug=375556)
    self.Fail('conformance/textures/misc/texture-size-limit.html',
        ['chromeos', 'intel'], bug=385361)

    # ChromeOS: pinetrail (alex, mario, zgb).
    self.Fail('conformance/attribs/gl-vertex-attrib-render.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-atan-xy.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-cos.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-sin.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/variables/gl-frontfacing.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/acos/acos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/asin/asin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/atan/atan_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/build/build_009_to_016.html',
        ['chromeos', ('intel', 0xa011)], bug=378938)
    self.Fail('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/discard/discard_001_to_002.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_065_to_072.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_081_to_088.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_097_to_104.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_105_to_112.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_113_to_120.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_121_to_126.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail(
        'conformance/ogles/GL/gl_FrontFacing/gl_FrontFacing_001_to_001.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log/log_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log2/log2_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/normalize/normalize_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/sin/sin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/point-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/polygon-offset.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/misc/texture-mips.html',
        ['chromeos', ('intel', 0xa011), 'no_passthrough'], bug=375554)
    self.Fail('conformance/textures/misc/texture-npot.html',
        ['chromeos', ('intel', 0xa011), 'no_passthrough'], bug=375554)
    self.Fail('conformance/textures/misc/texture-npot-video.html',
        ['chromeos', ('intel', 0xa011), 'no_passthrough'], bug=375554)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Skip('conformance/uniforms/uniform-default-values.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
