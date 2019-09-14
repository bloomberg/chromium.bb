export const description = `
setBindGroup validation tests.
`;

import { TestGroup, pcombine, poptions } from '../../../framework/index.js';
import GLSL from '../../../tools/glsl.macro.js';

import { ValidationTest } from './validation_test.js';

export class F extends ValidationTest {
  makeAttachmentTexture(): GPUTexture {
    return this.device.createTexture({
      format: 'rgba8unorm',
      size: { width: 16, height: 16, depth: 1 },
      usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
    });
  }

  testComputePass(
    bindGroup: GPUBindGroup,
    pipelineLayout: GPUPipelineLayout,
    dynamicOffsets: number[]
  ): void {
    const module = this.device.createShaderModule({
      code: GLSL(
        'compute',
        `#version 450
          layout(std140, set = 0, binding = 0) uniform UniformBuffer {
              float value1;
          };
          layout(std140, set = 0, binding = 1) buffer StorageBuffer {
              float value2;
          };
          void main() {
          }
        `
      ),
    });

    const pipeline = this.device.createComputePipeline({
      computeStage: { module, entryPoint: 'main' },
      layout: pipelineLayout,
    });

    const encoder = this.device.createCommandEncoder();
    const computePass = encoder.beginComputePass();
    computePass.setPipeline(pipeline);
    computePass.setBindGroup(0, bindGroup, dynamicOffsets);
    computePass.dispatch(1, 1, 1);
    computePass.endPass();

    encoder.finish();
  }

  testRender(
    bindGroup: GPUBindGroup,
    pipelineLayout: GPUPipelineLayout,
    dynamicOffsets: number[],
    encoder: GPURenderPassEncoder | GPURenderBundleEncoder
  ): void {
    const vertexModule = this.device.createShaderModule({
      code: GLSL(
        'vertex',
        `#version 450
          void main() {
            gl_Position = vec4(0);
          }
        `
      ),
    });

    const fragmentModule = this.device.createShaderModule({
      code: GLSL(
        'fragment',
        `#version 450
          layout(std140, set = 0, binding = 0) uniform UniformBuffer {
              vec2 value1;
          };
          layout(std140, set = 0, binding = 1) buffer StorageBuffer {
              vec2 value2;
          };
          void main() {
          }
        `
      ),
    });

    const pipeline = this.device.createRenderPipeline({
      vertexStage: { module: vertexModule, entryPoint: 'main' },
      fragmentStage: { module: fragmentModule, entryPoint: 'main' },
      layout: pipelineLayout,
      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
    });

    encoder.setPipeline(pipeline);
    encoder.setBindGroup(0, bindGroup, dynamicOffsets);
    encoder.draw(3, 1, 0, 0);
  }

  testRenderPass(
    bindGroup: GPUBindGroup,
    pipelineLayout: GPUPipelineLayout,
    dynamicOffsets: number[]
  ): void {
    const encoder = this.device.createCommandEncoder();
    const renderPass = encoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: this.makeAttachmentTexture().createView(),
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    });
    this.testRender(bindGroup, pipelineLayout, dynamicOffsets, renderPass);
    renderPass.endPass();
    encoder.finish();
  }

  testRenderBundle(
    bindGroup: GPUBindGroup,
    pipelineLayout: GPUPipelineLayout,
    dynamicOffsets: number[]
  ): void {
    const encoder = this.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
    });
    this.testRender(bindGroup, pipelineLayout, dynamicOffsets, encoder);
    encoder.finish();
  }
}

export const g = new TestGroup(F);

g.test('dynamic offsets passed but not expected/compute pass', async t => {
  const bindGroupLayout = t.device.createBindGroupLayout({ bindings: [] });
  const bindGroup = t.device.createBindGroup({ layout: bindGroupLayout, bindings: [] });

  const { type } = t.params;
  const dynamicOffsets = [0];

  await t.expectValidationError(() => {
    if (type === 'compute') {
      const encoder = t.device.createCommandEncoder();
      const computePass = encoder.beginComputePass();
      computePass.setBindGroup(0, bindGroup, dynamicOffsets);
      computePass.endPass();
      encoder.finish();
    } else if (type === 'renderpass') {
      const encoder = t.device.createCommandEncoder();
      const renderPass = encoder.beginRenderPass({
        colorAttachments: [
          {
            attachment: t.makeAttachmentTexture().createView(),
            loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
          },
        ],
      });
      renderPass.setBindGroup(0, bindGroup, dynamicOffsets);
      renderPass.endPass();
      encoder.finish();
    } else if (type === 'renderbundle') {
      const encoder = t.device.createRenderBundleEncoder({
        colorFormats: ['rgba8unorm'],
      });
      encoder.setBindGroup(0, bindGroup, dynamicOffsets);
      encoder.finish();
    } else {
      t.fail();
    }
  });
}).params(poptions('type', ['compute', 'renderpass', 'renderbundle']));

g.test('dynamic offsets match expectations in pass encoder', async t => {
  // Dynamic buffer offsets require offset to be divisible by 256
  const MIN_DYNAMIC_BUFFER_OFFSET_ALIGNMENT = 256;
  const BINDING_SIZE = 9;

  const bindGroupLayout = t.device.createBindGroupLayout({
    bindings: [
      {
        binding: 0,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        type: 'uniform-buffer',
        dynamic: true,
      },
      {
        binding: 1,
        visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
        type: 'storage-buffer',
        dynamic: true,
      },
    ],
  });

  const uniformBuffer = t.device.createBuffer({
    size: 2 * MIN_DYNAMIC_BUFFER_OFFSET_ALIGNMENT + 8,
    usage: GPUBufferUsage.UNIFORM,
  });

  const storageBuffer = t.device.createBuffer({
    size: 2 * MIN_DYNAMIC_BUFFER_OFFSET_ALIGNMENT + 8,
    usage: GPUBufferUsage.STORAGE,
  });

  const bindGroup = t.device.createBindGroup({
    layout: bindGroupLayout,
    bindings: [
      {
        binding: 0,
        resource: {
          buffer: uniformBuffer,
          size: BINDING_SIZE,
        },
      },
      {
        binding: 1,
        resource: {
          buffer: storageBuffer,
          size: BINDING_SIZE,
        },
      },
    ],
  });

  const pipelineLayout = t.device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  const { type, dynamicOffsets, success } = t.params;

  await t.expectValidationError(() => {
    if (type === 'compute') {
      t.testComputePass(bindGroup, pipelineLayout, dynamicOffsets);
    } else if (type === 'renderpass') {
      t.testRenderPass(bindGroup, pipelineLayout, dynamicOffsets);
    } else if (type === 'renderbundle') {
      t.testRenderBundle(bindGroup, pipelineLayout, dynamicOffsets);
    } else {
      t.fail();
    }
    t.testComputePass(bindGroup, pipelineLayout, dynamicOffsets);
  }, !success);
}).params(
  pcombine([
    poptions('type', ['compute', 'renderpass', 'renderbundle']),
    [
      { dynamicOffsets: [256, 0], success: true }, // Dynamic offsets aligned
      { dynamicOffsets: [1, 2], success: false }, // Dynamic offsets not aligned

      // Wrong number of dynamic offsets
      { dynamicOffsets: [256, 0, 0], success: false },
      { dynamicOffsets: [256], success: false },
      { dynamicOffsets: [], success: false },

      // Dynamic uniform buffer out of bounds because of binding size
      { dynamicOffsets: [512, 0], success: false },
      { dynamicOffsets: [1024, 0], success: false },
      { dynamicOffsets: [Number.MAX_SAFE_INTEGER, 0], success: false },

      // Dynamic storage buffer out of bounds because of binding size
      { dynamicOffsets: [0, 512], success: false },
      { dynamicOffsets: [0, 1024], success: false },
      { dynamicOffsets: [0, Number.MAX_SAFE_INTEGER], success: false },
    ],
  ])
);

// TODO: test error bind group
