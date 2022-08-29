import { assert, memcpy } from '../../../common/util/util.js';
import { EncodableTextureFormat, kTextureFormatInfo } from '../../capability_info.js';
import { reifyExtent3D, reifyOrigin3D } from '../unions.js';

import { kTexelRepresentationInfo, makeClampToRange, PerTexelComponent } from './texel_data.js';

/** Function taking some x,y,z coordinates and returning `Readonly<T>`. */
export type PerPixelAtLevel<T> = (coords: Required<GPUOrigin3DDict>) => Readonly<T>;

/**
 * Wrapper to view various representations of texture data in other ways. E.g., can:
 * - Provide a mapped buffer, containing copied texture data, and read color values.
 * - Provide a function that generates color values by coordinate, and convert to ULPs-from-zero.
 *
 * MAINTENANCE_TODO: Would need some refactoring to support block formats, which could be partially
 * supported if useful.
 */
export class TexelView {
  /** The GPUTextureFormat of the TexelView. */
  readonly format: EncodableTextureFormat;
  /** Generates the bytes for the texel at the given coordinates. */
  readonly bytes: PerPixelAtLevel<Uint8Array>;
  /** Generates the ULPs-from-zero for the texel at the given coordinates. */
  readonly ulpFromZero: PerPixelAtLevel<PerTexelComponent<number>>;
  /** Generates the color for the texel at the given coordinates. */
  readonly color: PerPixelAtLevel<PerTexelComponent<number>>;

  private constructor(
    format: EncodableTextureFormat,
    {
      bytes,
      ulpFromZero,
      color,
    }: {
      bytes: PerPixelAtLevel<Uint8Array>;
      ulpFromZero: PerPixelAtLevel<PerTexelComponent<number>>;
      color: PerPixelAtLevel<PerTexelComponent<number>>;
    }
  ) {
    this.format = format;
    this.bytes = bytes;
    this.ulpFromZero = ulpFromZero;
    this.color = color;
  }

  /**
   * Produces a TexelView from "linear image data", i.e. the `writeTexture` format. Takes a
   * reference to the input `subrectData`, so any changes to it will be visible in the TexelView.
   */
  static fromTextureDataByReference(
    format: EncodableTextureFormat,
    subrectData: Uint8Array | Uint8ClampedArray,
    {
      bytesPerRow,
      rowsPerImage,
      subrectOrigin,
      subrectSize,
    }: {
      bytesPerRow: number;
      rowsPerImage: number;
      subrectOrigin: GPUOrigin3D;
      subrectSize: GPUExtent3D;
    }
  ) {
    const origin = reifyOrigin3D(subrectOrigin);
    const size = reifyExtent3D(subrectSize);

    const info = kTextureFormatInfo[format];
    assert(info.blockWidth === 1 && info.blockHeight === 1, 'unimplemented for block formats');

    return TexelView.fromTexelsAsBytes(format, coords => {
      assert(
        coords.x >= origin.x &&
          coords.y >= origin.y &&
          coords.z >= origin.z &&
          coords.x < size.width &&
          coords.y < size.height &&
          coords.z < size.depthOrArrayLayers,
        'coordinate out of bounds'
      );

      const imageOffsetInRows = (coords.z - origin.z) * rowsPerImage;
      const rowOffset = (imageOffsetInRows + (coords.y - origin.y)) * bytesPerRow;
      const offset = rowOffset + (coords.x - origin.x) * info.bytesPerBlock;

      // MAINTENANCE_TODO: To support block formats, decode the block and then index into the result.
      return subrectData.subarray(offset, offset + info.bytesPerBlock) as Uint8Array;
    });
  }

  /** Produces a TexelView from a generator of bytes for individual texel blocks. */
  static fromTexelsAsBytes(
    format: EncodableTextureFormat,
    generator: PerPixelAtLevel<Uint8Array>
  ): TexelView {
    const info = kTextureFormatInfo[format];
    assert(info.blockWidth === 1 && info.blockHeight === 1, 'unimplemented for block formats');

    const repr = kTexelRepresentationInfo[format];
    return new TexelView(format, {
      bytes: generator,
      ulpFromZero: coords => repr.bitsToULPFromZero(repr.unpackBits(generator(coords))),
      color: coords => repr.bitsToNumber(repr.unpackBits(generator(coords))),
    });
  }

  /** Produces a TexelView from a generator of numeric "color" values for each texel. */
  static fromTexelsAsColors(
    format: EncodableTextureFormat,
    generator: PerPixelAtLevel<PerTexelComponent<number>>,
    { clampToFormatRange = false }: { clampToFormatRange?: boolean } = {}
  ): TexelView {
    const info = kTextureFormatInfo[format];
    assert(info.blockWidth === 1 && info.blockHeight === 1, 'unimplemented for block formats');

    if (clampToFormatRange) {
      const applyClamp = makeClampToRange(format);
      const oldGenerator = generator;
      generator = coords => applyClamp(oldGenerator(coords));
    }

    const repr = kTexelRepresentationInfo[format];
    return new TexelView(format, {
      bytes: coords => new Uint8Array(repr.pack(repr.encode(generator(coords)))),
      ulpFromZero: coords => repr.bitsToULPFromZero(repr.numberToBits(generator(coords))),
      color: generator,
    });
  }

  /** Writes the contents of a TexelView as "linear image data", i.e. the `writeTexture` format. */
  writeTextureData(
    subrectData: Uint8Array | Uint8ClampedArray,
    {
      bytesPerRow,
      rowsPerImage,
      subrectOrigin: subrectOrigin_,
      subrectSize: subrectSize_,
    }: {
      bytesPerRow: number;
      rowsPerImage: number;
      subrectOrigin: GPUOrigin3D;
      subrectSize: GPUExtent3D;
    }
  ): void {
    const subrectOrigin = reifyOrigin3D(subrectOrigin_);
    const subrectSize = reifyExtent3D(subrectSize_);

    const info = kTextureFormatInfo[this.format];
    assert(info.blockWidth === 1 && info.blockHeight === 1, 'unimplemented for block formats');

    for (let z = subrectOrigin.z; z < subrectOrigin.z + subrectSize.depthOrArrayLayers; ++z) {
      for (let y = subrectOrigin.y; y < subrectOrigin.y + subrectSize.height; ++y) {
        for (let x = subrectOrigin.x; x < subrectOrigin.x + subrectSize.width; ++x) {
          const start = (z * rowsPerImage + y) * bytesPerRow + x * info.bytesPerBlock;
          memcpy({ src: this.bytes({ x, y, z }) }, { dst: subrectData, start });
        }
      }
    }
  }
}
