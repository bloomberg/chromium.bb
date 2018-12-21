import * as w from "./webgpu";

const kNoExtensions: w.WebGPUExtensions = {
  anisotropicFiltering: false,
};
const kDefaultLimits: w.WebGPULimits = {
  maxBindGroups: 0,
};

class BindGroup implements w.WebGPUBindGroup {
}
class BindGroupLayout implements w.WebGPUBindGroupLayout {
}
class Buffer implements w.WebGPUBuffer {
  public readonly mapping: ArrayBuffer | null = null;

  constructor() {}
  public destroy() {}
  public unmap() {}
}
class CommandBuffer implements w.WebGPUCommandBuffer {
  public label: string | undefined;
  public beginComputePass(): w.WebGPUComputePassEncoder {
    return new ComputePassEncoder(this);
  }
  public beginRenderPass(descriptor: w.WebGPURenderPassDescriptor): w.WebGPURenderPassEncoder {
    return new RenderPassEncoder(this);
  }
  public blit(): void {}
  public copyBufferToBuffer(src: w.WebGPUBuffer, srcOffset: number, dst: w.WebGPUBuffer, dstOffset: number, size: number): void {}
  public copyBufferToTexture(source: w.WebGPUBufferCopyView, destination: w.WebGPUTextureCopyView, copySize: w.WebGPUExtent3D): void {}
  public copyTextureToBuffer(source: w.WebGPUTextureCopyView, destination: w.WebGPUBufferCopyView, copySize: w.WebGPUExtent3D): void {}
  public copyTextureToTexture(source: w.WebGPUTextureCopyView, destination: w.WebGPUTextureCopyView, copySize: w.WebGPUExtent3D): void {}
}
class ProgrammablePassEncoder {
  private commandBuffer: CommandBuffer;
  constructor(commandBuffer: CommandBuffer) {
    this.commandBuffer = commandBuffer;
  }
  public endPass(): w.WebGPUCommandBuffer {
    return this.commandBuffer;
  }
  public insertDebugMarker(markerLabel: string): void {}
  public popDebugGroup(groupLabel: string): void {}
  public pushDebugGroup(groupLabel: string): void {}
  public setBindGroup(index: number, bindGroup: BindGroup): void {}
  public setPipeline(pipeline: ComputePipeline | RenderPipeline): void {}
}
class ComputePassEncoder extends ProgrammablePassEncoder implements w.WebGPUComputePassEncoder {
  public label: string | undefined;
  constructor(commandBuffer: CommandBuffer) {
    super(commandBuffer);
  }
  public dispatch(x: number, y: number, z: number): void {}
}
class RenderPassEncoder extends ProgrammablePassEncoder implements w.WebGPURenderPassEncoder {
  public label: string | undefined;
  public draw(vertexCount: number, instanceCount: number, firstVertex: number, firstInstance: number): void {}
  public drawIndexed(indexCount: number, instanceCount: number, firstIndex: number, baseVertex: number, firstInstance: number): void {}
  public setBlendColor(r: number, g: number, b: number, a: number): void {}
  public setIndexBuffer(buffer: Buffer, offset: number): void {}
  public setScissorRect(x: number, y: number, width: number, height: number): void {}
  public setStencilReference(reference: number): void {}
  public setVertexBuffers(startSlot: number, buffers: Buffer[], offsets: number[]): void {}
  public setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void {}
}
class ComputePipeline implements w.WebGPUComputePipeline {
  public label: string | undefined;
}
class Fence implements w.WebGPUFence {
  public label: string | undefined;
  public getCompletedValue(): w.u64 {
    return 0;
  }
  public onCompletion(completionValue: w.u64): Promise<void> {
    return Promise.resolve();
  }
}
class PipelineLayout implements w.WebGPUPipelineLayout {
}
class RenderPipeline implements w.WebGPURenderPipeline {
  public label: string | undefined;
}
class Sampler implements w.WebGPUSampler {
}
class ShaderModule implements w.WebGPUShaderModule {
  public label: string | undefined;
}
class Texture implements w.WebGPUTexture {
  public createDefaultTextureView(): TextureView {
    return new TextureView();
  }
  public createTextureView(desc: w.WebGPUTextureViewDescriptor): TextureView {
    return new TextureView();
  }
  public destroy(): void {}
}
class TextureView {
  constructor() {}
}
class Queue implements w.WebGPUQueue {
  public label: string | undefined;
  public signal(fence: Fence, signalValue: w.u64): void {}
  public submit(buffers: CommandBuffer[]): void {}
  public wait(fence: Fence, valueToWait: w.u64): void {}
}

class Device implements w.WebGPUDevice {
  public readonly adapter: Adapter;
  public readonly extensions: w.WebGPUExtensions;
  public readonly limits = kDefaultLimits;
  public onLog: w.WebGPULogCallback | undefined;

  private queue: Queue = new Queue();

  constructor(adapter: Adapter, descriptor: w.WebGPUDeviceDescriptor) {
    this.adapter = adapter;
    this.extensions = descriptor.extensions || kNoExtensions;
  }

  public createBindGroup(descriptor: w.WebGPUBindGroupDescriptor): BindGroup {
    return new BindGroup();
  }
  public createBindGroupLayout(descriptor: w.WebGPUBindGroupLayoutDescriptor): BindGroupLayout {
    return new BindGroupLayout();
  }
  public createBuffer(descriptor: w.WebGPUBufferDescriptor): Buffer {
    return new Buffer();
  }
  public createCommandBuffer(descriptor: w.WebGPUCommandBufferDescriptor): CommandBuffer {
    return new CommandBuffer();
  }
  public createComputePipeline(descriptor: w.WebGPUComputePipelineDescriptor): ComputePipeline {
    return new ComputePipeline();
  }
  public createFence(descriptor: w.WebGPUFenceDescriptor): Fence {
    return new Fence();
  }
  public createPipelineLayout(descriptor: w.WebGPUPipelineLayoutDescriptor): PipelineLayout {
    return new PipelineLayout();
  }
  public createRenderPipeline(descriptor: w.WebGPURenderPipelineDescriptor): RenderPipeline {
    return new RenderPipeline();
  }
  public createSampler(descriptor: w.WebGPUSamplerDescriptor): Sampler {
    return new Sampler();
  }
  public createShaderModule(descriptor: w.WebGPUShaderModuleDescriptor): ShaderModule {
    return new ShaderModule();
  }
  public createTexture(descriptor: w.WebGPUTextureDescriptor): Texture {
    return new Texture();
  }
  public getObjectStatus(statusableObject: w.WebGPUStatusableObject): w.WebGPUObjectStatusQuery {
    return Promise.resolve("valid");
  }
  public getQueue(): Queue {
    return this.queue;
  }
}

class Adapter implements w.WebGPUAdapter {
  public extensions = kNoExtensions;
  public name = "dummy";
  public createDevice(descriptor: w.WebGPUDeviceDescriptor): Device {
    return new Device(this, descriptor);
  }
}

export const gpu = {
  requestAdapter(options?: w.WebGPURequestAdapterOptions): Promise<Adapter> {
    return Promise.resolve(new Adapter());
  },
};
