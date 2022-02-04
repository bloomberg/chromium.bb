export const description = `API Operation Tests for RenderPass StoreOp.

  Test Coverage:

  - Tests that color and depth-stencil store operations {'discard', 'store'} work correctly for a
    render pass with both a color attachment and depth-stencil attachment.
      TODO: use depth24plus-stencil8

  - Tests that store operations {'discard', 'store'} work correctly for a render pass with multiple
    color attachments.
      TODO: test with more interesting loadOp values

  - Tests that store operations {'discard', 'store'} work correctly for a render pass with a color
    attachment for:
      - All renderable color formats
      - mip level set to {'0', mip > '0'}
      - array layer set to {'0', layer > '1'} for 2D textures
      TODO: depth slice set to {'0', slice > '0'} for 3D textures

  - Tests that store operations {'discard', 'store'} work correctly for a render pass with a
    depth-stencil attachment for:
      - All renderable depth-stencil formats
      - mip level set to {'0', mip > '0'}
      - array layer set to {'0', layer > '1'} for 2D textures
      TODO: test depth24plus and depth24plus-stencil8 formats
      TODO: test that depth and stencil aspects are set seperately
      TODO: depth slice set to {'0', slice > '0'} for 3D textures
      TODO: test with more interesting loadOp values`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  kTextureFormatInfo,
  kEncodableTextureFormats,
  kSizedDepthStencilFormats,
} from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { PerTexelComponent } from '../../../util/texture/texel_data.js';

// Test with a zero and non-zero mip.
const kMipLevel: number[] = [0, 1];
const kMipLevelCount = 2;

// Test with different numbers of color attachments.
type NumColorAttachments = 1 | 2 | 3 | 4;
const kNumColorAttachments: NumColorAttachments[] = [1, 2, 3, 4];

// Test with a zero and non-zero array layer.
const kArrayLayers: number[] = [0, 1];

const kStoreOps: GPUStoreOp[] = ['discard', 'store'];

const kHeight = 2;
const kWidth = 2;

export const g = makeTestGroup(GPUTest);

// Tests a render pass with both a color and depth stencil attachment to ensure store operations are
// set independently.
g.test('render_pass_store_op,color_attachment_with_depth_stencil_attachment')
  .params(u =>
    u //
      .combine('colorStoreOperation', kStoreOps)
      .combine('depthStencilStoreOperation', kStoreOps)
  )
  .fn(t => {
    // Create a basic color attachment.
    const kColorFormat: GPUTextureFormat = 'rgba8unorm';
    const colorAttachment = t.device.createTexture({
      format: kColorFormat,
      size: { width: kWidth, height: kHeight, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const colorAttachmentView = colorAttachment.createView();

    // Create a basic depth/stencil attachment.
    const kDepthStencilFormat: GPUTextureFormat = 'depth32float';
    const depthStencilAttachment = t.device.createTexture({
      format: kDepthStencilFormat,
      size: { width: kWidth, height: kHeight, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Color load operation will clear to {1.0, 1.0, 1.0, 1.0}.
    // Depth & stencil load operations will clear to 1.0.
    // Store operations are determined by test the params.
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: colorAttachmentView,
          loadValue: { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
          storeOp: t.params.colorStoreOperation,
        },
      ],
      depthStencilAttachment: {
        view: depthStencilAttachment.createView(),
        depthLoadValue: 1.0,
        depthStoreOp: t.params.depthStencilStoreOperation,
        stencilLoadValue: 1.0,
        stencilStoreOp: t.params.depthStencilStoreOperation,
      },
    });
    pass.endPass();

    t.device.queue.submit([encoder.finish()]);

    // Check that the correct store operation occurred.
    let expectedColorValue: PerTexelComponent<number> = {};
    if (t.params.colorStoreOperation === 'discard') {
      // If colorStoreOp was clear, the texture should now contain {0.0, 0.0, 0.0, 0.0}.
      expectedColorValue = { R: 0.0, G: 0.0, B: 0.0, A: 0.0 };
    } else if (t.params.colorStoreOperation === 'store') {
      // If colorStoreOP was store, the texture should still contain {1.0, 1.0, 1.0, 1.0}.
      expectedColorValue = { R: 1.0, G: 1.0, B: 1.0, A: 1.0 };
    }
    t.expectSingleColor(colorAttachment, kColorFormat, {
      size: [kHeight, kWidth, 1],
      exp: expectedColorValue,
    });

    // Check that the correct store operation occurred.
    let expectedDepthValue: PerTexelComponent<number> = {};
    if (t.params.depthStencilStoreOperation === 'discard') {
      // If depthStencilStoreOperation was clear, the texture's depth component should be 0.0, and
      // the stencil component should be 0.0.
      expectedDepthValue = { Depth: 0.0 };
    } else if (t.params.depthStencilStoreOperation === 'store') {
      // If depthStencilStoreOperation was store, the texture's depth component should be 1.0, and
      // the stencil component should be 1.0.
      expectedDepthValue = { Depth: 1.0 };
    }
    t.expectSingleColor(depthStencilAttachment, kDepthStencilFormat, {
      size: [kHeight, kWidth, 1],
      exp: expectedDepthValue,
    });
  });

// Tests that render pass color attachment store operations work correctly for all renderable color
// formats, mip levels and array layers.
g.test('render_pass_store_op,color_attachment_only')
  .params(u =>
    u
      .combine('colorFormat', kEncodableTextureFormats)
      // Filter out any non-renderable formats
      .filter(({ colorFormat }) => {
        const info = kTextureFormatInfo[colorFormat];
        return info.color && info.renderable;
      })
      .combine('storeOperation', kStoreOps)
      .beginSubcases()
      .combine('mipLevel', kMipLevel)
      .combine('arrayLayer', kArrayLayers)
  )
  .fn(t => {
    const colorAttachment = t.device.createTexture({
      format: t.params.colorFormat,
      size: { width: kWidth, height: kHeight, depthOrArrayLayers: t.params.arrayLayer + 1 },
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const colorViewDesc: GPUTextureViewDescriptor = {
      baseArrayLayer: t.params.arrayLayer,
      baseMipLevel: t.params.mipLevel,
      mipLevelCount: 1,
      arrayLayerCount: 1,
    };

    const colorAttachmentView = colorAttachment.createView(colorViewDesc);

    // Color load operation will clear to {1.0, 0.0, 0.0, 1.0}.
    // Color store operation is determined by the test params.
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: colorAttachmentView,
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
          storeOp: t.params.storeOperation,
        },
      ],
    });
    pass.endPass();
    t.device.queue.submit([encoder.finish()]);

    // Check that the correct store operation occurred.
    let expectedValue: PerTexelComponent<number> = {};
    if (t.params.storeOperation === 'discard') {
      // If colorStoreOp was clear, the texture should now contain {0.0, 0.0, 0.0, 0.0}.
      expectedValue = { R: 0.0, G: 0.0, B: 0.0, A: 0.0 };
    } else if (t.params.storeOperation === 'store') {
      // If colorStoreOP was store, the texture should still contain {1.0, 0.0, 0.0, 1.0}.
      expectedValue = { R: 1.0, G: 0.0, B: 0.0, A: 1.0 };
    }

    t.expectSingleColor(colorAttachment, t.params.colorFormat, {
      size: [kHeight, kWidth, 1],
      slice: t.params.arrayLayer,
      exp: expectedValue,
      layout: { mipLevel: t.params.mipLevel },
    });
  });

// Test with multiple color attachments to ensure each attachment's storeOp is set independently.
g.test('render_pass_store_op,multiple_color_attachments')
  .params(u =>
    u
      .combine('storeOperation1', kStoreOps)
      .combine('storeOperation2', kStoreOps)
      .beginSubcases()
      .combine('colorAttachments', kNumColorAttachments)
  )
  .fn(t => {
    const kColorFormat: GPUTextureFormat = 'rgba8unorm';
    const colorAttachments: GPUTexture[] = [];

    for (let i = 0; i < t.params.colorAttachments; i++) {
      colorAttachments.push(
        t.device.createTexture({
          format: kColorFormat,
          size: { width: kWidth, height: kHeight, depthOrArrayLayers: 1 },
          usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
        })
      );
    }

    // Color load operation will clear to {1.0, 1.0, 1.0, 1.0}
    // Color store operation is determined by test params. Use storeOperation1 for even numbered
    // attachments and storeOperation2 for odd numbered attachments.
    const renderPassColorAttachments: GPURenderPassColorAttachment[] = [];
    for (let i = 0; i < t.params.colorAttachments; i++) {
      renderPassColorAttachments.push({
        view: colorAttachments[i].createView(),
        loadValue: { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
        storeOp: i % 2 === 0 ? t.params.storeOperation1 : t.params.storeOperation2,
      });
    }

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: renderPassColorAttachments,
    });
    pass.endPass();
    t.device.queue.submit([encoder.finish()]);

    // Check that the correct store operation occurred.
    let expectedValue: PerTexelComponent<number> = {};
    for (let i = 0; i < t.params.colorAttachments; i++) {
      if (renderPassColorAttachments[i].storeOp === 'discard') {
        // If colorStoreOp was clear, the texture should now contain {0.0, 0.0, 0.0, 0.0}.
        expectedValue = { R: 0.0, G: 0.0, B: 0.0, A: 0.0 };
      } else if (renderPassColorAttachments[i].storeOp === 'store') {
        // If colorStoreOP was store, the texture should still contain {1.0, 1.0, 1.0, 1.0}.
        expectedValue = { R: 1.0, G: 1.0, B: 1.0, A: 1.0 };
      }
      t.expectSingleColor(colorAttachments[i], kColorFormat, {
        size: [kHeight, kWidth, 1],
        exp: expectedValue,
      });
    }
  });

g.test('render_pass_store_op,depth_stencil_attachment_only')
  .desc(
    `
Tests that render pass depth stencil store operations work correctly for all renderable color
formats, mip levels and array layers.

- x= all (sized) depth stencil formats, all store ops, multiple mip levels, multiple array layers

TODO: Also test unsized depth/stencil formats [1]
  `
  )
  .params(u =>
    u
      .combine('depthStencilFormat', kSizedDepthStencilFormats) // [1]
      .combine('storeOperation', kStoreOps)
      .beginSubcases()
      .combine('mipLevel', kMipLevel)
      .combine('arrayLayer', kArrayLayers)
  )
  .fn(t => {
    const depthStencilAttachment = t.device.createTexture({
      format: t.params.depthStencilFormat,
      size: { width: kWidth, height: kHeight, depthOrArrayLayers: t.params.arrayLayer + 1 },
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const depthStencilViewDesc: GPUTextureViewDescriptor = {
      baseArrayLayer: t.params.arrayLayer,
      baseMipLevel: t.params.mipLevel,
      mipLevelCount: 1,
      arrayLayerCount: 1,
    };

    const depthStencilAttachmentView = depthStencilAttachment.createView(depthStencilViewDesc);

    // Depth-stencil load operation will clear to depth = 1.0, stencil = 1.0.
    // Depth-stencil store operate is determined by test params.
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [],
      depthStencilAttachment: {
        view: depthStencilAttachmentView,
        depthLoadValue: 1.0,
        depthStoreOp: t.params.storeOperation,
        stencilLoadValue: 1.0,
        stencilStoreOp: t.params.storeOperation,
      },
    });
    pass.endPass();
    t.device.queue.submit([encoder.finish()]);

    let expectedValue: PerTexelComponent<number> = {};
    if (t.params.storeOperation === 'discard') {
      // If depthStencilStoreOperation was clear, the texture's depth component should be 0.0,
      expectedValue = { Depth: 0.0 };
    } else if (t.params.storeOperation === 'store') {
      // If depthStencilStoreOperation was store, the texture's depth component should be 1.0,
      expectedValue = { Depth: 1.0 };
    }

    t.expectSingleColor(depthStencilAttachment, t.params.depthStencilFormat, {
      size: [kHeight, kWidth, 1],
      slice: t.params.arrayLayer,
      exp: expectedValue,
      layout: { mipLevel: t.params.mipLevel },
    });
  });
