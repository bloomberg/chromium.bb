//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderGL.cpp: Implements the class methods for ShaderGL.

#include "libANGLE/renderer/gl/ShaderGL.h"

#include "common/debug.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"

#include <iostream>

namespace rx
{

ShaderGL::ShaderGL(const gl::ShaderState &data,
                   GLuint shaderID,
                   MultiviewImplementationTypeGL multiviewImplementationType,
                   const FunctionsGL *functions)
    : ShaderImpl(data),
      mShaderID(shaderID),
      mMultiviewImplementationType(multiviewImplementationType),
      mFunctions(functions)
{
}

ShaderGL::~ShaderGL()
{
    ASSERT(mShaderID == 0);
}

void ShaderGL::destroy()
{
    mFunctions->deleteShader(mShaderID);
    mShaderID = 0;
}

ShCompileOptions ShaderGL::prepareSourceAndReturnOptions(const gl::Context *context,
                                                         std::stringstream *sourceStream,
                                                         std::string * /*sourcePath*/)
{
    *sourceStream << mData.getSource();

    ShCompileOptions options = SH_INIT_GL_POSITION;

    bool isWebGL = context->getExtensions().webglCompatibility;
    if (isWebGL && (mData.getShaderType() != gl::ShaderType::Compute))
    {
        options |= SH_INIT_OUTPUT_VARIABLES;
    }

    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    if (workarounds.doWhileGLSLCausesGPUHang)
    {
        options |= SH_REWRITE_DO_WHILE_LOOPS;
    }

    if (workarounds.emulateAbsIntFunction)
    {
        options |= SH_EMULATE_ABS_INT_FUNCTION;
    }

    if (workarounds.addAndTrueToLoopCondition)
    {
        options |= SH_ADD_AND_TRUE_TO_LOOP_CONDITION;
    }

    if (workarounds.emulateIsnanFloat)
    {
        options |= SH_EMULATE_ISNAN_FLOAT_FUNCTION;
    }

    if (workarounds.emulateAtan2Float)
    {
        options |= SH_EMULATE_ATAN2_FLOAT_FUNCTION;
    }

    if (workarounds.useUnusedBlocksWithStandardOrSharedLayout)
    {
        options |= SH_USE_UNUSED_STANDARD_SHARED_BLOCKS;
    }

    if (workarounds.dontRemoveInvariantForFragmentInput)
    {
        options |= SH_DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT;
    }

    if (workarounds.removeInvariantAndCentroidForESSL3)
    {
        options |= SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3;
    }

    if (workarounds.rewriteFloatUnaryMinusOperator)
    {
        options |= SH_REWRITE_FLOAT_UNARY_MINUS_OPERATOR;
    }

    if (!workarounds.dontInitializeUninitializedLocals)
    {
        options |= SH_INITIALIZE_UNINITIALIZED_LOCALS;
    }

    if (workarounds.clampPointSize)
    {
        options |= SH_CLAMP_POINT_SIZE;
    }

    if (workarounds.rewriteVectorScalarArithmetic)
    {
        options |= SH_REWRITE_VECTOR_SCALAR_ARITHMETIC;
    }

    if (workarounds.dontUseLoopsToInitializeVariables)
    {
        options |= SH_DONT_USE_LOOPS_TO_INITIALIZE_VARIABLES;
    }

    if (workarounds.clampFragDepth)
    {
        options |= SH_CLAMP_FRAG_DEPTH;
    }

    if (workarounds.rewriteRepeatedAssignToSwizzled)
    {
        options |= SH_REWRITE_REPEATED_ASSIGN_TO_SWIZZLED;
    }

    if (mMultiviewImplementationType == MultiviewImplementationTypeGL::NV_VIEWPORT_ARRAY2)
    {
        options |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
        options |= SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER;
    }

    return options;
}

bool ShaderGL::postTranslateCompile(gl::ShCompilerInstance *compiler, std::string *infoLog)
{
    // Translate the ESSL into GLSL
    const char *translatedSourceCString = mData.getTranslatedSource().c_str();

    // Set the source
    mFunctions->shaderSource(mShaderID, 1, &translatedSourceCString, nullptr);
    mFunctions->compileShader(mShaderID);

    // Check for compile errors from the native driver
    GLint compileStatus = GL_FALSE;
    mFunctions->getShaderiv(mShaderID, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_FALSE)
    {
        // Compilation failed, put the error into the info log
        GLint infoLogLength = 0;
        mFunctions->getShaderiv(mShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

        // Info log length includes the null terminator, so 1 means that the info log is an empty
        // string.
        if (infoLogLength > 1)
        {
            std::vector<char> buf(infoLogLength);
            mFunctions->getShaderInfoLog(mShaderID, infoLogLength, nullptr, &buf[0]);

            *infoLog = &buf[0];
            WARN() << std::endl << *infoLog;
        }
        else
        {
            WARN() << std::endl << "Shader compilation failed with no info log.";
        }
        return false;
    }

    return true;
}

std::string ShaderGL::getDebugInfo() const
{
    return mData.getTranslatedSource();
}

GLuint ShaderGL::getShaderID() const
{
    return mShaderID;
}

}
