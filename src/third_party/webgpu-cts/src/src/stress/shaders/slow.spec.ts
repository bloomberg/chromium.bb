export const description = `
Stress tests covering robustness in the presence of slow shaders.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('compute')
  .desc(`Tests execution of compute passes with very long-running dispatch operations.`)
  .fn(async t => {
    const kDispatchSize = 1000;
    const data = new Uint32Array(kDispatchSize);
    const buffer = t.makeBufferWithContents(data, GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC);
    const module = t.device.createShaderModule({
      code: `
        struct Buffer { data: array<u32>; };
        @group(0) @binding(0) var<storage, read_write> buffer: Buffer;
        @stage(compute) @workgroup_size(1) fn main(
            @builtin(global_invocation_id) id: vec3<u32>) {
          loop {
            if (buffer.data[id.x] == 1000000u) {
              break;
            }
            buffer.data[id.x] = buffer.data[id.x] + 1u;
          }
        }
      `,
    });
    const pipeline = t.device.createComputePipeline({ compute: { module, entryPoint: 'main' } });
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    const bindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer } }],
    });
    pass.setBindGroup(0, bindGroup);
    pass.dispatch(kDispatchSize);
    pass.end();
    t.device.queue.submit([encoder.finish()]);
    t.expectGPUBufferValuesEqual(buffer, new Uint32Array(new Array(kDispatchSize).fill(1000000)));
  });

g.test('vertex')
  .desc(`Tests execution of render passes with a very long-running vertex stage.`)
  .fn(async t => {
    const module = t.device.createShaderModule({
      code: `
        struct Data { counter: u32; increment: u32; };
        @group(0) @binding(0) var<uniform> data: Data;
        @stage(vertex) fn vmain() -> @builtin(position) vec4<f32> {
          var counter: u32 = data.counter;
          loop {
            counter = counter + data.increment;
            if (counter % 50000000u == 0u) {
              break;
            }
          }
          return vec4<f32>(1.0, 1.0, 0.0, f32(counter));
        }
        @stage(fragment) fn fmain() -> @location(0) vec4<f32> {
          return vec4<f32>(1.0, 1.0, 0.0, 1.0);
        }
      `,
    });

    const pipeline = t.device.createRenderPipeline({
      vertex: { module, entryPoint: 'vmain', buffers: [] },
      primitive: { topology: 'point-list' },
      fragment: {
        targets: [{ format: 'rgba8unorm' }],
        module,
        entryPoint: 'fmain',
      },
    });
    const uniforms = t.makeBufferWithContents(new Uint32Array([0, 1]), GPUBufferUsage.UNIFORM);
    const bindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: { buffer: uniforms },
        },
      ],
    });
    const renderTarget = t.device.createTexture({
      size: [3, 3],
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      format: 'rgba8unorm',
    });
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: renderTarget.createView(),
          clearValue: [0, 0, 0, 0],
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(1);
    pass.end();
    t.device.queue.submit([encoder.finish()]);
    t.expectSinglePixelIn2DTexture(
      renderTarget,
      'rgba8unorm',
      { x: 1, y: 1 },
      {
        exp: new Uint8Array([255, 255, 0, 255]),
      }
    );
  });

g.test('fragment')
  .desc(`Tests execution of render passes with a very long-running fragment stage.`)
  .fn(async t => {
    const module = t.device.createShaderModule({
      code: `
        struct Data { counter: u32; increment: u32; };
        @group(0) @binding(0) var<uniform> data: Data;
        @stage(vertex) fn vmain() -> @builtin(position) vec4<f32> {
          return vec4<f32>(0.0, 0.0, 0.0, 1.0);
        }
        @stage(fragment) fn fmain() -> @location(0) vec4<f32> {
          var counter: u32 = data.counter;
          loop {
            counter = counter + data.increment;
            if (counter % 50000000u == 0u) {
              break;
            }
          }
          return vec4<f32>(1.0, 1.0, 1.0 / f32(counter), 1.0);
        }
      `,
    });

    const pipeline = t.device.createRenderPipeline({
      vertex: { module, entryPoint: 'vmain', buffers: [] },
      primitive: { topology: 'point-list' },
      fragment: {
        targets: [{ format: 'rgba8unorm' }],
        module,
        entryPoint: 'fmain',
      },
    });
    const uniforms = t.makeBufferWithContents(new Uint32Array([0, 1]), GPUBufferUsage.UNIFORM);
    const bindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: { buffer: uniforms },
        },
      ],
    });
    const renderTarget = t.device.createTexture({
      size: [3, 3],
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      format: 'rgba8unorm',
    });
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: renderTarget.createView(),
          clearValue: [0, 0, 0, 0],
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(1);
    pass.end();
    t.device.queue.submit([encoder.finish()]);
    t.expectSinglePixelIn2DTexture(
      renderTarget,
      'rgba8unorm',
      { x: 1, y: 1 },
      {
        exp: new Uint8Array([255, 255, 0, 255]),
      }
    );
  });
