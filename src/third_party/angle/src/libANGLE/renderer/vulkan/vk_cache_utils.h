//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cache_utils.h:
//    Contains the classes for the Pipeline State Object cache as well as the RenderPass cache.
//    Also contains the structures for the packed descriptions for the RenderPass and Pipeline.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_

#include "common/Color.h"
#include "common/FixedVector.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

namespace vk
{
class ImageHelper;

using RenderPassAndSerial = ObjectAndSerial<RenderPass>;
using PipelineAndSerial   = ObjectAndSerial<Pipeline>;

using SharedDescriptorSetLayout = RefCounted<DescriptorSetLayout>;
using SharedPipelineLayout      = RefCounted<PipelineLayout>;

// Packed Vk resource descriptions.
// Most Vk types use many more bits than required to represent the underlying data.
// Since ANGLE wants cache things like RenderPasses and Pipeline State Objects using
// hashing (and also needs to check equality) we can optimize these operations by
// using fewer bits. Hence the packed types.
//
// One implementation note: these types could potentially be improved by using even
// fewer bits. For example, boolean values could be represented by a single bit instead
// of a uint8_t. However at the current time there are concerns about the portability
// of bitfield operators, and complexity issues with using bit mask operations. This is
// something likely we will want to investigate as the Vulkan implementation progresses.
//
// Second implementation note: the struct packing is also a bit fragile, and some of the
// packing requirements depend on using alignas and field ordering to get the result of
// packing nicely into the desired space. This is something we could also potentially fix
// with a redesign to use bitfields or bit mask operations.

// Enable struct padding warnings for the code below since it is used in caches.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS

class alignas(4) RenderPassDesc final
{
  public:
    RenderPassDesc();
    ~RenderPassDesc();
    RenderPassDesc(const RenderPassDesc &other);
    RenderPassDesc &operator=(const RenderPassDesc &other);

    // The caller must pack the depth/stencil attachment last.
    void packAttachment(const Format &format);

    size_t hash() const;

    size_t colorAttachmentCount() const;
    size_t attachmentCount() const;

    void setSamples(GLint samples);

    uint8_t samples() const { return mSamples; }

    angle::FormatID operator[](size_t index) const
    {
        ASSERT(index < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS + 1);
        return static_cast<angle::FormatID>(mAttachmentFormats[index]);
    }

  private:
    uint8_t mSamples;
    uint8_t mColorAttachmentCount : 4;
    uint8_t mDepthStencilAttachmentCount : 4;
    gl::AttachmentArray<uint8_t> mAttachmentFormats;
};

bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs);

constexpr size_t kRenderPassDescSize = sizeof(RenderPassDesc);
static_assert(kRenderPassDescSize == 12, "Size check failed");

struct alignas(8) PackedAttachmentOpsDesc final
{
    uint8_t loadOp;
    uint8_t storeOp;
    uint8_t stencilLoadOp;
    uint8_t stencilStoreOp;

    // 16-bits to force pad the structure to exactly 8 bytes.
    uint16_t initialLayout;
    uint16_t finalLayout;
};

static_assert(sizeof(PackedAttachmentOpsDesc) == 8, "Size check failed");

class AttachmentOpsArray final
{
  public:
    AttachmentOpsArray();
    ~AttachmentOpsArray();
    AttachmentOpsArray(const AttachmentOpsArray &other);
    AttachmentOpsArray &operator=(const AttachmentOpsArray &other);

    const PackedAttachmentOpsDesc &operator[](size_t index) const;
    PackedAttachmentOpsDesc &operator[](size_t index);

    // Initializes an attachment op with whatever values. Used for compatible RenderPass checks.
    void initDummyOp(size_t index, VkImageLayout initialLayout, VkImageLayout finalLayout);

    size_t hash() const;

  private:
    gl::AttachmentArray<PackedAttachmentOpsDesc> mOps;
};

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs);

static_assert(sizeof(AttachmentOpsArray) == 80, "Size check failed");

struct PackedShaderStageInfo final
{
    uint32_t stage;
    uint32_t moduleSerial;
    // TODO(jmadill): Do we want specialization constants?
};

static_assert(sizeof(PackedShaderStageInfo) == 8, "Size check failed");

struct PackedVertexInputBindingDesc final
{
    // Although techncially stride can be any value in ES 2.0, in practice supporting stride
    // greater than MAX_USHORT should not be that helpful. Note that stride limits are
    // introduced in ES 3.1.
    uint16_t stride;
    uint16_t inputRate;
};

static_assert(sizeof(PackedVertexInputBindingDesc) == 4, "Size check failed");

struct PackedRasterizationAndMultisampleStateInfo final
{
    // Padded to ensure there's no gaps in this structure or those that use it.
    uint32_t depthClampEnable : 4;
    uint32_t rasterizationDiscardEnable : 4;
    uint32_t polygonMode : 4;
    uint32_t cullMode : 4;
    uint32_t frontFace : 4;
    uint32_t depthBiasEnable : 4;
    uint32_t rasterizationSamples : 4;
    uint32_t sampleShadingEnable : 1;
    uint32_t alphaToCoverageEnable : 1;
    uint32_t alphaToOneEnable : 2;
    float minSampleShading;
    uint32_t sampleMask[gl::MAX_SAMPLE_MASK_WORDS];
    float depthBiasConstantFactor;
    // Note: depth bias clamp is only exposed in a 3.1 extension, but left here for completeness.
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;
};

static constexpr size_t kPackedRasterizationAndMultisampleStateSize =
    sizeof(PackedRasterizationAndMultisampleStateInfo);
static_assert(kPackedRasterizationAndMultisampleStateSize == 32, "Size check failed");

struct PackedStencilOpState final
{
    uint8_t failOp : 4;
    uint8_t passOp : 4;
    uint8_t depthFailOp : 4;
    uint8_t compareOp : 4;
    uint8_t compareMask;
    uint8_t writeMask;
};

static constexpr size_t kPackedStencilOpSize = sizeof(PackedStencilOpState);
static_assert(sizeof(PackedStencilOpState) == 4, "Size check failed");

struct PackedDepthStencilStateInfo final
{
    uint8_t depthTestEnable : 1;
    uint8_t depthWriteEnable : 1;
    uint8_t depthCompareOp : 4;
    uint8_t depthBoundsTestEnable : 1;
    uint8_t stencilTestEnable : 1;
    uint8_t frontStencilReference;
    uint8_t backStencilReference;
    uint8_t padding;
    float minDepthBounds;
    float maxDepthBounds;
    PackedStencilOpState front;
    PackedStencilOpState back;
};

constexpr size_t kPackedDepthStencilStateSize = sizeof(PackedDepthStencilStateInfo);
static_assert(kPackedDepthStencilStateSize == 20, "Size check failed");

struct PackedColorBlendAttachmentState final
{
    uint16_t srcColorBlendFactor : 5;
    uint16_t dstColorBlendFactor : 5;
    uint16_t colorBlendOp : 6;
    uint16_t srcAlphaBlendFactor : 5;
    uint16_t dstAlphaBlendFactor : 5;
    uint16_t alphaBlendOp : 6;
};

static_assert(sizeof(PackedColorBlendAttachmentState) == 4, "Size check failed");

struct PackedInputAssemblyAndColorBlendStateInfo final
{
    uint8_t logicOpEnable : 1;
    uint8_t logicOp : 7;
    uint8_t blendEnableMask;
    uint8_t colorWriteMaskBits[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS / 2];
    PackedColorBlendAttachmentState attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    uint16_t topology : 15;
    uint16_t primitiveRestartEnable : 1;
    float blendConstants[4];
};

constexpr size_t kPackedInputAssemblyAndColorBlendStateSize =
    sizeof(PackedInputAssemblyAndColorBlendStateInfo);
static_assert(kPackedInputAssemblyAndColorBlendStateSize == 56, "Size check failed");

using VertexInputBindings = gl::AttribArray<PackedVertexInputBindingDesc>;

struct VertexInputAttributes final
{
    uint8_t formats[gl::MAX_VERTEX_ATTRIBS];
    uint16_t offsets[gl::MAX_VERTEX_ATTRIBS];  // can only take 11 bits on NV
};

constexpr size_t kVertexInputBindingsSize   = sizeof(VertexInputBindings);
constexpr size_t kVertexInputAttributesSize = sizeof(VertexInputAttributes);

class GraphicsPipelineDesc final
{
  public:
    // Use aligned allocation and free so we can use the alignas keyword.
    void *operator new(std::size_t size);
    void operator delete(void *ptr);

    GraphicsPipelineDesc();
    ~GraphicsPipelineDesc();
    GraphicsPipelineDesc(const GraphicsPipelineDesc &other);
    GraphicsPipelineDesc &operator=(const GraphicsPipelineDesc &other);

    size_t hash() const;
    bool operator==(const GraphicsPipelineDesc &other) const;

    void initDefaults();

    angle::Result initializePipeline(vk::Context *context,
                                     const vk::PipelineCache &pipelineCacheVk,
                                     const RenderPass &compatibleRenderPass,
                                     const PipelineLayout &pipelineLayout,
                                     const gl::AttributesMask &activeAttribLocationsMask,
                                     const ShaderModule &vertexModule,
                                     const ShaderModule &fragmentModule,
                                     Pipeline *pipelineOut) const;

    // Vertex input state
    void updateVertexInputInfo(const VertexInputBindings &bindings,
                               const VertexInputAttributes &attribs);

    // Input assembly info
    void updateTopology(gl::PrimitiveMode drawMode);

    // Raster states
    void updateCullMode(const gl::RasterizerState &rasterState);
    void updateFrontFace(const gl::RasterizerState &rasterState, bool invertFrontFace);
    void updateLineWidth(float lineWidth);

    // RenderPass description.
    const RenderPassDesc &getRenderPassDesc() const;
    void updateRenderPassDesc(const RenderPassDesc &renderPassDesc);

    // Blend states
    void updateBlendEnabled(bool isBlendEnabled);
    void updateBlendColor(const gl::ColorF &color);
    void updateBlendFuncs(const gl::BlendState &blendState);
    void updateBlendEquations(const gl::BlendState &blendState);
    void updateColorWriteMask(VkColorComponentFlags colorComponentFlags,
                              const gl::DrawBufferMask &alphaMask);

    // Depth/stencil states.
    void updateDepthTestEnabled(const gl::DepthStencilState &depthStencilState,
                                const gl::Framebuffer *drawFramebuffer);
    void updateDepthFunc(const gl::DepthStencilState &depthStencilState);
    void updateDepthWriteEnabled(const gl::DepthStencilState &depthStencilState,
                                 const gl::Framebuffer *drawFramebuffer);
    void updateStencilTestEnabled(const gl::DepthStencilState &depthStencilState,
                                  const gl::Framebuffer *drawFramebuffer);
    void updateStencilFrontFuncs(GLint ref, const gl::DepthStencilState &depthStencilState);
    void updateStencilBackFuncs(GLint ref, const gl::DepthStencilState &depthStencilState);
    void updateStencilFrontOps(const gl::DepthStencilState &depthStencilState);
    void updateStencilBackOps(const gl::DepthStencilState &depthStencilState);
    void updateStencilFrontWriteMask(const gl::DepthStencilState &depthStencilState,
                                     const gl::Framebuffer *drawFramebuffer);
    void updateStencilBackWriteMask(const gl::DepthStencilState &depthStencilState,
                                    const gl::Framebuffer *drawFramebuffer);

    // Depth offset.
    void updatePolygonOffsetFillEnabled(bool enabled);
    void updatePolygonOffset(const gl::RasterizerState &rasterState);

  private:
    VertexInputBindings mVertexInputBindings;
    VertexInputAttributes mVertexInputAttribs;
    RenderPassDesc mRenderPassDesc;
    PackedRasterizationAndMultisampleStateInfo mRasterizationAndMultisampleStateInfo;
    PackedDepthStencilStateInfo mDepthStencilStateInfo;
    PackedInputAssemblyAndColorBlendStateInfo mInputAssembltyAndColorBlendStateInfo;
    // Viewport and scissor are applied as dynamic state.
};

// Verify the packed pipeline description has no gaps in the packing.
// This is not guaranteed by the spec, but is validated by a compile-time check.
// No gaps or padding at the end ensures that hashing and memcmp checks will not run
// into uninitialized memory regions.
constexpr size_t kGraphicsPipelineDescSumOfSizes =
    kVertexInputBindingsSize + kVertexInputAttributesSize +
    kPackedInputAssemblyAndColorBlendStateSize + kPackedRasterizationAndMultisampleStateSize +
    kPackedDepthStencilStateSize + kRenderPassDescSize;

static constexpr size_t kGraphicsPipelineDescSize = sizeof(GraphicsPipelineDesc);
static_assert(kGraphicsPipelineDescSize == kGraphicsPipelineDescSumOfSizes, "Size mismatch");

constexpr uint32_t kMaxDescriptorSetLayoutBindings = gl::IMPLEMENTATION_MAX_ACTIVE_TEXTURES;

using DescriptorSetLayoutBindingVector =
    angle::FixedVector<VkDescriptorSetLayoutBinding, kMaxDescriptorSetLayoutBindings>;

// A packed description of a descriptor set layout. Use similarly to RenderPassDesc and
// GraphicsPipelineDesc. Currently we only need to differentiate layouts based on sampler usage. In
// the future we could generalize this.
class DescriptorSetLayoutDesc final
{
  public:
    DescriptorSetLayoutDesc();
    ~DescriptorSetLayoutDesc();
    DescriptorSetLayoutDesc(const DescriptorSetLayoutDesc &other);
    DescriptorSetLayoutDesc &operator=(const DescriptorSetLayoutDesc &other);

    size_t hash() const;
    bool operator==(const DescriptorSetLayoutDesc &other) const;

    void update(uint32_t bindingIndex, VkDescriptorType type, uint32_t count);

    void unpackBindings(DescriptorSetLayoutBindingVector *bindings) const;

  private:
    struct PackedDescriptorSetBinding
    {
        uint16_t type;   // Stores a packed VkDescriptorType descriptorType.
        uint16_t count;  // Stores a packed uint32_t descriptorCount.
        // We currently make all descriptors available in the VS and FS shaders.
    };

    static_assert(sizeof(PackedDescriptorSetBinding) == sizeof(uint32_t), "Unexpected size");

    // This is a compact representation of a descriptor set layout.
    std::array<PackedDescriptorSetBinding, kMaxDescriptorSetLayoutBindings>
        mPackedDescriptorSetLayout;
};

// The following are for caching descriptor set layouts. Limited to max two descriptor set layouts
// and two push constants. One push constant per shader stage. This can be extended in the future.
constexpr size_t kMaxDescriptorSetLayouts = 3;
constexpr size_t kMaxPushConstantRanges   = 2;

struct PackedPushConstantRange
{
    uint32_t offset;
    uint32_t size;
};

template <typename T>
using DescriptorSetLayoutArray = std::array<T, kMaxDescriptorSetLayouts>;
using DescriptorSetLayoutPointerArray =
    DescriptorSetLayoutArray<BindingPointer<DescriptorSetLayout>>;
template <typename T>
using PushConstantRangeArray = std::array<T, kMaxPushConstantRanges>;

class PipelineLayoutDesc final
{
  public:
    PipelineLayoutDesc();
    ~PipelineLayoutDesc();
    PipelineLayoutDesc(const PipelineLayoutDesc &other);
    PipelineLayoutDesc &operator=(const PipelineLayoutDesc &rhs);

    size_t hash() const;
    bool operator==(const PipelineLayoutDesc &other) const;

    void updateDescriptorSetLayout(uint32_t setIndex, const DescriptorSetLayoutDesc &desc);
    void updatePushConstantRange(gl::ShaderType shaderType, uint32_t offset, uint32_t size);

    const PushConstantRangeArray<PackedPushConstantRange> &getPushConstantRanges() const;

  private:
    DescriptorSetLayoutArray<DescriptorSetLayoutDesc> mDescriptorSetLayouts;
    PushConstantRangeArray<PackedPushConstantRange> mPushConstantRanges;

    // Verify the arrays are properly packed.
    static_assert(sizeof(decltype(mDescriptorSetLayouts)) ==
                      (sizeof(DescriptorSetLayoutDesc) * kMaxDescriptorSetLayouts),
                  "Unexpected size");
    static_assert(sizeof(decltype(mPushConstantRanges)) ==
                      (sizeof(PackedPushConstantRange) * kMaxPushConstantRanges),
                  "Unexpected size");
};

// Verify the structure is properly packed.
static_assert(sizeof(PipelineLayoutDesc) ==
                  (sizeof(DescriptorSetLayoutArray<DescriptorSetLayoutDesc>) +
                   sizeof(std::array<PackedPushConstantRange, kMaxPushConstantRanges>)),
              "Unexpected Size");

// Disable warnings about struct padding.
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS
}  // namespace vk
}  // namespace rx

// Introduce a std::hash for a RenderPassDesc
namespace std
{
template <>
struct hash<rx::vk::RenderPassDesc>
{
    size_t operator()(const rx::vk::RenderPassDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::AttachmentOpsArray>
{
    size_t operator()(const rx::vk::AttachmentOpsArray &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::GraphicsPipelineDesc>
{
    size_t operator()(const rx::vk::GraphicsPipelineDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::DescriptorSetLayoutDesc>
{
    size_t operator()(const rx::vk::DescriptorSetLayoutDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::PipelineLayoutDesc>
{
    size_t operator()(const rx::vk::PipelineLayoutDesc &key) const { return key.hash(); }
};
}  // namespace std

namespace rx
{
// TODO(jmadill): Add cache trimming/eviction.
class RenderPassCache final : angle::NonCopyable
{
  public:
    RenderPassCache();
    ~RenderPassCache();

    void destroy(VkDevice device);

    angle::Result getCompatibleRenderPass(vk::Context *context,
                                          Serial serial,
                                          const vk::RenderPassDesc &desc,
                                          vk::RenderPass **renderPassOut);
    angle::Result getRenderPassWithOps(vk::Context *context,
                                       Serial serial,
                                       const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &attachmentOps,
                                       vk::RenderPass **renderPassOut);

  private:
    // Use a two-layer caching scheme. The top level matches the "compatible" RenderPass elements.
    // The second layer caches the attachment load/store ops and initial/final layout.
    using InnerCache = std::unordered_map<vk::AttachmentOpsArray, vk::RenderPassAndSerial>;
    using OuterCache = std::unordered_map<vk::RenderPassDesc, InnerCache>;

    OuterCache mPayload;
};

// TODO(jmadill): Add cache trimming/eviction.
class GraphicsPipelineCache final : angle::NonCopyable
{
  public:
    GraphicsPipelineCache();
    ~GraphicsPipelineCache();

    void destroy(VkDevice device);
    void release(RendererVk *renderer);

    void populate(const vk::GraphicsPipelineDesc &desc, vk::Pipeline &&pipeline);
    angle::Result getPipeline(vk::Context *context,
                              const vk::PipelineCache &pipelineCacheVk,
                              const vk::RenderPass &compatibleRenderPass,
                              const vk::PipelineLayout &pipelineLayout,
                              const gl::AttributesMask &activeAttribLocationsMask,
                              const vk::ShaderModule &vertexModule,
                              const vk::ShaderModule &fragmentModule,
                              const vk::GraphicsPipelineDesc &desc,
                              vk::PipelineAndSerial **pipelineOut);

  private:
    std::unordered_map<vk::GraphicsPipelineDesc, vk::PipelineAndSerial> mPayload;
};

class DescriptorSetLayoutCache final : angle::NonCopyable
{
  public:
    DescriptorSetLayoutCache();
    ~DescriptorSetLayoutCache();

    void destroy(VkDevice device);

    angle::Result getDescriptorSetLayout(
        vk::Context *context,
        const vk::DescriptorSetLayoutDesc &desc,
        vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut);

  private:
    std::unordered_map<vk::DescriptorSetLayoutDesc, vk::SharedDescriptorSetLayout> mPayload;
};

class PipelineLayoutCache final : angle::NonCopyable
{
  public:
    PipelineLayoutCache();
    ~PipelineLayoutCache();

    void destroy(VkDevice device);

    angle::Result getPipelineLayout(vk::Context *context,
                                    const vk::PipelineLayoutDesc &desc,
                                    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
                                    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut);

  private:
    std::unordered_map<vk::PipelineLayoutDesc, vk::SharedPipelineLayout> mPayload;
};

// Some descriptor set and pipeline layout constants.
constexpr uint32_t kVertexUniformsBindingIndex       = 0;
constexpr uint32_t kFragmentUniformsBindingIndex     = 1;
constexpr uint32_t kUniformsDescriptorSetIndex       = 0;
constexpr uint32_t kTextureDescriptorSetIndex        = 1;
constexpr uint32_t kDriverUniformsDescriptorSetIndex = 2;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_
