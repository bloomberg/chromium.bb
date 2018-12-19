type i32 = number;
type u32 = number;
type u64 = number;

type WebGPUObjectStatusQuery = Promise<WebGPUObjectStatus>;
type WebGPUStatusableObject = WebGPUBuffer | WebGPUTexture;
type WebGPUBufferUsageFlags = u32;
type WebGPUTextureUsageFlags = u32;
type WebGPUTextureAspectFlags = u32;
type WebGPUShaderStageFlags = u32;
type WebGPUBindingResource = WebGPUSampler | WebGPUTextureView | WebGPUBufferBinding;
type WebGPUColorWriteFlags = u32;

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
    binding?: u32;
    resource?: WebGPUBindingResource;
}

interface WebGPUBindGroupDescriptor {
    bindings?: WebGPUBindGroupBinding[];
    layout?: WebGPUBindGroupLayout;
}

interface WebGPUBindGroupLayoutBinding {
    binding?: u32;
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
    offset?: u32;
    size?: u32;
}

interface WebGPUBufferCopyView {
    buffer?: WebGPUBuffer;
    imageHeight?: u32;
    offset?: u32;
    rowPitch?: u32;
}

interface WebGPUBufferDescriptor {
    size?: u32;
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
    stencilReadMask?: u32;
    stencilWriteMask?: u32;
}

interface WebGPUDeviceDescriptor {
    extensions?: WebGPUExtensions;
}

interface WebGPUExtensions {
    anisotropicFiltering?: boolean;
}

interface WebGPUExtent3D {
    depth?: u32;
    height?: u32;
    width?: u32;
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
    maxBindGroups?: u32;
}

interface WebGPUOrigin3D {
    x?: u32;
    y?: u32;
    z?: u32;
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
    depthBias?: i32;
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
    clearStencil?: u32;
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
    sampleCount?: u32;
    vertexStage?: WebGPUPipelineStageDescriptor;
}

interface WebGPUSamplerDescriptor {
    borderColor?: WebGPUBorderColor;
    compareFunction?: WebGPUCompareFunction;
    lodMaxClamp?: number;
    lodMinClamp?: number;
    magFilter?: WebGPUFilterMode;
    maxAnisotropy?: u32;
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
    height?: u32;
    usage?: WebGPUTextureUsageFlags;
    width?: u32;
}

interface WebGPUTextureCopyView {
    level?: u32;
    origin?: WebGPUOrigin3D;
    slice?: u32;
    texture?: WebGPUTexture;
}

interface WebGPUTextureDescriptor {
    arraySize?: u32;
    dimension?: WebGPUTextureDimension;
    format?: WebGPUTextureFormat;
    levelCount?: u32;
    sampleCount?: u32;
    size?: WebGPUExtent3D;
    usage?: WebGPUTextureUsageFlags;
}

interface WebGPUTextureViewDescriptor {
    aspect?: WebGPUTextureAspectFlags;
    baseArrayLayer?: u32;
    baseMipLevel?: u32;
    dimension?: WebGPUTextureViewDimension;
    format?: WebGPUTextureFormat;
    layerCount?: u32;
    levelCount?: u32;
}

interface WebGPUVertexAttributeDescriptor {
    format?: WebGPUVertexFormat;
    inputSlot?: u32;
    offset?: u32;
    shaderLocation?: u32;
}

interface WebGPUVertexInputDescriptor {
    inputSlot?: u32;
    stepMode?: WebGPUInputStepMode;
    stride?: u32;
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
    readonly INDEX: u32;
    readonly MAP_READ: u32;
    readonly MAP_WRITE: u32;
    readonly NONE: u32;
    readonly STORAGE: u32;
    readonly TRANSFER_DST: u32;
    readonly TRANSFER_SRC: u32;
    readonly UNIFORM: u32;
    readonly VERTEX: u32;
}

declare var WebGPUBufferUsage: {
    prototype: WebGPUBufferUsage;
    new(): WebGPUBufferUsage;
    readonly INDEX: u32;
    readonly MAP_READ: u32;
    readonly MAP_WRITE: u32;
    readonly NONE: u32;
    readonly STORAGE: u32;
    readonly TRANSFER_DST: u32;
    readonly TRANSFER_SRC: u32;
    readonly UNIFORM: u32;
    readonly VERTEX: u32;
};

interface WebGPUColorWriteBits {
    readonly ALL: u32;
    readonly ALPHA: u32;
    readonly BLUE: u32;
    readonly GREEN: u32;
    readonly NONE: u32;
    readonly RED: u32;
}

declare var WebGPUColorWriteBits: {
    prototype: WebGPUColorWriteBits;
    new(): WebGPUColorWriteBits;
    readonly ALL: u32;
    readonly ALPHA: u32;
    readonly BLUE: u32;
    readonly GREEN: u32;
    readonly NONE: u32;
    readonly RED: u32;
};

interface WebGPUCommandBuffer extends WebGPUDebugLabel {
    beginComputePass(): WebGPUComputePassEncoder;
    beginRenderPass(descriptor: WebGPURenderPassDescriptor): WebGPURenderPassEncoder;
    blit(): void;
    copyBufferToBuffer(src: WebGPUBuffer, srcOffset: u32, dst: WebGPUBuffer, dstOffset: u32, size: u32): void;
    copyBufferToTexture(source: WebGPUBufferCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToBuffer(source: WebGPUTextureCopyView, destination: WebGPUBufferCopyView, copySize: WebGPUExtent3D): void;
    copyTextureToTexture(source: WebGPUTextureCopyView, destination: WebGPUTextureCopyView, copySize: WebGPUExtent3D): void;
}

declare var WebGPUCommandBuffer: {
    prototype: WebGPUCommandBuffer;
    new(): WebGPUCommandBuffer;
};

interface WebGPUComputePassEncoder extends WebGPUProgrammablePassEncoder {
    dispatch(x: u32, y: u32, z: u32): void;
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
    setBindGroup(index: u32, bindGroup: WebGPUBindGroup): void;
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
    draw(vertexCount: u32, instanceCount: u32, firstVertex: u32, firstInstance: u32): void;
    drawIndexed(indexCount: u32, instanceCount: u32, firstIndex: u32, baseVertex: i32, firstInstance: u32): void;
    setBlendColor(r: number, g: number, b: number, a: number): void;
    setIndexBuffer(buffer: WebGPUBuffer, offset: u32): void;
    setScissorRect(x: u32, y: u32, width: u32, height: u32): void;
    setStencilReference(reference: u32): void;
    setVertexBuffers(startSlot: u32, buffers: WebGPUBuffer[], offsets: u32[]): void;
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
    readonly COMPUTE: u32;
    readonly FRAGMENT: u32;
    readonly NONE: u32;
    readonly VERTEX: u32;
}

declare var WebGPUShaderStageBit: {
    prototype: WebGPUShaderStageBit;
    new(): WebGPUShaderStageBit;
    readonly COMPUTE: u32;
    readonly FRAGMENT: u32;
    readonly NONE: u32;
    readonly VERTEX: u32;
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
    readonly COLOR: u32;
    readonly DEPTH: u32;
    readonly STENCIL: u32;
}

declare var WebGPUTextureAspect: {
    prototype: WebGPUTextureAspect;
    new(): WebGPUTextureAspect;
    readonly COLOR: u32;
    readonly DEPTH: u32;
    readonly STENCIL: u32;
};

interface WebGPUTextureUsage {
    readonly NONE: u32;
    readonly OUTPUT_ATTACHMENT: u32;
    readonly PRESENT: u32;
    readonly SAMPLED: u32;
    readonly STORAGE: u32;
    readonly TRANSFER_DST: u32;
    readonly TRANSFER_SRC: u32;
}

declare var WebGPUTextureUsage: {
    prototype: WebGPUTextureUsage;
    new(): WebGPUTextureUsage;
    readonly NONE: u32;
    readonly OUTPUT_ATTACHMENT: u32;
    readonly PRESENT: u32;
    readonly SAMPLED: u32;
    readonly STORAGE: u32;
    readonly TRANSFER_DST: u32;
    readonly TRANSFER_SRC: u32;
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
