/* tslint:disable */
// https://github.com/gpuweb/gpuweb/blob/e4f6adf2625edbfc91f280efbb81309a70b9a094/design/sketch.webidl

export type u64 = number;

export type WebGPUObjectStatusQuery = Promise<WebGPUObjectStatus>;
export type WebGPUStatusableObject = WebGPUBuffer | WebGPUTexture;
export type WebGPUBindingResource = WebGPUSampler | WebGPUTextureView | WebGPUBufferBinding;

export type WebGPUAddressMode =
  | "clampToEdge"
  | "repeat"
  | "mirrorRepeat"
  | "clampToBorderColor";
export type WebGPUBindingType =
  | "uniformBuffer"
  | "sampler"
  | "sampledTexture"
  | "storageBuffer";
export type WebGPUBlendFactor =
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
export type WebGPUBlendOperation =
  | "add"
  | "subtract"
  | "reverseSubtract"
  | "min"
  | "max";
export type WebGPUBorderColor =
  | "transparentBlack"
  | "opaqueBlack"
  | "opaqueWhite";
export type WebGPUCompareFunction =
  | "never"
  | "less"
  | "equal"
  | "lessEqual"
  | "greater"
  | "notEqual"
  | "greaterEqual"
  | "always";
export type WebGPUCullMode =
  | "none"
  | "front"
  | "back";
export type WebGPUFilterMode =
  | "nearest"
  | "linear";
export type WebGPUFrontFace =
  | "ccw"
  | "cw";
export type WebGPUIndexFormat =
  | "uint16"
  | "uint32";
export type WebGPUInputStepMode =
  | "vertex"
  | "instance";
export type WebGPULoadOp =
  | "clear"
  | "load";
export type WebGPULogEntryType =
  | "device-lost"
  | "validation-error"
  | "recoverable-out-of-memory";
export type WebGPUObjectStatus =
  | "valid"
  | "out-of-memory"
  | "invalid";
export type WebGPUPrimitiveTopology =
  | "pointList"
  | "lineList"
  | "lineStrip"
  | "trangleList"
  | "triangleStrip";
export type WebGPUStencilOperation =
  | "keep"
  | "zero"
  | "replace"
  | "invert"
  | "incrementClamp"
  | "decrementClamp"
  | "incrementWrap"
  | "decrementWrap";
export type WebGPUStoreOp =
  | "store";
export type WebGPUTextureDimension =
  | "1d"
  | "2d"
  | "3d";
export type WebGPUTextureFormat =
  | "R8G8B8A8Unorm"
  | "R8G8B8A8Uint"
  | "B8G8R8A8Unorm"
  | "D32FloatS8Uint";
export type WebGPUTextureViewDimension =
  | "1d"
  | "2d"
  | "2darray"
  | "cube"
  | "cubearray"
  | "3d";
export type WebGPUVertexFormat =
  | "floatR32G32B32A32"
  | "floatR32G32B32"
  | "floatR32G32"
  | "floatR32";

export type WebGPUBufferUsageFlags = number;
export const enum WebGPUBufferUsage {
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

export type WebGPUColorWriteFlags = number;
export const enum WebGPUColorWriteBits {
  NONE = 0,
  RED = 1,
  GREEN = 2,
  BLUE = 4,
  ALPHA = 8,
  ALL = 15,
}

export type WebGPUShaderStageFlags = number;
export const enum WebGPUShaderStageBit {
  NONE = 0,
  VERTEX = 1,
  FRAGMENT = 2,
  COMPUTE = 4,
}

export type WebGPUTextureAspectFlags = number;
export const enum WebGPUTextureAspect {
  COLOR = 1,
  DEPTH = 2,
  STENCIL = 4,
}

export type WebGPUTextureUsageFlags = number;
export const enum WebGPUTextureUsage {
  NONE = 0,
  TRANSFER_SRC = 1,
  TRANSFER_DST = 2,
  SAMPLED = 4,
  STORAGE = 8,
  OUTPUT_ATTACHMENT = 16,
  PRESENT = 32,
}

export interface WebGPUAttachmentDescriptor {
  format?: WebGPUTextureFormat;
}

export interface WebGPUAttachmentsStateDescriptor {
  colorAttachments?: WebGPUAttachmentDescriptor[];
  depthStencilAttachment?: WebGPUAttachmentDescriptor | null;
}

export interface WebGPUBindGroupBinding {
  binding?: number;
  resource?: WebGPUBindingResource;
}

export interface WebGPUBindGroupDescriptor {
  bindings?: WebGPUBindGroupBinding[];
  layout?: WebGPUBindGroupLayout;
}

export interface WebGPUBindGroupLayoutBinding {
  binding?: number;
  type?: WebGPUBindingType;
  visibility?: WebGPUShaderStageFlags;
}

export interface WebGPUBindGroupLayoutDescriptor {
  bindings?: WebGPUBindGroupLayoutBinding[];
}

export interface WebGPUBlendDescriptor {
  dstFactor?: WebGPUBlendFactor;
  operation?: WebGPUBlendOperation;
  srcFactor?: WebGPUBlendFactor;
}

export interface WebGPUBlendStateDescriptor {
  alpha?: WebGPUBlendDescriptor;
  blendEnabled?: boolean;
  color?: WebGPUBlendDescriptor;
  writeMask?: WebGPUColorWriteFlags;
}

export interface WebGPUBufferBinding {
  buffer?: WebGPUBuffer;
  offset?: number;
  size?: number;
}

export interface WebGPUBufferCopyView {
  buffer?: WebGPUBuffer;
  imageHeight?: number;
  offset?: number;
  rowPitch?: number;
}

export interface WebGPUBufferDescriptor {
  size?: number;
  usage?: WebGPUBufferUsageFlags;
}

export interface WebGPUColor {
  a?: number;
  b?: number;
  g?: number;
  r?: number;
}

export interface WebGPUCommandBufferDescriptor {
  label?: string;
}

export interface WebGPUComputePipelineDescriptor extends WebGPUPipelineDescriptorBase {
  computeStage?: WebGPUPipelineStageDescriptor;
}

export interface WebGPUDepthStencilStateDescriptor {
  back?: WebGPUStencilStateFaceDescriptor;
  depthCompare?: WebGPUCompareFunction;
  depthWriteEnabled?: boolean;
  front?: WebGPUStencilStateFaceDescriptor;
  stencilReadMask?: number;
  stencilWriteMask?: number;
}

export interface WebGPUDeviceDescriptor {
  extensions?: WebGPUExtensions;
}

export interface WebGPUExtensions {
  anisotropicFiltering?: boolean;
}

export interface WebGPUExtent3D {
  depth?: number;
  height?: number;
  width?: number;
}

export interface WebGPUFenceDescriptor {
  initialValue?: u64;
  label?: string;
  signalQueue?: WebGPUQueue;
}

export interface WebGPUInputStateDescriptor {
  attributes?: WebGPUVertexAttributeDescriptor[];
  indexFormat?: WebGPUIndexFormat;
  inputs?: WebGPUVertexInputDescriptor[];
}

export interface WebGPULimits {
  maxBindGroups?: number;
}

export interface WebGPUOrigin3D {
  x?: number;
  y?: number;
  z?: number;
}

export interface WebGPUPipelineDescriptorBase {
  label?: string;
  layout?: WebGPUPipelineLayout;
}

export interface WebGPUPipelineLayoutDescriptor {
  bindGroupLayouts?: WebGPUBindGroupLayout[];
}

export interface WebGPUPipelineStageDescriptor {
  entryPoint?: string;
  module?: WebGPUShaderModule;
}

export interface WebGPURasterizationStateDescriptor {
  cullMode?: WebGPUCullMode;
  depthBias?: number;
  depthBiasClamp?: number;
  depthBiasSlopeScale?: number;
  frontFace?: WebGPUFrontFace;
}

export interface WebGPURenderPassColorAttachmentDescriptor {
  attachment?: WebGPUTextureView;
  clearColor?: WebGPUColor;
  loadOp?: WebGPULoadOp;
  resolveTarget?: WebGPUTextureView | null;
  storeOp?: WebGPUStoreOp;
}

export interface WebGPURenderPassDepthStencilAttachmentDescriptor {
  attachment?: WebGPUTextureView;
  clearDepth?: number;
  clearStencil?: number;
  depthLoadOp?: WebGPULoadOp;
  depthStoreOp?: WebGPUStoreOp;
  stencilLoadOp?: WebGPULoadOp;
  stencilStoreOp?: WebGPUStoreOp;
}

export interface WebGPURenderPassDescriptor {
  colorAttachments?: WebGPURenderPassColorAttachmentDescriptor[];
  depthStencilAttachment?: WebGPURenderPassDepthStencilAttachmentDescriptor;
}

export interface WebGPURenderPipelineDescriptor extends WebGPUPipelineDescriptorBase {
  attachmentsState?: WebGPUAttachmentsStateDescriptor;
  blendStates?: WebGPUBlendStateDescriptor[];
  depthStencilState?: WebGPUDepthStencilStateDescriptor;
  fragmentStage?: WebGPUPipelineStageDescriptor;
  inputState?: WebGPUInputStateDescriptor;
  primitiveTopology?: WebGPUPrimitiveTopology;
  rasterizationState?: WebGPURasterizationStateDescriptor;
  sampleCount?: number;
  vertexStage?: WebGPUPipelineStageDescriptor;
}

export interface WebGPUSamplerDescriptor {
  borderColor?: WebGPUBorderColor;
  compareFunction?: WebGPUCompareFunction;
  lodMaxClamp?: number;
  lodMinClamp?: number;
  magFilter?: WebGPUFilterMode;
  maxAnisotropy?: number;
  minFilter?: WebGPUFilterMode;
  mipmapFilter?: WebGPUFilterMode;
  rAddressMode?: WebGPUAddressMode;
  sAddressMode?: WebGPUAddressMode;
  tAddressMode?: WebGPUAddressMode;
}

export interface WebGPUShaderModuleDescriptor {
  code: ArrayBuffer | string;
  label?: string;
}

export interface WebGPUStencilStateFaceDescriptor {
  compare?: WebGPUCompareFunction;
  depthFailOp?: WebGPUStencilOperation;
  passOp?: WebGPUStencilOperation;
  stencilFailOp?: WebGPUStencilOperation;
}

export interface WebGPUSwapChainDescriptor {
  device?: WebGPUDevice | null;
  format?: WebGPUTextureFormat;
  height?: number;
  usage?: WebGPUTextureUsageFlags;
  width?: number;
}

export interface WebGPUTextureCopyView {
  level?: number;
  origin?: WebGPUOrigin3D;
  slice?: number;
  texture?: WebGPUTexture;
}

export interface WebGPUTextureDescriptor {
  arraySize?: number;
  dimension?: WebGPUTextureDimension;
  format?: WebGPUTextureFormat;
  levelCount?: number;
  sampleCount?: number;
  size?: WebGPUExtent3D;
  usage?: WebGPUTextureUsageFlags;
}

export interface WebGPUTextureViewDescriptor {
  aspect?: WebGPUTextureAspectFlags;
  baseArrayLayer?: number;
  baseMipLevel?: number;
  dimension?: WebGPUTextureViewDimension;
  format?: WebGPUTextureFormat;
  layerCount?: number;
  levelCount?: number;
}

export interface WebGPUVertexAttributeDescriptor {
  format?: WebGPUVertexFormat;
  inputSlot?: number;
  offset?: number;
  shaderLocation?: number;
}

export interface WebGPUVertexInputDescriptor {
  inputSlot?: number;
  stepMode?: WebGPUInputStepMode;
  stride?: number;
}

export interface WebGPUAdapter {
  readonly extensions: WebGPUExtensions;
  readonly name: string;
  createDevice(descriptor: WebGPUDeviceDescriptor): WebGPUDevice;
}

export interface WebGPUBindGroup {
}

export interface WebGPUBindGroupLayout {
}

export interface WebGPUBuffer {
  readonly mapping: ArrayBuffer | null;
  destroy(): void;
  unmap(): void;

  // TODO: TBD
  mapReadAsync(offset: number, size: number, callback: (ab: ArrayBuffer) => void): void;
  setSubData(offset: number, ab: ArrayBuffer): void;
}

export interface WebGPUCommandBuffer extends WebGPUDebugLabel {
  beginComputePass(): WebGPUComputePassEncoder;
  beginRenderPass(descriptor: WebGPURenderPassDescriptor): WebGPURenderPassEncoder;
  blit(): void;
  copyBufferToBuffer(src: WebGPUBuffer, srcOffset: number, dst: WebGPUBuffer, dstOffset: number, size: number): void;
  copyBufferToTexture(source: WebGPUBufferCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
  copyTextureToBuffer(source: WebGPUTextureCopyView, destination: WebGPUBufferCopyView, copySize: WebGPUExtent3D): void;
  copyTextureToTexture(source: WebGPUTextureCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
}

export interface WebGPUComputePassEncoder extends WebGPUProgrammablePassEncoder {
  dispatch(x: number, y: number, z: number): void;
}

export interface WebGPUComputePipeline extends WebGPUDebugLabel {
}

export interface WebGPUDebugLabel {
  label: string | undefined;
}

export interface WebGPUDevice {
  readonly adapter: WebGPUAdapter;
  readonly extensions: WebGPUExtensions;
  readonly limits: WebGPULimits;
  onLog?: WebGPULogCallback;
  createBindGroup(descriptor: WebGPUBindGroupDescriptor): WebGPUBindGroup;
  createBindGroupLayout(descriptor: WebGPUBindGroupLayoutDescriptor): WebGPUBindGroupLayout;
  createBuffer(descriptor: WebGPUBufferDescriptor): WebGPUBuffer;
  createCommandBuffer(descriptor: WebGPUCommandBufferDescriptor): WebGPUCommandBuffer;
  createComputePipeline(descriptor: WebGPUComputePipelineDescriptor): WebGPUComputePipeline;
  createFence(descriptor: WebGPUFenceDescriptor): WebGPUFence;
  createPipelineLayout(descriptor: WebGPUPipelineLayoutDescriptor): WebGPUPipelineLayout;
  createRenderPipeline(descriptor: WebGPURenderPipelineDescriptor): WebGPURenderPipeline;
  createSampler(descriptor: WebGPUSamplerDescriptor): WebGPUSampler;
  createShaderModule(descriptor: WebGPUShaderModuleDescriptor): WebGPUShaderModule;
  createTexture(descriptor: WebGPUTextureDescriptor): WebGPUTexture;
  getObjectStatus(statusableObject: WebGPUStatusableObject): WebGPUObjectStatusQuery;
  getQueue(): WebGPUQueue;

  // TODO: temporary
  flush(): void;
}

export interface WebGPUFence extends WebGPUDebugLabel {
  getCompletedValue(): u64;
  onCompletion(completionValue: u64): Promise<void>;
}

export interface WebGPULogEntry {
  readonly reason: string | null;
  readonly sourceObject: any;
  readonly type: WebGPULogEntryType;
}

export interface WebGPUPipelineLayout {
}

export interface WebGPUProgrammablePassEncoder extends WebGPUDebugLabel {
  endPass(): WebGPUCommandBuffer;
  insertDebugMarker(markerLabel: string): void;
  popDebugGroup(groupLabel: string): void;
  pushDebugGroup(groupLabel: string): void;
  setBindGroup(index: number, bindGroup: WebGPUBindGroup): void;
  setPipeline(pipeline: WebGPUComputePipeline | WebGPURenderPipeline): void;
}

export interface WebGPUQueue extends WebGPUDebugLabel {
  signal(fence: WebGPUFence, signalValue: u64): void;
  submit(buffers: WebGPUCommandBuffer[]): void;
  wait(fence: WebGPUFence, valueToWait: u64): void;
}

export interface WebGPURenderPassEncoder extends WebGPUProgrammablePassEncoder {
  draw(vertexCount: number, instanceCount: number, firstVertex: number, firstInstance: number): void;
  drawIndexed(indexCount: number, instanceCount: number, firstIndex: number, baseVertex: number, firstInstance: number): void;
  setBlendColor(r: number, g: number, b: number, a: number): void;
  setIndexBuffer(buffer: WebGPUBuffer, offset: number): void;
  setScissorRect(x: number, y: number, width: number, height: number): void;
  setStencilReference(reference: number): void;
  setVertexBuffers(startSlot: number, buffers: WebGPUBuffer[], offsets: number[]): void;
  setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void;
}

export interface WebGPURenderPipeline extends WebGPUDebugLabel {
}

export interface WebGPURenderingContext extends WebGPUSwapChain {
}

export interface WebGPUSampler {
}

export interface WebGPUShaderModule extends WebGPUDebugLabel {
}

export interface WebGPUSwapChain {
  configure(descriptor: WebGPUSwapChainDescriptor): void;
  getNextTexture(): WebGPUTexture;
  present(): void;
}

export interface WebGPUTexture {
  createDefaultTextureView(): WebGPUTextureView;
  createTextureView(desc: WebGPUTextureViewDescriptor): WebGPUTextureView;
  destroy(): void;
}

export interface WebGPUTextureView {
}

export type WebGPULogCallback = (error: WebGPULogEntry) => void;

export type WebGPUPowerPreference =
  | "low-power"
  | "high-performance";
export interface WebGPURequestAdapterOptions {
  powerPreference: WebGPUPowerPreference;
}

export interface GPU {
  requestAdapter(options?: WebGPURequestAdapterOptions): Promise<WebGPUAdapter>;
  getDevice(): WebGPUDevice;
}
