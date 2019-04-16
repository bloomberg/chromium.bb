/* tslint:disable:max-line-length */

const kNoExtensions: GPUExtensions = {
  anisotropicFiltering: false,
};
const kDefaultLimits: GPULimits = {
  maxBindGroups: 4,
};

class BindGroup implements GPUBindGroup {
  public label: string | undefined;
}
class BindGroupLayout implements GPUBindGroupLayout {
  public label: string | undefined;
}
class Buffer implements GPUBuffer {
  public label: string | undefined;
  public readonly mapping: ArrayBuffer | null = null;
  constructor() {}
  public destroy() {}
  public unmap() {}

  public async mapReadAsync(): Promise<ArrayBuffer> {
    return new ArrayBuffer(0);
  }
  public async mapWriteAsync(): Promise<ArrayBuffer> {
    return new ArrayBuffer(0);
  }
  public setSubData(offset: number, src: ArrayBufferView, srcOffset: number = 0, byteLength: number = 0): void {}
}
class CommandEncoder implements GPUCommandEncoder {
  public label: string | undefined;
  public beginComputePass(): GPUComputePassEncoder {
    return new ComputePassEncoder();
  }
  public beginRenderPass(descriptor: GPURenderPassDescriptor): GPURenderPassEncoder {
    return new RenderPassEncoder();
  }
  public blit(): void {}
  public copyBufferToBuffer(src: GPUBuffer, srcOffset: number, dst: GPUBuffer, dstOffset: number, size: number): void {}
  public copyBufferToTexture(source: GPUBufferCopyView, destination: GPUTextureCopyView, copySize: GPUExtent3D): void {}
  public copyTextureToBuffer(source: GPUTextureCopyView, destination: GPUBufferCopyView, copySize: GPUExtent3D): void {}
  public copyTextureToTexture(source: GPUTextureCopyView, destination: GPUTextureCopyView, copySize: GPUExtent3D): void {}
  public finish(): GPUCommandBuffer {
    return new CommandBuffer();
  }
}
class CommandBuffer implements GPUCommandBuffer {
  public label: string | undefined;
}
class ProgrammablePassEncoder implements GPUProgrammablePassEncoder {
  public label: string | undefined;
  constructor() {}
  public endPass(): void {}
  public insertDebugMarker(markerLabel: string): void {}
  public popDebugGroup(): void {}
  public pushDebugGroup(groupLabel: string): void {}
  public setBindGroup(index: number, bindGroup: BindGroup): void {}
  public setPipeline(pipeline: ComputePipeline | RenderPipeline): void {}
}
class ComputePassEncoder extends ProgrammablePassEncoder implements GPUComputePassEncoder {
  public label: string | undefined;
  public dispatch(x: number, y: number, z: number): void {}
}
class RenderPassEncoder extends ProgrammablePassEncoder implements GPURenderPassEncoder {
  public label: string | undefined;
  public draw(vertexCount: number, instanceCount: number, firstVertex: number, firstInstance: number): void {}
  public drawIndexed(indexCount: number, instanceCount: number, firstIndex: number, baseVertex: number, firstInstance: number): void {}
  public setBlendColor(color: GPUColor): void {}
  public setIndexBuffer(buffer: Buffer, offset: number): void {}
  public setScissorRect(x: number, y: number, width: number, height: number): void {}
  public setStencilReference(reference: number): void {}
  public setVertexBuffers(startSlot: number, buffers: Buffer[], offsets: number[]): void {}
  public setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void {}
}
class ComputePipeline implements GPUComputePipeline {
  public label: string | undefined;
}
class Fence implements GPUFence {
  public label: string | undefined;
  public getCompletedValue(): number {
    return 0;
  }
  public onCompletion(completionValue: number): Promise<void> {
    return Promise.resolve();
  }
}
class PipelineLayout implements GPUPipelineLayout {
  public label: string | undefined;
}
class RenderPipeline implements GPURenderPipeline {
  public label: string | undefined;
}
class Sampler implements GPUSampler {
  public label: string | undefined;
}
class ShaderModule implements GPUShaderModule {
  public label: string | undefined;
}
class Texture implements GPUTexture {
  public label: string | undefined;
  constructor() {}
  public createDefaultView(): TextureView {
    return new TextureView();
  }
  public createView(desc: GPUTextureViewDescriptor): TextureView {
    return new TextureView();
  }
  public destroy() {}
}
class TextureView implements GPUTextureView {
  public label: string | undefined;
  constructor() {}
}
class Queue implements GPUQueue {
  public label: string | undefined;
  public createFence(descriptor: GPUFenceDescriptor): Fence { return new Fence(); }
  public signal(fence: Fence, signalValue: number): void {}
  public submit(buffers: CommandBuffer[]): void {}
  public wait(fence: Fence, valueToWait: number): void {}
}
class SwapChain implements GPUSwapChain {
  public getCurrentTexture(): Texture { return new Texture(); }
}
class Device extends EventTarget implements GPUDevice {
  public readonly adapter: Adapter;
  public readonly extensions: GPUExtensions;
  public readonly limits = kDefaultLimits;
  private queue: Queue = new Queue();
  constructor(adapter: Adapter, descriptor: GPUDeviceDescriptor) {
    super();
    this.adapter = adapter;
    this.extensions = descriptor.extensions || kNoExtensions;
  }
  public createBindGroup(descriptor: GPUBindGroupDescriptor): BindGroup { return new BindGroup(); }
  public createBindGroupLayout(descriptor: GPUBindGroupLayoutDescriptor): BindGroupLayout { return new BindGroupLayout(); }
  public createBuffer(descriptor: GPUBufferDescriptor): Buffer { return new Buffer(); }
  public createCommandEncoder(descriptor: GPUCommandEncoderDescriptor): CommandEncoder { return new CommandEncoder(); }
  public createComputePipeline(descriptor: GPUComputePipelineDescriptor): ComputePipeline { return new ComputePipeline(); }
  public createPipelineLayout(descriptor: GPUPipelineLayoutDescriptor): PipelineLayout { return new PipelineLayout(); }
  public createRenderPipeline(descriptor: GPURenderPipelineDescriptor): RenderPipeline { return new RenderPipeline(); }
  public createSampler(descriptor: GPUSamplerDescriptor): Sampler { return new Sampler(); }
  public createShaderModule(descriptor: GPUShaderModuleDescriptor): ShaderModule { return new ShaderModule(); }
  public createTexture(descriptor: GPUTextureDescriptor): Texture { return new Texture(); }

  public getQueue(): Queue { return this.queue; }

  public createSwapChain(descriptor: GPUSwapChainDescriptor): SwapChain { return new SwapChain(); }
  public async getSwapChainPreferredFormat(context: GPUCanvasContext): Promise<GPUTextureFormat> { return "rgba8unorm"; }

  public onuncapturederror: Event | undefined;
  public pushErrorScope(filter: GPUErrorFilter): void {}
  public async popErrorScope(): Promise<GPUError | null> { return null; }
}

class Adapter implements GPUAdapter {
  public extensions = kNoExtensions;
  public name = "dummy";
  public async requestDevice(descriptor: GPUDeviceDescriptor): Promise<Device> {
    return new Device(this, descriptor);
  }
}

const gpu: GPU = {
  async requestAdapter(options?: GPURequestAdapterOptions): Promise<GPUAdapter> {
    return new Adapter();
  },
};
export default gpu;
