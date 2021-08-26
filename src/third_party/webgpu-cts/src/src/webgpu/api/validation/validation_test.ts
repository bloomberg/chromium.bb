import { assert, unreachable } from '../../../common/util/util.js';
import { BindableResource, kMaxQueryCount } from '../../capability_info.js';
import { GPUTest } from '../../gpu_test.js';

import { CommandBufferMaker, EncoderType } from './util/command_buffer_maker.js';

const kResourceStateValues = ['valid', 'invalid', 'destroyed'] as const;
export type ResourceState = typeof kResourceStateValues[number];
export const kResourceStates: readonly ResourceState[] = kResourceStateValues;

/**
 * Base fixture for WebGPU validation tests.
 */
export class ValidationTest extends GPUTest {
  /**
   * Create a GPUTexture in the specified state.
   * A `descriptor` may optionally be passed, which is used when `state` is not `'invalid'`.
   */
  createTextureWithState(
    state: ResourceState,
    descriptor?: Readonly<GPUTextureDescriptor>
  ): GPUTexture {
    descriptor = descriptor ?? {
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage:
        GPUTextureUsage.COPY_SRC |
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.STORAGE_BINDING |
        GPUTextureUsage.RENDER_ATTACHMENT,
    };

    switch (state) {
      case 'valid':
        return this.trackForCleanup(this.device.createTexture(descriptor));
      case 'invalid':
        return this.getErrorTexture();
      case 'destroyed': {
        const texture = this.device.createTexture(descriptor);
        texture.destroy();
        return texture;
      }
    }
  }

  /**
   * Create a GPUTexture in the specified state. A `descriptor` may optionally be passed;
   * if `state` is `'invalid'`, it will be modified to add an invalid combination of usages.
   */
  createBufferWithState(
    state: ResourceState,
    descriptor?: Readonly<GPUBufferDescriptor>
  ): GPUBuffer {
    descriptor = descriptor ?? {
      size: 4,
      usage: GPUBufferUsage.VERTEX,
    };

    switch (state) {
      case 'valid':
        return this.trackForCleanup(this.device.createBuffer(descriptor));

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

  /**
   * Create a GPUQuerySet in the specified state.
   * A `descriptor` may optionally be passed, which is used when `state` is not `'invalid'`.
   */
  createQuerySetWithState(
    state: ResourceState,
    desc?: Readonly<GPUQuerySetDescriptor>
  ): GPUQuerySet {
    const descriptor = { type: 'occlusion' as const, count: 2, ...desc };

    switch (state) {
      case 'valid':
        return this.trackForCleanup(this.device.createQuerySet(descriptor));
      case 'invalid': {
        // Make the queryset invalid because of the count out of bounds.
        descriptor.count = kMaxQueryCount + 1;
        return this.expectGPUError('validation', () => this.device.createQuerySet(descriptor));
      }
      case 'destroyed': {
        const queryset = this.device.createQuerySet(descriptor);
        queryset.destroy();
        return queryset;
      }
    }
  }

  /** Create an arbitrarily-sized GPUBuffer with the STORAGE usage. */
  getStorageBuffer(): GPUBuffer {
    return this.trackForCleanup(
      this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.STORAGE })
    );
  }

  /** Create an arbitrarily-sized GPUBuffer with the UNIFORM usage. */
  getUniformBuffer(): GPUBuffer {
    return this.trackForCleanup(
      this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.UNIFORM })
    );
  }

  /** Return an invalid GPUBuffer. */
  getErrorBuffer(): GPUBuffer {
    return this.createBufferWithState('invalid');
  }

  /** Return an invalid GPUSampler. */
  getErrorSampler(): GPUSampler {
    this.device.pushErrorScope('validation');
    const sampler = this.device.createSampler({ lodMinClamp: -1 });
    this.device.popErrorScope();
    return sampler;
  }

  /**
   * Return an arbitrarily-configured GPUTexture with the `SAMPLED` usage and specified sampleCount.
   */
  getSampledTexture(sampleCount: number = 1): GPUTexture {
    return this.trackForCleanup(
      this.device.createTexture({
        size: { width: 16, height: 16, depthOrArrayLayers: 1 },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.TEXTURE_BINDING,
        sampleCount,
      })
    );
  }

  /** Return an arbitrarily-configured GPUTexture with the `STORAGE` usage. */
  getStorageTexture(): GPUTexture {
    return this.trackForCleanup(
      this.device.createTexture({
        size: { width: 16, height: 16, depthOrArrayLayers: 1 },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.STORAGE_BINDING,
      })
    );
  }

  /** Return an arbitrarily-configured GPUTexture with the `RENDER_ATTACHMENT` usage. */
  getRenderTexture(): GPUTexture {
    return this.trackForCleanup(
      this.device.createTexture({
        size: { width: 16, height: 16, depthOrArrayLayers: 1 },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      })
    );
  }

  /** Return an invalid GPUTexture. */
  getErrorTexture(): GPUTexture {
    this.device.pushErrorScope('validation');
    const texture = this.device.createTexture({
      size: { width: 0, height: 0, depthOrArrayLayers: 0 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.TEXTURE_BINDING,
    });
    this.device.popErrorScope();
    return texture;
  }

  /** Return an invalid GPUTextureView (created from an invalid GPUTexture). */
  getErrorTextureView(): GPUTextureView {
    this.device.pushErrorScope('validation');
    const view = this.getErrorTexture().createView();
    this.device.popErrorScope();
    return view;
  }

  /**
   * Return an arbitrary object of the specified {@link BindableResource} type
   * (e.g. `'errorBuf'`, `'nonFiltSamp'`, `sampledTexMS`, etc.)
   */
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

  /** Create a GPURenderPipeline in the specified state. */
  createRenderPipelineWithState(state: 'valid' | 'invalid'): GPURenderPipeline {
    return state === 'valid' ? this.createNoOpRenderPipeline() : this.createErrorRenderPipeline();
  }

  /** Return a GPURenderPipeline with default options and no-op vertex and fragment shaders. */
  createNoOpRenderPipeline(): GPURenderPipeline {
    return this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: `[[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
  return vec4<f32>();
}`,
        }),
        entryPoint: 'main',
      },
      fragment: {
        module: this.device.createShaderModule({
          code: '[[stage(fragment)]] fn main() {}',
        }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm', writeMask: 0 }],
      },
      primitive: { topology: 'triangle-list' },
    });
  }

  /** Return an invalid GPURenderPipeline. */
  createErrorRenderPipeline(): GPURenderPipeline {
    this.device.pushErrorScope('validation');
    const pipeline = this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: '',
        }),
        entryPoint: '',
      },
    });
    this.device.popErrorScope();
    return pipeline;
  }

  /** Return a GPUComputePipeline with a no-op shader. */
  createNoOpComputePipeline(layout?: GPUPipelineLayout): GPUComputePipeline {
    return this.device.createComputePipeline({
      layout,
      compute: {
        module: this.device.createShaderModule({
          code: '[[stage(compute), workgroup_size(1)]] fn main() {}',
        }),
        entryPoint: 'main',
      },
    });
  }

  /** Return an invalid GPUComputePipeline. */
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

  /**
   * Returns a GPUCommandEncoder, GPUComputePassEncoder, GPURenderPassEncoder, or
   * GPURenderBundleEncoder, and a `finish` method returning a GPUCommandBuffer.
   * Allows testing methods which have the same signature across multiple encoder interfaces.
   *
   * @example
   * ```
   * g.test('popDebugGroup')
   *   .params(u => u.combine('encoderType', kEncoderTypes))
   *   .fn(t => {
   *     const { encoder, finish } = t.createEncoder(t.params.encoderType);
   *     encoder.popDebugGroup();
   *   });
   *
   * g.test('writeTimestamp')
   *   .params(u => u.combine('encoderType', ['non-pass', 'compute pass', 'render pass'] as const)
   *   .fn(t => {
   *     const { encoder, finish } = t.createEncoder(t.params.encoderType);
   *     // Encoder type is inferred, so `writeTimestamp` can be used even though it doesn't exist
   *     // on GPURenderBundleEncoder.
   *     encoder.writeTimestamp(args);
   *   });
   * ```
   */
  createEncoder<T extends EncoderType>(
    encoderType: T,
    {
      attachmentInfo,
      occlusionQuerySet,
    }: {
      attachmentInfo?: GPURenderBundleEncoderDescriptor;
      occlusionQuerySet?: GPUQuerySet;
    } = {}
  ): CommandBufferMaker<T> {
    const fullAttachmentInfo = {
      // Defaults if not overridden:
      colorFormats: ['rgba8unorm'],
      sampleCount: 1,
      // Passed values take precedent.
      ...attachmentInfo,
    } as const;

    switch (encoderType) {
      case 'non-pass': {
        const encoder = this.device.createCommandEncoder();

        return new CommandBufferMaker(this, encoder, (shouldSucceed: boolean) =>
          this.expectGPUError('validation', () => encoder.finish(), !shouldSucceed)
        );
      }
      case 'render bundle': {
        const device = this.device;
        const rbEncoder = device.createRenderBundleEncoder(fullAttachmentInfo);
        const pass = this.createEncoder('render pass', { attachmentInfo });

        return new CommandBufferMaker(this, rbEncoder, (shouldSucceed: boolean) => {
          // If !shouldSucceed, the resulting bundle should be invalid.
          const rb = this.expectGPUError('validation', () => rbEncoder.finish(), !shouldSucceed);
          pass.encoder.executeBundles([rb]);
          // Then, the pass should also be invalid if the bundle was invalid.
          return pass.validateFinish(shouldSucceed);
        });
      }
      case 'compute pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginComputePass();

        return new CommandBufferMaker(this, encoder, (shouldSucceed: boolean) => {
          encoder.endPass();
          return this.expectGPUError('validation', () => commandEncoder.finish(), !shouldSucceed);
        });
      }
      case 'render pass': {
        const makeAttachmentView = (format: GPUTextureFormat) =>
          this.trackForCleanup(
            this.device.createTexture({
              size: [16, 16, 1],
              format,
              usage: GPUTextureUsage.RENDER_ATTACHMENT,
              sampleCount: fullAttachmentInfo.sampleCount,
            })
          ).createView();

        const passDesc: GPURenderPassDescriptor = {
          colorAttachments: Array.from(fullAttachmentInfo.colorFormats, format => ({
            view: makeAttachmentView(format),
            loadValue: [0, 0, 0, 0],
            storeOp: 'store',
          })),
          depthStencilAttachment:
            fullAttachmentInfo.depthStencilFormat !== undefined
              ? {
                  view: makeAttachmentView(fullAttachmentInfo.depthStencilFormat),
                  depthLoadValue: 0,
                  depthStoreOp: 'discard',
                  stencilLoadValue: 1,
                  stencilStoreOp: 'discard',
                }
              : undefined,
          occlusionQuerySet,
        };

        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginRenderPass(passDesc);
        return new CommandBufferMaker(this, encoder, (shouldSucceed: boolean) => {
          encoder.endPass();
          return this.expectGPUError('validation', () => commandEncoder.finish(), !shouldSucceed);
        });
      }
    }
    unreachable();
  }

  /**
   * Expect a validation error inside the callback.
   *
   * Tests should always do just one WebGPU call in the callback, to make sure that's what's tested.
   */
  expectValidationError(fn: () => void, shouldError: boolean = true): void {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (shouldError) {
      this.device.pushErrorScope('validation');
    }

    // Note: A return value is not allowed for the callback function. This is to avoid confusion
    // about what the actual behavior would be; either of the following could be reasonable:
    //   - Make expectValidationError async, and have it await on fn(). This causes an async split
    //     between pushErrorScope and popErrorScope, so if the caller doesn't `await` on
    //     expectValidationError (either accidentally or because it doesn't care to do so), then
    //     other test code will be (nondeterministically) caught by the error scope.
    //   - Make expectValidationError NOT await fn(), but just execute its first block (until the
    //     first await) and return the return value (a Promise). This would be confusing because it
    //     would look like the error scope includes the whole async function, but doesn't.
    // If we do decide we need to return a value, we should use the latter semantic.
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
