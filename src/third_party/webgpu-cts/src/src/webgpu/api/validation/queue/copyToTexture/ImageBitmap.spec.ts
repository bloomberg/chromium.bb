export const description = `
copyImageBitmapToTexture Validation Tests in Queue.
TODO: Should this be the same file as, or next to, web_platform/copyImageBitmapToTexture.spec.ts?

TODO: Split this test plan per-test.

Test Plan:
- For source.imageBitmap:
  - imageBitmap generated from ImageData:
    - Check that an error is generated when imageBitmap is closed.

- For destination.texture:
  - For 2d destination textures:
    - Check that an error is generated when texture is in destroyed state.
    - Check that an error is generated when texture is an error texture.
    - Check that an error is generated when texture is created without usage COPY_DST.
    - Check that an error is generated when sample count is not 1.
    - Check that an error is generated when mipLevel is too large.
    - Check that an error is generated when texture format is not valid.

- For copySize:
  - No-op copy shouldn't throw any exception or return any validation error.
  - Check that an error is generated when destination.texture.origin + copySize is too large.

TODO: copying into slices of 2d array textures. 1d and 3d as well if they're not invalid.
`;

import { poptions, params, pbool } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import {
  kAllTextureFormatInfo,
  kAllTextureFormats,
  kTextureUsages,
} from '../../../../capability_info.js';
import { ValidationTest } from '../../validation_test.js';

const kDefaultBytesPerPixel = 4; // using 'bgra8unorm' or 'rgba8unorm'
const kDefaultWidth = 32;
const kDefaultHeight = 32;
const kDefaultDepth = 1;
const kDefaultMipLevelCount = 6;

// From spec
const kValidTextureFormatsForCopyIB2T = [
  'rgba8unorm',
  'rgba8unorm-srgb',
  'bgra8unorm',
  'bgra8unorm-srgb',
  'rgb10a2unorm',
  'rgba16float',
  'rgba32float',
  'rg8unorm',
  'rg16float',
];

function computeMipMapSize(width: number, height: number, mipLevel: number) {
  return {
    mipWidth: Math.max(width >> mipLevel, 1),
    mipHeight: Math.max(height >> mipLevel, 1),
  };
}

interface WithMipLevel {
  mipLevel: number;
}

interface WithDstOriginMipLevel extends WithMipLevel {
  dstOrigin: Required<GPUOrigin3DDict>;
}

// Helper function to generate copySize for src OOB test
function generateCopySizeForSrcOOB({ srcOrigin }: { srcOrigin: Required<GPUOrigin2DDict> }) {
  // OOB origin fails even with no-op copy.
  if (srcOrigin.x > kDefaultWidth || srcOrigin.y > kDefaultHeight) {
    return poptions('copySize', [{ width: 0, height: 0, depthOrArrayLayers: 0 }]);
  }

  const justFitCopySize = {
    width: kDefaultWidth - srcOrigin.x,
    height: kDefaultHeight - srcOrigin.y,
    depthOrArrayLayers: 1,
  };

  return poptions('copySize', [
    justFitCopySize, // correct size, maybe no-op copy.
    { width: justFitCopySize.width + 1, height: justFitCopySize.height, depthOrArrayLayers: 1 }, // OOB in width
    { width: justFitCopySize.width, height: justFitCopySize.height + 1, depthOrArrayLayers: 1 }, // OOB in height
    { width: justFitCopySize.width, height: justFitCopySize.height, depthOrArrayLayers: 2 }, // OOB in depthOrArrayLayers
  ]);
}

// Helper function to generate dst origin value based on mipLevel.
function generateDstOriginValue({ mipLevel }: WithMipLevel) {
  const origin = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

  return poptions('dstOrigin', [
    { x: 0, y: 0, z: 0 },
    { x: origin.mipWidth - 1, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight - 1, z: 0 },
    { x: origin.mipWidth, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight, z: 0 },
    { x: 0, y: 0, z: kDefaultDepth },
    { x: origin.mipWidth + 1, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight + 1, z: 0 },
    { x: 0, y: 0, z: kDefaultDepth + 1 },
  ]);
}

// Helper function to generate copySize for dst OOB test
function generateCopySizeForDstOOB({ mipLevel, dstOrigin }: WithDstOriginMipLevel) {
  const dstMipMapSize = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

  // OOB origin fails even with no-op copy.
  if (
    dstOrigin.x > dstMipMapSize.mipWidth ||
    dstOrigin.y > dstMipMapSize.mipHeight ||
    dstOrigin.z > kDefaultDepth
  ) {
    return poptions('copySize', [{ width: 0, height: 0, depthOrArrayLayers: 0 }]);
  }

  const justFitCopySize = {
    width: dstMipMapSize.mipWidth - dstOrigin.x,
    height: dstMipMapSize.mipHeight - dstOrigin.y,
    depthOrArrayLayers: kDefaultDepth - dstOrigin.z,
  };

  return poptions('copySize', [
    justFitCopySize,
    {
      width: justFitCopySize.width + 1,
      height: justFitCopySize.height,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers,
    }, // OOB in width
    {
      width: justFitCopySize.width,
      height: justFitCopySize.height + 1,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers,
    }, // OOB in height
    {
      width: justFitCopySize.width,
      height: justFitCopySize.height,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers + 1,
    }, // OOB in depthOrArrayLayers
  ]);
}

class CopyImageBitmapToTextureTest extends ValidationTest {
  getImageData(width: number, height: number): ImageData {
    const pixelSize = kDefaultBytesPerPixel * width * height;
    const imagePixels = new Uint8ClampedArray(pixelSize);
    return new ImageData(imagePixels, width, height);
  }

  runTest(
    imageBitmapCopyView: GPUImageCopyImageBitmap,
    textureCopyView: GPUImageCopyTexture,
    copySize: GPUExtent3D,
    validationScopeSuccess: boolean,
    exceptionName?: string
  ): void {
    // CopyImageBitmapToTexture will generate two types of errors. One is synchronous exceptions;
    // the other is asynchronous validation error scope errors.
    if (exceptionName) {
      this.shouldThrow(exceptionName, () => {
        this.device.queue.copyImageBitmapToTexture(imageBitmapCopyView, textureCopyView, copySize);
      });
    } else {
      this.expectValidationError(() => {
        this.device.queue.copyImageBitmapToTexture(imageBitmapCopyView, textureCopyView, copySize);
      }, !validationScopeSuccess);
    }
  }
}

export const g = makeTestGroup(CopyImageBitmapToTextureTest);

g.test('source_imageBitmap,state')
  .params(
    params()
      .combine(pbool('closed'))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { closed, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST,
    });

    if (closed) imageBitmap.close();

    t.runTest(
      { imageBitmap },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      closed ? 'InvalidStateError' : ''
    );
  });

g.test('destination_texture,state')
  .params(
    params()
      .combine(poptions('state', ['valid', 'invalid', 'destroyed'] as const))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { state, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.createTextureWithState(state);

    t.runTest({ imageBitmap }, { texture: dstTexture }, copySize, state === 'valid');
  });

g.test('destination_texture,usage')
  .params(
    params()
      .combine(poptions('usage', kTextureUsages))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { usage, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage,
    });

    t.runTest(
      { imageBitmap },
      { texture: dstTexture },
      copySize,
      !!(usage & GPUTextureUsage.COPY_DST)
    );
  });

g.test('destination_texture,sample_count')
  .params(
    params()
      .combine(poptions('sampleCount', [1, 4]))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { sampleCount, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      sampleCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST,
    });

    t.runTest({ imageBitmap }, { texture: dstTexture }, copySize, sampleCount === 1);
  });

g.test('destination_texture,mipLevel')
  .params(
    params()
      .combine(poptions('mipLevel', [0, kDefaultMipLevelCount - 1, kDefaultMipLevelCount]))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { mipLevel, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: kDefaultWidth, height: kDefaultHeight, depthOrArrayLayers: kDefaultDepth },
      mipLevelCount: kDefaultMipLevelCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST,
    });

    t.runTest(
      { imageBitmap },
      { texture: dstTexture, mipLevel },
      copySize,
      mipLevel < kDefaultMipLevelCount
    );
  });

g.test('destination_texture,format')
  .params(
    params()
      .combine(poptions('format', kAllTextureFormats))
      .combine(
        poptions('copySize', [
          { width: 0, height: 0, depthOrArrayLayers: 0 },
          { width: 1, height: 1, depthOrArrayLayers: 1 },
        ])
      )
  )
  .fn(async t => {
    const { format, copySize } = t.params;
    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].feature);

    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));

    // createTexture with all possible texture format may have validation error when using
    // compressed texture format.
    t.device.pushErrorScope('validation');
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.COPY_DST,
    });
    t.device.popErrorScope();

    const success = kValidTextureFormatsForCopyIB2T.includes(format);

    t.runTest(
      { imageBitmap },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      success ? '' : 'TypeError'
    );
  });

g.test('OOB,source')
  .params(
    params()
      .combine(
        poptions('srcOrigin', [
          { x: 0, y: 0 }, // origin is on top-left
          { x: kDefaultWidth - 1, y: 0 }, // x near the border
          { x: 0, y: kDefaultHeight - 1 }, // y is near the border
          { x: kDefaultWidth, y: kDefaultHeight }, // origin is on bottom-right
          { x: kDefaultWidth + 1, y: 0 }, // x is too large
          { x: 0, y: kDefaultHeight + 1 }, // y is too large
        ])
      )
      .expand(generateCopySizeForSrcOOB)
  )
  .fn(async t => {
    const { srcOrigin, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(kDefaultWidth, kDefaultHeight));
    const dstTexture = t.device.createTexture({
      size: {
        width: kDefaultWidth + 1,
        height: kDefaultHeight + 1,
        depthOrArrayLayers: kDefaultDepth,
      },
      mipLevelCount: kDefaultMipLevelCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST,
    });

    let success = true;

    if (
      srcOrigin.x + copySize.width > kDefaultWidth ||
      srcOrigin.y + copySize.height > kDefaultHeight ||
      copySize.depthOrArrayLayers > 1
    ) {
      success = false;
    }

    t.runTest({ imageBitmap, origin: srcOrigin }, { texture: dstTexture }, copySize, success);
  });

g.test('OOB,destination')
  .params(
    params()
      .combine(poptions('mipLevel', [0, 1, kDefaultMipLevelCount - 2]))
      .expand(generateDstOriginValue)
      .expand(generateCopySizeForDstOOB)
  )
  .fn(async t => {
    const { mipLevel, dstOrigin, copySize } = t.params;

    const imageBitmap = await createImageBitmap(
      t.getImageData(kDefaultWidth + 1, kDefaultHeight + 1)
    );
    const dstTexture = t.device.createTexture({
      size: {
        width: kDefaultWidth,
        height: kDefaultHeight,
        depthOrArrayLayers: kDefaultDepth,
      },
      format: 'bgra8unorm',
      mipLevelCount: kDefaultMipLevelCount,
      usage: GPUTextureUsage.COPY_DST,
    });

    let success = true;
    const dstMipMapSize = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

    if (
      copySize.depthOrArrayLayers > 1 ||
      dstOrigin.x + copySize.width > dstMipMapSize.mipWidth ||
      dstOrigin.y + copySize.height > dstMipMapSize.mipHeight ||
      dstOrigin.z + copySize.depthOrArrayLayers > kDefaultDepth
    ) {
      success = false;
    }

    t.runTest(
      { imageBitmap },
      {
        texture: dstTexture,
        mipLevel,
        origin: dstOrigin,
      },
      copySize,
      success
    );
  });
