import { unreachable } from '../../../framework/index.js';
import { GPUTest } from '../gpu_test.js';

export enum BindingResourceType {
  'error-buffer' = 'error-buffer',
  'error-sampler' = 'error-sampler',
  'error-textureview' = 'error-textureview',
  'uniform-buffer' = 'uniform-buffer',
  'storage-buffer' = 'storage-buffer',
  'sampler' = 'sampler',
  'sampled-textureview' = 'sampled-textureview',
  'storage-textureview' = 'storage-textureview',
}

export function resourceBindingMatches(b: GPUBindingType, r: BindingResourceType): boolean {
  switch (b) {
    case 'storage-buffer':
    case 'readonly-storage-buffer':
      return r === 'storage-buffer';
    case 'sampled-texture':
      return r === 'sampled-textureview';
    case 'sampler':
      return r === 'sampler';
    case 'storage-texture':
      return r === 'storage-textureview';
    case 'uniform-buffer':
      return r === 'uniform-buffer';
    default:
      unreachable('unknown GPUBindingType');
  }
}

export class ValidationTest extends GPUTest {
  getStorageBuffer(): GPUBuffer {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.STORAGE });
  }

  getUniformBuffer(): GPUBuffer {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.UNIFORM });
  }

  getErrorBuffer(): GPUBuffer {
    this.device.pushErrorScope('validation');
    const errorBuffer = this.device.createBuffer({
      size: 1024,
      usage: 0xffff, // Invalid GPUBufferUsage
    });
    this.device.popErrorScope();
    return errorBuffer;
  }

  getSampler(): GPUSampler {
    return this.device.createSampler();
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

  getErrorTextureView(): GPUTextureView {
    this.device.pushErrorScope('validation');
    const view = this.device
      .createTexture({
        size: { width: 0, height: 0, depth: 0 },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.SAMPLED,
      })
      .createView();
    this.device.popErrorScope();
    return view;
  }

  getBindingResource(bindingType: BindingResourceType): GPUBindingResource {
    switch (bindingType) {
      case 'error-buffer':
        return { buffer: this.getErrorBuffer() };
      case 'error-sampler':
        return this.getErrorSampler();
      case 'error-textureview':
        return this.getErrorTextureView();
      case 'uniform-buffer':
        return { buffer: this.getUniformBuffer() };
      case 'storage-buffer':
        return { buffer: this.getStorageBuffer() };
      case 'sampler':
        return this.getSampler();
      case 'sampled-textureview':
        return this.getSampledTexture().createView();
      case 'storage-textureview':
        return this.getStorageTexture().createView();
      default:
        unreachable('unknown binding resource type');
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
        this.rec.fail(niceStack);
      } else if (gpuValidationError instanceof GPUValidationError) {
        niceStack.message = `Captured validation error - ${gpuValidationError.message}`;
        this.rec.debug(niceStack);
      }
    });
  }
}
