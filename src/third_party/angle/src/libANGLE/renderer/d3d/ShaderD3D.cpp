//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.cpp: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#include "libANGLE/renderer/d3d/ShaderD3D.h"

#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Shader.h"
#include "libANGLE/features.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace rx
{

ShaderD3D::ShaderD3D(const gl::ShaderState &data,
                     const angle::WorkaroundsD3D &workarounds,
                     const gl::Extensions &extensions)
    : ShaderImpl(data), mAdditionalOptions(0)
{
    uncompile();

    if (workarounds.expandIntegerPowExpressions)
    {
        mAdditionalOptions |= SH_EXPAND_SELECT_HLSL_INTEGER_POW_EXPRESSIONS;
    }

    if (workarounds.getDimensionsIgnoresBaseLevel)
    {
        mAdditionalOptions |= SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL;
    }

    if (workarounds.preAddTexelFetchOffsets)
    {
        mAdditionalOptions |= SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH;
    }
    if (workarounds.rewriteUnaryMinusOperator)
    {
        mAdditionalOptions |= SH_REWRITE_INTEGER_UNARY_MINUS_OPERATOR;
    }
    if (workarounds.emulateIsnanFloat)
    {
        mAdditionalOptions |= SH_EMULATE_ISNAN_FLOAT_FUNCTION;
    }
    if (workarounds.skipVSConstantRegisterZero && mData.getShaderType() == gl::ShaderType::Vertex)
    {
        mAdditionalOptions |= SH_SKIP_D3D_CONSTANT_REGISTER_ZERO;
    }
    if (extensions.multiview)
    {
        mAdditionalOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
    }
}

ShaderD3D::~ShaderD3D()
{
}

std::string ShaderD3D::getDebugInfo() const
{
    if (mDebugInfo.empty())
    {
        return "";
    }

    return mDebugInfo + std::string("\n// ") + gl::GetShaderTypeString(mData.getShaderType()) +
           " SHADER END\n";
}

// initialize/clean up previous state
void ShaderD3D::uncompile()
{
    // set by compileToHLSL
    mCompilerOutputType = SH_ESSL_OUTPUT;

    mUsesMultipleRenderTargets = false;
    mUsesFragColor = false;
    mUsesFragData = false;
    mUsesFragCoord = false;
    mUsesFrontFacing = false;
    mUsesPointSize = false;
    mUsesPointCoord = false;
    mUsesDepthRange = false;
    mUsesFragDepth = false;
    mHasANGLEMultiviewEnabled    = false;
    mUsesViewID                  = false;
    mUsesDiscardRewriting = false;
    mUsesNestedBreak = false;
    mRequiresIEEEStrictCompiling = false;

    mDebugInfo.clear();
}

void ShaderD3D::generateWorkarounds(angle::CompilerWorkaroundsD3D *workarounds) const
{
    if (mUsesDiscardRewriting)
    {
        // ANGLE issue 486:
        // Work-around a D3D9 compiler bug that presents itself when using conditional discard, by disabling optimization
        workarounds->skipOptimization = true;
    }
    else if (mUsesNestedBreak)
    {
        // ANGLE issue 603:
        // Work-around a D3D9 compiler bug that presents itself when using break in a nested loop, by maximizing optimization
        // We want to keep the use of ANGLE_D3D_WORKAROUND_MAX_OPTIMIZATION minimal to prevent hangs, so usesDiscard takes precedence
        workarounds->useMaxOptimization = true;
    }

    if (mRequiresIEEEStrictCompiling)
    {
        // IEEE Strictness for D3D compiler needs to be enabled for NaNs to work.
        workarounds->enableIEEEStrictness = true;
    }
}

unsigned int ShaderD3D::getUniformRegister(const std::string &uniformName) const
{
    ASSERT(mUniformRegisterMap.count(uniformName) > 0);
    return mUniformRegisterMap.find(uniformName)->second;
}

unsigned int ShaderD3D::getUniformBlockRegister(const std::string &blockName) const
{
    ASSERT(mUniformBlockRegisterMap.count(blockName) > 0);
    return mUniformBlockRegisterMap.find(blockName)->second;
}

unsigned int ShaderD3D::getShaderStorageBlockRegister(const std::string &blockName) const
{
    ASSERT(mShaderStorageBlockRegisterMap.count(blockName) > 0);
    return mShaderStorageBlockRegisterMap.find(blockName)->second;
}

ShShaderOutput ShaderD3D::getCompilerOutputType() const
{
    return mCompilerOutputType;
}

ShCompileOptions ShaderD3D::prepareSourceAndReturnOptions(const gl::Context *context,
                                                          std::stringstream *shaderSourceStream,
                                                          std::string *sourcePath)
{
    uncompile();

    ShCompileOptions additionalOptions = 0;

    const std::string &source = mData.getSource();

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
    if (gl::DebugAnnotationsActive())
    {
        *sourcePath = getTempPath();
        writeFile(sourcePath->c_str(), source.c_str(), source.length());
        additionalOptions |= SH_LINE_DIRECTIVES | SH_SOURCE_PATH;
    }
#endif

    additionalOptions |= mAdditionalOptions;

    *shaderSourceStream << source;
    return additionalOptions;
}

bool ShaderD3D::hasUniform(const std::string &name) const
{
    return mUniformRegisterMap.find(name) != mUniformRegisterMap.end();
}

const std::map<std::string, unsigned int> &GetUniformRegisterMap(
    const std::map<std::string, unsigned int> *uniformRegisterMap)
{
    ASSERT(uniformRegisterMap);
    return *uniformRegisterMap;
}

bool ShaderD3D::postTranslateCompile(gl::ShCompilerInstance *compiler, std::string *infoLog)
{
    // TODO(jmadill): We shouldn't need to cache this.
    mCompilerOutputType = compiler->getShaderOutputType();

    const std::string &translatedSource = mData.getTranslatedSource();

    mUsesMultipleRenderTargets = translatedSource.find("GL_USES_MRT") != std::string::npos;
    mUsesFragColor             = translatedSource.find("GL_USES_FRAG_COLOR") != std::string::npos;
    mUsesFragData              = translatedSource.find("GL_USES_FRAG_DATA") != std::string::npos;
    mUsesFragCoord             = translatedSource.find("GL_USES_FRAG_COORD") != std::string::npos;
    mUsesFrontFacing           = translatedSource.find("GL_USES_FRONT_FACING") != std::string::npos;
    mUsesPointSize             = translatedSource.find("GL_USES_POINT_SIZE") != std::string::npos;
    mUsesPointCoord            = translatedSource.find("GL_USES_POINT_COORD") != std::string::npos;
    mUsesDepthRange            = translatedSource.find("GL_USES_DEPTH_RANGE") != std::string::npos;
    mUsesFragDepth             = translatedSource.find("GL_USES_FRAG_DEPTH") != std::string::npos;
    mHasANGLEMultiviewEnabled =
        translatedSource.find("GL_ANGLE_MULTIVIEW_ENABLED") != std::string::npos;
    mUsesViewID = translatedSource.find("GL_USES_VIEW_ID") != std::string::npos;
    mUsesDiscardRewriting =
        translatedSource.find("ANGLE_USES_DISCARD_REWRITING") != std::string::npos;
    mUsesNestedBreak  = translatedSource.find("ANGLE_USES_NESTED_BREAK") != std::string::npos;
    mRequiresIEEEStrictCompiling =
        translatedSource.find("ANGLE_REQUIRES_IEEE_STRICT_COMPILING") != std::string::npos;

    ShHandle compilerHandle = compiler->getHandle();

    mUniformRegisterMap = GetUniformRegisterMap(sh::GetUniformRegisterMap(compilerHandle));

    for (const sh::InterfaceBlock &interfaceBlock : mData.getUniformBlocks())
    {
        if (interfaceBlock.active)
        {
            unsigned int index = static_cast<unsigned int>(-1);
            bool blockRegisterResult =
                sh::GetUniformBlockRegister(compilerHandle, interfaceBlock.name, &index);
            ASSERT(blockRegisterResult);

            mUniformBlockRegisterMap[interfaceBlock.name] = index;
        }
    }

    for (const sh::InterfaceBlock &interfaceBlock : mData.getShaderStorageBlocks())
    {
        if (interfaceBlock.active)
        {
            unsigned int index = static_cast<unsigned int>(-1);
            bool blockRegisterResult =
                sh::GetShaderStorageBlockRegister(compilerHandle, interfaceBlock.name, &index);
            ASSERT(blockRegisterResult);

            mShaderStorageBlockRegisterMap[interfaceBlock.name] = index;
        }
    }

    mDebugInfo +=
        std::string("// ") + gl::GetShaderTypeString(mData.getShaderType()) + " SHADER BEGIN\n";
    mDebugInfo += "\n// GLSL BEGIN\n\n" + mData.getSource() + "\n\n// GLSL END\n\n\n";
    mDebugInfo += "// INITIAL HLSL BEGIN\n\n" + translatedSource + "\n// INITIAL HLSL END\n\n\n";
    // Successive steps will append more info
    return true;
}

}  // namespace rx
