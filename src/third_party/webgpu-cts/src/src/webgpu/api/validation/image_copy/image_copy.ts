import { poptions } from '../../../../common/framework/params_builder.js';
import { kSizedTextureFormatInfo, SizedTextureFormat } from '../../../capability_info.js';
import { ImageCopyType } from '../../../util/texture/image_copy.js';
import { ValidationTest } from '../validation_test.js';

export class ImageCopyTest extends ValidationTest {
  testRun(
    textureCopyView: GPUTextureCopyView,
    textureDataLayout: GPUTextureDataLayout,
    size: GPUExtent3D,
    {
      method,
      dataSize,
      success,
      submit = false,
    }: {
      method: ImageCopyType;
      dataSize: number;
      success: boolean;
      /** If submit is true, the validaton error is expected to come from the submit and encoding
       * should succeed. */
      submit?: boolean;
    }
  ): void {
    switch (method) {
      case 'WriteTexture': {
        const data = new Uint8Array(dataSize);

        this.expectValidationError(() => {
          this.device.queue.writeTexture(textureCopyView, data, textureDataLayout, size);
        }, !success);

        break;
      }
      case 'CopyB2T': {
        const buffer = this.device.createBuffer({
          size: dataSize,
          usage: GPUBufferUsage.COPY_SRC,
        });

        const encoder = this.device.createCommandEncoder();
        encoder.copyBufferToTexture({ buffer, ...textureDataLayout }, textureCopyView, size);

        if (submit) {
          const cmd = encoder.finish();
          this.expectValidationError(() => {
            this.device.queue.submit([cmd]);
          }, !success);
        } else {
          this.expectValidationError(() => {
            encoder.finish();
          }, !success);
        }

        break;
      }
      case 'CopyT2B': {
        const buffer = this.device.createBuffer({
          size: dataSize,
          usage: GPUBufferUsage.COPY_DST,
        });

        const encoder = this.device.createCommandEncoder();
        encoder.copyTextureToBuffer(textureCopyView, { buffer, ...textureDataLayout }, size);

        if (submit) {
          const cmd = encoder.finish();
          this.expectValidationError(() => {
            this.device.queue.submit([cmd]);
          }, !success);
        } else {
          this.expectValidationError(() => {
            encoder.finish();
          }, !success);
        }

        break;
      }
    }
  }

  // This is a helper function used for creating a texture when we don't have to be very
  // precise about its size as long as it's big enough and properly aligned.
  createAlignedTexture(
    format: SizedTextureFormat,
    copySize: Required<GPUExtent3DDict> = { width: 1, height: 1, depthOrArrayLayers: 1 },
    origin: Required<GPUOrigin3DDict> = { x: 0, y: 0, z: 0 }
  ): GPUTexture {
    const info = kSizedTextureFormatInfo[format];
    return this.device.createTexture({
      size: {
        width: Math.max(1, copySize.width + origin.x) * info.blockWidth,
        height: Math.max(1, copySize.height + origin.y) * info.blockHeight,
        depthOrArrayLayers: Math.max(1, copySize.depthOrArrayLayers + origin.z),
      },
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });
  }
}

// For testing divisibility by a number we test all the values returned by this function:
function valuesToTestDivisibilityBy(number: number): Iterable<number> {
  const values = [];
  for (let i = 0; i <= 2 * number; ++i) {
    values.push(i);
  }
  values.push(3 * number);
  return values;
}

interface WithFormat {
  format: SizedTextureFormat;
}

interface WithFormatAndCoordinate extends WithFormat {
  coordinateToTest: keyof GPUOrigin3DDict | keyof GPUExtent3DDict;
}

interface WithFormatAndMethod extends WithFormat {
  method: string;
}

// This is a helper function used for expanding test parameters for texel block alignment tests on offset
export function texelBlockAlignmentTestExpanderForOffset({ format }: WithFormat) {
  return poptions(
    'offset',
    valuesToTestDivisibilityBy(kSizedTextureFormatInfo[format].bytesPerBlock)
  );
}

// This is a helper function used for expanding test parameters for texel block alignment tests on rowsPerImage
export function texelBlockAlignmentTestExpanderForRowsPerImage({ format }: WithFormat) {
  return poptions(
    'rowsPerImage',
    valuesToTestDivisibilityBy(kSizedTextureFormatInfo[format].blockHeight)
  );
}

// This is a helper function used for expanding test parameters for texel block alignment tests on origin and size
export function texelBlockAlignmentTestExpanderForValueToCoordinate({
  format,
  coordinateToTest,
}: WithFormatAndCoordinate) {
  switch (coordinateToTest) {
    case 'x':
    case 'width':
      return poptions(
        'valueToCoordinate',
        valuesToTestDivisibilityBy(kSizedTextureFormatInfo[format].blockWidth!)
      );

    case 'y':
    case 'height':
      return poptions(
        'valueToCoordinate',
        valuesToTestDivisibilityBy(kSizedTextureFormatInfo[format].blockHeight!)
      );

    case 'z':
    case 'depthOrArrayLayers':
      return poptions('valueToCoordinate', valuesToTestDivisibilityBy(1));
  }
}

// This is a helper function used for filtering test parameters
export function formatCopyableWithMethod({ format, method }: WithFormatAndMethod): boolean {
  if (method === 'CopyTextureToBuffer') {
    return kSizedTextureFormatInfo[format].copySrc;
  } else {
    return kSizedTextureFormatInfo[format].copyDst;
  }
}
