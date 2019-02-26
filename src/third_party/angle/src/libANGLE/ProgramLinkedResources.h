//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramLinkedResources.h: implements link-time checks for default block uniforms, and generates
// uniform locations. Populates data structures related to uniforms so that they can be stored in
// program state.

#ifndef LIBANGLE_UNIFORMLINKER_H_
#define LIBANGLE_UNIFORMLINKER_H_

#include "angle_gl.h"
#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "libANGLE/VaryingPacking.h"

#include <functional>

namespace sh
{
struct BlockMemberInfo;
struct InterfaceBlock;
struct ShaderVariable;
struct Uniform;
}  // namespace sh

namespace gl
{
struct BufferVariable;
struct Caps;
class Context;
class InfoLog;
struct InterfaceBlock;
enum class LinkMismatchError;
struct LinkedUniform;
class ProgramState;
class ProgramBindings;
class Shader;
struct ShaderVariableBuffer;
struct UnusedUniform;
struct VariableLocation;

using AtomicCounterBuffer = ShaderVariableBuffer;

class UniformLinker final : angle::NonCopyable
{
  public:
    UniformLinker(const ProgramState &state);
    ~UniformLinker();

    bool link(const Caps &caps, InfoLog &infoLog, const ProgramBindings &uniformLocationBindings);

    void getResults(std::vector<LinkedUniform> *uniforms,
                    std::vector<UnusedUniform> *unusedUniforms,
                    std::vector<VariableLocation> *uniformLocations);

  private:
    struct ShaderUniformCount
    {
        ShaderUniformCount() : vectorCount(0), samplerCount(0), imageCount(0), atomicCounterCount(0)
        {}
        ShaderUniformCount(const ShaderUniformCount &other) = default;
        ShaderUniformCount &operator=(const ShaderUniformCount &other) = default;

        ShaderUniformCount &operator+=(const ShaderUniformCount &other)
        {
            vectorCount += other.vectorCount;
            samplerCount += other.samplerCount;
            imageCount += other.imageCount;
            atomicCounterCount += other.atomicCounterCount;
            return *this;
        }

        unsigned int vectorCount;
        unsigned int samplerCount;
        unsigned int imageCount;
        unsigned int atomicCounterCount;
    };

    bool validateGraphicsUniforms(InfoLog &infoLog) const;

    bool flattenUniformsAndCheckCapsForShader(Shader *shader,
                                              const Caps &caps,
                                              std::vector<LinkedUniform> &samplerUniforms,
                                              std::vector<LinkedUniform> &imageUniforms,
                                              std::vector<LinkedUniform> &atomicCounterUniforms,
                                              std::vector<UnusedUniform> &unusedUniforms,
                                              InfoLog &infoLog);

    bool flattenUniformsAndCheckCaps(const Caps &caps, InfoLog &infoLog);
    bool checkMaxCombinedAtomicCounters(const Caps &caps, InfoLog &infoLog);

    ShaderUniformCount flattenUniform(const sh::Uniform &uniform,
                                      std::vector<LinkedUniform> *samplerUniforms,
                                      std::vector<LinkedUniform> *imageUniforms,
                                      std::vector<LinkedUniform> *atomicCounterUniforms,
                                      std::vector<UnusedUniform> *unusedUniforms,
                                      ShaderType shaderType);

    ShaderUniformCount flattenArrayOfStructsUniform(
        const sh::ShaderVariable &uniform,
        unsigned int arrayNestingIndex,
        const std::string &namePrefix,
        const std::string &mappedNamePrefix,
        std::vector<LinkedUniform> *samplerUniforms,
        std::vector<LinkedUniform> *imageUniforms,
        std::vector<LinkedUniform> *atomicCounterUniforms,
        std::vector<UnusedUniform> *unusedUniforms,
        ShaderType shaderType,
        bool markActive,
        bool markStaticUse,
        int binding,
        int offset,
        int *location);

    ShaderUniformCount flattenStructUniform(const std::vector<sh::ShaderVariable> &fields,
                                            const std::string &namePrefix,
                                            const std::string &mappedNamePrefix,
                                            std::vector<LinkedUniform> *samplerUniforms,
                                            std::vector<LinkedUniform> *imageUniforms,
                                            std::vector<LinkedUniform> *atomicCounterUniforms,
                                            std::vector<UnusedUniform> *unusedUniforms,
                                            ShaderType shaderType,
                                            bool markActive,
                                            bool markStaticUse,
                                            int binding,
                                            int offset,
                                            int *location);

    ShaderUniformCount flattenArrayUniform(const sh::ShaderVariable &uniform,
                                           const std::string &fullName,
                                           const std::string &fullMappedName,
                                           std::vector<LinkedUniform> *samplerUniforms,
                                           std::vector<LinkedUniform> *imageUniforms,
                                           std::vector<LinkedUniform> *atomicCounterUniforms,
                                           std::vector<UnusedUniform> *unusedUniforms,
                                           ShaderType shaderType,
                                           bool markActive,
                                           bool markStaticUse,
                                           int binding,
                                           int offset,
                                           int *location);

    // markActive and markStaticUse are given as separate parameters because they are tracked here
    // at struct granularity.
    ShaderUniformCount flattenUniformImpl(const sh::ShaderVariable &uniform,
                                          const std::string &fullName,
                                          const std::string &fullMappedName,
                                          std::vector<LinkedUniform> *samplerUniforms,
                                          std::vector<LinkedUniform> *imageUniforms,
                                          std::vector<LinkedUniform> *atomicCounterUniforms,
                                          std::vector<UnusedUniform> *unusedUniforms,
                                          ShaderType shaderType,
                                          bool markActive,
                                          bool markStaticUse,
                                          int binding,
                                          int offset,
                                          int *location);

    bool indexUniforms(InfoLog &infoLog, const ProgramBindings &uniformLocationBindings);
    bool gatherUniformLocationsAndCheckConflicts(InfoLog &infoLog,
                                                 const ProgramBindings &uniformLocationBindings,
                                                 std::set<GLuint> *ignoredLocations,
                                                 int *maxUniformLocation);
    void pruneUnusedUniforms();

    const ProgramState &mState;
    std::vector<LinkedUniform> mUniforms;
    std::vector<UnusedUniform> mUnusedUniforms;
    std::vector<VariableLocation> mUniformLocations;
};

// This class is intended to be used during the link step to store interface block information.
// It is called by the Impl class during ProgramImpl::link so that it has access to the
// real block size and layout.
class InterfaceBlockLinker : angle::NonCopyable
{
  public:
    virtual ~InterfaceBlockLinker();

    using GetBlockSize = std::function<
        bool(const std::string &blockName, const std::string &blockMappedName, size_t *sizeOut)>;
    using GetBlockMemberInfo = std::function<
        bool(const std::string &name, const std::string &mappedName, sh::BlockMemberInfo *infoOut)>;

    // This is called once per shader stage. It stores a pointer to the block vector, so it's
    // important that this class does not persist longer than the duration of Program::link.
    void addShaderBlocks(ShaderType shader, const std::vector<sh::InterfaceBlock> *blocks);

    // This is called once during a link operation, after all shader blocks are added.
    void linkBlocks(const GetBlockSize &getBlockSize,
                    const GetBlockMemberInfo &getMemberInfo) const;

  protected:
    InterfaceBlockLinker(std::vector<InterfaceBlock> *blocksOut);
    void defineInterfaceBlock(const GetBlockSize &getBlockSize,
                              const GetBlockMemberInfo &getMemberInfo,
                              const sh::InterfaceBlock &interfaceBlock,
                              ShaderType shaderType) const;

    template <typename VarT>
    void defineBlockMembers(const GetBlockMemberInfo &getMemberInfo,
                            const std::vector<VarT> &fields,
                            const std::string &prefix,
                            const std::string &mappedPrefix,
                            int blockIndex,
                            bool singleEntryForTopLevelArray,
                            int topLevelArraySize,
                            ShaderType shaderType) const;
    template <typename VarT>
    void defineBlockMember(const GetBlockMemberInfo &getMemberInfo,
                           const VarT &field,
                           const std::string &fullName,
                           const std::string &fullMappedName,
                           int blockIndex,
                           bool singleEntryForTopLevelArray,
                           int topLevelArraySize,
                           ShaderType shaderType) const;

    virtual void defineBlockMemberImpl(const sh::ShaderVariable &field,
                                       const std::string &fullName,
                                       const std::string &fullMappedName,
                                       int blockIndex,
                                       const sh::BlockMemberInfo &memberInfo,
                                       int topLevelArraySize,
                                       ShaderType shaderType) const = 0;
    virtual size_t getCurrentBlockMemberIndex() const               = 0;
    virtual void updateBlockMemberActiveImpl(const std::string &fullName,
                                             ShaderType shaderType,
                                             bool active) const     = 0;

    ShaderMap<const std::vector<sh::InterfaceBlock> *> mShaderBlocks;

    std::vector<InterfaceBlock> *mBlocksOut;

  private:
    template <typename VarT>
    void defineArrayOfStructsBlockMembers(const GetBlockMemberInfo &getMemberInfo,
                                          const VarT &field,
                                          unsigned int arrayNestingIndex,
                                          const std::string &prefix,
                                          const std::string &mappedPrefix,
                                          int blockIndex,
                                          bool singleEntryForTopLevelArray,
                                          int topLevelArraySize,
                                          ShaderType shaderType) const;
};

class UniformBlockLinker final : public InterfaceBlockLinker
{
  public:
    UniformBlockLinker(std::vector<InterfaceBlock> *blocksOut,
                       std::vector<LinkedUniform> *uniformsOut);
    ~UniformBlockLinker() override;

  private:
    void defineBlockMemberImpl(const sh::ShaderVariable &field,
                               const std::string &fullName,
                               const std::string &fullMappedName,
                               int blockIndex,
                               const sh::BlockMemberInfo &memberInfo,
                               int topLevelArraySize,
                               ShaderType shaderType) const override;
    size_t getCurrentBlockMemberIndex() const override;
    void updateBlockMemberActiveImpl(const std::string &fullName,
                                     ShaderType shaderType,
                                     bool active) const override;
    std::vector<LinkedUniform> *mUniformsOut;
};

class ShaderStorageBlockLinker final : public InterfaceBlockLinker
{
  public:
    ShaderStorageBlockLinker(std::vector<InterfaceBlock> *blocksOut,
                             std::vector<BufferVariable> *bufferVariablesOut);
    ~ShaderStorageBlockLinker() override;

  private:
    void defineBlockMemberImpl(const sh::ShaderVariable &field,
                               const std::string &fullName,
                               const std::string &fullMappedName,
                               int blockIndex,
                               const sh::BlockMemberInfo &memberInfo,
                               int topLevelArraySize,
                               ShaderType shaderType) const override;
    size_t getCurrentBlockMemberIndex() const override;
    void updateBlockMemberActiveImpl(const std::string &fullName,
                                     ShaderType shaderType,
                                     bool active) const override;
    std::vector<BufferVariable> *mBufferVariablesOut;
};

class AtomicCounterBufferLinker final : angle::NonCopyable
{
  public:
    AtomicCounterBufferLinker(std::vector<AtomicCounterBuffer> *atomicCounterBuffersOut);
    ~AtomicCounterBufferLinker();

    void link(const std::map<int, unsigned int> &sizeMap) const;

  private:
    std::vector<AtomicCounterBuffer> *mAtomicCounterBuffersOut;
};

// The link operation is responsible for finishing the link of uniform and interface blocks.
// This way it can filter out unreferenced resources and still have access to the info.
// TODO(jmadill): Integrate uniform linking/filtering as well as interface blocks.
struct UnusedUniform
{
    UnusedUniform(std::string name, bool isSampler)
    {
        this->name      = name;
        this->isSampler = isSampler;
    }

    std::string name;
    bool isSampler;
};

struct ProgramLinkedResources
{
    VaryingPacking varyingPacking;
    UniformBlockLinker uniformBlockLinker;
    ShaderStorageBlockLinker shaderStorageBlockLinker;
    AtomicCounterBufferLinker atomicCounterBufferLinker;
    std::vector<UnusedUniform> unusedUniforms;
};

}  // namespace gl

#endif  // LIBANGLE_UNIFORMLINKER_H_
