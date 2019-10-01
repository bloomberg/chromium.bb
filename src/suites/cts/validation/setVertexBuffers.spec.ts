export const description = `
setVertexBuffers validation tests.
`;

import { TestGroup, range } from '../../../framework/index.js';

import { ValidationTest } from './validation_test.js';

class F extends ValidationTest {
  async init(): Promise<void> {
    await Promise.all([super.init(), this.initGLSL()]);
  }

  getVertexBuffer(): GPUBuffer {
    return this.device.createBuffer({
      size: 256,
      usage: GPUBufferUsage.VERTEX,
    });
  }

  createRenderPipeline(bufferCount: number): GPURenderPipeline {
    const descriptor: GPURenderPipelineDescriptor = {
      vertexStage: this.getVertexStage(bufferCount),
      fragmentStage: this.getFragmentStage(),
      layout: this.getPipelineLayout(),
      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
      vertexInput: {
        vertexBuffers: [
          {
            stride: 3 * 4,
            attributeSet: range(bufferCount, i => ({
              format: 'float3',
              shaderLocation: i,
            })),
          },
        ],
      },
    };
    return this.device.createRenderPipeline(descriptor);
  }

  getVertexStage(bufferCount: number): GPUProgrammableStageDescriptor {
    const code = `
      #version 450
      ${range(bufferCount, i => `\nlayout(location = ${i}) in vec3 a_position${i};`).join('')}
      void main() {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
       }
    `;
    return {
      module: this.makeShaderModule('vertex', code),
      entryPoint: 'main',
    };
  }

  getFragmentStage(): GPUProgrammableStageDescriptor {
    const code = `
      #version 450
      layout(location = 0) out vec4 fragColor;
      void main() {
        fragColor = vec4(0.0, 1.0, 0.0, 1.0);
      }
    `;
    return {
      module: this.makeShaderModule('fragment', code),
      entryPoint: 'main',
    };
  }

  getPipelineLayout(): GPUPipelineLayout {
    return this.device.createPipelineLayout({ bindGroupLayouts: [] });
  }

  beginRenderPass(commandEncoder: GPUCommandEncoder): GPURenderPassEncoder {
    const attachmentTexture = this.device.createTexture({
      format: 'rgba8unorm',
      size: { width: 16, height: 16, depth: 1 },
      usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
    });

    return commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: attachmentTexture.createView(),
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    });
  }
}

export const g = new TestGroup(F);

g.test('vertex buffers inherit from previous pipeline', async t => {
  const pipeline1 = t.createRenderPipeline(1);
  const pipeline2 = t.createRenderPipeline(2);

  const vertexBuffers = [t.getVertexBuffer(), t.getVertexBuffer()];
  const offsets = [0, 0];

  {
    // Check failure when vertex buffer is not set
    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = t.beginRenderPass(commandEncoder);
    renderPass.setPipeline(pipeline1);
    renderPass.draw(3, 1, 0, 0);
    renderPass.endPass();

    await t.expectValidationError(() => {
      commandEncoder.finish();
    });
  }
  {
    // Check success when vertex buffer is inherited from previous pipeline
    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = t.beginRenderPass(commandEncoder);
    renderPass.setPipeline(pipeline2);
    renderPass.setVertexBuffers(0, vertexBuffers, offsets);
    renderPass.draw(3, 1, 0, 0);
    renderPass.setPipeline(pipeline1);
    renderPass.draw(3, 1, 0, 0);
    renderPass.endPass();

    commandEncoder.finish();
  }
});

g.test('vertex buffers do not inherit between render passes', async t => {
  const pipeline1 = t.createRenderPipeline(1);
  const pipeline2 = t.createRenderPipeline(2);

  const vertexBuffers = [t.getVertexBuffer(), t.getVertexBuffer()];
  const offsets = [0, 0];

  {
    // Check success when vertex buffer is set for each render pass
    const commandEncoder = t.device.createCommandEncoder();
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline2);
      renderPass.setVertexBuffers(0, vertexBuffers, offsets);
      renderPass.draw(3, 1, 0, 0);
      renderPass.endPass();
    }
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline1);
      renderPass.setVertexBuffers(0, [vertexBuffers[0]], [offsets[0]]);
      renderPass.draw(3, 1, 0, 0);
      renderPass.endPass();
    }
    commandEncoder.finish();
  }
  {
    // Check failure because vertex buffer is not inherited in second subpass
    const commandEncoder = t.device.createCommandEncoder();
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline2);
      renderPass.setVertexBuffers(0, vertexBuffers, offsets);
      renderPass.draw(3, 1, 0, 0);
      renderPass.endPass();
    }
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline1);
      renderPass.draw(3, 1, 0, 0);
      renderPass.endPass();
    }

    await t.expectValidationError(() => {
      commandEncoder.finish();
    });
  }
});
