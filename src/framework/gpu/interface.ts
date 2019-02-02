/* tslint:disable */
// https://github.com/gpuweb/gpuweb/blob/e4f6adf2625edbfc91f280efbb81309a70b9a094/design/sketch.webidl

export type u64 = number;

export type GPUBindingResource = GPUSampler | GPUTextureView | GPUBufferBinding;

export type GPUAddressMode =
  | "clampToEdge"
  | "repeat"
  | "mirrorRepeat"
  | "clampToBorderColor";
export type GPUBindingType =
  | "uniformBuffer"
  | "sampler"
  | "sampledTexture"
  | "storageBuffer";
export type GPUBlendFactor =
  | "zero"
  | "one"
  | "srcColor"
  | "oneMinusSrcColor"
  | "srcAlpha"
  | "oneMinusSrcAlpha"
  | "dstColor"
  | "oneMinusDstColor"
  | "dstAlpha"
  | "oneMinusDstAlpha"
  | "srcAlphaSaturated"
  | "blendColor"
  | "oneMinusBlendColor";
export type GPUBlendOperation =
  | "add"
  | "subtract"
  | "reverseSubtract"
  | "min"
  | "max";
export type GPUBorderColor =
  | "transparentBlack"
  | "opaqueBlack"
  | "opaqueWhite";
export type GPUCompareFunction =
  | "never"
  | "less"
  | "equal"
  | "lessEqual"
  | "greater"
  | "notEqual"
  | "greaterEqual"
  | "always";
export type GPUCullMode =
  | "none"
  | "front"
  | "back";
export type GPUFilterMode =
  | "nearest"
  | "linear";
export type GPUFrontFace =
  | "ccw"
  | "cw";
export type GPUIndexFormat =
  | "uint16"
  | "uint32";
export type GPUInputStepMode =
  | "vertex"
  | "instance";
export type GPULoadOp =
  | "clear"
  | "load";
export type GPUPrimitiveTopology =
  | "pointList"
  | "lineList"
  | "lineStrip"
  | "trangleList"
  | "triangleStrip";
export type GPUStencilOperation =
  | "keep"
  | "zero"
  | "replace"
  | "invert"
  | "incrementClamp"
  | "decrementClamp"
  | "incrementWrap"
  | "decrementWrap";
export type GPUStoreOp =
  | "store";
export type GPUTextureDimension =
  | "1d"
  | "2d"
  | "3d";
export type GPUTextureFormat =
  | "R8G8B8A8Unorm"
  | "R8G8B8A8Uint"
  | "B8G8R8A8Unorm"
  | "D32FloatS8Uint";
export type GPUTextureViewDimension =
  | "1d"
  | "2d"
  | "2darray"
  | "cube"
  | "cubearray"
  | "3d";
export type GPUVertexFormat =
  | "floatR32G32B32A32"
  | "floatR32G32B32"
  | "floatR32G32"
  | "floatR32";

export type GPUBufferUsageFlags = number;
export const enum GPUBufferUsage {
  NONE = 0,
  MAP_READ = 1,
  MAP_WRITE = 2,
  TRANSFER_SRC = 4,
  TRANSFER_DST = 8,
  INDEX = 16,
  VERTEX = 32,
  UNIFORM = 64,
  STORAGE = 128,
}

export type GPUColorWriteFlags = number;
export const enum GPUColorWriteBits {
  NONE = 0,
  RED = 1,
  GREEN = 2,
  BLUE = 4,
  ALPHA = 8,
  ALL = 15,
}

export type GPUShaderStageFlags = number;
export const enum GPUShaderStageBit {
  NONE = 0,
  VERTEX = 1,
  FRAGMENT = 2,
  COMPUTE = 4,
}

export type GPUTextureAspectFlags = number;
export const enum GPUTextureAspect {
  COLOR = 1,
  DEPTH = 2,
  STENCIL = 4,
}

export type GPUTextureUsageFlags = number;
export const enum GPUTextureUsage {
  NONE = 0,
  TRANSFER_SRC = 1,
  TRANSFER_DST = 2,
  SAMPLED = 4,
  STORAGE = 8,
  OUTPUT_ATTACHMENT = 16,
  PRESENT = 32,
}

export interface GPUAttachmentDescriptor {
  format?: GPUTextureFormat;
}

export interface GPUAttachmentsStateDescriptor {
  colorAttachments?: GPUAttachmentDescriptor[];
  depthStencilAttachment?: GPUAttachmentDescriptor | null;
}

export interface GPUBindGroupBinding {
  binding?: number;
  resource?: GPUBindingResource;
}

export interface GPUBindGroupDescriptor {
  bindings?: GPUBindGroupBinding[];
  layout?: GPUBindGroupLayout;
}

export interface GPUBindGroupLayoutBinding {
  binding?: number;
  type?: GPUBindingType;
  visibility?: GPUShaderStageFlags;
}

export interface GPUBindGroupLayoutDescriptor {
  bindings?: GPUBindGroupLayoutBinding[];
}

export interface GPUBlendDescriptor {
  dstFactor?: GPUBlendFactor;
  operation?: GPUBlendOperation;
  srcFactor?: GPUBlendFactor;
}

export interface GPUBlendStateDescriptor {
  alpha?: GPUBlendDescriptor;
  blendEnabled?: boolean;
  color?: GPUBlendDescriptor;
  writeMask?: GPUColorWriteFlags;
}

export interface GPUBufferBinding {
  buffer?: GPUBuffer;
  offset?: number;
  size?: number;
}

export interface GPUBufferCopyView {
  buffer?: GPUBuffer;
  imageHeight?: number;
  offset?: number;
  rowPitch?: number;
}

export interface GPUBufferDescriptor {
  size?: number;
  usage?: GPUBufferUsageFlags;
}

export interface GPUColor {
  a?: number;
  b?: number;
  g?: number;
  r?: number;
}

export interface GPUCommandBufferDescriptor {
  label?: string;
}

export interface GPUComputePipelineDescriptor extends GPUPipelineDescriptorBase {
  computeStage?: GPUPipelineStageDescriptor;
}

export interface GPUDepthStencilStateDescriptor {
  back?: GPUStencilStateFaceDescriptor;
  depthCompare?: GPUCompareFunction;
  depthWriteEnabled?: boolean;
  front?: GPUStencilStateFaceDescriptor;
  stencilReadMask?: number;
  stencilWriteMask?: number;
}

export interface GPUDeviceDescriptor {
  extensions?: GPUExtensions;
}

export interface GPUExtensions {
  anisotropicFiltering?: boolean;
}

export interface GPUExtent3D {
  depth?: number;
  height?: number;
  width?: number;
}

export interface GPUFenceDescriptor {
  initialValue?: u64;
  label?: string;
  signalQueue?: GPUQueue;
}

export interface GPUInputStateDescriptor {
  attributes?: GPUVertexAttributeDescriptor[];
  indexFormat?: GPUIndexFormat;
  inputs?: GPUVertexInputDescriptor[];
}

export interface GPULimits {
  maxBindGroups?: number;
}

export interface GPUOrigin3D {
  x?: number;
  y?: number;
  z?: number;
}

export interface GPUPipelineDescriptorBase {
  label?: string;
  layout?: GPUPipelineLayout;
}

export interface GPUPipelineLayoutDescriptor {
  bindGroupLayouts?: GPUBindGroupLayout[];
}

export interface GPUPipelineStageDescriptor {
  entryPoint?: string;
  module?: GPUShaderModule;
}

export interface GPURasterizationStateDescriptor {
  cullMode?: GPUCullMode;
  depthBias?: number;
  depthBiasClamp?: number;
  depthBiasSlopeScale?: number;
  frontFace?: GPUFrontFace;
}

export interface GPURenderPassColorAttachmentDescriptor {
  attachment?: GPUTextureView;
  clearColor?: GPUColor;
  loadOp?: GPULoadOp;
  resolveTarget?: GPUTextureView | null;
  storeOp?: GPUStoreOp;
}

export interface GPURenderPassDepthStencilAttachmentDescriptor {
  attachment?: GPUTextureView;
  clearDepth?: number;
  clearStencil?: number;
  depthLoadOp?: GPULoadOp;
  depthStoreOp?: GPUStoreOp;
  stencilLoadOp?: GPULoadOp;
  stencilStoreOp?: GPUStoreOp;
}

export interface GPURenderPassDescriptor {
  colorAttachments?: GPURenderPassColorAttachmentDescriptor[];
  depthStencilAttachment?: GPURenderPassDepthStencilAttachmentDescriptor;
}

export interface GPURenderPipelineDescriptor extends GPUPipelineDescriptorBase {
  attachmentsState?: GPUAttachmentsStateDescriptor;
  blendStates?: GPUBlendStateDescriptor[];
  depthStencilState?: GPUDepthStencilStateDescriptor;
  fragmentStage?: GPUPipelineStageDescriptor;
  inputState?: GPUInputStateDescriptor;
  primitiveTopology?: GPUPrimitiveTopology;
  rasterizationState?: GPURasterizationStateDescriptor;
  sampleCount?: number;
  vertexStage?: GPUPipelineStageDescriptor;
}

export interface GPUSamplerDescriptor {
  borderColor?: GPUBorderColor;
  compareFunction?: GPUCompareFunction;
  lodMaxClamp?: number;
  lodMinClamp?: number;
  magFilter?: GPUFilterMode;
  maxAnisotropy?: number;
  minFilter?: GPUFilterMode;
  mipmapFilter?: GPUFilterMode;
  rAddressMode?: GPUAddressMode;
  sAddressMode?: GPUAddressMode;
  tAddressMode?: GPUAddressMode;
}

export interface GPUShaderModuleDescriptor {
  code: ArrayBuffer | string;
  label?: string;
}

export interface GPUStencilStateFaceDescriptor {
  compare?: GPUCompareFunction;
  depthFailOp?: GPUStencilOperation;
  passOp?: GPUStencilOperation;
  stencilFailOp?: GPUStencilOperation;
}

export interface GPUSwapChainDescriptor {
  device?: GPUDevice | null;
  format?: GPUTextureFormat;
  height?: number;
  usage?: GPUTextureUsageFlags;
  width?: number;
}

export interface GPUTextureCopyView {
  level?: number;
  origin?: GPUOrigin3D;
  slice?: number;
  texture?: GPUTexture;
}

export interface GPUTextureDescriptor {
  arraySize?: number;
  dimension?: GPUTextureDimension;
  format?: GPUTextureFormat;
  levelCount?: number;
  sampleCount?: number;
  size?: GPUExtent3D;
  usage?: GPUTextureUsageFlags;
}

export interface GPUTextureViewDescriptor {
  aspect?: GPUTextureAspectFlags;
  baseArrayLayer?: number;
  baseMipLevel?: number;
  dimension?: GPUTextureViewDimension;
  format?: GPUTextureFormat;
  layerCount?: number;
  levelCount?: number;
}

export interface GPUVertexAttributeDescriptor {
  format?: GPUVertexFormat;
  inputSlot?: number;
  offset?: number;
  shaderLocation?: number;
}

export interface GPUVertexInputDescriptor {
  inputSlot?: number;
  stepMode?: GPUInputStepMode;
  stride?: number;
}

export interface GPUAdapter {
  readonly extensions: GPUExtensions;
  readonly name: string;
  requestDevice(descriptor: GPUDeviceDescriptor, onDeviceLost: GPUDeviceLostCallback): Promise<GPUDevice>;
}

export type GPUDeviceLostCallback = (info: GPUDeviceLostInfo) => void;

export interface GPUDeviceLostInfo {
  readonly device: GPUDevice;
  readonly reason: string;
}

export interface GPUBindGroup {
}

export interface GPUBindGroupLayout {
}

export interface GPUBuffer {
  readonly mapping: ArrayBuffer | null;
  destroy(): void;
  unmap(): void;

  // TODO: TBD
  mapReadAsync(offset: number, size: number, callback: (ab: ArrayBuffer) => void): void;
  setSubData(offset: number, ab: ArrayBuffer): void;
}

export interface GPUCommandBuffer extends GPUDebugLabel {
  beginComputePass(): GPUComputePassEncoder;
  beginRenderPass(descriptor: GPURenderPassDescriptor): GPURenderPassEncoder;
  blit(): void;
  copyBufferToBuffer(src: GPUBuffer, srcOffset: number, dst: GPUBuffer, dstOffset: number, size: number): void;
  copyBufferToTexture(source: GPUBufferCopyView, destination: GPUTextureCopyView, copySize: GPUExtent3D): void;
  copyTextureToBuffer(source: GPUTextureCopyView, destination: GPUBufferCopyView, copySize: GPUExtent3D): void;
  copyTextureToTexture(source: GPUTextureCopyView, destination: GPUTextureCopyView, copySize: GPUExtent3D): void;
}

export interface GPUComputePassEncoder extends GPUProgrammablePassEncoder {
  dispatch(x: number, y: number, z: number): void;
}

export interface GPUComputePipeline extends GPUDebugLabel {
}

export interface GPUDebugLabel {
  label: string | undefined;
}

export interface GPUDevice extends EventTarget {
  readonly adapter: GPUAdapter;
  readonly extensions: GPUExtensions;
  readonly limits: GPULimits;

  createBindGroup(descriptor: GPUBindGroupDescriptor): GPUBindGroup;
  createBindGroupLayout(descriptor: GPUBindGroupLayoutDescriptor): GPUBindGroupLayout;
  createCommandBuffer(descriptor: GPUCommandBufferDescriptor): GPUCommandBuffer;
  createFence(descriptor: GPUFenceDescriptor): GPUFence;
  createPipelineLayout(descriptor: GPUPipelineLayoutDescriptor): GPUPipelineLayout;
  createSampler(descriptor: GPUSamplerDescriptor): GPUSampler;
  createShaderModule(descriptor: GPUShaderModuleDescriptor): GPUShaderModule;

  createBuffer(descriptor: GPUBufferDescriptor): GPUBuffer;
  createTexture(descriptor: GPUTextureDescriptor): GPUTexture;
  tryCreateBuffer(descriptor: GPUBufferDescriptor): Promise<GPUBuffer>;
  tryCreateTexture(descriptor: GPUTextureDescriptor): Promise<GPUTexture>;

  createComputePipeline(descriptor: GPUComputePipelineDescriptor): GPUComputePipeline;
  createRenderPipeline(descriptor: GPURenderPipelineDescriptor): GPURenderPipeline;
  createReadyComputePipeline(descriptor: GPUComputePipelineDescriptor): Promise<GPUComputePipeline>;
  createReadyRenderPipeline(descriptor: GPURenderPipelineDescriptor): Promise<GPURenderPipeline>;

  getQueue(): GPUQueue;

  // TODO: temporary
  flush(): void;
}

export interface GPUFence extends GPUDebugLabel {
  getCompletedValue(): u64;
  onCompletion(completionValue: u64): Promise<void>;
}

export interface GPULogEntryEvent extends Event {
  readonly object: any;
  readonly reason: string;
}

export interface GPUPipelineLayout {
}

export interface GPUProgrammablePassEncoder extends GPUDebugLabel {
  endPass(): GPUCommandBuffer;
  insertDebugMarker(markerLabel: string): void;
  popDebugGroup(groupLabel: string): void;
  pushDebugGroup(groupLabel: string): void;
  setBindGroup(index: number, bindGroup: GPUBindGroup): void;
  setPipeline(pipeline: GPUComputePipeline | GPURenderPipeline): void;
}

export interface GPUQueue extends GPUDebugLabel {
  signal(fence: GPUFence, signalValue: u64): void;
  submit(buffers: GPUCommandBuffer[]): void;
  wait(fence: GPUFence, valueToWait: u64): void;
}

export interface GPURenderPassEncoder extends GPUProgrammablePassEncoder {
  draw(vertexCount: number, instanceCount: number, firstVertex: number, firstInstance: number): void;
  drawIndexed(indexCount: number, instanceCount: number, firstIndex: number, baseVertex: number, firstInstance: number): void;
  setBlendColor(r: number, g: number, b: number, a: number): void;
  setIndexBuffer(buffer: GPUBuffer, offset: number): void;
  setScissorRect(x: number, y: number, width: number, height: number): void;
  setStencilReference(reference: number): void;
  setVertexBuffers(startSlot: number, buffers: GPUBuffer[], offsets: number[]): void;
  setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void;
}

export interface GPURenderPipeline extends GPUDebugLabel {
}

export interface GPURenderingContext extends GPUSwapChain {
}

export interface GPUSampler {
}

export interface GPUShaderModule extends GPUDebugLabel {
}

export interface GPUSwapChain {
  configure(descriptor: GPUSwapChainDescriptor): void;
  getNextTexture(): GPUTexture;
  present(): void;
}

export interface GPUTexture {
  createDefaultTextureView(): GPUTextureView;
  createTextureView(desc: GPUTextureViewDescriptor): GPUTextureView;
  destroy(): void;
}

export interface GPUTextureView {
}

export type GPUPowerPreference =
  | "low-power"
  | "high-performance";
export interface GPURequestAdapterOptions {
  powerPreference?: GPUPowerPreference;
}

export interface GPU {
  requestAdapter(options?: GPURequestAdapterOptions): Promise<GPUAdapter>;

  // TODO: temporary
  getDevice(): GPUDevice;
}
