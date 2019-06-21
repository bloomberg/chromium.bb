/* tslint:disable:max-line-length */

const kNoExtensions: GPUExtensions = {
  anisotropicFiltering: false,
};
const kDefaultLimits: GPULimits = {
  maxBindGroups: 4,
};

class BindGroup implements GPUBindGroup {
  label: string | undefined;
}
class BindGroupLayout implements GPUBindGroupLayout {
  label: string | undefined;
}
class Buffer implements GPUBuffer {
  label: string | undefined;
  readonly mapping: ArrayBuffer | null = null;
  constructor() {}
  destroy() {}
  unmap() {}

  async mapReadAsync(): Promise<ArrayBuffer> {
    return new ArrayBuffer(0);
  }
  async mapWriteAsync(): Promise<ArrayBuffer> {
    return new ArrayBuffer(0);
  }
  setSubData(offset: number, src: ArrayBufferView, srcOffset = 0, byteLength = 0): void {}
}
class CommandEncoder implements GPUCommandEncoder {
  label: string | undefined;
  beginComputePass(): GPUComputePassEncoder {
    return new ComputePassEncoder();
  }
  beginRenderPass(descriptor: GPURenderPassDescriptor): GPURenderPassEncoder {
    return new RenderPassEncoder();
  }
  blit(): void {}
  copyBufferToBuffer(
    src: GPUBuffer,
    srcOffset: number,
    dst: GPUBuffer,
    dstOffset: number,
    size: number
  ): void {}
  copyBufferToTexture(
    source: GPUBufferCopyView,
    destination: GPUTextureCopyView,
    copySize: GPUExtent3D
  ): void {}
  copyTextureToBuffer(
    source: GPUTextureCopyView,
    destination: GPUBufferCopyView,
    copySize: GPUExtent3D
  ): void {}
  copyTextureToTexture(
    source: GPUTextureCopyView,
    destination: GPUTextureCopyView,
    copySize: GPUExtent3D
  ): void {}
  finish(): GPUCommandBuffer {
    return new CommandBuffer();
  }
}
class CommandBuffer implements GPUCommandBuffer {
  label: string | undefined;
}
class ProgrammablePassEncoder implements GPUProgrammablePassEncoder {
  label: string | undefined;
  constructor() {}
  endPass(): void {}
  insertDebugMarker(markerLabel: string): void {}
  popDebugGroup(): void {}
  pushDebugGroup(groupLabel: string): void {}
  setBindGroup(index: number, bindGroup: BindGroup): void {}
  setPipeline(pipeline: ComputePipeline | RenderPipeline): void {}
}
class ComputePassEncoder extends ProgrammablePassEncoder implements GPUComputePassEncoder {
  label: string | undefined;
  dispatch(x: number, y: number, z: number): void {}
}
class RenderPassEncoder extends ProgrammablePassEncoder implements GPURenderPassEncoder {
  label: string | undefined;
  draw(
    vertexCount: number,
    instanceCount: number,
    firstVertex: number,
    firstInstance: number
  ): void {}
  drawIndexed(
    indexCount: number,
    instanceCount: number,
    firstIndex: number,
    baseVertex: number,
    firstInstance: number
  ): void {}
  setBlendColor(color: GPUColor): void {}
  setIndexBuffer(buffer: Buffer, offset: number): void {}
  setScissorRect(x: number, y: number, width: number, height: number): void {}
  setStencilReference(reference: number): void {}
  setVertexBuffers(startSlot: number, buffers: Buffer[], offsets: number[]): void {}
  setViewport(
    x: number,
    y: number,
    width: number,
    height: number,
    minDepth: number,
    maxDepth: number
  ): void {}
}
class ComputePipeline implements GPUComputePipeline {
  label: string | undefined;
}
class Fence implements GPUFence {
  label: string | undefined;
  getCompletedValue(): number {
    return 0;
  }
  onCompletion(completionValue: number): Promise<void> {
    return Promise.resolve();
  }
}
class PipelineLayout implements GPUPipelineLayout {
  label: string | undefined;
}
class RenderPipeline implements GPURenderPipeline {
  label: string | undefined;
}
class Sampler implements GPUSampler {
  label: string | undefined;
}
class ShaderModule implements GPUShaderModule {
  label: string | undefined;
}
class Texture implements GPUTexture {
  label: string | undefined;
  constructor() {}
  createDefaultView(): TextureView {
    return new TextureView();
  }
  createView(desc: GPUTextureViewDescriptor): TextureView {
    return new TextureView();
  }
  destroy() {}
}
class TextureView implements GPUTextureView {
  label: string | undefined;
  constructor() {}
}
class Queue implements GPUQueue {
  label: string | undefined;
  createFence(descriptor: GPUFenceDescriptor): Fence {
    return new Fence();
  }
  signal(fence: Fence, signalValue: number): void {}
  submit(buffers: CommandBuffer[]): void {}
  wait(fence: Fence, valueToWait: number): void {}
}
class SwapChain implements GPUSwapChain {
  getCurrentTexture(): Texture {
    return new Texture();
  }
}
class Device extends EventTarget implements GPUDevice {
  readonly adapter: Adapter;
  readonly extensions: GPUExtensions;
  readonly limits = kDefaultLimits;
  readonly lost: Promise<GPUDeviceLostInfo> = new Promise(() => {});

  onuncapturederror: Event | undefined;
  private queue: Queue = new Queue();
  constructor(adapter: Adapter, descriptor: GPUDeviceDescriptor) {
    super();
    this.adapter = adapter;
    this.extensions = descriptor.extensions || kNoExtensions;
  }
  createBindGroup(descriptor: GPUBindGroupDescriptor): BindGroup {
    return new BindGroup();
  }
  createBindGroupLayout(descriptor: GPUBindGroupLayoutDescriptor): BindGroupLayout {
    return new BindGroupLayout();
  }
  createBuffer(descriptor: GPUBufferDescriptor): Buffer {
    return new Buffer();
  }
  createCommandEncoder(descriptor: GPUCommandEncoderDescriptor): CommandEncoder {
    return new CommandEncoder();
  }
  createComputePipeline(descriptor: GPUComputePipelineDescriptor): ComputePipeline {
    return new ComputePipeline();
  }
  createPipelineLayout(descriptor: GPUPipelineLayoutDescriptor): PipelineLayout {
    return new PipelineLayout();
  }
  createRenderPipeline(descriptor: GPURenderPipelineDescriptor): RenderPipeline {
    return new RenderPipeline();
  }
  createSampler(descriptor: GPUSamplerDescriptor): Sampler {
    return new Sampler();
  }
  createShaderModule(descriptor: GPUShaderModuleDescriptor): ShaderModule {
    return new ShaderModule();
  }
  createTexture(descriptor: GPUTextureDescriptor): Texture {
    return new Texture();
  }

  getQueue(): Queue {
    return this.queue;
  }

  createSwapChain(descriptor: GPUSwapChainDescriptor): SwapChain {
    return new SwapChain();
  }
  async getSwapChainPreferredFormat(context: GPUCanvasContext): Promise<GPUTextureFormat> {
    return 'rgba8unorm';
  }

  pushErrorScope(filter: GPUErrorFilter): void {}
  async popErrorScope(): Promise<GPUError | null> {
    return null;
  }
}

class Adapter implements GPUAdapter {
  extensions = kNoExtensions;
  name = 'dummy';
  async requestDevice(descriptor: GPUDeviceDescriptor): Promise<Device> {
    return new Device(this, descriptor);
  }
}

const gpu: GPU = {
  async requestAdapter(options?: GPURequestAdapterOptions): Promise<GPUAdapter> {
    return new Adapter();
  },
};
export default gpu;
