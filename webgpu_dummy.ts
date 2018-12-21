import w from "./webgpu";

const kNoExtensions: w.WebGPUExtensions = {
  anisotropicFiltering: false,
};
const kDefaultLimits: w.WebGPULimits = {
  maxBindGroups: 0,
};

class BindGroup {}
class BindGroupLayout {}
class Buffer {}
class CommandBuffer {}
class ComputePipeline {}
class Fence {}
class PipelineLayout {}
class RenderPipeline {}
class Sampler {}
class ShaderModule {}
class Texture {}
class ObjectStatusQuery {}
class Queue {}

class Device implements w.WebGPUDevice {
  adapter: Adapter;
  extensions: w.WebGPUExtensions;
  limits = kDefaultLimits;
  onLog: w.WebGPULogCallback | undefined;

  constructor(adapter: Adapter, descriptor: w.WebGPUDeviceDescriptor) {
    this.adapter = adapter;
    this.extensions = descriptor.extensions || kNoExtensions;
  }

  createBindGroup(descriptor: w.WebGPUBindGroupDescriptor): BindGroup {
    return new BindGroup(descriptor);
  }
  createBindGroupLayout(descriptor: w.WebGPUBindGroupLayoutDescriptor): BindGroupLayout {
  }
  createBuffer(descriptor: w.WebGPUBufferDescriptor): Buffer {
  }
  createCommandBuffer(descriptor: w.WebGPUCommandBufferDescriptor): CommandBuffer {
  }
  createComputePipeline(descriptor: w.WebGPUComputePipelineDescriptor): ComputePipeline {
  }
  createFence(descriptor: w.WebGPUFenceDescriptor): Fence {
  }
  createPipelineLayout(descriptor: w.WebGPUPipelineLayoutDescriptor): PipelineLayout {
  }
  createRenderPipeline(descriptor: w.WebGPURenderPipelineDescriptor): RenderPipeline {
  }
  createSampler(descriptor: w.WebGPUSamplerDescriptor): Sampler {
  }
  createShaderModule(descriptor: w.WebGPUShaderModuleDescriptor): ShaderModule {
  }
  createTexture(descriptor: w.WebGPUTextureDescriptor): Texture {
  }
  getObjectStatus(statusableObject: w.WebGPUStatusableObject): ObjectStatusQuery {
  }
  getQueue(): Queue {
  }
}

class Adapter implements w.WebGPUAdapter {
  extensions = kNoExtensions;
  name = 'dummy';
  createDevice(descriptor: w.WebGPUDeviceDescriptor): w.WebGPUDevice {
    return new Device(this, descriptor);
  }
}

export namespace gpu {
  export function requestAdapter(options?: w.WebGPURequestAdapterOptions): Promise<w.WebGPUAdapter> {
    return Promise.resolve(new Adapter());
  }
}
