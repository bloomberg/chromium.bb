export const description = `
Test uninitialized textures are initialized to zero when read.

TODO:
- 1d, 3d
- test by sampling depth/stencil
- test by copying out of stencil
`;

// TODO: This is a test file, it probably shouldn't export anything.
// Everything that's exported should be moved to another file.

import { TestCaseRecorder } from '../../../../common/framework/logging/test_case_recorder.js';
import {
  params,
  poptions,
  pbool,
  ParamsBuilder,
} from '../../../../common/framework/params_builder.js';
import { CaseParams } from '../../../../common/framework/params_utils.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../../common/framework/util/util.js';
import {
  kAllTextureFormatInfo,
  kEncodableTextureFormatInfo,
  kTextureAspects,
  kUncompressedTextureFormatInfo,
  kUncompressedTextureFormats,
  EncodableTextureFormat,
  UncompressedTextureFormat,
} from '../../../capability_info.js';
import { GPUConst } from '../../../constants.js';
import { GPUTest } from '../../../gpu_test.js';
import { createTextureUploadBuffer } from '../../../util/texture/layout.js';
import { BeginEndRange, mipSize, SubresourceRange } from '../../../util/texture/subresource.js';
import { PerTexelComponent, kTexelRepresentationInfo } from '../../../util/texture/texel_data.js';

export enum UninitializeMethod {
  Creation = 'Creation', // The texture was just created. It is uninitialized.
  StoreOpClear = 'StoreOpClear', // The texture was rendered to with GPUStoreOp "clear"
}
const kUninitializeMethods = Object.keys(UninitializeMethod) as UninitializeMethod[];

export const enum ReadMethod {
  Sample = 'Sample', // The texture is sampled from
  CopyToBuffer = 'CopyToBuffer', // The texture is copied to a buffer
  CopyToTexture = 'CopyToTexture', // The texture is copied to another texture
  DepthTest = 'DepthTest', // The texture is read as a depth buffer
  StencilTest = 'StencilTest', // The texture is read as a stencil buffer
  ColorBlending = 'ColorBlending', // Read the texture by blending as a color attachment
  Storage = 'Storage', // Read the texture as a storage texture
}

// Test with these mip level counts
type MipLevels = 1 | 5;
const kMipLevelCounts: MipLevels[] = [1, 5];

// For each mip level count, define the mip ranges to leave uninitialized.
const kUninitializedMipRangesToTest: { [k in MipLevels]: BeginEndRange[] } = {
  1: [{ begin: 0, end: 1 }], // Test the only mip
  5: [
    { begin: 0, end: 2 },
    { begin: 3, end: 4 },
  ], // Test a range and a single mip
};

// Test with these sample counts.
const kSampleCounts: number[] = [1, 4];

// Test with these slice counts. This means the depth of a 3d texture or the number
// or layers in a 2D or a 1D texture array.
type SliceCounts = 1 | 7;

// For each slice count, define the slices to leave uninitialized.
const kUninitializedSliceRangesToTest: { [k in SliceCounts]: BeginEndRange[] } = {
  1: [{ begin: 0, end: 1 }], // Test the only slice
  7: [
    { begin: 2, end: 4 },
    { begin: 6, end: 7 },
  ], // Test a range and a single slice
};

// Test with these combinations of texture dimension and sliceCount.
const kCreationSizes: Array<{
  dimension: GPUTextureDimension;
  sliceCount: SliceCounts;
}> = [
  // { dimension: '1d', sliceCount: 7 }, // TODO: 1d textures
  { dimension: '2d', sliceCount: 1 }, // 2d textures
  { dimension: '2d', sliceCount: 7 }, // 2d array textures
  // { dimension: '3d', sliceCount: 7 }, // TODO: 3d textures
];

// Enums to abstract over color / depth / stencil values in textures. Depending on the texture format,
// the data for each value may have a different representation. These enums are converted to a
// representation such that their values can be compared. ex.) An integer is needed to upload to an
// unsigned normalized format, but its value is read as a float in the shader.
export const enum InitializedState {
  Canary, // Set on initialized subresources. It should stay the same. On discarded resources, we should observe zero.
  Zero, // We check that uninitialized subresources are in this state when read back.
}

const initializedStateAsFloat = {
  [InitializedState.Zero]: 0,
  [InitializedState.Canary]: 1,
};

const initializedStateAsUint = {
  [InitializedState.Zero]: 0,
  [InitializedState.Canary]: 1,
};

const initializedStateAsSint = {
  [InitializedState.Zero]: 0,
  [InitializedState.Canary]: -1,
};

function initializedStateAsColor(
  state: InitializedState,
  format: GPUTextureFormat
): [number, number, number, number] {
  let value;
  if (format.indexOf('uint') !== -1) {
    value = initializedStateAsUint[state];
  } else if (format.indexOf('sint') !== -1) {
    value = initializedStateAsSint[state];
  } else {
    value = initializedStateAsFloat[state];
  }
  return [value, value, value, value];
}

const initializedStateAsDepth = {
  [InitializedState.Zero]: 0,
  [InitializedState.Canary]: 0.8,
};

const initializedStateAsStencil = {
  [InitializedState.Zero]: 0,
  [InitializedState.Canary]: 42,
};

function getRequiredTextureUsage(
  format: UncompressedTextureFormat,
  sampleCount: number,
  uninitializeMethod: UninitializeMethod,
  readMethod: ReadMethod
): GPUTextureUsageFlags {
  let usage: GPUTextureUsageFlags = GPUConst.TextureUsage.COPY_DST;

  switch (uninitializeMethod) {
    case UninitializeMethod.Creation:
      break;
    case UninitializeMethod.StoreOpClear:
      usage |= GPUConst.TextureUsage.RENDER_ATTACHMENT;
      break;
    default:
      unreachable();
  }

  switch (readMethod) {
    case ReadMethod.CopyToBuffer:
    case ReadMethod.CopyToTexture:
      usage |= GPUConst.TextureUsage.COPY_SRC;
      break;
    case ReadMethod.Sample:
      usage |= GPUConst.TextureUsage.SAMPLED;
      break;
    case ReadMethod.Storage:
      usage |= GPUConst.TextureUsage.STORAGE;
      break;
    case ReadMethod.DepthTest:
    case ReadMethod.StencilTest:
    case ReadMethod.ColorBlending:
      usage |= GPUConst.TextureUsage.RENDER_ATTACHMENT;
      break;
    default:
      unreachable();
  }

  if (sampleCount > 1) {
    // Copies to multisampled textures are not allowed. We need OutputAttachment to initialize
    // canary data in multisampled textures.
    usage |= GPUConst.TextureUsage.RENDER_ATTACHMENT;
  }

  if (!kUncompressedTextureFormatInfo[format].copyDst) {
    // Copies are not possible. We need OutputAttachment to initialize
    // canary data.
    assert(kUncompressedTextureFormatInfo[format].renderable);
    usage |= GPUConst.TextureUsage.RENDER_ATTACHMENT;
  }

  return usage;
}

export class TextureZeroInitTest extends GPUTest {
  readonly stateToTexelComponents: { [k in InitializedState]: PerTexelComponent<number> };

  private p: Params;
  constructor(rec: TestCaseRecorder, params: CaseParams) {
    super(rec, params);
    this.p = params as Params;

    const stateToTexelComponents = (state: InitializedState) => {
      const [R, G, B, A] = initializedStateAsColor(state, this.p.format);
      return {
        R,
        G,
        B,
        A,
        Depth: initializedStateAsDepth[state],
        Stencil: initializedStateAsStencil[state],
      };
    };

    this.stateToTexelComponents = {
      [InitializedState.Zero]: stateToTexelComponents(InitializedState.Zero),
      [InitializedState.Canary]: stateToTexelComponents(InitializedState.Canary),
    };
  }

  get textureWidth(): number {
    let width = 1 << this.p.mipLevelCount;
    if (this.p.nonPowerOfTwo) {
      width = 2 * width - 1;
    }
    return width;
  }

  get textureHeight(): number {
    let height = 1 << this.p.mipLevelCount;
    if (this.p.nonPowerOfTwo) {
      height = 2 * height - 1;
    }
    return height;
  }

  // Used to iterate subresources and check that their uninitialized contents are zero when accessed
  *iterateUninitializedSubresources(): Generator<SubresourceRange> {
    for (const mipRange of kUninitializedMipRangesToTest[this.p.mipLevelCount]) {
      for (const sliceRange of kUninitializedSliceRangesToTest[this.p.sliceCount]) {
        yield new SubresourceRange({ mipRange, sliceRange });
      }
    }
  }

  // Used to iterate and initialize other subresources not checked for zero-initialization.
  // Zero-initialization of uninitialized subresources should not have side effects on already
  // initialized subresources.
  *iterateInitializedSubresources(): Generator<SubresourceRange> {
    const uninitialized: boolean[][] = new Array(this.p.mipLevelCount);
    for (let level = 0; level < uninitialized.length; ++level) {
      uninitialized[level] = new Array(this.p.sliceCount);
    }
    for (const subresources of this.iterateUninitializedSubresources()) {
      for (const { level, slice } of subresources.each()) {
        uninitialized[level][slice] = true;
      }
    }
    for (let level = 0; level < uninitialized.length; ++level) {
      for (let slice = 0; slice < uninitialized[level].length; ++slice) {
        if (!uninitialized[level][slice]) {
          yield new SubresourceRange({
            mipRange: { begin: level, count: 1 },
            sliceRange: { begin: slice, count: 1 },
          });
        }
      }
    }
  }

  *generateTextureViewDescriptorsForRendering(
    aspect: GPUTextureAspect,
    subresourceRange?: SubresourceRange
  ): Generator<GPUTextureViewDescriptor> {
    const viewDescriptor: GPUTextureViewDescriptor = {
      dimension: '2d',
      aspect,
    };

    if (subresourceRange === undefined) {
      return viewDescriptor;
    }

    for (const { level, slice } of subresourceRange.each()) {
      yield {
        ...viewDescriptor,
        baseMipLevel: level,
        mipLevelCount: 1,
        baseArrayLayer: slice,
        arrayLayerCount: 1,
      };
    }
  }

  private initializeWithStoreOp(
    state: InitializedState,
    texture: GPUTexture,
    subresourceRange?: SubresourceRange
  ): void {
    const commandEncoder = this.device.createCommandEncoder();
    for (const viewDescriptor of this.generateTextureViewDescriptorsForRendering(
      this.p.aspect,
      subresourceRange
    )) {
      if (kUncompressedTextureFormatInfo[this.p.format].color) {
        commandEncoder
          .beginRenderPass({
            colorAttachments: [
              {
                view: texture.createView(viewDescriptor),
                storeOp: 'store',
                loadValue: initializedStateAsColor(state, this.p.format),
              },
            ],
          })
          .endPass();
      } else {
        commandEncoder
          .beginRenderPass({
            colorAttachments: [],
            depthStencilAttachment: {
              view: texture.createView(viewDescriptor),
              depthStoreOp: 'store',
              depthLoadValue: initializedStateAsDepth[state],
              stencilStoreOp: 'store',
              stencilLoadValue: initializedStateAsStencil[state],
            },
          })
          .endPass();
      }
    }
    this.queue.submit([commandEncoder.finish()]);
  }

  private initializeWithCopy(
    texture: GPUTexture,
    state: InitializedState,
    subresourceRange: SubresourceRange
  ): void {
    assert(this.p.format in kEncodableTextureFormatInfo);
    const format = this.p.format as EncodableTextureFormat;

    if (this.p.dimension === '1d' || this.p.dimension === '3d') {
      // TODO: https://github.com/gpuweb/gpuweb/issues/69
      // Copies with 1D and 3D textures are not yet specified
      unreachable();
    }

    const firstSubresource = subresourceRange.each().next().value;
    assert(typeof firstSubresource !== 'undefined');

    const [largestWidth, largestHeight] = mipSize(
      [this.textureWidth, this.textureHeight],
      firstSubresource.level
    );

    const rep = kTexelRepresentationInfo[format];
    const texelData = new Uint8Array(rep.pack(rep.encode(this.stateToTexelComponents[state])));
    const { buffer, bytesPerRow, rowsPerImage } = createTextureUploadBuffer(
      texelData,
      this.device,
      format,
      this.p.dimension,
      [largestWidth, largestHeight, 1]
    );

    const commandEncoder = this.device.createCommandEncoder();

    for (const { level, slice } of subresourceRange.each()) {
      const [width, height] = mipSize([this.textureWidth, this.textureHeight], level);

      commandEncoder.copyBufferToTexture(
        {
          buffer,
          bytesPerRow,
          rowsPerImage,
        },
        { texture, mipLevel: level, origin: { x: 0, y: 0, z: slice } },
        { width, height, depthOrArrayLayers: 1 }
      );
    }
    this.queue.submit([commandEncoder.finish()]);
    buffer.destroy();
  }

  initializeTexture(
    texture: GPUTexture,
    state: InitializedState,
    subresourceRange: SubresourceRange
  ): void {
    if (this.p.sampleCount > 1 || !kUncompressedTextureFormatInfo[this.p.format].copyDst) {
      // Copies to multisampled textures not yet specified.
      // Use a storeOp for now.
      assert(kUncompressedTextureFormatInfo[this.p.format].renderable);
      this.initializeWithStoreOp(state, texture, subresourceRange);
    } else {
      this.initializeWithCopy(texture, state, subresourceRange);
    }
  }

  discardTexture(texture: GPUTexture, subresourceRange: SubresourceRange): void {
    const commandEncoder = this.device.createCommandEncoder();

    for (const desc of this.generateTextureViewDescriptorsForRendering(
      this.p.aspect,
      subresourceRange
    )) {
      if (kUncompressedTextureFormatInfo[this.p.format].color) {
        commandEncoder
          .beginRenderPass({
            colorAttachments: [
              {
                view: texture.createView(desc),
                storeOp: 'clear',
                loadValue: 'load',
              },
            ],
          })
          .endPass();
      } else {
        commandEncoder
          .beginRenderPass({
            colorAttachments: [],
            depthStencilAttachment: {
              view: texture.createView(desc),
              depthStoreOp: 'clear',
              depthLoadValue: 'load',
              stencilStoreOp: 'clear',
              stencilLoadValue: 'load',
            },
          })
          .endPass();
      }
    }
    this.queue.submit([commandEncoder.finish()]);
  }
}

const paramsBuilder = params()
  .combine(
    poptions('readMethod', [
      ReadMethod.CopyToBuffer,
      ReadMethod.CopyToTexture,
      ReadMethod.Sample,
      ReadMethod.DepthTest,
      ReadMethod.StencilTest,
    ])
  )
  .combine(poptions('format', kUncompressedTextureFormats))
  .combine(poptions('aspect', kTextureAspects))
  .unless(({ readMethod, format, aspect }) => {
    const info = kUncompressedTextureFormatInfo[format];
    // console.log(readMethod, format, aspect, info.depth, info.stencil);
    return (
      (readMethod === ReadMethod.DepthTest && (!info.depth || aspect === 'stencil-only')) ||
      (readMethod === ReadMethod.StencilTest && (!info.stencil || aspect === 'depth-only')) ||
      (readMethod === ReadMethod.ColorBlending && !info.color) ||
      // TODO: Test with depth/stencil sampling
      (readMethod === ReadMethod.Sample && (info.depth || info.stencil)) ||
      (aspect === 'depth-only' && !info.depth) ||
      (aspect === 'stencil-only' && !info.stencil) ||
      (aspect === 'all' && info.depth && info.stencil) ||
      // Cannot copy from a packed depth format.
      // TODO: Test copying out of the stencil aspect.
      ((readMethod === ReadMethod.CopyToBuffer || readMethod === ReadMethod.CopyToTexture) &&
        (format === 'depth24plus' || format === 'depth24plus-stencil8'))
    );
  })
  .combine(poptions('mipLevelCount', kMipLevelCounts))
  .combine(poptions('sampleCount', kSampleCounts))
  .unless(
    ({ readMethod, sampleCount }) =>
      // We can only read from multisampled textures by sampling.
      sampleCount > 1 &&
      (readMethod === ReadMethod.CopyToBuffer || readMethod === ReadMethod.CopyToTexture)
  )
  // Multisampled textures may only have one mip
  .unless(({ sampleCount, mipLevelCount }) => sampleCount > 1 && mipLevelCount > 1)
  .combine(poptions('uninitializeMethod', kUninitializeMethods))
  .combine(kCreationSizes)
  // Multisampled 3D / 2D array textures not supported.
  .unless(({ sampleCount, sliceCount }) => sampleCount > 1 && sliceCount > 1)
  .unless(({ format, sampleCount, uninitializeMethod, readMethod }) => {
    const usage = getRequiredTextureUsage(format, sampleCount, uninitializeMethod, readMethod);
    const info = kUncompressedTextureFormatInfo[format];

    return (
      ((usage & GPUConst.TextureUsage.RENDER_ATTACHMENT) !== 0 && !info.renderable) ||
      ((usage & GPUConst.TextureUsage.STORAGE) !== 0 && !info.storage)
    );
  })
  .combine(pbool('nonPowerOfTwo'))
  .combine(pbool('canaryOnCreation'))
  .filter(({ canaryOnCreation, format }) => {
    // We can only initialize the texture if it's encodable or renderable.
    const canInitialize =
      format in kEncodableTextureFormatInfo || kAllTextureFormatInfo[format].renderable;

    // Filter out cases where we want canary values but can't initialize.
    return !canaryOnCreation || canInitialize;
  });

export type Params = typeof paramsBuilder extends ParamsBuilder<infer I> ? I : never;

export type CheckContents = (
  t: TextureZeroInitTest,
  params: Params,
  texture: GPUTexture,
  state: InitializedState,
  subresourceRange: SubresourceRange
) => void;

import { checkContentsByBufferCopy, checkContentsByTextureCopy } from './check_texture/by_copy.js';
import {
  checkContentsByDepthTest,
  checkContentsByStencilTest,
} from './check_texture/by_ds_test.js';
import { checkContentsBySampling } from './check_texture/by_sampling.js';

const checkContentsImpl: { [k in ReadMethod]: CheckContents } = {
  Sample: checkContentsBySampling,
  CopyToBuffer: checkContentsByBufferCopy,
  CopyToTexture: checkContentsByTextureCopy,
  DepthTest: checkContentsByDepthTest,
  StencilTest: checkContentsByStencilTest,
  ColorBlending: t => t.skip('Not implemented'),
  Storage: t => t.skip('Not implemented'),
};

export const g = makeTestGroup(TextureZeroInitTest);

g.test('uninitialized_texture_is_zero')
  .params(paramsBuilder)
  .fn(async t => {
    await t.selectDeviceOrSkipTestCase(kUncompressedTextureFormatInfo[t.params.format].feature);

    const usage = getRequiredTextureUsage(
      t.params.format,
      t.params.sampleCount,
      t.params.uninitializeMethod,
      t.params.readMethod
    );

    const texture = t.device.createTexture({
      size: [t.textureWidth, t.textureHeight, t.params.sliceCount],
      format: t.params.format,
      dimension: t.params.dimension,
      usage,
      mipLevelCount: t.params.mipLevelCount,
      sampleCount: t.params.sampleCount,
    });

    if (t.params.canaryOnCreation) {
      // Initialize some subresources with canary values
      for (const subresourceRange of t.iterateInitializedSubresources()) {
        t.initializeTexture(texture, InitializedState.Canary, subresourceRange);
      }
    }

    switch (t.params.uninitializeMethod) {
      case UninitializeMethod.Creation:
        break;
      case UninitializeMethod.StoreOpClear:
        // Initialize the rest of the resources.
        for (const subresourceRange of t.iterateUninitializedSubresources()) {
          t.initializeTexture(texture, InitializedState.Canary, subresourceRange);
        }
        // Then use a store op to discard their contents.
        for (const subresourceRange of t.iterateUninitializedSubresources()) {
          t.discardTexture(texture, subresourceRange);
        }
        break;
      default:
        unreachable();
    }

    // Check that all uninitialized resources are zero.
    for (const subresourceRange of t.iterateUninitializedSubresources()) {
      checkContentsImpl[t.params.readMethod](
        t,
        t.params,
        texture,
        InitializedState.Zero,
        subresourceRange
      );
    }

    if (t.params.canaryOnCreation) {
      // Check the all other resources are unchanged.
      for (const subresourceRange of t.iterateInitializedSubresources()) {
        checkContentsImpl[t.params.readMethod](
          t,
          t.params,
          texture,
          InitializedState.Canary,
          subresourceRange
        );
      }
    }
  });
