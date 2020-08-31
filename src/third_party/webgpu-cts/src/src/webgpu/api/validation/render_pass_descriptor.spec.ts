export const description = `
render pass descriptor validation tests.
`;

import { TestGroup } from '../../../common/framework/test_group.js';

import { ValidationTest } from './validation_test.js';

class F extends ValidationTest {
  createTexture(
    options: {
      format?: GPUTextureFormat;
      width?: number;
      height?: number;
      arrayLayerCount?: number;
      mipLevelCount?: number;
      sampleCount?: number;
      usage?: GPUTextureUsageFlags;
    } = {}
  ): GPUTexture {
    const {
      format = 'rgba8unorm',
      width = 16,
      height = 16,
      arrayLayerCount = 1,
      mipLevelCount = 1,
      sampleCount = 1,
      usage = GPUTextureUsage.OUTPUT_ATTACHMENT,
    } = options;

    return this.device.createTexture({
      size: { width, height, depth: arrayLayerCount },
      format,
      mipLevelCount,
      sampleCount,
      usage,
    });
  }

  getColorAttachment(
    texture: GPUTexture,
    textureViewDescriptor?: GPUTextureViewDescriptor
  ): GPURenderPassColorAttachmentDescriptor {
    const attachment = texture.createView(textureViewDescriptor);

    return {
      attachment,
      loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
    };
  }

  getDepthStencilAttachment(
    texture: GPUTexture,
    textureViewDescriptor?: GPUTextureViewDescriptor
  ): GPURenderPassDepthStencilAttachmentDescriptor {
    const attachment = texture.createView(textureViewDescriptor);

    return {
      attachment,
      depthLoadValue: 1.0,
      depthStoreOp: 'store',
      stencilLoadValue: 0,
      stencilStoreOp: 'store',
    };
  }

  async tryRenderPass(success: boolean, descriptor: GPURenderPassDescriptor): Promise<void> {
    const commandEncoder = this.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass(descriptor);
    renderPass.endPass();

    this.expectValidationError(() => {
      commandEncoder.finish();
    }, !success);
  }
}

export const g = new TestGroup(F);

g.test('a render pass with only one color is ok', t => {
  const colorTexture = t.createTexture({ format: 'rgba8unorm' });
  const descriptor = {
    colorAttachments: [t.getColorAttachment(colorTexture)],
  };

  t.tryRenderPass(true, descriptor);
});

g.test('a render pass with only one depth attachment is ok', t => {
  const depthStencilTexture = t.createTexture({ format: 'depth24plus-stencil8' });
  const descriptor = {
    colorAttachments: [],
    depthStencilAttachment: t.getDepthStencilAttachment(depthStencilTexture),
  };

  t.tryRenderPass(true, descriptor);
});

g.test('OOB color attachment indices are handled', async t => {
  const { colorAttachmentsCount, _success } = t.params;

  const colorAttachments = [];
  for (let i = 0; i < colorAttachmentsCount; i++) {
    const colorTexture = t.createTexture();
    colorAttachments.push(t.getColorAttachment(colorTexture));
  }

  await t.tryRenderPass(_success, { colorAttachments });
}).params([
  { colorAttachmentsCount: 4, _success: true }, // Control case
  { colorAttachmentsCount: 5, _success: false }, // Out of bounds
]);

g.test('attachments must have the same size', async t => {
  const colorTexture1x1A = t.createTexture({ width: 1, height: 1, format: 'rgba8unorm' });
  const colorTexture1x1B = t.createTexture({ width: 1, height: 1, format: 'rgba8unorm' });
  const colorTexture2x2 = t.createTexture({ width: 2, height: 2, format: 'rgba8unorm' });
  const depthStencilTexture1x1 = t.createTexture({
    width: 1,
    height: 1,
    format: 'depth24plus-stencil8',
  });
  const depthStencilTexture2x2 = t.createTexture({
    width: 2,
    height: 2,
    format: 'depth24plus-stencil8',
  });

  {
    // Control case: all the same size (1x1)
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        t.getColorAttachment(colorTexture1x1A),
        t.getColorAttachment(colorTexture1x1B),
      ],
      depthStencilAttachment: t.getDepthStencilAttachment(depthStencilTexture1x1),
    };

    t.tryRenderPass(true, descriptor);
  }
  {
    // One of the color attachments has a different size
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        t.getColorAttachment(colorTexture1x1A),
        t.getColorAttachment(colorTexture2x2),
      ],
    };

    await t.tryRenderPass(false, descriptor);
  }
  {
    // The depth stencil attachment has a different size
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        t.getColorAttachment(colorTexture1x1A),
        t.getColorAttachment(colorTexture1x1B),
      ],
      depthStencilAttachment: t.getDepthStencilAttachment(depthStencilTexture2x2),
    };

    await t.tryRenderPass(false, descriptor);
  }
});

g.test('attachments must match whether they are used for color or depth stencil', async t => {
  const colorTexture = t.createTexture({ format: 'rgba8unorm' });
  const depthStencilTexture = t.createTexture({ format: 'depth24plus-stencil8' });

  {
    // Using depth-stencil for color
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(depthStencilTexture)],
    };

    await t.tryRenderPass(false, descriptor);
  }
  {
    // Using color for depth-stencil
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [],
      depthStencilAttachment: t.getDepthStencilAttachment(colorTexture),
    };

    await t.tryRenderPass(false, descriptor);
  }
});

g.test('check layer count for color or depth stencil', async t => {
  const { arrayLayerCount, baseArrayLayer, _success } = t.params;

  const ARRAY_LAYER_COUNT = 10;
  const MIP_LEVEL_COUNT = 1;
  const COLOR_FORMAT = 'rgba8unorm';
  const DEPTH_STENCIL_FORMAT = 'depth24plus-stencil8';

  const colorTexture = t.createTexture({
    format: COLOR_FORMAT,
    width: 32,
    height: 32,
    mipLevelCount: MIP_LEVEL_COUNT,
    arrayLayerCount: ARRAY_LAYER_COUNT,
  });
  const depthStencilTexture = t.createTexture({
    format: DEPTH_STENCIL_FORMAT,
    width: 32,
    height: 32,
    mipLevelCount: MIP_LEVEL_COUNT,
    arrayLayerCount: ARRAY_LAYER_COUNT,
  });

  const baseTextureViewDescriptor: GPUTextureViewDescriptor = {
    dimension: '2d-array',
    baseArrayLayer,
    arrayLayerCount,
    baseMipLevel: 0,
    mipLevelCount: MIP_LEVEL_COUNT,
  };

  {
    // Check 2D array texture view for color
    const textureViewDescriptor: GPUTextureViewDescriptor = {
      ...baseTextureViewDescriptor,
      format: COLOR_FORMAT,
    };

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(colorTexture, textureViewDescriptor)],
    };

    await t.tryRenderPass(_success, descriptor);
  }
  {
    // Check 2D array texture view for depth stencil
    const textureViewDescriptor: GPUTextureViewDescriptor = {
      ...baseTextureViewDescriptor,
      format: DEPTH_STENCIL_FORMAT,
    };

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [],
      depthStencilAttachment: t.getDepthStencilAttachment(
        depthStencilTexture,
        textureViewDescriptor
      ),
    };

    await t.tryRenderPass(_success, descriptor);
  }
}).params([
  { arrayLayerCount: 5, baseArrayLayer: 0, _success: false }, // using 2D array texture view with arrayLayerCount > 1 is not allowed
  { arrayLayerCount: 1, baseArrayLayer: 0, _success: true }, // using 2D array texture view that covers the first layer of the texture is OK
  { arrayLayerCount: 1, baseArrayLayer: 9, _success: true }, // using 2D array texture view that covers the last layer is OK for depth stencil
]);

g.test('check mip level count for color or depth stencil', async t => {
  const { mipLevelCount, baseMipLevel, _success } = t.params;

  const ARRAY_LAYER_COUNT = 1;
  const MIP_LEVEL_COUNT = 4;
  const COLOR_FORMAT = 'rgba8unorm';
  const DEPTH_STENCIL_FORMAT = 'depth24plus-stencil8';

  const colorTexture = t.createTexture({
    format: COLOR_FORMAT,
    width: 32,
    height: 32,
    mipLevelCount: MIP_LEVEL_COUNT,
    arrayLayerCount: ARRAY_LAYER_COUNT,
  });
  const depthStencilTexture = t.createTexture({
    format: DEPTH_STENCIL_FORMAT,
    width: 32,
    height: 32,
    mipLevelCount: MIP_LEVEL_COUNT,
    arrayLayerCount: ARRAY_LAYER_COUNT,
  });

  const baseTextureViewDescriptor: GPUTextureViewDescriptor = {
    dimension: '2d',
    baseArrayLayer: 0,
    arrayLayerCount: ARRAY_LAYER_COUNT,
    baseMipLevel,
    mipLevelCount,
  };

  {
    // Check 2D texture view for color
    const textureViewDescriptor: GPUTextureViewDescriptor = {
      ...baseTextureViewDescriptor,
      format: COLOR_FORMAT,
    };

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(colorTexture, textureViewDescriptor)],
    };

    await t.tryRenderPass(_success, descriptor);
  }
  {
    // Check 2D texture view for depth stencil
    const textureViewDescriptor: GPUTextureViewDescriptor = {
      ...baseTextureViewDescriptor,
      format: DEPTH_STENCIL_FORMAT,
    };

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [],
      depthStencilAttachment: t.getDepthStencilAttachment(
        depthStencilTexture,
        textureViewDescriptor
      ),
    };

    await t.tryRenderPass(_success, descriptor);
  }
}).params([
  { mipLevelCount: 2, baseMipLevel: 0, _success: false }, // using 2D texture view with mipLevelCount > 1 is not allowed
  { mipLevelCount: 1, baseMipLevel: 0, _success: true }, // using 2D texture view that covers the first level of the texture is OK
  { mipLevelCount: 1, baseMipLevel: 3, _success: true }, // using 2D texture view that covers the last level of the texture is OK
]);

g.test('it is invalid to set resolve target if color attachment is non multisampled', async t => {
  const colorTexture = t.createTexture({ sampleCount: 1 });
  const resolveTargetTexture = t.createTexture({ sampleCount: 1 });

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [
      {
        attachment: colorTexture.createView(),
        resolveTarget: resolveTargetTexture.createView(),
        loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      },
    ],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('check the use of multisampled textures as color attachments', async t => {
  const colorTexture = t.createTexture({ sampleCount: 1 });
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });

  {
    // It is allowed to use a multisampled color attachment without setting resolve target
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(multisampledColorTexture)],
    };
    t.tryRenderPass(true, descriptor);
  }
  {
    // It is not allowed to use multiple color attachments with different sample counts
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        t.getColorAttachment(colorTexture),
        t.getColorAttachment(multisampledColorTexture),
      ],
    };

    await t.tryRenderPass(false, descriptor);
  }
});

g.test('it is invalid to use a multisampled resolve target', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const multisampledResolveTargetTexture = t.createTexture({ sampleCount: 4 });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = multisampledResolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('it is invalid to use a resolve target with array layer count greater than 1', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({ arrayLayerCount: 2 });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = resolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('it is invalid to use a resolve target with mipmap level count greater than 1', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({ mipLevelCount: 2 });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = resolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('it is invalid to use a resolve target whose usage is not output attachment', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
  });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = resolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('it is invalid to use a resolve target in error state', async t => {
  const ARRAY_LAYER_COUNT = 1;

  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({ arrayLayerCount: ARRAY_LAYER_COUNT });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  t.expectValidationError(() => {
    colorAttachment.resolveTarget = resolveTargetTexture.createView({
      dimension: '2d',
      format: 'rgba8unorm',
      baseArrayLayer: ARRAY_LAYER_COUNT + 1,
    });
  });

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('use of multisampled attachment and non multisampled resolve target is allowed', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({ sampleCount: 1 });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = resolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  t.tryRenderPass(true, descriptor);
});

g.test('use a resolve target in a format different than the attachment is not allowed', async t => {
  const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({ format: 'bgra8unorm' });

  const colorAttachment = t.getColorAttachment(multisampledColorTexture);
  colorAttachment.resolveTarget = resolveTargetTexture.createView();

  const descriptor: GPURenderPassDescriptor = {
    colorAttachments: [colorAttachment],
  };

  await t.tryRenderPass(false, descriptor);
});

g.test('size of the resolve target must be the same as the color attachment', async t => {
  const size = 16;
  const multisampledColorTexture = t.createTexture({ width: size, height: size, sampleCount: 4 });
  const resolveTargetTexture = t.createTexture({
    width: size * 2,
    height: size * 2,
    mipLevelCount: 2,
  });

  {
    const resolveTargetTextureView = resolveTargetTexture.createView({
      baseMipLevel: 0,
      mipLevelCount: 1,
    });

    const colorAttachment = t.getColorAttachment(multisampledColorTexture);
    colorAttachment.resolveTarget = resolveTargetTextureView;

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [colorAttachment],
    };

    await t.tryRenderPass(false, descriptor);
  }
  {
    const resolveTargetTextureView = resolveTargetTexture.createView({ baseMipLevel: 1 });

    const colorAttachment = t.getColorAttachment(multisampledColorTexture);
    colorAttachment.resolveTarget = resolveTargetTextureView;

    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [colorAttachment],
    };

    t.tryRenderPass(true, descriptor);
  }
});

g.test('check depth stencil attachment sample counts mismatch', async t => {
  const multisampledDepthStencilTexture = t.createTexture({
    sampleCount: 4,
    format: 'depth24plus-stencil8',
  });

  {
    // It is not allowed to use a depth stencil attachment whose sample count is different from the
    // one of the color attachment
    const depthStencilTexture = t.createTexture({
      sampleCount: 1,
      format: 'depth24plus-stencil8',
    });
    const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(multisampledColorTexture)],
      depthStencilAttachment: t.getDepthStencilAttachment(depthStencilTexture),
    };

    await t.tryRenderPass(false, descriptor);
  }
  {
    const colorTexture = t.createTexture({ sampleCount: 1 });
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(colorTexture)],
      depthStencilAttachment: t.getDepthStencilAttachment(multisampledDepthStencilTexture),
    };

    await t.tryRenderPass(false, descriptor);
  }
  {
    // It is allowed to use a multisampled depth stencil attachment whose sample count is equal to
    // the one of the color attachment.
    const multisampledColorTexture = t.createTexture({ sampleCount: 4 });
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [t.getColorAttachment(multisampledColorTexture)],
      depthStencilAttachment: t.getDepthStencilAttachment(multisampledDepthStencilTexture),
    };

    t.tryRenderPass(true, descriptor);
  }
  {
    // It is allowed to use a multisampled depth stencil attachment with no color attachment
    const descriptor: GPURenderPassDescriptor = {
      colorAttachments: [],
      depthStencilAttachment: t.getDepthStencilAttachment(multisampledDepthStencilTexture),
    };

    t.tryRenderPass(true, descriptor);
  }
});
