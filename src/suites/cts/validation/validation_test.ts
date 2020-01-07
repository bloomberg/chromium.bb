import { unreachable } from '../../../framework/index.js';
import { bindingTypeInfo } from '../format_info.js';
import { GPUTest } from '../gpu_test.js';

export enum BindingResourceType {
  'uniform-buffer' = 'uniform-buffer',
  'storage-buffer' = 'storage-buffer',
  'sampler' = 'sampler',
  'sampled-texture' = 'sampled-texture',
  'storage-texture' = 'storage-texture',
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

  getBindingResource(bindingType: BindingResourceType, error: boolean = false): GPUBindingResource {
    if (error) {
      const objectType = bindingTypeInfo[bindingType].type;
      if (objectType === 'buffer') {
        return { buffer: this.getErrorBuffer() };
      } else if (objectType === 'sampler') {
        return this.getErrorSampler();
      } else if (objectType === 'texture') {
        return this.getErrorTextureView();
      } else {
        unreachable('unknown binding resource type');
      }
    } else {
      if (bindingType === 'uniform-buffer') {
        return { buffer: this.getUniformBuffer() };
      } else if (bindingType === 'storage-buffer') {
        return { buffer: this.getStorageBuffer() };
      } else if (bindingType === 'sampler') {
        return this.getSampler();
      } else if (bindingType === 'sampled-texture') {
        return this.getSampledTexture().createView();
      } else if (bindingType === 'storage-texture') {
        return this.getStorageTexture().createView();
      } else {
        unreachable('unknown binding resource type');
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
        this.rec.fail(niceStack);
      } else if (gpuValidationError instanceof GPUValidationError) {
        niceStack.message = `Captured validation error - ${gpuValidationError.message}`;
        this.rec.debug(niceStack);
      }
    });
  }
}
