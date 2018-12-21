type u64 = number;

type WebGPUObjectStatusQuery = Promise<WebGPUObjectStatus>;
type WebGPUStatusableObject = WebGPUBuffer | WebGPUTexture;
type WebGPUBindingResource = WebGPUSampler | WebGPUTextureView | WebGPUBufferBinding;

type WebGPUAddressMode = "clampToEdge" | "repeat" | "mirrorRepeat" | "clampToBorderColor";
type WebGPUBindingType = "uniformBuffer" | "sampler" | "sampledTexture" | "storageBuffer";
type WebGPUBlendFactor = "zero" | "one" | "srcColor" | "oneMinusSrcColor" | "srcAlpha" | "oneMinusSrcAlpha" | "dstColor" | "oneMinusDstColor" | "dstAlpha" | "oneMinusDstAlpha" | "srcAlphaSaturated" | "blendColor" | "oneMinusBlendColor";
type WebGPUBlendOperation = "add" | "subtract" | "reverseSubtract" | "min" | "max";
type WebGPUBorderColor = "transparentBlack" | "opaqueBlack" | "opaqueWhite";
type WebGPUCompareFunction = "never" | "less" | "equal" | "lessEqual" | "greater" | "notEqual" | "greaterEqual" | "always";
type WebGPUCullMode = "none" | "front" | "back";
type WebGPUFilterMode = "nearest" | "linear";
type WebGPUFrontFace = "ccw" | "cw";
type WebGPUIndexFormat = "uint16" | "uint32";
type WebGPUInputStepMode = "vertex" | "instance";
type WebGPULoadOp = "clear" | "load";
type WebGPULogEntryType = "device-lost" | "validation-error" | "recoverable-out-of-memory";
type WebGPUObjectStatus = "valid" | "out-of-memory" | "invalid";
type WebGPUPrimitiveTopology = "pointList" | "lineList" | "lineStrip" | "trangleList" | "triangleStrip";
type WebGPUStencilOperation = "keep" | "zero" | "replace" | "invert" | "incrementClamp" | "decrementClamp" | "incrementWrap" | "decrementWrap";
type WebGPUStoreOp = "store";
type WebGPUTextureDimension = "1d" | "2d" | "3d";
type WebGPUTextureFormat = "R8G8B8A8Unorm" | "R8G8B8A8Uint" | "B8G8R8A8Unorm" | "D32FloatS8Uint";
type WebGPUTextureViewDimension = "1d" | "2d" | "2darray" | "cube" | "cubearray" | "3d";
type WebGPUVertexFormat = "floatR32G32B32A32" | "floatR32G32B32" | "floatR32G32" | "floatR32";

type WebGPUBufferUsageFlags = number;
declare const enum WebGPUBufferUsage {
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

type WebGPUColorWriteFlags = number;
declare const enum WebGPUColorWriteBits {
    NONE = 0,
    RED = 1,
    GREEN = 2,
    BLUE = 4,
    ALPHA = 8,
    ALL = 15,
}

type WebGPUShaderStageFlags = number;
declare const enum WebGPUShaderStageBit {
  NONE = 0,
  VERTEX = 1,
  FRAGMENT = 2,
  COMPUTE = 4,
}

type WebGPUTextureAspectFlags = number;
declare const enum WebGPUTextureAspect {
  COLOR = 1,
  DEPTH = 2,
  STENCIL = 4,
}

type WebGPUTextureUsageFlags = number;
declare const enum WebGPUTextureUsage {
  NONE = 0,
  TRANSFER_SRC = 1,
  TRANSFER_DST = 2,
  SAMPLED = 4,
  STORAGE = 8,
  OUTPUT_ATTACHMENT = 16,
  PRESENT = 32,
}

interface WebGPUAttachmentDescriptor {
    format?: WebGPUTextureFormat;
}

interface WebGPUAttachmentsStateDescriptor {
    colorAttachments?: WebGPUAttachmentDescriptor[];
    depthStencilAttachment?: WebGPUAttachmentDescriptor | null;
}

interface WebGPUBindGroupBinding {
    binding?: number;
    resource?: WebGPUBindingResource;
}

interface WebGPUBindGroupDescriptor {
    bindings?: WebGPUBindGroupBinding[];
    layout?: WebGPUBindGroupLayout;
}

interface WebGPUBindGroupLayoutBinding {
    binding?: number;
    type?: WebGPUBindingType;
    visibility?: WebGPUShaderStageFlags;
}

interface WebGPUBindGroupLayoutDescriptor {
    bindings?: WebGPUBindGroupLayoutBinding[];
}

interface WebGPUBlendDescriptor {
    dstFactor?: WebGPUBlendFactor;
    operation?: WebGPUBlendOperation;
    srcFactor?: WebGPUBlendFactor;
}

interface WebGPUBlendStateDescriptor {
    alpha?: WebGPUBlendDescriptor;
    blendEnabled?: boolean;
    color?: WebGPUBlendDescriptor;
    writeMask?: WebGPUColorWriteFlags;
}

interface WebGPUBufferBinding {
    buffer?: WebGPUBuffer;
    offset?: number;
    size?: number;
}

interface WebGPUBufferCopyView {
    buffer?: WebGPUBuffer;
    imageHeight?: number;
    offset?: number;
    rowPitch?: number;
}

interface WebGPUBufferDescriptor {
    size?: number;
    usage?: WebGPUBufferUsageFlags;
}

interface WebGPUColor {
    a?: number;
    b?: number;
    g?: number;
    r?: number;
}

interface WebGPUCommandBufferDescriptor {
    label?: string;
}

interface WebGPUComputePipelineDescriptor extends WebGPUPipelineDescriptorBase {
    computeStage?: WebGPUPipelineStageDescriptor;
}

interface WebGPUDepthStencilStateDescriptor {
    back?: WebGPUStencilStateFaceDescriptor;
    depthCompare?: WebGPUCompareFunction;
    depthWriteEnabled?: boolean;
    front?: WebGPUStencilStateFaceDescriptor;
    stencilReadMask?: number;
    stencilWriteMask?: number;
}

interface WebGPUDeviceDescriptor {
    extensions?: WebGPUExtensions;
}

interface WebGPUExtensions {
    anisotropicFiltering?: boolean;
}

interface WebGPUExtent3D {
    depth?: number;
    height?: number;
    width?: number;
}

interface WebGPUFenceDescriptor {
    initialValue?: u64;
    label?: string;
    signalQueue?: WebGPUQueue;
}

interface WebGPUInputStateDescriptor {
    attributes?: WebGPUVertexAttributeDescriptor[];
    indexFormat?: WebGPUIndexFormat;
    inputs?: WebGPUVertexInputDescriptor[];
}

interface WebGPULimits {
    maxBindGroups?: number;
}

interface WebGPUOrigin3D {
    x?: number;
    y?: number;
    z?: number;
}

interface WebGPUPipelineDescriptorBase {
    label?: string;
    layout?: WebGPUPipelineLayout;
}

interface WebGPUPipelineLayoutDescriptor {
    bindGroupLayouts?: WebGPUBindGroupLayout[];
}

interface WebGPUPipelineStageDescriptor {
    entryPoint?: string;
    module?: WebGPUShaderModule;
}

interface WebGPURasterizationStateDescriptor {
    cullMode?: WebGPUCullMode;
    depthBias?: number;
    depthBiasClamp?: number;
    depthBiasSlopeScale?: number;
    frontFace?: WebGPUFrontFace;
}

interface WebGPURenderPassColorAttachmentDescriptor {
    attachment?: WebGPUTextureView;
    clearColor?: WebGPUColor;
    loadOp?: WebGPULoadOp;
    resolveTarget?: WebGPUTextureView | null;
    storeOp?: WebGPUStoreOp;
}

interface WebGPURenderPassDepthStencilAttachmentDescriptor {
    attachment?: WebGPUTextureView;
    clearDepth?: number;
    clearStencil?: number;
    depthLoadOp?: WebGPULoadOp;
    depthStoreOp?: WebGPUStoreOp;
    stencilLoadOp?: WebGPULoadOp;
    stencilStoreOp?: WebGPUStoreOp;
}

interface WebGPURenderPassDescriptor {
    colorAttachments?: WebGPURenderPassColorAttachmentDescriptor[];
    depthStencilAttachment?: WebGPURenderPassDepthStencilAttachmentDescriptor;
}

interface WebGPURenderPipelineDescriptor extends WebGPUPipelineDescriptorBase {
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

interface WebGPUSamplerDescriptor {
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

interface WebGPUShaderModuleDescriptor {
    code: ArrayBuffer | string;
    label?: string;
}

interface WebGPUStencilStateFaceDescriptor {
    compare?: WebGPUCompareFunction;
    depthFailOp?: WebGPUStencilOperation;
    passOp?: WebGPUStencilOperation;
    stencilFailOp?: WebGPUStencilOperation;
}

interface WebGPUSwapChainDescriptor {
    device?: WebGPUDevice | null;
    format?: WebGPUTextureFormat;
    height?: number;
    usage?: WebGPUTextureUsageFlags;
    width?: number;
}

interface WebGPUTextureCopyView {
    level?: number;
    origin?: WebGPUOrigin3D;
    slice?: number;
    texture?: WebGPUTexture;
}

interface WebGPUTextureDescriptor {
    arraySize?: number;
    dimension?: WebGPUTextureDimension;
    format?: WebGPUTextureFormat;
    levelCount?: number;
    sampleCount?: number;
    size?: WebGPUExtent3D;
    usage?: WebGPUTextureUsageFlags;
}

interface WebGPUTextureViewDescriptor {
    aspect?: WebGPUTextureAspectFlags;
    baseArrayLayer?: number;
    baseMipLevel?: number;
    dimension?: WebGPUTextureViewDimension;
    format?: WebGPUTextureFormat;
    layerCount?: number;
    levelCount?: number;
}

interface WebGPUVertexAttributeDescriptor {
    format?: WebGPUVertexFormat;
    inputSlot?: number;
    offset?: number;
    shaderLocation?: number;
}

interface WebGPUVertexInputDescriptor {
    inputSlot?: number;
    stepMode?: WebGPUInputStepMode;
    stride?: number;
}

interface WebGPUAdapter {
    readonly extensions: WebGPUExtensions;
    readonly name: string;
    createDevice(descriptor: WebGPUDeviceDescriptor): WebGPUDevice;
}

interface WebGPUBindGroup {
}

interface WebGPUBindGroupLayout {
}

interface WebGPUBuffer {
    readonly mapping: ArrayBuffer | null;
    destroy(): void;
    unmap(): void;
}

interface WebGPUCommandBuffer extends WebGPUDebugLabel {
    beginComputePass(): WebGPUComputePassEncoder;
    beginRenderPass(descriptor: WebGPURenderPassDescriptor): WebGPURenderPassEncoder;
    blit(): void;
    copyBufferToBuffer(src: WebGPUBuffer, srcOffset: number, dst: WebGPUBuffer, dstOffset: number, size: number): void;
    copyBufferToTexture(source: WebGPUBufferCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToBuffer(source: WebGPUTextureCopyView, destination: WebGPUBufferCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToTexture(source: WebGPUTextureCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
}

interface WebGPUComputePassEncoder extends WebGPUProgrammablePassEncoder {
    dispatch(x: number, y: number, z: number): void;
}

interface WebGPUComputePipeline extends WebGPUDebugLabel {
}

interface WebGPUDebugLabel {
    label: string;
}

interface WebGPUDevice {
    readonly adapter: WebGPUAdapter;
    readonly extensions: WebGPUExtensions;
    readonly limits: WebGPULimits;
    onLog: WebGPULogCallback;
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
}

interface WebGPUFence extends WebGPUDebugLabel {
    getCompletedValue(): u64;
    onCompletion(completionValue: u64): Promise<void>;
}

interface WebGPULogEntry {
    readonly reason: string | null;
    readonly sourceObject: any;
    readonly type: WebGPULogEntryType;
}

interface WebGPUPipelineLayout {
}

interface WebGPUProgrammablePassEncoder extends WebGPUDebugLabel {
    endPass(): WebGPUCommandBuffer;
    insertDebugMarker(markerLabel: string): void;
    popDebugGroup(groupLabel: string): void;
    pushDebugGroup(groupLabel: string): void;
    setBindGroup(index: number, bindGroup: WebGPUBindGroup): void;
    setPipeline(pipeline: WebGPUComputePipeline | WebGPURenderPipeline): void;
}

interface WebGPUQueue extends WebGPUDebugLabel {
    signal(fence: WebGPUFence, signalValue: u64): void;
    submit(buffers: WebGPUCommandBuffer[]): void;
    wait(fence: WebGPUFence, valueToWait: u64): void;
}

interface WebGPURenderPassEncoder extends WebGPUProgrammablePassEncoder {
    draw(vertexCount: number, instanceCount: number, firstVertex: number, firstInstance: number): void;
    drawIndexed(indexCount: number, instanceCount: number, firstIndex: number, baseVertex: number, firstInstance: number): void;
    setBlendColor(r: number, g: number, b: number, a: number): void;
    setIndexBuffer(buffer: WebGPUBuffer, offset: number): void;
    setScissorRect(x: number, y: number, width: number, height: number): void;
    setStencilReference(reference: number): void;
    setVertexBuffers(startSlot: number, buffers: WebGPUBuffer[], offsets: number[]): void;
    setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void;
}

interface WebGPURenderPipeline extends WebGPUDebugLabel {
}

interface WebGPURenderingContext extends WebGPUSwapChain {
}

interface WebGPUSampler {
}

interface WebGPUShaderModule extends WebGPUDebugLabel {
}

interface WebGPUSwapChain {
    configure(descriptor: WebGPUSwapChainDescriptor): void;
    getNextTexture(): WebGPUTexture;
    present(): void;
}

interface WebGPUTexture {
    createDefaultTextureView(): WebGPUTextureView;
    createTextureView(desc: WebGPUTextureViewDescriptor): WebGPUTextureView;
    destroy(): void;
}

interface WebGPUTextureView {
}

type WebGPULogCallback = (error: WebGPULogEntry) => void;
