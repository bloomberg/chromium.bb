type u64 = number;

type WebGPUObjectStatusQuery = Promise<WebGPUObjectStatus>;
type WebGPUStatusableObject = WebGPUBuffer | WebGPUTexture;
type WebGPUBufferUsageFlags = number;
type WebGPUTextureUsageFlags = number;
type WebGPUTextureAspectFlags = number;
type WebGPUShaderStageFlags = number;
type WebGPUBindingResource = WebGPUSampler | WebGPUTextureView | WebGPUBufferBinding;
type WebGPUColorWriteFlags = number;

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

declare var WebGPUAdapter: {
    prototype: WebGPUAdapter;
    new(): WebGPUAdapter;
};

interface WebGPUBindGroup {
}

declare var WebGPUBindGroup: {
    prototype: WebGPUBindGroup;
    new(): WebGPUBindGroup;
};

interface WebGPUBindGroupLayout {
}

declare var WebGPUBindGroupLayout: {
    prototype: WebGPUBindGroupLayout;
    new(): WebGPUBindGroupLayout;
};

interface WebGPUBuffer {
    readonly mapping: ArrayBuffer | null;
    destroy(): void;
    unmap(): void;
}

declare var WebGPUBuffer: {
    prototype: WebGPUBuffer;
    new(): WebGPUBuffer;
};

interface WebGPUBufferUsage {
    readonly INDEX: number;
    readonly MAP_READ: number;
    readonly MAP_WRITE: number;
    readonly NONE: number;
    readonly STORAGE: number;
    readonly TRANSFER_DST: number;
    readonly TRANSFER_SRC: number;
    readonly UNIFORM: number;
    readonly VERTEX: number;
}

declare var WebGPUBufferUsage: {
    prototype: WebGPUBufferUsage;
    new(): WebGPUBufferUsage;
    readonly INDEX: number;
    readonly MAP_READ: number;
    readonly MAP_WRITE: number;
    readonly NONE: number;
    readonly STORAGE: number;
    readonly TRANSFER_DST: number;
    readonly TRANSFER_SRC: number;
    readonly UNIFORM: number;
    readonly VERTEX: number;
};

interface WebGPUColorWriteBits {
    readonly ALL: number;
    readonly ALPHA: number;
    readonly BLUE: number;
    readonly GREEN: number;
    readonly NONE: number;
    readonly RED: number;
}

declare var WebGPUColorWriteBits: {
    prototype: WebGPUColorWriteBits;
    new(): WebGPUColorWriteBits;
    readonly ALL: number;
    readonly ALPHA: number;
    readonly BLUE: number;
    readonly GREEN: number;
    readonly NONE: number;
    readonly RED: number;
};

interface WebGPUCommandBuffer extends WebGPUDebugLabel {
    beginComputePass(): WebGPUComputePassEncoder;
    beginRenderPass(descriptor: WebGPURenderPassDescriptor): WebGPURenderPassEncoder;
    blit(): void;
    copyBufferToBuffer(src: WebGPUBuffer, srcOffset: number, dst: WebGPUBuffer, dstOffset: number, size: number): void;
    copyBufferToTexture(source: WebGPUBufferCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToBuffer(source: WebGPUTextureCopyView, destination: WebGPUBufferCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToTexture(source: WebGPUTextureCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
}

declare var WebGPUCommandBuffer: {
    prototype: WebGPUCommandBuffer;
    new(): WebGPUCommandBuffer;
};

interface WebGPUComputePassEncoder extends WebGPUProgrammablePassEncoder {
    dispatch(x: number, y: number, z: number): void;
}

declare var WebGPUComputePassEncoder: {
    prototype: WebGPUComputePassEncoder;
    new(): WebGPUComputePassEncoder;
};

interface WebGPUComputePipeline extends WebGPUDebugLabel {
}

declare var WebGPUComputePipeline: {
    prototype: WebGPUComputePipeline;
    new(): WebGPUComputePipeline;
};

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

declare var WebGPUDevice: {
    prototype: WebGPUDevice;
    new(): WebGPUDevice;
};

interface WebGPUFence extends WebGPUDebugLabel {
    getCompletedValue(): u64;
    onCompletion(completionValue: u64): Promise<void>;
}

declare var WebGPUFence: {
    prototype: WebGPUFence;
    new(): WebGPUFence;
};

interface WebGPULogEntry {
    readonly reason: string | null;
    readonly sourceObject: any;
    readonly type: WebGPULogEntryType;
}

declare var WebGPULogEntry: {
    prototype: WebGPULogEntry;
    new(): WebGPULogEntry;
};

interface WebGPUPipelineLayout {
}

declare var WebGPUPipelineLayout: {
    prototype: WebGPUPipelineLayout;
    new(): WebGPUPipelineLayout;
};

interface WebGPUProgrammablePassEncoder extends WebGPUDebugLabel {
    endPass(): WebGPUCommandBuffer;
    insertDebugMarker(markerLabel: string): void;
    popDebugGroup(groupLabel: string): void;
    pushDebugGroup(groupLabel: string): void;
    setBindGroup(index: number, bindGroup: WebGPUBindGroup): void;
    setPipeline(pipeline: WebGPUComputePipeline | WebGPURenderPipeline): void;
}

declare var WebGPUProgrammablePassEncoder: {
    prototype: WebGPUProgrammablePassEncoder;
    new(): WebGPUProgrammablePassEncoder;
};

interface WebGPUQueue extends WebGPUDebugLabel {
    signal(fence: WebGPUFence, signalValue: u64): void;
    submit(buffers: WebGPUCommandBuffer[]): void;
    wait(fence: WebGPUFence, valueToWait: u64): void;
}

declare var WebGPUQueue: {
    prototype: WebGPUQueue;
    new(): WebGPUQueue;
};

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

declare var WebGPURenderPassEncoder: {
    prototype: WebGPURenderPassEncoder;
    new(): WebGPURenderPassEncoder;
};

interface WebGPURenderPipeline extends WebGPUDebugLabel {
}

declare var WebGPURenderPipeline: {
    prototype: WebGPURenderPipeline;
    new(): WebGPURenderPipeline;
};

interface WebGPURenderingContext extends WebGPUSwapChain {
}

declare var WebGPURenderingContext: {
    prototype: WebGPURenderingContext;
    new(): WebGPURenderingContext;
};

interface WebGPUSampler {
}

declare var WebGPUSampler: {
    prototype: WebGPUSampler;
    new(): WebGPUSampler;
};

interface WebGPUShaderModule extends WebGPUDebugLabel {
}

declare var WebGPUShaderModule: {
    prototype: WebGPUShaderModule;
    new(): WebGPUShaderModule;
};

interface WebGPUShaderStageBit {
    readonly COMPUTE: number;
    readonly FRAGMENT: number;
    readonly NONE: number;
    readonly VERTEX: number;
}

declare var WebGPUShaderStageBit: {
    prototype: WebGPUShaderStageBit;
    new(): WebGPUShaderStageBit;
    readonly COMPUTE: number;
    readonly FRAGMENT: number;
    readonly NONE: number;
    readonly VERTEX: number;
};

interface WebGPUSwapChain {
    configure(descriptor: WebGPUSwapChainDescriptor): void;
    getNextTexture(): WebGPUTexture;
    present(): void;
}

declare var WebGPUSwapChain: {
    prototype: WebGPUSwapChain;
    new(): WebGPUSwapChain;
};

interface WebGPUTexture {
    createDefaultTextureView(): WebGPUTextureView;
    createTextureView(desc: WebGPUTextureViewDescriptor): WebGPUTextureView;
    destroy(): void;
}

declare var WebGPUTexture: {
    prototype: WebGPUTexture;
    new(): WebGPUTexture;
};

interface WebGPUTextureAspect {
    readonly COLOR: number;
    readonly DEPTH: number;
    readonly STENCIL: number;
}

declare var WebGPUTextureAspect: {
    prototype: WebGPUTextureAspect;
    new(): WebGPUTextureAspect;
    readonly COLOR: number;
    readonly DEPTH: number;
    readonly STENCIL: number;
};

interface WebGPUTextureUsage {
    readonly NONE: number;
    readonly OUTPUT_ATTACHMENT: number;
    readonly PRESENT: number;
    readonly SAMPLED: number;
    readonly STORAGE: number;
    readonly TRANSFER_DST: number;
    readonly TRANSFER_SRC: number;
}

declare var WebGPUTextureUsage: {
    prototype: WebGPUTextureUsage;
    new(): WebGPUTextureUsage;
    readonly NONE: number;
    readonly OUTPUT_ATTACHMENT: number;
    readonly PRESENT: number;
    readonly SAMPLED: number;
    readonly STORAGE: number;
    readonly TRANSFER_DST: number;
    readonly TRANSFER_SRC: number;
};

interface WebGPUTextureView {
}

declare var WebGPUTextureView: {
    prototype: WebGPUTextureView;
    new(): WebGPUTextureView;
};

interface WebGPULogCallback {
    (error: WebGPULogEntry): void;
}
