//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DispatchComputePerf:
//   Performance tests for ANGLE DispatchCompute call overhead.
//

#include "ANGLEPerfTest.h"
#include "shader_utils.h"

namespace
{
unsigned int kIterationsPerStep = 50;

struct DispatchComputePerfParams final : public RenderTestParams
{
    DispatchComputePerfParams()
    {
        iterationsPerStep = kIterationsPerStep;
        majorVersion      = 3;
        minorVersion      = 1;
    }

    std::string suffix() const override;

    unsigned int localSizeX    = 16;
    unsigned int localSizeY    = 16;
    unsigned int textureWidth  = 32;
    unsigned int textureHeight = 32;
};

std::string DispatchComputePerfParams::suffix() const
{
    std::stringstream suffixStr;
    suffixStr << RenderTestParams::suffix();

    if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
    {
        suffixStr << "_null";
    }
    return suffixStr.str();
}

std::ostream &operator<<(std::ostream &os, const DispatchComputePerfParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

class DispatchComputePerfBenchmark : public ANGLERenderTest,
                                     public ::testing::WithParamInterface<DispatchComputePerfParams>
{
  public:
    DispatchComputePerfBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    void initComputeShader();
    void initTextures();

    GLuint mProgram      = 0;
    GLuint mReadTexture  = 0;
    GLuint mWriteTexture = 0;
    GLuint mDispatchX    = 0;
    GLuint mDispatchY    = 0;
};

DispatchComputePerfBenchmark::DispatchComputePerfBenchmark()
    : ANGLERenderTest("DispatchComputePerf", GetParam())
{}

void DispatchComputePerfBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    initComputeShader();
    initTextures();

    glUseProgram(mProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mReadTexture);
    glUniform1i(glGetUniformLocation(mProgram, "readTexture"), 0);
    glBindImageTexture(4, mWriteTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    mDispatchX = params.textureWidth / params.localSizeX;
    mDispatchY = params.textureHeight / params.localSizeY;
    ASSERT_GL_NO_ERROR();
}

void DispatchComputePerfBenchmark::initComputeShader()
{
    const std::string &csSource =
        R"(#version 310 es
        #define LOCAL_SIZE_X 16
        #define LOCAL_SIZE_Y 16
        layout(local_size_x=LOCAL_SIZE_X, local_size_y=LOCAL_SIZE_Y) in;
        precision highp float;
        uniform sampler2D readTexture;
        layout(r32f, binding = 4) writeonly uniform highp image2D  outImage;

        void main() {
            float sum = 0.;
            sum += texelFetch(readTexture, ivec2(gl_GlobalInvocationID.xy), 0).r;
            imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(sum));
        })";

    mProgram = CompileComputeProgram(csSource, false);
    ASSERT_NE(0u, mProgram);
}

void DispatchComputePerfBenchmark::initTextures()
{
    const auto &params = GetParam();

    unsigned int textureDataSize = params.textureWidth * params.textureHeight;
    std::vector<GLfloat> textureInputData(textureDataSize, 0.2f);
    std::vector<GLfloat> textureOutputData(textureDataSize, 0.1f);

    glGenTextures(1, &mReadTexture);
    glBindTexture(GL_TEXTURE_2D, mReadTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, params.textureWidth, params.textureHeight, 0, GL_RED,
                 GL_FLOAT, textureInputData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &mWriteTexture);
    glBindTexture(GL_TEXTURE_2D, mWriteTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, params.textureWidth, params.textureHeight);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, params.textureWidth, params.textureHeight, GL_RED,
                    GL_FLOAT, textureOutputData.data());
    ASSERT_GL_NO_ERROR();
}

void DispatchComputePerfBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
    glDeleteTextures(1, &mReadTexture);
    glDeleteTextures(1, &mWriteTexture);
}

void DispatchComputePerfBenchmark::drawBenchmark()
{
    const auto &params = GetParam();
    for (unsigned int it = 0; it < params.iterationsPerStep; it++)
    {
        glDispatchCompute(mDispatchX, mDispatchY, 1);
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    }
    ASSERT_GL_NO_ERROR();
}

DispatchComputePerfParams DispatchComputePerfOpenGLOrGLESParams(bool useNullDevice)
{
    DispatchComputePerfParams params;
    params.eglParameters = angle::egl_platform::OPENGL_OR_GLES(useNullDevice);
    return params;
}

TEST_P(DispatchComputePerfBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(DispatchComputePerfBenchmark,
                       DispatchComputePerfOpenGLOrGLESParams(true),
                       DispatchComputePerfOpenGLOrGLESParams(false));

}  // namespace
