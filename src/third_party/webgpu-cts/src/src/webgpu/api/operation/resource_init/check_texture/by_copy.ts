import { assert } from '../../../../../common/framework/util/util.js';
import {
  EncodableTextureFormat,
  kEncodableTextureFormatInfo,
} from '../../../../capability_info.js';
import { CheckContents } from '../texture_zero.spec.js';

export const checkContentsByBufferCopy: CheckContents = (
  t,
  params,
  texture,
  state,
  subresourceRange
) => {
  for (const { level: mipLevel, slice } of subresourceRange.each()) {
    assert(params.dimension === '2d');
    assert(params.format in kEncodableTextureFormatInfo);
    const format = params.format as EncodableTextureFormat;

    t.expectSingleColor(texture, format, {
      size: [t.textureWidth, t.textureHeight, 1],
      dimension: params.dimension,
      slice,
      layout: { mipLevel },
      exp: t.stateToTexelComponents[state],
    });
  }
};

export const checkContentsByTextureCopy: CheckContents = (
  t,
  params,
  texture,
  state,
  subresourceRange
) => {
  for (const { level, slice } of subresourceRange.each()) {
    assert(params.dimension === '2d');
    assert(params.format in kEncodableTextureFormatInfo);
    const format = params.format as EncodableTextureFormat;

    const width = t.textureWidth >> level;
    const height = t.textureHeight >> level;

    const dst = t.device.createTexture({
      size: [width, height, 1],
      format: params.format,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
    });

    const commandEncoder = t.device.createCommandEncoder();
    commandEncoder.copyTextureToTexture(
      { texture, mipLevel: level, origin: { x: 0, y: 0, z: slice } },
      { texture: dst, mipLevel: 0 },
      { width, height, depth: 1 }
    );
    t.queue.submit([commandEncoder.finish()]);

    t.expectSingleColor(dst, format, {
      size: [width, height, 1],
      exp: t.stateToTexelComponents[state],
    });
  }
};
