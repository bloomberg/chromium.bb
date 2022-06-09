export const description = `
Validation for attachment compatibility between render passes, bundles, and pipelines

TODO: Add sparse color attachment compatibility test when defined by specification
`;

import { makeTestGroup } from '../../../common/framework/test_group.js';
import { range } from '../../../common/util/util.js';
import {
  kRegularTextureFormats,
  kSizedDepthStencilFormats,
  kUnsizedDepthStencilFormats,
  kTextureSampleCounts,
  kMaxColorAttachments,
  kTextureFormatInfo,
} from '../../capability_info.js';

import { ValidationTest } from './validation_test.js';

const kColorAttachmentCounts = range(kMaxColorAttachments, i => i + 1);
const kDepthStencilAttachmentFormats = [
  undefined,
  ...kSizedDepthStencilFormats,
  ...kUnsizedDepthStencilFormats,
] as const;

class F extends ValidationTest {
  createAttachmentTextureView(format: GPUTextureFormat, sampleCount?: number) {
    return this.device
      .createTexture({
        // Size matching the "arbitrary" size used by ValidationTest helpers.
        size: [16, 16, 1],
        format,
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
        sampleCount,
      })
      .createView();
  }

  createColorAttachment(
    format: GPUTextureFormat,
    sampleCount?: number
  ): GPURenderPassColorAttachment {
    return {
      view: this.createAttachmentTextureView(format, sampleCount),
      loadValue: [0, 0, 0, 0],
      storeOp: 'store',
    };
  }

  createDepthAttachment(
    format: GPUTextureFormat,
    sampleCount?: number
  ): GPURenderPassDepthStencilAttachment {
    return {
      view: this.createAttachmentTextureView(format, sampleCount),
      depthLoadValue: 0,
      depthStoreOp: 'discard',
      stencilLoadValue: 1,
      stencilStoreOp: 'discard',
    };
  }

  createRenderPipeline(
    targets: Iterable<GPUColorTargetState>,
    depthStencil?: GPUDepthStencilState,
    sampleCount?: number
  ) {
    return this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: `
            [[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
              return vec4<f32>(0.0, 0.0, 0.0, 0.0);
            }`,
        }),
        entryPoint: 'main',
      },
      fragment: {
        module: this.device.createShaderModule({
          code: '[[stage(fragment)]] fn main() {}',
        }),
        entryPoint: 'main',
        targets,
      },
      primitive: { topology: 'triangle-list' },
      depthStencil,
      multisample: { count: sampleCount },
    });
  }
}

export const g = makeTestGroup(F);

const kColorAttachmentFormats = kRegularTextureFormats.filter(format => {
  const info = kTextureFormatInfo[format];
  return info.color && info.renderable;
});

g.test('render_pass_and_bundle,color_format')
  .desc('Test that color attachment formats in render passes and bundles must match.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('passFormat', kColorAttachmentFormats)
      .combine('bundleFormat', kColorAttachmentFormats)
  )
  .fn(t => {
    const { passFormat, bundleFormat } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: [bundleFormat],
    });
    const bundle = bundleEncoder.finish();

    const { encoder, validateFinishAndSubmit } = t.createEncoder('non-pass');
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment(passFormat)],
    });
    pass.executeBundles([bundle]);
    pass.endPass();
    validateFinishAndSubmit(passFormat === bundleFormat, true);
  });

g.test('render_pass_and_bundle,color_count')
  .desc(
    `
  Test that the number of color attachments in render passes and bundles must match.

  TODO: Add sparse color attachment compatibility test when defined by specification
  `
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('passCount', kColorAttachmentCounts)
      .combine('bundleCount', kColorAttachmentCounts)
  )
  .fn(t => {
    const { passCount, bundleCount } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: range(bundleCount, () => 'rgba8unorm'),
    });
    const bundle = bundleEncoder.finish();

    const { encoder, validateFinishAndSubmit } = t.createEncoder('non-pass');
    const pass = encoder.beginRenderPass({
      colorAttachments: range(passCount, () => t.createColorAttachment('rgba8unorm')),
    });
    pass.executeBundles([bundle]);
    pass.endPass();
    validateFinishAndSubmit(passCount === bundleCount, true);
  });

g.test('render_pass_and_bundle,depth_format')
  .desc('Test that the depth attachment format in render passes and bundles must match.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('passFormat', kDepthStencilAttachmentFormats)
      .combine('bundleFormat', kDepthStencilAttachmentFormats)
  )
  .fn(async t => {
    const { passFormat, bundleFormat } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase([passFormat, bundleFormat]);

    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
      depthStencilFormat: bundleFormat,
    });
    const bundle = bundleEncoder.finish();

    const { encoder, validateFinishAndSubmit } = t.createEncoder('non-pass');
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment('rgba8unorm')],
      depthStencilAttachment:
        passFormat !== undefined ? t.createDepthAttachment(passFormat) : undefined,
    });
    pass.executeBundles([bundle]);
    pass.endPass();
    validateFinishAndSubmit(passFormat === bundleFormat, true);
  });

g.test('render_pass_and_bundle,sample_count')
  .desc('Test that the sample count in render passes and bundles must match.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('renderSampleCount', kTextureSampleCounts)
      .combine('bundleSampleCount', kTextureSampleCounts)
  )
  .fn(t => {
    const { renderSampleCount, bundleSampleCount } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
      sampleCount: bundleSampleCount,
    });
    const bundle = bundleEncoder.finish();
    const { encoder, validateFinishAndSubmit } = t.createEncoder('non-pass');
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment('rgba8unorm', renderSampleCount)],
    });
    pass.executeBundles([bundle]);
    pass.endPass();
    validateFinishAndSubmit(renderSampleCount === bundleSampleCount, true);
  });

g.test('render_pass_or_bundle_and_pipeline,color_format')
  .desc(
    `
Test that color attachment formats in render passes or bundles match the pipeline color format.
`
  )
  .params(u =>
    u
      .combine('encoderType', ['render pass', 'render bundle'] as const)
      .beginSubcases()
      .combine('encoderFormat', kColorAttachmentFormats)
      .combine('pipelineFormat', kColorAttachmentFormats)
  )
  .fn(t => {
    const { encoderType, encoderFormat, pipelineFormat } = t.params;
    const pipeline = t.createRenderPipeline([{ format: pipelineFormat, writeMask: 0 }]);

    const { encoder, validateFinishAndSubmit } = t.createEncoder(encoderType, {
      attachmentInfo: { colorFormats: [encoderFormat] },
    });
    encoder.setPipeline(pipeline);
    validateFinishAndSubmit(encoderFormat === pipelineFormat, true);
  });

g.test('render_pass_or_bundle_and_pipeline,color_count')
  .desc(
    `
Test that the number of color attachments in render passes or bundles match the pipeline color
count.

TODO: Add sparse color attachment compatibility test when defined by specification
`
  )
  .params(u =>
    u
      .combine('encoderType', ['render pass', 'render bundle'] as const)
      .beginSubcases()
      .combine('encoderCount', kColorAttachmentCounts)
      .combine('pipelineCount', kColorAttachmentCounts)
  )
  .fn(t => {
    const { encoderType, encoderCount, pipelineCount } = t.params;
    const pipeline = t.createRenderPipeline(
      range(pipelineCount, () => ({ format: 'rgba8unorm', writeMask: 0 }))
    );

    const { encoder, validateFinishAndSubmit } = t.createEncoder(encoderType, {
      attachmentInfo: { colorFormats: range(encoderCount, () => 'rgba8unorm') },
    });
    encoder.setPipeline(pipeline);
    validateFinishAndSubmit(encoderCount === pipelineCount, true);
  });

g.test('render_pass_or_bundle_and_pipeline,depth_format')
  .desc(
    `
Test that the depth attachment format in render passes or bundles match the pipeline depth format.
`
  )
  .params(u =>
    u
      .combine('encoderType', ['render pass', 'render bundle'] as const)
      .beginSubcases()
      .combine('encoderFormat', kDepthStencilAttachmentFormats)
      .combine('pipelineFormat', kDepthStencilAttachmentFormats)
  )
  .fn(async t => {
    const { encoderType, encoderFormat, pipelineFormat } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase([encoderFormat, pipelineFormat]);

    const pipeline = t.createRenderPipeline(
      [{ format: 'rgba8unorm', writeMask: 0 }],
      pipelineFormat !== undefined ? { format: pipelineFormat } : undefined
    );

    const { encoder, validateFinishAndSubmit } = t.createEncoder(encoderType, {
      attachmentInfo: { colorFormats: ['rgba8unorm'], depthStencilFormat: encoderFormat },
    });
    encoder.setPipeline(pipeline);
    validateFinishAndSubmit(encoderFormat === pipelineFormat, true);
  });

g.test('render_pass_or_bundle_and_pipeline,sample_count')
  .desc(
    `
Test that the sample count in render passes or bundles match the pipeline sample count for both color texture and depthstencil texture.
`
  )
  .params(u =>
    u
      .combine('encoderType', ['render pass', 'render bundle'] as const)
      .combine('attachmentType', ['color', 'depthstencil'] as const)
      .beginSubcases()
      .combine('encoderSampleCount', kTextureSampleCounts)
      .combine('pipelineSampleCount', kTextureSampleCounts)
  )
  .fn(t => {
    const { encoderType, attachmentType, encoderSampleCount, pipelineSampleCount } = t.params;

    const colorFormats = attachmentType === 'color' ? ['rgba8unorm' as const] : [];
    const depthStencilFormat =
      attachmentType === 'depthstencil' ? ('depth24plus-stencil8' as const) : undefined;

    const pipeline = t.createRenderPipeline(
      colorFormats.map(format => ({ format, writeMask: 0 })),
      depthStencilFormat ? { format: depthStencilFormat } : undefined,
      pipelineSampleCount
    );

    const { encoder, validateFinishAndSubmit } = t.createEncoder(encoderType, {
      attachmentInfo: { colorFormats, depthStencilFormat, sampleCount: encoderSampleCount },
    });
    encoder.setPipeline(pipeline);
    validateFinishAndSubmit(encoderSampleCount === pipelineSampleCount, true);
  });
