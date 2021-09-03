export const description = `Test culling and rasterizaion state.

Test coverage:
Test all culling combinations of GPUFrontFace and GPUCullMode show the correct output.

Use 2 triangles with different winding orders:

- Test that the counter-clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)

- Test that the clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)
`;

import { poptions, params } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

function faceIsCulled(face: 'cw' | 'ccw', frontFace: GPUFrontFace, cullMode: GPUCullMode): boolean {
  return cullMode !== 'none' && (frontFace === face) === (cullMode === 'front');
}

function faceColor(face: 'cw' | 'ccw', frontFace: GPUFrontFace, cullMode: GPUCullMode): Uint8Array {
  // front facing color is green, non front facing is red, background is blue
  const isCulled = faceIsCulled(face, frontFace, cullMode);
  if (!isCulled && face === frontFace) {
    return new Uint8Array([0x00, 0xff, 0x00, 0xff]);
  } else if (isCulled) {
    return new Uint8Array([0x00, 0x00, 0xff, 0xff]);
  } else {
    return new Uint8Array([0xff, 0x00, 0x00, 0xff]);
  }
}

export const g = makeTestGroup(GPUTest);

g.test('culling')
  .params(
    params()
      .combine(poptions('frontFace', ['ccw', 'cw'] as const))
      .combine(poptions('cullMode', ['none', 'front', 'back'] as const))
      .combine(
        poptions('depthStencilFormat', [
          null,
          'depth24plus',
          'depth32float',
          'depth24plus-stencil8',
        ] as const)
      )
      // TODO: test triangle-strip as well
      .combine(poptions('primitiveTopology', ['triangle-list'] as const))
  )
  .fn(t => {
    const size = 4;
    const format = 'rgba8unorm';

    const texture = t.device.createTexture({
      size: { width: size, height: size, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });

    const depthTexture = t.params.depthStencilFormat
      ? t.device.createTexture({
          size: { width: size, height: size, depthOrArrayLayers: 1 },
          format: t.params.depthStencilFormat,
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        })
      : null;

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: texture.createView(),
          loadValue: { r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
          storeOp: 'store',
        },
      ],
      depthStencilAttachment: depthTexture
        ? {
            view: depthTexture.createView(),
            depthLoadValue: 1.0,
            depthStoreOp: 'store',
            stencilLoadValue: 0,
            stencilStoreOp: 'store',
          }
        : undefined,
    });

    // Draw two triangles with different winding orders:
    // 1. The top-left one is counterclockwise (CCW)
    // 2. The bottom-right one is clockwise (CW)
    pass.setPipeline(
      t.device.createRenderPipeline({
        vertex: {
          module: t.device.createShaderModule({
            code: `
              [[stage(vertex)]] fn main(
                [[builtin(vertex_index)]] VertexIndex : i32
                ) -> [[builtin(position)]] vec4<f32> {
                let pos : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                    vec2<f32>(-1.0,  1.0),
                    vec2<f32>(-1.0,  0.0),
                    vec2<f32>( 0.0,  1.0),
                    vec2<f32>( 0.0, -1.0),
                    vec2<f32>( 1.0,  0.0),
                    vec2<f32>( 1.0, -1.0));
                return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
              }`,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: t.device.createShaderModule({
            code: `
              [[stage(fragment)]] fn main(
                [[builtin(front_facing)]] FrontFacing : bool
                ) -> [[location(0)]] vec4<f32> {
                var color : vec4<f32>;
                if (FrontFacing) {
                  color = vec4<f32>(0.0, 1.0, 0.0, 1.0);
                } else {
                  color = vec4<f32>(1.0, 0.0, 0.0, 1.0);
                }
                return color;
              }`,
          }),
          entryPoint: 'main',
          targets: [{ format }],
        },
        primitive: {
          topology: t.params.primitiveTopology,
          frontFace: t.params.frontFace,
          cullMode: t.params.cullMode,
        },
        depthStencil: depthTexture
          ? { format: t.params.depthStencilFormat as GPUTextureFormat }
          : undefined,
      })
    );

    pass.draw(6, 1, 0, 0);
    pass.endPass();

    t.device.queue.submit([encoder.finish()]);

    // front facing color is green, non front facing is red, background is blue
    const kCCWTriangleTopLeftColor = faceColor('ccw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: 0, y: 0 },
      { exp: kCCWTriangleTopLeftColor }
    );

    const kCWTriangleBottomRightColor = faceColor('cw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: size - 1, y: size - 1 },
      { exp: kCWTriangleBottomRightColor }
    );
    // TODO: check the contents of the depth and stencil outputs
  });
