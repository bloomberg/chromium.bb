export const description = `
Basic command buffer rendering tests.
`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('clear').fn(async t => {
  const dst = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
  });

  const colorAttachment = t.device.createTexture({
    format: 'rgba8unorm',
    size: { width: 1, height: 1, depthOrArrayLayers: 1 },
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
  });
  const colorAttachmentView = colorAttachment.createView();

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        view: colorAttachmentView,
        loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
        storeOp: 'store',
      },
    ],
  });
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
    { buffer: dst, bytesPerRow: 256 },
    { width: 1, height: 1, depthOrArrayLayers: 1 }
  );
  t.device.queue.submit([encoder.finish()]);

  t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});

g.test('fullscreen_quad').fn(async t => {
  const dst = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
  });

  const colorAttachment = t.device.createTexture({
    format: 'rgba8unorm',
    size: { width: 1, height: 1, depthOrArrayLayers: 1 },
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
  });
  const colorAttachmentView = colorAttachment.createView();

  const pipeline = t.device.createRenderPipeline({
    vertex: {
      module: t.device.createShaderModule({
        code: `
        [[stage(vertex)]] fn main(
          [[builtin(vertex_index)]] VertexIndex : i32
          ) -> [[builtin(position)]] vec4<f32> {
            let pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -3.0),
                vec2<f32>(3.0, 1.0),
                vec2<f32>(-1.0, 1.0));
            return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
          }
          `,
      }),
      entryPoint: 'main',
    },
    fragment: {
      module: t.device.createShaderModule({
        code: `
          [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
            return vec4<f32>(0.0, 1.0, 0.0, 1.0);
          }
          `,
      }),
      entryPoint: 'main',
      targets: [{ format: 'rgba8unorm' }],
    },
    primitive: { topology: 'triangle-list' },
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        view: colorAttachmentView,
        storeOp: 'store',
        loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      },
    ],
  });
  pass.setPipeline(pipeline);
  pass.draw(3);
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
    { buffer: dst, bytesPerRow: 256 },
    { width: 1, height: 1, depthOrArrayLayers: 1 }
  );
  t.device.queue.submit([encoder.finish()]);

  t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});

g.test('large_draw')
  .desc(`Test reasonably-sized large {draw, drawIndexed} (see also stress tests).`)
  .unimplemented();
