export const description = `
copyImageBitmapToTexture from ImageBitmaps created from various sources.

TODO: Test ImageBitmap generated from all possible ImageBitmapSource, relevant ImageBitmapOptions
    (https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#images-2)
    and various source filetypes and metadata (weird dimensions, EXIF orientations, video rotations
    and visible/crop rectangles, etc. (In theory these things are handled inside createImageBitmap,
    but in theory could affect the internal representation of the ImageBitmap.)

TODO: Test zero-sized copies from all sources (just make sure params cover it) (e.g. 0x0, 0x4, 4x0).
`;

import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/util/util.js';
import {
  RegularTextureFormat,
  kTextureFormatInfo,
  kValidTextureFormatsForCopyIB2T,
} from '../../capability_info.js';
import { GPUTest } from '../../gpu_test.js';
import { kTexelRepresentationInfo } from '../../util/texture/texel_data.js';

function calculateRowPitch(width: number, bytesPerPixel: number): number {
  const bytesPerRow = width * bytesPerPixel;
  // Rounds up to a multiple of 256 according to WebGPU requirements.
  return (((bytesPerRow - 1) >> 8) + 1) << 8;
}

enum Color {
  Red,
  Green,
  Blue,
  White,
  OpaqueBlack,
  TransparentBlack,
}

// These two types correspond to |premultiplyAlpha| and |imageOrientation| in |ImageBitmapOptions|.
type TransparentOp = 'premultiply' | 'none' | 'non-transparent';
type OrientationOp = 'flipY' | 'none';

// Cache for generated pixels.
const generatedPixelCache: Map<
  RegularTextureFormat,
  Map<Color, Map<TransparentOp, Uint8Array>>
> = new Map();

class F extends GPUTest {
  // TODO(crbug.com/dawn/868): Should be possible to consolidate this along with texture checking
  checkCopyImageBitmapResult(
    src: GPUBuffer,
    expected: ArrayBufferView,
    width: number,
    height: number,
    bytesPerPixel: number
  ): void {
    const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);
    const rowPitch = calculateRowPitch(width, bytesPerPixel);

    const readbackPromise = this.readGPUBufferRangeTyped(src, {
      type: Uint8Array,
      typedLength: rowPitch * height,
    });

    this.eventualAsyncExpectation(async niceStack => {
      const readback = await readbackPromise;
      const check = this.checkBufferWithRowPitch(
        readback.data,
        exp,
        width,
        height,
        rowPitch,
        bytesPerPixel
      );
      if (check !== undefined) {
        niceStack.message = check;
        this.rec.expectationFailed(niceStack);
      }
      readback.cleanup();
    });
  }

  // TODO(crbug.com/dawn/868): Should be possible to consolidate this along with texture checking
  checkBufferWithRowPitch(
    actual: Uint8Array,
    exp: Uint8Array,
    width: number,
    height: number,
    rowPitch: number,
    bytesPerPixel: number
  ): string | undefined {
    const failedByteIndices: string[] = [];
    const failedByteExpectedValues: string[] = [];
    const failedByteActualValues: string[] = [];
    iLoop: for (let i = 0; i < height; ++i) {
      const bytesPerRow = width * bytesPerPixel;
      for (let j = 0; j < bytesPerRow; ++j) {
        const indexExp = j + i * bytesPerRow;
        const indexActual = j + rowPitch * i;
        if (actual[indexActual] !== exp[indexExp]) {
          if (failedByteIndices.length >= 4) {
            failedByteIndices.push('...');
            failedByteExpectedValues.push('...');
            failedByteActualValues.push('...');
            break iLoop;
          }
          failedByteIndices.push(`(${i},${j})`);
          failedByteExpectedValues.push(exp[indexExp].toString());
          failedByteActualValues.push(actual[indexActual].toString());
        }
      }
    }
    if (failedByteIndices.length > 0) {
      return `at [${failedByteIndices.join(', ')}], \
expected [${failedByteExpectedValues.join(', ')}], \
got [${failedByteActualValues.join(', ')}]`;
    }
    return undefined;
  }

  doTestAndCheckResult(
    imageBitmapCopyView: GPUImageCopyImageBitmap,
    dstTextureCopyView: GPUImageCopyTexture,
    copySize: GPUExtent3DDict,
    bytesPerPixel: number,
    expectedData: Uint8ClampedArray
  ): void {
    this.device.queue.copyImageBitmapToTexture(imageBitmapCopyView, dstTextureCopyView, copySize);

    const imageBitmap = imageBitmapCopyView.imageBitmap;
    const dstTexture = dstTextureCopyView.texture;

    const bytesPerRow = calculateRowPitch(imageBitmap.width, bytesPerPixel);
    const testBuffer = this.device.createBuffer({
      size: bytesPerRow * imageBitmap.height,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const encoder = this.device.createCommandEncoder();

    encoder.copyTextureToBuffer(
      { texture: dstTexture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
      { buffer: testBuffer, bytesPerRow },
      { width: imageBitmap.width, height: imageBitmap.height, depthOrArrayLayers: 1 }
    );
    this.device.queue.submit([encoder.finish()]);

    this.checkCopyImageBitmapResult(
      testBuffer,
      expectedData,
      imageBitmap.width,
      imageBitmap.height,
      bytesPerPixel
    );
  }

  generatePixel(
    color: Color,
    format: RegularTextureFormat,
    transparentOp: TransparentOp
  ): Uint8Array {
    let formatEntry = generatedPixelCache.get(format);
    if (formatEntry === undefined) {
      formatEntry = new Map();
      generatedPixelCache.set(format, formatEntry);
    }

    let colorEntry = formatEntry.get(color);
    if (colorEntry === undefined) {
      colorEntry = new Map();
      formatEntry.set(color, colorEntry);
    }

    // None of the dst texture format is 'uint' or 'sint', so we can always use float value.
    if (!colorEntry.has(transparentOp)) {
      const rep = kTexelRepresentationInfo[format];
      let rgba: { R: number; G: number; B: number; A: number };
      switch (color) {
        case Color.Red:
          rgba = { R: 1.0, G: 0.0, B: 0.0, A: 1.0 };
          break;
        case Color.Green:
          rgba = { R: 0.0, G: 1.0, B: 0.0, A: 1.0 };
          break;
        case Color.Blue:
          rgba = { R: 0.0, G: 0.0, B: 1.0, A: 1.0 };
          break;
        case Color.White:
          rgba = { R: 0.0, G: 0.0, B: 0.0, A: 1.0 };
          break;
        case Color.OpaqueBlack:
          rgba = { R: 1.0, G: 1.0, B: 1.0, A: 1.0 };
          break;
        case Color.TransparentBlack:
          rgba = { R: 1.0, G: 1.0, B: 1.0, A: 0.0 };
          break;
        default:
          unreachable();
      }

      if (transparentOp === 'premultiply') {
        rgba.R *= rgba.A;
        rgba.G *= rgba.A;
        rgba.B *= rgba.A;
      }

      const pixels = new Uint8Array(rep.pack(rep.encode(rgba)));
      colorEntry.set(transparentOp, pixels);
    }

    return colorEntry.get(transparentOp)!;
  }

  // Helper functions to generate imagePixels based input configs.
  getImagePixels({
    format,
    width,
    height,
    transparentOp,
    orientationOp,
  }: {
    format: RegularTextureFormat;
    width: number;
    height: number;
    transparentOp: TransparentOp;
    orientationOp: OrientationOp;
  }): Uint8ClampedArray {
    const bytesPerPixel = kTextureFormatInfo[format].bytesPerBlock;

    // Generate input contents by iterating 'Color' enum
    const imagePixels = new Uint8ClampedArray(bytesPerPixel * width * height);
    const testColors = [Color.Red, Color.Green, Color.Blue, Color.White, Color.OpaqueBlack];
    if (transparentOp !== 'non-transparent') testColors.push(Color.TransparentBlack);

    for (let i = 0; i < height; ++i) {
      for (let j = 0; j < width; ++j) {
        const pixelPos = i * width + j;
        const currentColorIndex =
          orientationOp === 'flipY' ? (height - i - 1) * width + j : pixelPos;
        const currentPixel = testColors[currentColorIndex % testColors.length];
        const pixelData = this.generatePixel(currentPixel, format, transparentOp);
        imagePixels.set(pixelData, pixelPos * bytesPerPixel);
      }
    }

    return imagePixels;
  }
}

export const g = makeTestGroup(F);

g.test('from_ImageData')
  .desc(
    `
  Test ImageBitmap generated from ImageData can be copied to WebGPU
  texture correctly. These imageBitmaps are highly possible living
  in CPU back resource.
  `
  )
  .params(u =>
    u
      .combine('alpha', ['none', 'premultiply'] as const)
      .combine('orientation', ['none', 'flipY'] as const)
      .combine('dstColorFormat', kValidTextureFormatsForCopyIB2T)
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const { width, height, alpha, orientation, dstColorFormat } = t.params;

    // Generate input contents by iterating 'Color' enum
    const imagePixels = t.getImagePixels({
      format: 'rgba8unorm',
      width,
      height,
      transparentOp: 'none',
      orientationOp: 'none',
    });

    // Generate correct expected values
    const imageData = new ImageData(imagePixels, width, height);
    const imageBitmap = await createImageBitmap(imageData, {
      premultiplyAlpha: alpha,
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: {
        width: imageBitmap.width,
        height: imageBitmap.height,
        depthOrArrayLayers: 1,
      },
      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Construct expected value for different dst color format
    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const expectedPixels = t.getImagePixels({
      format: dstColorFormat,
      width,
      height,
      transparentOp: alpha,
      orientationOp: orientation,
    });

    t.doTestAndCheckResult(
      { imageBitmap, origin: { x: 0, y: 0 } },
      { texture: dst },
      { width: imageBitmap.width, height: imageBitmap.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels
    );
  });

g.test('from_canvas')
  .desc(
    `
  Test ImageBitmap generated from canvas/offscreenCanvas can be copied to WebGPU
  texture correctly. These imageBitmaps are highly possible living in GPU back resource.
  `
  )
  .params(u =>
    u
      .combine('orientation', ['none', 'flipY'] as const)
      .combine('dstColorFormat', kValidTextureFormatsForCopyIB2T)
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const { width, height, orientation, dstColorFormat } = t.params;

    // CTS sometimes runs on worker threads, where document is not available.
    // In this case, OffscreenCanvas can be used instead of <canvas>.
    // But some browsers don't support OffscreenCanvas, and some don't
    // support '2d' contexts on OffscreenCanvas.
    // In this situation, the case will be skipped.
    let imageCanvas;
    if (typeof document !== 'undefined') {
      imageCanvas = document.createElement('canvas');
      imageCanvas.width = width;
      imageCanvas.height = height;
    } else if (typeof OffscreenCanvas === 'undefined') {
      t.skip('OffscreenCanvas is not supported');
      return;
    } else {
      imageCanvas = new OffscreenCanvas(width, height);
    }
    const imageCanvasContext = imageCanvas.getContext('2d');
    if (imageCanvasContext === null) {
      t.skip('OffscreenCanvas "2d" context not available');
      return;
    }

    // Generate non-transparent pixel data to avoid canvas
    // different opt behaviour on putImageData()
    // from browsers.
    const imagePixels = t.getImagePixels({
      format: 'rgba8unorm',
      width,
      height,
      transparentOp: 'non-transparent',
      orientationOp: 'none',
    });

    const imageData = new ImageData(imagePixels, width, height);

    // Use putImageData to prevent color space conversion.
    imageCanvasContext.putImageData(imageData, 0, 0);

    const imageBitmap = await createImageBitmap(imageCanvas, {
      premultiplyAlpha: 'premultiply',
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: {
        width: imageBitmap.width,
        height: imageBitmap.height,
        depthOrArrayLayers: 1,
      },
      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const expectedData = t.getImagePixels({
      format: dstColorFormat,
      width,
      height,
      transparentOp: 'non-transparent',
      orientationOp: orientation,
    });

    t.doTestAndCheckResult(
      { imageBitmap, origin: { x: 0, y: 0 } },
      { texture: dst },
      { width: imageBitmap.width, height: imageBitmap.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedData
    );
  });
