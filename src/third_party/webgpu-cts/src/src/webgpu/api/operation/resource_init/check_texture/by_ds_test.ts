import { assert } from '../../../../../common/framework/util/util.js';
import { GPUTest } from '../../../../gpu_test.js';
import { mipSize } from '../../../../util/texture/subresource.js';
import { CheckContents } from '../texture_zero.spec.js';

function makeFullscreenVertexModule(device: GPUDevice) {
  return device.createShaderModule({
    code: `
    [[builtin(position)]] var<out> Position : vec4<f32>;
    [[builtin(vertex_index)]] var<in> VertexIndex : i32;

    [[stage(vertex)]]
    fn main() -> void {
      const pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>( 3.0,  1.0),
        vec2<f32>(-1.0,  1.0));
      Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
      return;
    }
    `,
  });
}

function getDepthTestEqualPipeline(
  t: GPUTest,
  format: GPUTextureFormat,
  sampleCount: number,
  expected: number
): GPURenderPipeline {
  return t.device.createRenderPipeline({
    vertex: {
      entryPoint: 'main',
      module: makeFullscreenVertexModule(t.device),
    },
    fragment: {
      entryPoint: 'main',
      module: t.device.createShaderModule({
        code: `
        [[builtin(frag_depth)]] var<out> FragDepth : f32;
        [[location(0)]] var<out> outSuccess : f32;

        [[stage(fragment)]]
        fn main() -> void {
          FragDepth = f32(${expected});
          outSuccess = 1.0;
          return;
        }
        `,
      }),
      targets: [{ format: 'r8unorm' }],
    },
    depthStencil: {
      format,
      depthCompare: 'equal',
    },
    primitive: { topology: 'triangle-list' },
    multisample: { count: sampleCount },
  });
}

function getStencilTestEqualPipeline(
  t: GPUTest,
  format: GPUTextureFormat,
  sampleCount: number
): GPURenderPipeline {
  return t.device.createRenderPipeline({
    vertex: {
      entryPoint: 'main',
      module: makeFullscreenVertexModule(t.device),
    },
    fragment: {
      entryPoint: 'main',
      module: t.device.createShaderModule({
        code: `
        [[location(0)]] var<out> outSuccess : f32;

        [[stage(fragment)]]
        fn main() -> void {
          outSuccess = 1.0;
          return;
        }
        `,
      }),
      targets: [{ format: 'r8unorm' }],
    },
    depthStencil: {
      format,
      stencilFront: { compare: 'equal' },
      stencilBack: { compare: 'equal' },
    },
    primitive: { topology: 'triangle-list' },
    multisample: { count: sampleCount },
  });
}

const checkContents: (type: 'depth' | 'stencil', ...args: Parameters<CheckContents>) => void = (
  type,
  t,
  params,
  texture,
  state,
  subresourceRange
) => {
  for (const viewDescriptor of t.generateTextureViewDescriptorsForRendering(
    params.aspect,
    subresourceRange
  )) {
    assert(viewDescriptor.baseMipLevel !== undefined);
    const [width, height] = mipSize([t.textureWidth, t.textureHeight], viewDescriptor.baseMipLevel);

    const renderTexture = t.device.createTexture({
      size: [width, height, 1],
      format: 'r8unorm',
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      sampleCount: params.sampleCount,
    });

    let resolveTexture = undefined;
    let resolveTarget = undefined;
    if (params.sampleCount > 1) {
      resolveTexture = t.device.createTexture({
        size: [width, height, 1],
        format: 'r8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });
      resolveTarget = resolveTexture.createView();
    }

    const commandEncoder = t.device.createCommandEncoder();
    const pass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: renderTexture.createView(),
          resolveTarget,
          loadValue: [0, 0, 0, 0],
          storeOp: 'store',
        },
      ],
      depthStencilAttachment: {
        attachment: texture.createView(viewDescriptor),
        depthStoreOp: 'store',
        depthLoadValue: 'load',
        stencilStoreOp: 'store',
        stencilLoadValue: 'load',
      },
    });

    switch (type) {
      case 'depth': {
        const expectedDepth = t.stateToTexelComponents[state].Depth;
        assert(expectedDepth !== undefined);

        pass.setPipeline(
          getDepthTestEqualPipeline(t, params.format, params.sampleCount, expectedDepth)
        );
        break;
      }

      case 'stencil': {
        const expectedStencil = t.stateToTexelComponents[state].Stencil;
        assert(expectedStencil !== undefined);

        pass.setPipeline(getStencilTestEqualPipeline(t, params.format, params.sampleCount));
        pass.setStencilReference(expectedStencil);
        break;
      }
    }

    pass.draw(3);
    pass.endPass();

    t.queue.submit([commandEncoder.finish()]);

    t.expectSingleColor(resolveTexture || renderTexture, 'r8unorm', {
      size: [width, height, 1],
      exp: { R: 1 },
    });
  }
};

export const checkContentsByDepthTest = (...args: Parameters<CheckContents>) =>
  checkContents('depth', ...args);

export const checkContentsByStencilTest = (...args: Parameters<CheckContents>) =>
  checkContents('stencil', ...args);
