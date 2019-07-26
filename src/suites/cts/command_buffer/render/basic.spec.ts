export const description = `
Basic command buffer rendering tests.
`;

import { TestGroup } from '../../../../framework/index.js';
import { GPUTest } from '../../gpu_test.js';

export const g = new TestGroup(GPUTest);

g.test('clear', async t => {
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });

  const colorAttachment = t.device.createTexture({
    format: 'rgba8unorm',
    size: { width: 1, height: 1, depth: 1 },
    usage: 1 | 16,
  });
  const colorAttachmentView = colorAttachment.createDefaultView();

  const encoder = t.device.createCommandEncoder({});
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        attachment: colorAttachmentView,
        loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
        storeOp: 'store',
      },
    ],
  });
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
    { buffer: dst, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1 }
  );
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});

g.test('fullscreen quad', async t => {
  const dst = t.device.createBuffer({ size: 4, usage: 4 | 8 });

  const colorAttachment = t.device.createTexture({
    format: 'rgba8unorm',
    size: { width: 1, height: 1, depth: 1 },
    usage: 1 | 16,
  });
  const colorAttachmentView = colorAttachment.createDefaultView();

  const vertexModule = t.makeShaderModule(
    'v',
    `#version 450
    void main() {
      const vec2 pos[3] = vec2[3](
          vec2(-1.f, -3.f), vec2(3.f, 1.f), vec2(-1.f, 1.f));
      gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
    }
  `
  );
  const fragmentModule = t.makeShaderModule(
    'f',
    `#version 450
    layout(location = 0) out vec4 fragColor;
    void main() {
      fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
  `
  );
  const pl = t.device.createPipelineLayout({ bindGroupLayouts: [] });
  const pipeline = t.device.createRenderPipeline({
    vertexStage: { module: vertexModule, entryPoint: 'main' },
    fragmentStage: { module: fragmentModule, entryPoint: 'main' },
    layout: pl,
    primitiveTopology: 'triangle-list',
    rasterizationState: {
      frontFace: 'ccw',
    },
    colorStates: [{ format: 'rgba8unorm', alphaBlend: {}, colorBlend: {} }],
    vertexInput: {
      indexFormat: 'uint16',
      vertexBuffers: [],
    },
  });

  const encoder = t.device.createCommandEncoder({});
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        attachment: colorAttachmentView,
        loadOp: 'clear',
        storeOp: 'store',
        clearColor: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      },
    ],
  });
  pass.setPipeline(pipeline);
  pass.draw(3, 1, 0, 0);
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
    { buffer: dst, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1 }
  );
  t.device.getQueue().submit([encoder.finish()]);

  await t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});
