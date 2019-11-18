export const description = `
Basic command buffer compute tests.
`;

import { TestGroup } from '../../../../framework/index.js';
import GLSL from '../../../../tools/glsl.macro.js';
import { GPUTest } from '../../gpu_test.js';

export const g = new TestGroup(GPUTest);

g.test('memcpy', async t => {
  const data = new Uint32Array([0x01020304]);
  const src = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
  });
  const dst = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
  });
  src.setSubData(0, data);

  const bgl = t.device.createBindGroupLayout({
    bindings: [
      { binding: 0, visibility: 4, type: 'storage-buffer' },
      { binding: 1, visibility: 4, type: 'storage-buffer' },
    ],
  });
  const bg = t.device.createBindGroup({
    bindings: [
      { binding: 0, resource: { buffer: src, offset: 0, size: 4 } },
      { binding: 1, resource: { buffer: dst, offset: 0, size: 4 } },
    ],
    layout: bgl,
  });

  const module = t.createShaderModule({
    code: GLSL(
      'compute',
      `#version 310 es
        layout(std140, set = 0, binding = 0) buffer Src {
          int value;
        } src;
        layout(std140, set = 0, binding = 1) buffer Dst {
          int value;
        } dst;

        void main() {
          dst.value = src.value;
        }
      `
    ),
  });
  const pl = t.device.createPipelineLayout({ bindGroupLayouts: [bgl] });
  const pipeline = t.device.createComputePipeline({
    computeStage: { module, entryPoint: 'main' },
    layout: pl,
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatch(1, 1, 1);
  pass.endPass();
  t.device.defaultQueue.submit([encoder.finish()]);

  t.expectContents(dst, data);
});
