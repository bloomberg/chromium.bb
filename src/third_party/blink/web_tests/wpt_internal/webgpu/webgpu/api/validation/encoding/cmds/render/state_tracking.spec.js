/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation tests for setVertexBuffer/setIndexBuffer state (not validation). See also operation tests.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { range } from '../../../../../../common/framework/util/util.js';
import { ValidationTest } from '../../../validation_test.js';

class F extends ValidationTest {
  getVertexBuffer() {
    return this.device.createBuffer({
      size: 256,
      usage: GPUBufferUsage.VERTEX,
    });
  }

  createRenderPipeline(bufferCount) {
    return this.device.createRenderPipeline({
      vertexStage: {
        module: this.device.createShaderModule({
          code: `
            ${range(
              bufferCount,
              i => `\n[[location(${i})]] var<in> a_position${i} : vec3<f32>;`
            ).join('')}
            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[stage(vertex)]] fn main() -> void {
              Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
              return;
            }`,
        }),

        entryPoint: 'main',
      },

      fragmentStage: {
        module: this.device.createShaderModule({
          code: `
            [[location(0)]] var<out> fragColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
              fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
              return;
            }`,
        }),

        entryPoint: 'main',
      },

      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
      vertexState: {
        vertexBuffers: [
          {
            arrayStride: 3 * 4,
            attributes: range(bufferCount, i => ({
              format: 'float3',
              offset: 0,
              shaderLocation: i,
            })),
          },
        ],
      },
    });
  }

  beginRenderPass(commandEncoder) {
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

export const g = makeTestGroup(F);

g.test('vertex_buffers_inherit_from_previous_pipeline').fn(async t => {
  const pipeline1 = t.createRenderPipeline(1);
  const pipeline2 = t.createRenderPipeline(2);

  const vertexBuffer1 = t.getVertexBuffer();
  const vertexBuffer2 = t.getVertexBuffer();

  {
    // Check failure when vertex buffer is not set
    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = t.beginRenderPass(commandEncoder);
    renderPass.setPipeline(pipeline1);
    renderPass.draw(3);
    renderPass.endPass();

    t.expectValidationError(() => {
      commandEncoder.finish();
    });
  }
  {
    // Check success when vertex buffer is inherited from previous pipeline
    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = t.beginRenderPass(commandEncoder);
    renderPass.setPipeline(pipeline2);
    renderPass.setVertexBuffer(0, vertexBuffer1);
    renderPass.setVertexBuffer(1, vertexBuffer2);
    renderPass.draw(3);
    renderPass.setPipeline(pipeline1);
    renderPass.draw(3);
    renderPass.endPass();

    commandEncoder.finish();
  }
});

g.test('vertex_buffers_do_not_inherit_between_render_passes').fn(async t => {
  const pipeline1 = t.createRenderPipeline(1);
  const pipeline2 = t.createRenderPipeline(2);

  const vertexBuffer1 = t.getVertexBuffer();
  const vertexBuffer2 = t.getVertexBuffer();

  {
    // Check success when vertex buffer is set for each render pass
    const commandEncoder = t.device.createCommandEncoder();
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline2);
      renderPass.setVertexBuffer(0, vertexBuffer1);
      renderPass.setVertexBuffer(1, vertexBuffer2);
      renderPass.draw(3);
      renderPass.endPass();
    }
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline1);
      renderPass.setVertexBuffer(0, vertexBuffer1);
      renderPass.draw(3);
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
      renderPass.setVertexBuffer(0, vertexBuffer1);
      renderPass.setVertexBuffer(1, vertexBuffer2);
      renderPass.draw(3);
      renderPass.endPass();
    }
    {
      const renderPass = t.beginRenderPass(commandEncoder);
      renderPass.setPipeline(pipeline1);
      renderPass.draw(3);
      renderPass.endPass();
    }

    t.expectValidationError(() => {
      commandEncoder.finish();
    });
  }
});
