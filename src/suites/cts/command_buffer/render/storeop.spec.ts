export const description = `
renderPass store op test that drawn quad is either stored or cleared based on storeop`;

import { TestGroup } from '../../../../framework/index.js';
import GLSL from '../../../../tools/glsl.macro.js';
import { GPUTest } from '../../gpu_test.js';

export const g = new TestGroup(GPUTest);

g.test('storeOp controls whether 1x1 drawn quad is stored', async t => {
  const renderTexture = t.device.createTexture({
    size: { width: 1, height: 1, depth: 1 },
    format: 'r8unorm',
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.OUTPUT_ATTACHMENT,
  });

  // create render pipeline
  const vertexModule = t.createShaderModule({
    code: GLSL(
      'vertex',
      `#version 450
      const vec2 pos[3] = vec2[3](
                              vec2( 1.0f, -1.0f),
                              vec2( 1.0f,  1.0f),
                              vec2(-1.0f,  1.0f)
                              );

      void main() {
          gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
      }`
    ),
  });
  const fragmentModule = t.createShaderModule({
    code: GLSL(
      'fragment',
      `#version 450
      layout(location = 0) out vec4 fragColor;
      void main() {
          fragColor = vec4(1.0, 0.0, 0.0, 1.0);
      }`
    ),
  });
  const renderPipeline = t.device.createRenderPipeline({
    vertexStage: { module: vertexModule, entryPoint: 'main' },
    fragmentStage: { module: fragmentModule, entryPoint: 'main' },
    layout: t.device.createPipelineLayout({ bindGroupLayouts: [] }),
    primitiveTopology: 'triangle-list',
    colorStates: [{ format: 'r8unorm' }],
  });

  // encode pass and submit
  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        attachment: renderTexture.createView(),
        storeOp: t.params.storeOp,
        loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
      },
    ],
  });
  pass.setPipeline(renderPipeline);
  pass.draw(3, 1, 0, 0);
  pass.endPass();
  const dstBuffer = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC,
  });
  encoder.copyTextureToBuffer(
    { texture: renderTexture },
    { buffer: dstBuffer, rowPitch: 256, imageHeight: 1 },
    { width: 1, height: 1, depth: 1 }
  );
  t.device.defaultQueue.submit([encoder.finish()]);

  // expect the buffer to be clear
  const expectedContent = new Uint32Array([t.params._expected]);
  t.expectContents(dstBuffer, expectedContent);
}).params([
  { storeOp: 'store', _expected: 255 }, //
  { storeOp: 'clear', _expected: 0 },
]);
