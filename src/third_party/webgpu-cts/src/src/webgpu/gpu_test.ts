import { Fixture } from '../common/framework/fixture.js';
import { attemptGarbageCollection } from '../common/framework/util/collect_garbage.js';
import { assert } from '../common/framework/util/util.js';

import {
  EncodableTextureFormat,
  SizedTextureFormat,
  kAllTextureFormatInfo,
  kQueryTypeInfo,
} from './capability_info.js';
import { makeBufferWithContents } from './util/buffer.js';
import {
  DevicePool,
  DeviceProvider,
  TestOOMedShouldAttemptGC,
  UncanonicalizedDeviceDescriptor,
} from './util/device_pool.js';
import { align } from './util/math.js';
import {
  fillTextureDataWithTexelValue,
  getTextureCopyLayout,
  LayoutOptions as TextureLayoutOptions,
} from './util/texture/layout.js';
import { PerTexelComponent, kTexelRepresentationInfo } from './util/texture/texel_data.js';

export type TypedArrayBufferView =
  | Uint8Array
  | Uint16Array
  | Uint32Array
  | Int8Array
  | Int16Array
  | Int32Array
  | Float32Array
  | Float64Array;

export type TypedArrayBufferViewConstructor =
  | Uint8ArrayConstructor
  | Uint16ArrayConstructor
  | Uint32ArrayConstructor
  | Int8ArrayConstructor
  | Int16ArrayConstructor
  | Int32ArrayConstructor
  | Float32ArrayConstructor
  | Float64ArrayConstructor;

const devicePool = new DevicePool();

export class GPUTest extends Fixture {
  private provider: DeviceProvider | undefined;
  /** Must not be replaced once acquired. */
  private acquiredDevice: GPUDevice | undefined;

  get device(): GPUDevice {
    assert(
      this.provider !== undefined,
      'No provider available right now; did you "await" selectDeviceOrSkipTestCase?'
    );
    if (!this.acquiredDevice) {
      this.acquiredDevice = this.provider.acquire();
    }
    return this.acquiredDevice;
  }

  get queue(): GPUQueue {
    return this.device.queue;
  }

  async init(): Promise<void> {
    await super.init();

    this.provider = await devicePool.reserve();
  }

  /**
   * When a GPUTest test accesses `.device` for the first time, a "default" GPUDevice
   * (descriptor = `undefined`) is provided by default.
   * However, some tests or cases need particular nonGuaranteedFeatures to be enabled.
   * Call this function with a descriptor or feature name (or `undefined`) to select a
   * GPUDevice with matching capabilities.
   *
   * If the request descriptor can't be supported, throws an exception to skip the entire test case.
   */
  async selectDeviceOrSkipTestCase(
    descriptor:
      | UncanonicalizedDeviceDescriptor
      | GPUFeatureName
      | undefined
      | Array<GPUFeatureName | undefined>
  ): Promise<void> {
    if (descriptor === undefined) return;
    if (typeof descriptor === 'string') {
      descriptor = { nonGuaranteedFeatures: [descriptor] };
    } else if (descriptor instanceof Array) {
      descriptor = {
        nonGuaranteedFeatures: descriptor.filter(f => f !== undefined) as GPUFeatureName[],
      };
    }

    assert(this.provider !== undefined);
    // Make sure the device isn't replaced after it's been retrieved once.
    assert(
      !this.acquiredDevice,
      "Can't selectDeviceOrSkipTestCase() after the device has been used"
    );

    const oldProvider = this.provider;
    this.provider = undefined;
    await devicePool.release(oldProvider);

    this.provider = await devicePool.reserve(descriptor);
    this.acquiredDevice = this.provider.acquire();
  }

  async selectDeviceForTextureFormatOrSkipTestCase(
    formats: GPUTextureFormat | undefined | (GPUTextureFormat | undefined)[]
  ): Promise<void> {
    if (!Array.isArray(formats)) {
      formats = [formats];
    }
    const features = new Set<GPUFeatureName | undefined>();
    for (const format of formats) {
      if (format !== undefined) {
        features.add(kAllTextureFormatInfo[format].feature);
      }
    }

    await this.selectDeviceOrSkipTestCase(Array.from(features));
  }

  async selectDeviceForQueryTypeOrSkipTestCase(
    types: GPUQueryType | GPUQueryType[]
  ): Promise<void> {
    if (!Array.isArray(types)) {
      types = [types];
    }
    const features = types.map(t => kQueryTypeInfo[t].feature);
    await this.selectDeviceOrSkipTestCase(features);
  }

  // Note: finalize is called even if init was unsuccessful.
  async finalize(): Promise<void> {
    await super.finalize();

    if (this.provider) {
      let threw: undefined | Error;
      {
        const provider = this.provider;
        this.provider = undefined;
        try {
          await devicePool.release(provider);
        } catch (ex) {
          threw = ex;
        }
      }
      // The GPUDevice and GPUQueue should now have no outstanding references.

      if (threw) {
        if (threw instanceof TestOOMedShouldAttemptGC) {
          // Try to clean up, in case there are stray GPU resources in need of collection.
          await attemptGarbageCollection();
        }
        throw threw;
      }
    }
  }

  createCopyForMapRead(src: GPUBuffer, srcOffset: number, size: number): GPUBuffer {
    assert(srcOffset % 4 === 0);
    assert(size % 4 === 0);

    const dst = this.device.createBuffer({
      size,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });

    const c = this.device.createCommandEncoder();
    c.copyBufferToBuffer(src, srcOffset, dst, 0, size);

    this.queue.submit([c.finish()]);

    return dst;
  }

  // TODO: add an expectContents for textures, which logs data: uris on failure

  // Offset and size passed to createCopyForMapRead must be divisible by 4. For that
  // we might need to copy more bytes from the buffer than we want to map.
  // begin and end values represent the part of the copied buffer that stores the contents
  // we initially wanted to map.
  // The copy will not cause an OOB error because the buffer size must be 4-aligned.
  createAlignedCopyForMapRead(
    src: GPUBuffer,
    size: number,
    offset: number
  ): { dst: GPUBuffer; begin: number; end: number } {
    const alignedOffset = Math.floor(offset / 4) * 4;
    const offsetDifference = offset - alignedOffset;
    const alignedSize = align(size + offsetDifference, 4);
    const dst = this.createCopyForMapRead(src, alignedOffset, alignedSize);
    return { dst, begin: offsetDifference, end: offsetDifference + size };
  }

  expectContents(
    src: GPUBuffer,
    expected: TypedArrayBufferView,
    srcOffset: number = 0,
    { generateWarningOnly = false }: { generateWarningOnly?: boolean } = {}
  ): void {
    const { dst, begin, end } = this.createAlignedCopyForMapRead(
      src,
      expected.byteLength,
      srcOffset
    );

    this.eventualAsyncExpectation(async niceStack => {
      const constructor = expected.constructor as TypedArrayBufferViewConstructor;
      await dst.mapAsync(GPUMapMode.READ);
      const actual = new constructor(dst.getMappedRange());
      const check = this.checkBuffer(actual.subarray(begin, end), expected);
      if (check !== undefined) {
        niceStack.message = check;
        if (generateWarningOnly) {
          this.rec.warn(niceStack);
        } else {
          this.rec.expectationFailed(niceStack);
        }
      }
      dst.destroy();
    });
  }

  expectSingleValueContents(
    src: GPUBuffer,
    expected: TypedArrayBufferView,
    byteSize: number,
    srcOffset: number = 0,
    { generateWarningOnly = false }: { generateWarningOnly?: boolean } = {}
  ): void {
    const { dst, begin, end } = this.createAlignedCopyForMapRead(src, byteSize, srcOffset);

    this.eventualAsyncExpectation(async niceStack => {
      const constructor = expected.constructor as TypedArrayBufferViewConstructor;
      await dst.mapAsync(GPUMapMode.READ);
      const actual = new constructor(dst.getMappedRange());
      const check = this.checkSingleValueBuffer(actual.subarray(begin, end), expected);
      if (check !== undefined) {
        niceStack.message = check;
        if (generateWarningOnly) {
          this.rec.warn(niceStack);
        } else {
          this.rec.expectationFailed(niceStack);
        }
      }
      dst.destroy();
    });
  }

  expectContentsBetweenTwoValues(
    src: GPUBuffer,
    expected: [TypedArrayBufferView, TypedArrayBufferView],
    srcOffset: number = 0,
    { generateWarningOnly = false }: { generateWarningOnly?: boolean }
  ): void {
    assert(expected[0].constructor === expected[1].constructor);
    assert(expected[0].length === expected[1].length);
    const { dst, begin, end } = this.createAlignedCopyForMapRead(
      src,
      expected[0].byteLength,
      srcOffset
    );

    this.eventualAsyncExpectation(async niceStack => {
      const constructor = expected[0].constructor as TypedArrayBufferViewConstructor;
      await dst.mapAsync(GPUMapMode.READ);
      const actual = new constructor(dst.getMappedRange());

      const bounds = [new constructor(expected[0].length), new constructor(expected[1].length)];
      for (let i = 0, len = expected[0].length; i < len; i++) {
        bounds[0][i] = Math.min(expected[0][i], expected[1][i]);
        bounds[1][i] = Math.max(expected[0][i], expected[1][i]);
      }

      const check = this.checkBufferFn(
        actual.subarray(begin, end),
        (actual: TypedArrayBufferView) => {
          if (actual.length !== expected[0].length) {
            return false;
          }
          for (let i = 0, len = actual.length; i < len; i++) {
            if (actual[i] < bounds[0][i] || actual[i] > bounds[1][i]) {
              return false;
            }
          }
          return true;
        },
        () => {
          const lines: string[] = [];
          const format = (x: number) => x.toString(16).padStart(2, '0');
          const exp0Hex = Array.from(
            new Uint8Array(
              expected[0].buffer,
              expected[0].byteOffset,
              Math.min(expected[0].byteLength, 256)
            ),
            format
          ).join('');
          lines.push('EXPECT\nbetween:\t  ' + expected[0].join(' '));
          lines.push('\t0x' + exp0Hex);
          if (expected[0].byteLength > 256) {
            lines.push(' ...');
          }
          const exp1Hex = Array.from(
            new Uint8Array(
              expected[1].buffer,
              expected[1].byteOffset,
              Math.min(expected[1].byteLength, 256)
            )
          )
            .map(format)
            .join('');
          lines.push('and:    \t  ' + expected[1].join(' '));
          lines.push('\t0x' + exp1Hex);
          if (expected[1].byteLength > 256) {
            lines.push(' ...');
          }
          return lines.join('\n');
        }
      );
      if (check !== undefined) {
        niceStack.message = check;
        if (generateWarningOnly) {
          this.rec.warn(niceStack);
        } else {
          this.rec.expectationFailed(niceStack);
        }
      }
      dst.destroy();
    });
  }

  // We can expand this function in order to support multiple valid values or two mixed vectors
  // if needed. See the discussion at https://github.com/gpuweb/cts/pull/384#discussion_r533101429
  expectContentsTwoValidValues(
    src: GPUBuffer,
    expected1: TypedArrayBufferView,
    expected2: TypedArrayBufferView,
    srcOffset: number = 0
  ): void {
    assert(expected1.byteLength === expected2.byteLength);
    const { dst, begin, end } = this.createAlignedCopyForMapRead(
      src,
      expected1.byteLength,
      srcOffset
    );

    this.eventualAsyncExpectation(async niceStack => {
      const constructor = expected1.constructor as TypedArrayBufferViewConstructor;
      await dst.mapAsync(GPUMapMode.READ);
      const actual = new constructor(dst.getMappedRange());
      const check1 = this.checkBuffer(actual.subarray(begin, end), expected1);
      const check2 = this.checkBuffer(actual.subarray(begin, end), expected2);
      if (check1 !== undefined && check2 !== undefined) {
        niceStack.message = `Expected one of the following two checks to succeed:
  - ${check1}
  - ${check2}`;
        this.rec.expectationFailed(niceStack);
      }
      dst.destroy();
    });
  }

  expectBuffer(actual: Uint8Array, exp: Uint8Array): void {
    const check = this.checkBuffer(actual, exp);
    if (check !== undefined) {
      this.rec.expectationFailed(new Error(check));
    }
  }

  checkBuffer(
    actual: TypedArrayBufferView,
    exp: TypedArrayBufferView,
    tolerance: number | ((i: number) => number) = 0
  ): string | undefined {
    assert(actual.constructor === exp.constructor);

    const size = exp.byteLength;
    if (actual.byteLength !== size) {
      return 'size mismatch';
    }
    const failedByteIndices: string[] = [];
    const failedByteExpectedValues: string[] = [];
    const failedByteActualValues: string[] = [];
    for (let i = 0; i < size; ++i) {
      const tol = typeof tolerance === 'function' ? tolerance(i) : tolerance;
      if (Math.abs(actual[i] - exp[i]) > tol) {
        if (failedByteIndices.length >= 4) {
          failedByteIndices.push('...');
          failedByteExpectedValues.push('...');
          failedByteActualValues.push('...');
          break;
        }
        failedByteIndices.push(i.toString());
        failedByteExpectedValues.push(exp[i].toString());
        failedByteActualValues.push(actual[i].toString());
      }
    }
    const summary = `at [${failedByteIndices.join(', ')}], \
expected [${failedByteExpectedValues.join(', ')}], \
got [${failedByteActualValues.join(', ')}]`;
    const lines = [summary];

    // TODO: Could make a more convenient message, which could look like e.g.:
    //
    //   Starting at offset 48,
    //              got 22222222 ABCDABCD 99999999
    //     but expected 22222222 55555555 99999999
    //
    // or
    //
    //   Starting at offset 0,
    //              got 00000000 00000000 00000000 00000000 (... more)
    //     but expected 00FF00FF 00FF00FF 00FF00FF 00FF00FF (... more)
    //
    // Or, maybe these diffs aren't actually very useful (given we have the prints just above here),
    // and we should remove them. More important will be logging of texture data in a visual format.

    if (size <= 256 && failedByteIndices.length > 0) {
      const expHex = Array.from(new Uint8Array(exp.buffer, exp.byteOffset, exp.byteLength))
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      const actHex = Array.from(new Uint8Array(actual.buffer, actual.byteOffset, actual.byteLength))
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      lines.push('EXPECT:\t  ' + exp.join(' '));
      lines.push('\t0x' + expHex);
      lines.push('ACTUAL:\t  ' + actual.join(' '));
      lines.push('\t0x' + actHex);
    }
    if (failedByteIndices.length) {
      return lines.join('\n');
    }
    return undefined;
  }

  checkSingleValueBuffer(
    actual: TypedArrayBufferView,
    exp: TypedArrayBufferView,
    tolerance: number | ((i: number) => number) = 0
  ): string | undefined {
    assert(actual.constructor === exp.constructor);

    const size = actual.length;
    if (1 !== exp.length) {
      return 'expected single value typed array for expected value';
    }
    const failedByteIndices: string[] = [];
    const failedByteExpectedValues: string[] = [];
    const failedByteActualValues: string[] = [];
    for (let i = 0; i < size; ++i) {
      const tol = typeof tolerance === 'function' ? tolerance(i) : tolerance;
      if (Math.abs(actual[i] - exp[0]) > tol) {
        if (failedByteIndices.length >= 4) {
          failedByteIndices.push('...');
          failedByteExpectedValues.push('...');
          failedByteActualValues.push('...');
          break;
        }
        failedByteIndices.push(i.toString());
        failedByteExpectedValues.push(exp.toString());
        failedByteActualValues.push(actual[i].toString());
      }
    }
    const summary = `at [${failedByteIndices.join(', ')}], \
expected [${failedByteExpectedValues.join(', ')}], \
got [${failedByteActualValues.join(', ')}]`;
    const lines = [summary];

    // TODO: Could make a more convenient message, which could look like e.g.:
    //
    //   Starting at offset 48,
    //              got 22222222 ABCDABCD 99999999
    //     but expected 22222222 55555555 99999999
    //
    // or
    //
    //   Starting at offset 0,
    //              got 00000000 00000000 00000000 00000000 (... more)
    //     but expected 00FF00FF 00FF00FF 00FF00FF 00FF00FF (... more)
    //
    // Or, maybe these diffs aren't actually very useful (given we have the prints just above here),
    // and we should remove them. More important will be logging of texture data in a visual format.

    if (size <= 256 && failedByteIndices.length > 0) {
      const expHex = Array.from(new Uint8Array(exp.buffer, exp.byteOffset, exp.byteLength))
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      const actHex = Array.from(new Uint8Array(actual.buffer, actual.byteOffset, actual.byteLength))
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      lines.push('EXPECT:\t  ' + exp.join(' '));
      lines.push('\t0x' + expHex);
      lines.push('ACTUAL:\t  ' + actual.join(' '));
      lines.push('\t0x' + actHex);
    }
    if (failedByteIndices.length) {
      return lines.join('\n');
    }
    return undefined;
  }

  checkBufferFn(
    actual: TypedArrayBufferView,
    checkFn: (actual: TypedArrayBufferView) => boolean,
    errorMsgFn?: () => string
  ): string | undefined {
    const result = checkFn(actual);

    if (result) {
      return undefined;
    }

    const summary = 'CheckBuffer failed';
    const lines = [summary];

    const actHex = Array.from(
      new Uint8Array(actual.buffer, actual.byteOffset, Math.min(actual.byteLength, 256))
    )
      .map(x => x.toString(16).padStart(2, '0'))
      .join('');
    lines.push('ACTUAL:\t  ' + actual.join(' '));
    lines.push('\t0x' + actHex);

    if (actual.byteLength > 256) {
      lines.push(' ...');
    }

    if (errorMsgFn !== undefined) {
      lines.push(errorMsgFn());
    }

    return lines.join('\n');
  }

  expectSingleColor(
    src: GPUTexture,
    format: EncodableTextureFormat,
    {
      size,
      exp,
      dimension = '2d',
      slice = 0,
      layout,
    }: {
      size: [number, number, number];
      exp: PerTexelComponent<number>;
      dimension?: GPUTextureDimension;
      slice?: number;
      layout?: TextureLayoutOptions;
    }
  ): void {
    const { byteLength, bytesPerRow, rowsPerImage, mipSize } = getTextureCopyLayout(
      format,
      dimension,
      size,
      layout
    );
    const rep = kTexelRepresentationInfo[format];
    const expectedTexelData = rep.pack(rep.encode(exp));

    const buffer = this.device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      { texture: src, mipLevel: layout?.mipLevel, origin: { x: 0, y: 0, z: slice } },
      { buffer, bytesPerRow, rowsPerImage },
      mipSize
    );
    this.queue.submit([commandEncoder.finish()]);
    const arrayBuffer = new ArrayBuffer(byteLength);
    fillTextureDataWithTexelValue(expectedTexelData, format, dimension, arrayBuffer, size, layout);
    this.expectContents(buffer, new Uint8Array(arrayBuffer));
  }

  // return a GPUBuffer that data are going to be written into
  private readSinglePixelFrom2DTexture(
    src: GPUTexture,
    format: SizedTextureFormat,
    { x, y }: { x: number; y: number },
    { slice = 0, layout }: { slice?: number; layout?: TextureLayoutOptions }
  ): GPUBuffer {
    const { byteLength, bytesPerRow, rowsPerImage, mipSize } = getTextureCopyLayout(
      format,
      '2d',
      [1, 1, 1],
      layout
    );
    const buffer = this.device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      { texture: src, mipLevel: layout?.mipLevel, origin: { x, y, z: slice } },
      { buffer, bytesPerRow, rowsPerImage },
      mipSize
    );
    this.queue.submit([commandEncoder.finish()]);

    return buffer;
  }

  // TODO: Add check for values of depth/stencil, probably through sampling of shader
  // TODO(natashalee): Can refactor this and expectSingleColor to use a similar base expect
  expectSinglePixelIn2DTexture(
    src: GPUTexture,
    format: SizedTextureFormat,
    { x, y }: { x: number; y: number },
    {
      exp,
      slice = 0,
      layout,
      generateWarningOnly = false,
    }: {
      exp: Uint8Array;
      slice?: number;
      layout?: TextureLayoutOptions;
      generateWarningOnly?: boolean;
    }
  ): void {
    const buffer = this.readSinglePixelFrom2DTexture(src, format, { x, y }, { slice, layout });
    this.expectContents(buffer, exp, 0, { generateWarningOnly });
  }

  expectSinglePixelBetweenTwoValuesIn2DTexture(
    src: GPUTexture,
    format: SizedTextureFormat,
    { x, y }: { x: number; y: number },
    {
      exp,
      slice = 0,
      layout,
      generateWarningOnly = false,
    }: {
      exp: [TypedArrayBufferView, TypedArrayBufferView];
      slice?: number;
      layout?: TextureLayoutOptions;
      generateWarningOnly?: boolean;
    }
  ): void {
    const buffer = this.readSinglePixelFrom2DTexture(src, format, { x, y }, { slice, layout });
    this.expectContentsBetweenTwoValues(buffer, exp, 0, { generateWarningOnly });
  }

  expectGPUError<R>(filter: GPUErrorFilter, fn: () => R, shouldError: boolean = true): R {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (!shouldError) {
      return fn();
    }

    this.device.pushErrorScope(filter);
    const returnValue = fn();
    const promise = this.device.popErrorScope();

    this.eventualAsyncExpectation(async niceStack => {
      const error = await promise;

      let failed = false;
      switch (filter) {
        case 'out-of-memory':
          failed = !(error instanceof GPUOutOfMemoryError);
          break;
        case 'validation':
          failed = !(error instanceof GPUValidationError);
          break;
      }

      if (failed) {
        niceStack.message = `Expected ${filter} error`;
        this.rec.expectationFailed(niceStack);
      } else {
        niceStack.message = `Captured ${filter} error`;
        if (error instanceof GPUValidationError) {
          niceStack.message += ` - ${error.message}`;
        }
        this.rec.debug(niceStack);
      }
    });

    return returnValue;
  }

  makeBufferWithContents(dataArray: TypedArrayBufferView, usage: GPUBufferUsageFlags): GPUBuffer {
    return makeBufferWithContents(this.device, dataArray, usage);
  }

  createTexture2DWithMipmaps(mipmapDataArray: TypedArrayBufferView[]): GPUTexture {
    const format = 'rgba8unorm';
    const mipLevelCount = mipmapDataArray.length;
    const textureSizeMipmap0 = 1 << (mipLevelCount - 1);
    const texture = this.device.createTexture({
      mipLevelCount,
      size: { width: textureSizeMipmap0, height: textureSizeMipmap0, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
    });

    const textureEncoder = this.device.createCommandEncoder();
    for (let i = 0; i < mipLevelCount; i++) {
      const { byteLength, bytesPerRow, rowsPerImage, mipSize } = getTextureCopyLayout(
        format,
        '2d',
        [textureSizeMipmap0, textureSizeMipmap0, 1],
        { mipLevel: i }
      );

      const data: Uint8Array = new Uint8Array(byteLength);
      const mipLevelData = mipmapDataArray[i];
      assert(rowsPerImage === mipSize[0]); // format is rgba8unorm and block size should be 1
      for (let r = 0; r < rowsPerImage; r++) {
        const o = r * bytesPerRow;
        for (let c = o, end = o + mipSize[1] * 4; c < end; c += 4) {
          data[c] = mipLevelData[0];
          data[c + 1] = mipLevelData[1];
          data[c + 2] = mipLevelData[2];
          data[c + 3] = mipLevelData[3];
        }
      }
      const buffer = this.makeBufferWithContents(
        data,
        GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST
      );

      textureEncoder.copyBufferToTexture(
        { buffer, bytesPerRow, rowsPerImage },
        { texture, mipLevel: i, origin: [0, 0, 0] },
        mipSize
      );
    }
    this.device.queue.submit([textureEncoder.finish()]);

    return texture;
  }
}
