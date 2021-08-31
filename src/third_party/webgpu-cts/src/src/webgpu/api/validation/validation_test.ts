import { assert, unreachable } from '../../../common/framework/util/util.js';
import { BindableResource, kMaxQueryCount } from '../../capability_info.js';
import { GPUTest } from '../../gpu_test.js';

export const kRenderEncodeTypes = ['render pass', 'render bundle'] as const;
export type RenderEncodeType = typeof kRenderEncodeTypes[number];
export const kProgrammableEncoderTypes = ['compute pass', ...kRenderEncodeTypes] as const;
export type ProgrammableEncoderType = typeof kProgrammableEncoderTypes[number];
export const kEncoderTypes = ['non-pass', ...kProgrammableEncoderTypes] as const;
export type EncoderType = typeof kEncoderTypes[number];

export interface CommandBufferMaker<T extends EncoderType> {
  // Look up the type of the encoder based on `T`. If `T` is a union, this will be too!
  readonly encoder: {
    'non-pass': GPUCommandEncoder;
    'compute pass': GPUComputePassEncoder;
    'render pass': GPURenderPassEncoder;
    'render bundle': GPURenderBundleEncoder;
  }[T];
  finish(): GPUCommandBuffer;
}

export class ValidationTest extends GPUTest {
  createTextureWithState(
    state: 'valid' | 'invalid' | 'destroyed',
    descriptor?: Readonly<GPUTextureDescriptor>
  ): GPUTexture {
    descriptor = descriptor ?? {
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage:
        GPUTextureUsage.COPY_SRC |
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.SAMPLED |
        GPUTextureUsage.STORAGE |
        GPUTextureUsage.RENDER_ATTACHMENT,
    };

    switch (state) {
      case 'valid':
        return this.device.createTexture(descriptor);
      case 'invalid':
        return this.getErrorTexture();
      case 'destroyed': {
        const texture = this.device.createTexture(descriptor);
        texture.destroy();
        return texture;
      }
    }
  }

  createBufferWithState(
    state: 'valid' | 'invalid' | 'destroyed',
    descriptor?: Readonly<GPUBufferDescriptor>
  ): GPUBuffer {
    descriptor = descriptor ?? {
      size: 4,
      usage: GPUBufferUsage.VERTEX,
    };

    switch (state) {
      case 'valid':
        return this.device.createBuffer(descriptor);

      case 'invalid': {
        // Make the buffer invalid because of an invalid combination of usages but keep the
        // descriptor passed as much as possible (for mappedAtCreation and friends).
        this.device.pushErrorScope('validation');
        const buffer = this.device.createBuffer({
          ...descriptor,
          usage: descriptor.usage | GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_SRC,
        });
        this.device.popErrorScope();
        return buffer;
      }
      case 'destroyed': {
        const buffer = this.device.createBuffer(descriptor);
        buffer.destroy();
        return buffer;
      }
    }
  }

  createQuerySetWithState(
    state: 'valid' | 'invalid' | 'destroyed',
    descriptor?: Readonly<GPUQuerySetDescriptor>
  ): GPUQuerySet {
    descriptor = descriptor ?? {
      type: 'occlusion',
      count: 2,
    };

    switch (state) {
      case 'valid':
        return this.device.createQuerySet(descriptor);
      case 'invalid': {
        // Make the queryset invalid because of the count out of bounds.
        this.device.pushErrorScope('validation');
        const queryset = this.device.createQuerySet({
          type: 'occlusion',
          count: kMaxQueryCount + 1,
        });
        this.device.popErrorScope();
        return queryset;
      }
      case 'destroyed': {
        const queryset = this.device.createQuerySet(descriptor);
        queryset.destroy();
        return queryset;
      }
    }
  }

  getStorageBuffer(): GPUBuffer {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.STORAGE });
  }

  getUniformBuffer(): GPUBuffer {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.UNIFORM });
  }

  getErrorBuffer(): GPUBuffer {
    return this.createBufferWithState('invalid');
  }

  getErrorSampler(): GPUSampler {
    this.device.pushErrorScope('validation');
    const sampler = this.device.createSampler({ lodMinClamp: -1 });
    this.device.popErrorScope();
    return sampler;
  }

  getSampledTexture(sampleCount: number = 1): GPUTexture {
    return this.device.createTexture({
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
      sampleCount,
    });
  }

  getStorageTexture(): GPUTexture {
    return this.device.createTexture({
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.STORAGE,
    });
  }

  getRenderTexture(): GPUTexture {
    return this.device.createTexture({
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });
  }

  getErrorTexture(): GPUTexture {
    this.device.pushErrorScope('validation');
    const texture = this.device.createTexture({
      size: { width: 0, height: 0, depthOrArrayLayers: 0 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
    });
    this.device.popErrorScope();
    return texture;
  }

  getErrorTextureView(): GPUTextureView {
    this.device.pushErrorScope('validation');
    const view = this.getErrorTexture().createView();
    this.device.popErrorScope();
    return view;
  }

  getBindingResource(bindingType: BindableResource): GPUBindingResource {
    switch (bindingType) {
      case 'errorBuf':
        return { buffer: this.getErrorBuffer() };
      case 'errorSamp':
        return this.getErrorSampler();
      case 'errorTex':
        return this.getErrorTextureView();
      case 'uniformBuf':
        return { buffer: this.getUniformBuffer() };
      case 'storageBuf':
        return { buffer: this.getStorageBuffer() };
      case 'filtSamp':
        return this.device.createSampler({ minFilter: 'linear' });
      case 'nonFiltSamp':
        return this.device.createSampler();
      case 'compareSamp':
        return this.device.createSampler({ compare: 'never' });
      case 'sampledTex':
        return this.getSampledTexture(1).createView();
      case 'sampledTexMS':
        return this.getSampledTexture(4).createView();
      case 'storageTex':
        return this.getStorageTexture().createView();
    }
  }

  createNoOpRenderPipeline(): GPURenderPipeline {
    return this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: '[[stage(vertex)]] fn main() {}',
        }),
        entryPoint: 'main',
      },
      fragment: {
        module: this.device.createShaderModule({
          code: '[[stage(fragment)]] fn main() {}',
        }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },
      primitive: { topology: 'triangle-list' },
    });
  }

  createNoOpComputePipeline(): GPUComputePipeline {
    return this.device.createComputePipeline({
      compute: {
        module: this.device.createShaderModule({
          code: '[[stage(compute)]] fn main() {}',
        }),
        entryPoint: 'main',
      },
    });
  }

  createErrorComputePipeline(): GPUComputePipeline {
    this.device.pushErrorScope('validation');
    const pipeline = this.device.createComputePipeline({
      compute: {
        module: this.device.createShaderModule({
          code: '',
        }),
        entryPoint: '',
      },
    });
    this.device.popErrorScope();
    return pipeline;
  }

  createEncoder<T extends EncoderType>(encoderType: T): CommandBufferMaker<T> {
    const colorFormat = 'rgba8unorm';
    switch (encoderType) {
      case 'non-pass': {
        const encoder = this.device.createCommandEncoder();
        return {
          encoder,
          finish: () => {
            return encoder.finish();
          },
        } as CommandBufferMaker<T>;
      }
      case 'render bundle': {
        const device = this.device;
        const encoder = device.createRenderBundleEncoder({
          colorFormats: [colorFormat],
        });
        const pass = this.createEncoder('render pass');
        return {
          encoder,
          finish: () => {
            const bundle = encoder.finish();
            pass.encoder.executeBundles([bundle]);
            return pass.finish();
          },
        } as CommandBufferMaker<T>;
      }
      case 'compute pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginComputePass();
        return {
          encoder,
          finish: () => {
            encoder.endPass();
            return commandEncoder.finish();
          },
        } as CommandBufferMaker<T>;
      }
      case 'render pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const view = this.device
          .createTexture({
            format: colorFormat,
            size: { width: 16, height: 16, depthOrArrayLayers: 1 },
            usage: GPUTextureUsage.RENDER_ATTACHMENT,
          })
          .createView();
        const encoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view,
              loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
              storeOp: 'store',
            },
          ],
        });
        return {
          encoder,
          finish: () => {
            encoder.endPass();
            return commandEncoder.finish();
          },
        } as CommandBufferMaker<T>;
      }
    }
    unreachable();
  }

  /**
   * Expect a validation error inside the callback.
   *
   * Tests should always do just one WebGPU call in the callback, to make sure that's what's tested.
   */
  // Note: A return value is not allowed for the callback function. This is to avoid confusion
  // about what the actual behavior would be. We could either:
  //   - Make expectValidationError async, and have it await on fn(). This causes an async split
  //     between pushErrorScope and popErrorScope, so if the caller doesn't `await` on
  //     expectValidationError (either accidentally or because it doesn't care to do so), then
  //     other test code will be (nondeterministically) caught by the error scope.
  //   - Make expectValidationError NOT await fn(), but just execute its first block (until the
  //     first await) and return the return value (a Promise). This would be confusing because it
  //     would look like the error scope includes the whole async function, but doesn't.
  expectValidationError(fn: () => void, shouldError: boolean = true): void {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (shouldError) {
      this.device.pushErrorScope('validation');
    }

    const returnValue = fn() as unknown;
    assert(
      returnValue === undefined,
      'expectValidationError callback should not return a value (or be async)'
    );

    if (shouldError) {
      const promise = this.device.popErrorScope();

      this.eventualAsyncExpectation(async niceStack => {
        const gpuValidationError = await promise;
        if (!gpuValidationError) {
          niceStack.message = 'Validation succeeded unexpectedly.';
          this.rec.validationFailed(niceStack);
        } else if (gpuValidationError instanceof GPUValidationError) {
          niceStack.message = `Validation failed, as expected - ${gpuValidationError.message}`;
          this.rec.debug(niceStack);
        }
      });
    }
  }
}
