import { unreachable } from '../../../common/framework/util/util.js';
import { BindableResource } from '../../capability_info.js';
import { GPUTest } from '../../gpu_test.js';

type Encoder = GPUCommandEncoder | GPUProgrammablePassEncoder | GPURenderBundleEncoder;
export const kEncoderTypes = ['non-pass', 'compute pass', 'render pass', 'render bundle'] as const;
type EncoderType = typeof kEncoderTypes[number];

interface CommandBufferMaker<E extends Encoder> {
  readonly encoder: E;
  finish(): GPUCommandBuffer;
}

export class ValidationTest extends GPUTest {
  createTextureWithState(
    state: 'valid' | 'invalid' | 'destroyed',
    descriptor?: Readonly<GPUTextureDescriptor>
  ): GPUTexture {
    descriptor = descriptor ?? {
      size: { width: 1, height: 1, depth: 1 },
      format: 'rgba8unorm',
      usage:
        GPUTextureUsage.COPY_SRC |
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.SAMPLED |
        GPUTextureUsage.STORAGE |
        GPUTextureUsage.OUTPUT_ATTACHMENT,
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
      case 'invalid':
        // Make the buffer invalid because of an invalid combination of usages but keep the
        // descriptor passed as much as possible (for mappedAtCreation and friends).
        return this.device.createBuffer({
          ...descriptor,
          usage: descriptor.usage | GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_SRC,
        });
      case 'destroyed': {
        const buffer = this.device.createBuffer(descriptor);
        buffer.destroy();
        return buffer;
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

  getSampler(): GPUSampler {
    return this.device.createSampler();
  }

  getComparisonSampler(): GPUSampler {
    return this.device.createSampler({ compare: 'never' });
  }

  getErrorSampler(): GPUSampler {
    this.device.pushErrorScope('validation');
    const sampler = this.device.createSampler({ lodMinClamp: -1 });
    this.device.popErrorScope();
    return sampler;
  }

  getSampledTexture(): GPUTexture {
    return this.device.createTexture({
      size: { width: 16, height: 16, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
    });
  }

  getStorageTexture(): GPUTexture {
    return this.device.createTexture({
      size: { width: 16, height: 16, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.STORAGE,
    });
  }

  getErrorTexture(): GPUTexture {
    this.device.pushErrorScope('validation');
    const texture = this.device.createTexture({
      size: { width: 0, height: 0, depth: 0 },
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
      case 'plainSamp':
        return this.getSampler();
      case 'compareSamp':
        return this.getComparisonSampler();
      case 'sampledTex':
        return this.getSampledTexture().createView();
      case 'storageTex':
        return this.getStorageTexture().createView();
      default:
        unreachable('unknown binding resource type');
    }
  }

  createNoOpRenderPipeline(): GPURenderPipeline {
    const wgslVertex = `
      fn main() -> void {
        return;
      }

      entry_point vertex = main;
    `;
    const wgslFragment = `
      fn main() -> void {
        return;
      }

      entry_point fragment = main;
    `;

    return this.device.createRenderPipeline({
      vertexStage: {
        module: this.device.createShaderModule({
          code: wgslVertex,
        }),
        entryPoint: 'main',
      },
      fragmentStage: {
        module: this.device.createShaderModule({
          code: wgslFragment,
        }),
        entryPoint: 'main',
      },
      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
    });
  }

  createNoOpComputePipeline(): GPUComputePipeline {
    const wgslCompute = `
      fn main() -> void {
        return;
      }

      entry_point compute = main;
    `;

    return this.device.createComputePipeline({
      computeStage: {
        module: this.device.createShaderModule({
          code: wgslCompute,
        }),
        entryPoint: 'main',
      },
    });
  }

  createErrorComputePipeline(): GPUComputePipeline {
    this.device.pushErrorScope('validation');
    const pipeline = this.device.createComputePipeline({
      computeStage: {
        module: this.device.createShaderModule({
          code: '',
        }),
        entryPoint: '',
      },
    });
    this.device.popErrorScope();
    return pipeline;
  }

  createEncoder(encoderType: 'non-pass'): CommandBufferMaker<GPUCommandEncoder>;
  createEncoder(encoderType: 'render pass'): CommandBufferMaker<GPURenderPassEncoder>;
  createEncoder(encoderType: 'compute pass'): CommandBufferMaker<GPUComputePassEncoder>;
  createEncoder(encoderType: 'render bundle'): CommandBufferMaker<GPURenderBundleEncoder>;
  createEncoder(
    encoderType: 'render pass' | 'render bundle'
  ): CommandBufferMaker<GPURenderPassEncoder | GPURenderBundleEncoder>;
  createEncoder(
    encoderType: 'compute pass' | 'render pass' | 'render bundle'
  ): CommandBufferMaker<GPUProgrammablePassEncoder>;
  createEncoder(encoderType: EncoderType): CommandBufferMaker<Encoder>;
  createEncoder(encoderType: EncoderType): CommandBufferMaker<Encoder> {
    const colorFormat = 'rgba8unorm';
    switch (encoderType) {
      case 'non-pass': {
        const encoder = this.device.createCommandEncoder();
        return {
          encoder,

          finish: () => {
            return encoder.finish();
          },
        };
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
        };
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
        };
      }
      case 'render pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const attachment = this.device
          .createTexture({
            format: colorFormat,
            size: { width: 16, height: 16, depth: 1 },
            usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
          })
          .createView();
        const encoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              attachment,
              loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
            },
          ],
        });
        return {
          encoder,
          finish: () => {
            encoder.endPass();
            return commandEncoder.finish();
          },
        };
      }
    }
  }

  expectValidationError(fn: Function, shouldError: boolean = true): void {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (shouldError === false) {
      fn();
      return;
    }

    this.device.pushErrorScope('validation');
    fn();
    const promise = this.device.popErrorScope();

    this.eventualAsyncExpectation(async niceStack => {
      const gpuValidationError = await promise;
      if (!gpuValidationError) {
        niceStack.message = 'Validation error was expected.';
        this.rec.validationFailed(niceStack);
      } else if (gpuValidationError instanceof GPUValidationError) {
        niceStack.message = `Captured validation error - ${gpuValidationError.message}`;
        this.rec.debug(niceStack);
      }
    });
  }
}
