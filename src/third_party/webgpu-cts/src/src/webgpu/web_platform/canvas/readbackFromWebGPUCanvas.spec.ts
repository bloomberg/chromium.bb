export const description = `
Tests for readback from WebGPU Canvas.
`;

import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert, raceWithRejectOnTimeout, unreachable } from '../../../common/util/util.js';
import { GPUTest } from '../../gpu_test.js';
import { checkElementsEqual } from '../../util/check_contents.js';
import {
  allCanvasTypes,
  canvasTypes,
  createCanvas,
  createOnscreenCanvas,
} from '../../util/create_elements.js';

export const g = makeTestGroup(GPUTest);

// Use four pixels rectangle for the test:
// blue: top-left;
// green: top-right;
// red: bottom-left;
// yellow: bottom-right;
const expect = new Uint8ClampedArray([
  0x00,
  0x00,
  0xff,
  0xff, // blue
  0x00,
  0xff,
  0x00,
  0xff, // green
  0xff,
  0x00,
  0x00,
  0xff, // red
  0xff,
  0xff,
  0x00,
  0xff, // yellow
]);

// WebGL has opposite Y direction so we need to
// flipY to get correct expects.
const webglExpect = new Uint8ClampedArray([
  0xff,
  0x00,
  0x00,
  0xff, // red
  0xff,
  0xff,
  0x00,
  0xff, // yellow
  0x00,
  0x00,
  0xff,
  0xff, // blue
  0x00,
  0xff,
  0x00,
  0xff, // green
]);

async function initCanvasContent(
  t: GPUTest,
  canvasType: canvasTypes
): Promise<HTMLCanvasElement | OffscreenCanvas> {
  const canvas = createCanvas(t, canvasType, 2, 2);
  const ctx = canvas.getContext('webgpu');
  assert(ctx !== null, 'Failed to get WebGPU context from canvas');

  ctx.configure({
    device: t.device,
    format: 'bgra8unorm',
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
  });

  const rows = 2;
  const bytesPerRow = 256;
  const buffer = t.device.createBuffer({
    mappedAtCreation: true,
    size: rows * bytesPerRow,
    usage: GPUBufferUsage.COPY_SRC,
  });
  const mapping = buffer.getMappedRange();
  const data = new Uint8Array(mapping);
  data.set(new Uint8Array([0xff, 0x00, 0x00, 0xff]), 0); // blue
  data.set(new Uint8Array([0x00, 0xff, 0x00, 0xff]), 4); // green
  data.set(new Uint8Array([0x00, 0x00, 0xff, 0xff]), 256 + 0); // red
  data.set(new Uint8Array([0x00, 0xff, 0xff, 0xff]), 256 + 4); // yellow
  buffer.unmap();

  const texture = ctx.getCurrentTexture();
  const encoder = t.device.createCommandEncoder();
  encoder.copyBufferToTexture({ buffer, bytesPerRow }, { texture }, [2, 2, 1]);
  t.device.queue.submit([encoder.finish()]);
  await t.device.queue.onSubmittedWorkDone();

  return canvas;
}

function checkImageResult(t: GPUTest, image: CanvasImageSource, expect: Uint8ClampedArray) {
  const canvas: HTMLCanvasElement = createOnscreenCanvas(t, 2, 2);
  const ctx = canvas.getContext('2d');
  assert(ctx !== null);
  ctx.drawImage(image, 0, 0);
  readPixelsFrom2DCanvasAndCompare(t, ctx, expect);
}

function readPixelsFrom2DCanvasAndCompare(
  t: GPUTest,
  ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
  expect: Uint8ClampedArray
) {
  const actual = ctx.getImageData(0, 0, 2, 2).data;

  t.expectOK(checkElementsEqual(actual, expect));
}

g.test('onscreenCanvas,snapshot')
  .desc(
    `
    Ensure snapshot of canvas with WebGPU context is correct
     
    TODO: Snapshot canvas to jpeg, webp and other mime type and
          different quality. Maybe we should test them in reftest.
    `
  )
  .params(u =>
    u //
      .combine('snapshotType', ['toDataURL', 'toBlob', 'imageBitmap'])
  )
  .fn(async t => {
    const canvas = (await initCanvasContent(t, 'onscreen')) as HTMLCanvasElement;

    let snapshot: HTMLImageElement | ImageBitmap;
    switch (t.params.snapshotType) {
      case 'toDataURL': {
        const url = canvas.toDataURL();
        const img = new Image(canvas.width, canvas.height);
        img.src = url;
        await raceWithRejectOnTimeout(img.decode(), 5000, 'load image timeout');
        snapshot = img;
        break;
      }
      case 'toBlob': {
        const blobFromCanvs = new Promise(resolve => {
          canvas.toBlob(blob => resolve(blob));
        });
        const blob = (await blobFromCanvs) as Blob;
        const url = URL.createObjectURL(blob);
        const img = new Image(canvas.width, canvas.height);
        img.src = url;
        await raceWithRejectOnTimeout(img.decode(), 5000, 'load image timeout');
        snapshot = img;
        break;
      }
      case 'imageBitmap': {
        snapshot = await createImageBitmap(canvas);
        break;
      }
      default:
        unreachable();
    }

    checkImageResult(t, snapshot, expect);
  });

g.test('offscreenCanvas,snapshot')
  .desc(
    `
    Ensure snapshot of offscreenCanvas with WebGPU context is correct
     
    TODO: Snapshot offscreenCanvas to jpeg, webp and other mime type and
          different quality. Maybe we should test them in reftest.
    `
  )
  .params(u =>
    u //
      .combine('snapshotType', ['convertToBlob', 'transferToImageBitmap', 'imageBitmap'])
  )
  .fn(async t => {
    const offscreenCanvas = (await initCanvasContent(t, 'offscreen')) as OffscreenCanvas;

    let snapshot: HTMLImageElement | ImageBitmap;
    switch (t.params.snapshotType) {
      case 'convertToBlob': {
        if (typeof offscreenCanvas.convertToBlob === undefined) {
          t.skip("Browser doesn't support OffscreenCanvas.convertToBlob");
          return;
        }
        const blob = await offscreenCanvas.convertToBlob();
        const url = URL.createObjectURL(blob);
        const img = new Image(offscreenCanvas.width, offscreenCanvas.height);
        img.src = url;
        await raceWithRejectOnTimeout(img.decode(), 5000, 'load image timeout');
        snapshot = img;
        break;
      }
      case 'transferToImageBitmap': {
        if (typeof offscreenCanvas.transferToImageBitmap === undefined) {
          t.skip("Browser doesn't support OffscreenCanvas.transferToImageBitmap");
          return;
        }
        snapshot = offscreenCanvas.transferToImageBitmap();
        break;
      }
      case 'imageBitmap': {
        snapshot = await createImageBitmap(offscreenCanvas);
        break;
      }
      default:
        unreachable();
    }

    checkImageResult(t, snapshot, expect);
  });

g.test('onscreenCanvas,uploadToWebGL')
  .desc(
    `
    Ensure upload WebGPU context canvas to webgl texture is correct.
    `
  )
  .params(u =>
    u //
      .combine('webgl', ['webgl', 'webgl2'])
      .combine('upload', ['texImage2D', 'texSubImage2D'])
  )
  .fn(async t => {
    const { webgl, upload } = t.params;
    const canvas = (await initCanvasContent(t, 'onscreen')) as HTMLCanvasElement;

    const expectCanvas: HTMLCanvasElement = createOnscreenCanvas(t, canvas.width, canvas.height);
    const gl = expectCanvas.getContext(webgl) as WebGLRenderingContext | WebGL2RenderingContext;
    if (gl === null) {
      return;
    }

    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    switch (upload) {
      case 'texImage2D': {
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, canvas);
        break;
      }
      case 'texSubImage2D': {
        gl.texImage2D(
          gl.TEXTURE_2D,
          0,
          gl.RGBA,
          canvas.width,
          canvas.height,
          0,
          gl.RGBA,
          gl.UNSIGNED_BYTE,
          null
        );
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, canvas);
        break;
      }
      default:
        unreachable();
    }

    const fb = gl.createFramebuffer();

    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);

    const pixels = new Uint8Array(canvas.width * canvas.height * 4);
    gl.readPixels(0, 0, 2, 2, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    const actual = new Uint8ClampedArray(pixels);

    t.expectOK(checkElementsEqual(actual, webglExpect));
  });

g.test('drawTo2DCanvas')
  .desc(
    `
    Ensure draw WebGPU context canvas to 2d context canvas/offscreenCanvas is correct.
    `
  )
  .params(u =>
    u //
      .combine('webgpuCanvasType', allCanvasTypes)
      .combine('canvas2DType', allCanvasTypes)
  )
  .fn(async t => {
    const { webgpuCanvasType, canvas2DType } = t.params;

    const canvas = await initCanvasContent(t, webgpuCanvasType);

    const expectCanvas = createCanvas(t, canvas2DType, canvas.width, canvas.height);
    const ctx = expectCanvas.getContext('2d');
    if (ctx === null) {
      t.skip(canvas2DType + ' canvas cannot get 2d context');
      return;
    }
    ctx.drawImage(canvas, 0, 0);

    readPixelsFrom2DCanvasAndCompare(t, ctx, expect);
  });
