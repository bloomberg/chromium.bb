import { GPUTest } from '../../../../gpu_test.js';
import { CommandBufferMaker } from '../../validation_test.js';

export function createQuerySetWithType(
  t: GPUTest,
  type: GPUQueryType,
  count: GPUSize32
): GPUQuerySet {
  return t.device.createQuerySet({
    type,
    count,
    pipelineStatistics:
      type === 'pipeline-statistics' ? (['clipper-invocations'] as const) : ([] as const),
  });
}

export function beginRenderPassWithQuerySet(
  t: GPUTest,
  encoder: GPUCommandEncoder,
  querySet?: GPUQuerySet
): GPURenderPassEncoder {
  const view = t.device
    .createTexture({
      format: 'rgba8unorm' as const,
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    })
    .createView();
  return encoder.beginRenderPass({
    colorAttachments: [
      {
        view,
        loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        storeOp: 'store',
      },
    ],
    occlusionQuerySet: querySet,
  });
}

export function createRenderEncoderWithQuerySet(
  t: GPUTest,
  querySet?: GPUQuerySet
): CommandBufferMaker<'render pass'> {
  const commandEncoder = t.device.createCommandEncoder();
  const encoder = beginRenderPassWithQuerySet(t, commandEncoder, querySet);
  return {
    encoder,
    finish: () => {
      encoder.endPass();
      return commandEncoder.finish();
    },
  } as const;
}
