// Copyright 2017 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// OpenGL ES unit tests that provide coverage for functionality not tested by
// the dEQP test suite. Also used as a smoke test.

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GL/glcorearb.h>
#include <GL/glext.h>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <string.h>
#include <cstdint>

#define EXPECT_GLENUM_EQ(expected, actual) EXPECT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))

class SwiftShaderTest : public testing::Test
{
protected:
	void SetUp() override
	{
		#if defined(_WIN32) && !defined(STANDALONE)
			// The DLLs are delay loaded (see BUILD.gn), so we can load
			// the correct ones from Chrome's swiftshader subdirectory.
			HMODULE libEGL = LoadLibraryA("swiftshader\\libEGL.dll");
			EXPECT_NE((HMODULE)NULL, libEGL);

			HMODULE libGLESv2 = LoadLibraryA("swiftshader\\libGLESv2.dll");
			EXPECT_NE((HMODULE)NULL, libGLESv2);
		#endif
	}

	void expectFramebufferColor(const unsigned char referenceColor[4], GLint x = 0, GLint y = 0)
	{
		unsigned char color[4] = { 0 };
		glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
		EXPECT_EQ(color[0], referenceColor[0]);
		EXPECT_EQ(color[1], referenceColor[1]);
		EXPECT_EQ(color[2], referenceColor[2]);
		EXPECT_EQ(color[3], referenceColor[3]);
	}

	void expectFramebufferColor(const float referenceColor[4], GLint x = 0, GLint y = 0)
	{
		float color[4] = { 0 };
		glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &color);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
		EXPECT_EQ(color[0], referenceColor[0]);
		EXPECT_EQ(color[1], referenceColor[1]);
		EXPECT_EQ(color[2], referenceColor[2]);
		EXPECT_EQ(color[3], referenceColor[3]);
	}

	void Initialize(int version, bool withChecks)
	{
		EXPECT_EQ(EGL_SUCCESS, eglGetError());

		display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

		if(withChecks)
		{
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_NE(EGL_NO_DISPLAY, display);

			eglQueryString(display, EGL_VENDOR);
			EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
		}

		EGLint major;
		EGLint minor;
		EGLBoolean initialized = eglInitialize(display, &major, &minor);

		if(withChecks)
		{
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_EQ((EGLBoolean)EGL_TRUE, initialized);
			EXPECT_EQ(1, major);
			EXPECT_EQ(4, minor);

			const char *eglVendor = eglQueryString(display, EGL_VENDOR);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_STREQ("Google Inc.", eglVendor);

			const char *eglVersion = eglQueryString(display, EGL_VERSION);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_THAT(eglVersion, testing::HasSubstr("1.4 SwiftShader "));
		}

		eglBindAPI(EGL_OPENGL_ES_API);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());

		const EGLint configAttributes[] =
		{
			EGL_SURFACE_TYPE,		EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
			EGL_ALPHA_SIZE,			8,
			EGL_NONE
		};

		EGLint num_config = -1;
		EGLBoolean success = eglChooseConfig(display, configAttributes, &config, 1, &num_config);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ(num_config, 1);
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);

		if(withChecks)
		{
			EGLint conformant = 0;
			eglGetConfigAttrib(display, config, EGL_CONFORMANT, &conformant);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_TRUE(conformant & EGL_OPENGL_ES2_BIT);

			EGLint renderableType = 0;
			eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &renderableType);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_TRUE(renderableType & EGL_OPENGL_ES2_BIT);

			EGLint surfaceType = 0;
			eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &surfaceType);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_TRUE(surfaceType & EGL_WINDOW_BIT);
		}

		EGLint surfaceAttributes[] =
		{
			EGL_WIDTH, 1920,
			EGL_HEIGHT, 1080,
			EGL_NONE
		};

		surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_NE(EGL_NO_SURFACE, surface);

		EGLint contextAttributes[] =
		{
			EGL_CONTEXT_CLIENT_VERSION, version,
			EGL_NONE
		};

		context = eglCreateContext(display, config, NULL, contextAttributes);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_NE(EGL_NO_CONTEXT, context);

		success = eglMakeCurrent(display, surface, surface, context);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);

		if(withChecks)
		{
			EGLDisplay currentDisplay = eglGetCurrentDisplay();
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_EQ(display, currentDisplay);

			EGLSurface currentDrawSurface = eglGetCurrentSurface(EGL_DRAW);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_EQ(surface, currentDrawSurface);

			EGLSurface currentReadSurface = eglGetCurrentSurface(EGL_READ);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_EQ(surface, currentReadSurface);

			EGLContext currentContext = eglGetCurrentContext();
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
			EXPECT_EQ(context, currentContext);
		}

		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
	}

	void Uninitialize()
	{
		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());

		EGLBoolean success = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);

		EGLDisplay currentDisplay = eglGetCurrentDisplay();
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ(EGL_NO_DISPLAY, currentDisplay);

		EGLSurface currentDrawSurface = eglGetCurrentSurface(EGL_DRAW);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ(EGL_NO_SURFACE, currentDrawSurface);

		EGLSurface currentReadSurface = eglGetCurrentSurface(EGL_READ);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ(EGL_NO_SURFACE, currentReadSurface);

		EGLContext currentContext = eglGetCurrentContext();
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ(EGL_NO_CONTEXT, currentContext);

		success = eglDestroyContext(display, context);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);

		success = eglDestroySurface(display, surface);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);

		success = eglTerminate(display);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
		EXPECT_EQ((EGLBoolean)EGL_TRUE, success);
	}

	struct ProgramHandles
	{
		GLuint program;
		GLuint vertexShader;
		GLuint fragmentShader;
	};

	ProgramHandles createProgram(const std::string& vs, const std::string& fs)
	{
		ProgramHandles ph;
		ph.program = glCreateProgram();
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		ph.vertexShader = glCreateShader(GL_VERTEX_SHADER);
		const char* vsSource[1] = { vs.c_str() };
		glShaderSource(ph.vertexShader, 1, vsSource, nullptr);
		glCompileShader(ph.vertexShader);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
		GLint vsCompileStatus = 0;
		glGetShaderiv(ph.vertexShader, GL_COMPILE_STATUS, &vsCompileStatus);
		EXPECT_EQ(vsCompileStatus, GL_TRUE);

		ph.fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		const char* fsSource[1] = { fs.c_str() };
		glShaderSource(ph.fragmentShader, 1, fsSource, nullptr);
		glCompileShader(ph.fragmentShader);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
		GLint fsCompileStatus = 0;
		glGetShaderiv(ph.fragmentShader, GL_COMPILE_STATUS, &fsCompileStatus);
		EXPECT_EQ(fsCompileStatus, GL_TRUE);

		glAttachShader(ph.program, ph.vertexShader);
		glAttachShader(ph.program, ph.fragmentShader);
		glLinkProgram(ph.program);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		GLint linkStatus = 0;
		glGetProgramiv(ph.program, GL_LINK_STATUS, &linkStatus);
		EXPECT_NE(linkStatus, 0);

		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		return ph;
	}

	void deleteProgram(const ProgramHandles& ph)
	{
		glDeleteShader(ph.fragmentShader);
		glDeleteShader(ph.vertexShader);
		glDeleteProgram(ph.program);

		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	}

	void drawQuad(GLuint program, const char* textureName = nullptr)
	{
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

		glUseProgram(program);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		GLint posLoc = glGetAttribLocation(program, "position");
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		if(textureName)
		{
			GLint location = glGetUniformLocation(program, textureName);
			ASSERT_NE(-1, location);
			glUniform1i(location, 0);
		}

		float vertices[18] = { -1.0f,  1.0f, 0.5f,
		                       -1.0f, -1.0f, 0.5f,
		                        1.0f, -1.0f, 0.5f,
		                       -1.0f,  1.0f, 0.5f,
		                        1.0f, -1.0f, 0.5f,
		                        1.0f,  1.0f, 0.5f };

		glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0, vertices);
		glEnableVertexAttribArray(posLoc);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());

		glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
		glDisableVertexAttribArray(posLoc);
		glUseProgram(prevProgram);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	}

	EGLDisplay getDisplay() const { return display; }
	EGLConfig getConfig() const { return config; }
	EGLSurface getSurface() const { return surface; }
	EGLContext getContext() const { return context; }

private:
	EGLDisplay display;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;
};

TEST_F(SwiftShaderTest, Initalization)
{
	Initialize(2, true);

	const GLubyte *glVendor = glGetString(GL_VENDOR);
	EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
	EXPECT_STREQ("Google Inc.", (const char*)glVendor);

	const GLubyte *glRenderer = glGetString(GL_RENDERER);
	EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
	EXPECT_STREQ("Google SwiftShader", (const char*)glRenderer);

	// SwiftShader return an OpenGL ES 3.0 context when a 2.0 context is requested, as allowed by the spec.
	const GLubyte *glVersion = glGetString(GL_VERSION);
	EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
	EXPECT_THAT((const char*)glVersion, testing::HasSubstr("OpenGL ES 3.0 SwiftShader "));

	Uninitialize();
}

// Test unrolling of a loop
TEST_F(SwiftShaderTest, UnrollLoop)
{
	Initialize(3, false);

	unsigned char green[4] = { 0, 255, 0, 255 };

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"out vec4 color;\n"
		"void main()\n"
		"{\n"
		"   for(int i = 0; i < 4; i++)\n"
		"   {\n"
		"       color[i] = (i % 2 == 0) ? 0.0 : 1.0;\n"
		"   }\n"
		"	gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"	fragColor = color;\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	// Expect the info log to contain "unrolled". This is not a spec requirement.
	GLsizei length = 0;
	glGetShaderiv(ph.vertexShader, GL_INFO_LOG_LENGTH, &length);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_NE(length, 0);
	char *log = new char[length];
	GLsizei written = 0;
	glGetShaderInfoLog(ph.vertexShader, length, &written, log);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_EQ(length, written + 1);
	EXPECT_NE(strstr(log, "unrolled"), nullptr);
	delete[] log;

	glUseProgram(ph.program);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program);

	deleteProgram(ph);

	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test non-canonical or non-deterministic loops do not get unrolled
TEST_F(SwiftShaderTest, DynamicLoop)
{
	Initialize(3, false);

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"out vec4 color;\n"
		"void main()\n"
		"{\n"
		"   for(int i = 0; i < 4; )\n"
		"   {\n"
		"       color[i] = (i % 2 == 0) ? 0.0 : 1.0;\n"
		"       i++;"
		"   }\n"
		"	gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"   vec4 temp;"
		"   for(int i = 0; i < 4; i++)\n"
		"   {\n"
		"       if(color.x < 0.0) return;"
		"       temp[i] = color[i];\n"
		"   }\n"
		"	fragColor = vec4(temp[0], temp[1], temp[2], temp[3]);\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	// Expect the info logs to be empty. This is not a spec requirement.
	GLsizei length = 0;
	glGetShaderiv(ph.vertexShader, GL_INFO_LOG_LENGTH, &length);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_EQ(length, 0);
	glGetShaderiv(ph.fragmentShader, GL_INFO_LOG_LENGTH, &length);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_EQ(length, 0);

	glUseProgram(ph.program);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program);

	deleteProgram(ph);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test dynamic indexing
TEST_F(SwiftShaderTest, DynamicIndexing)
{
	Initialize(3, false);

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"out float color[4];\n"
		"void main()\n"
		"{\n"
		"   for(int i = 0; i < 4; )\n"
		"   {\n"
		"       int j = (gl_VertexID + i) % 4;\n"
		"       color[j] = (j % 2 == 0) ? 0.0 : 1.0;\n"
		"       i++;"
		"   }\n"
		"	gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in float color[4];\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"   float temp[4];"
		"   for(int i = 0; i < 4; )\n"
		"   {\n"
		"       temp[i] = color[i];\n"
		"       i++;"
		"   }\n"
		"	fragColor = vec4(temp[0], temp[1], temp[2], temp[3]);\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	glUseProgram(ph.program);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program);

	deleteProgram(ph);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test vertex attribute location linking
TEST_F(SwiftShaderTest, AttributeLocation)
{
	Initialize(3, false);

	const std::string vs =
		"#version 300 es\n"
		"layout(location = 0) in vec4 a0;\n"   // Explicitly bound in GLSL
		"layout(location = 2) in vec4 a2;\n"   // Explicitly bound in GLSL
		"in vec4 a5;\n"                        // Bound to location 5 by API
		"in mat2 a3;\n"                        // Implicit location
		"in vec4 a1;\n"                        // Implicit location
		"in vec4 a6;\n"                        // Implicit location
		"out vec4 color;\n"
		"void main()\n"
		"{\n"
		"   vec4 a34 = vec4(a3[0], a3[1]);\n"
		"	gl_Position = a0;\n"
		"   color = (a2 == vec4(1.0, 2.0, 3.0, 4.0) &&\n"
		"            a34 == vec4(5.0, 6.0, 7.0, 8.0) &&\n"
		"            a5 == vec4(9.0, 10.0, 11.0, 12.0) &&\n"
		"            a1 == vec4(13.0, 14.0, 15.0, 16.0) &&\n"
		"            a6 == vec4(17.0, 18.0, 19.0, 20.0)) ?\n"
		"           vec4(0.0, 1.0, 0.0, 1.0) :\n"
		"           vec4(1.0, 0.0, 0.0, 1.0);"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"	fragColor = color;\n"
		"}\n";

	ProgramHandles ph;
	ph.program = glCreateProgram();
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	ph.vertexShader = glCreateShader(GL_VERTEX_SHADER);
	const char* vsSource[1] = { vs.c_str() };
	glShaderSource(ph.vertexShader, 1, vsSource, nullptr);
	glCompileShader(ph.vertexShader);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	GLint vsCompileStatus = 0;
	glGetShaderiv(ph.vertexShader, GL_COMPILE_STATUS, &vsCompileStatus);
	EXPECT_EQ(vsCompileStatus, GL_TRUE);

	ph.fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const char* fsSource[1] = { fs.c_str() };
	glShaderSource(ph.fragmentShader, 1, fsSource, nullptr);
	glCompileShader(ph.fragmentShader);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	GLint fsCompileStatus = 0;
	glGetShaderiv(ph.fragmentShader, GL_COMPILE_STATUS, &fsCompileStatus);
	EXPECT_EQ(fsCompileStatus, GL_TRUE);

	// Not assigned a layout location in GLSL. Bind it explicitly with the API.
	glBindAttribLocation(ph.program, 5, "a5");
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Should not override GLSL layout location qualifier
	glBindAttribLocation(ph.program, 8, "a2");
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	glAttachShader(ph.program, ph.vertexShader);
	glAttachShader(ph.program, ph.fragmentShader);
	glLinkProgram(ph.program);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Changes after linking should have no effect
	glBindAttribLocation(ph.program, 0, "a1");
	glBindAttribLocation(ph.program, 6, "a2");
	glBindAttribLocation(ph.program, 2, "a6");

	GLint linkStatus = 0;
	glGetProgramiv(ph.program, GL_LINK_STATUS, &linkStatus);
	EXPECT_NE(linkStatus, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	float vertices[6][3] = { { -1.0f,  1.0f, 0.5f },
	                         { -1.0f, -1.0f, 0.5f },
	                         {  1.0f, -1.0f, 0.5f },
	                         { -1.0f,  1.0f, 0.5f },
	                         {  1.0f, -1.0f, 0.5f },
	                         {  1.0f,  1.0f, 0.5f } };

	float attributes[5][4] = { { 1.0f, 2.0f, 3.0f, 4.0f },
	                           { 5.0f, 6.0f, 7.0f, 8.0f },
	                           { 9.0f, 10.0f, 11.0f, 12.0f },
	                           { 13.0f, 14.0f, 15.0f, 16.0f },
	                           { 17.0f, 18.0f, 19.0f, 20.0f } };

	GLint a0 = glGetAttribLocation(ph.program, "a0");
	EXPECT_EQ(a0, 0);
	glVertexAttribPointer(a0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(a0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLint a2 = glGetAttribLocation(ph.program, "a2");
	EXPECT_EQ(a2, 2);
	glVertexAttribPointer(a2, 4, GL_FLOAT, GL_FALSE, 0, attributes[0]);
	glVertexAttribDivisor(a2, 1);
	glEnableVertexAttribArray(a2);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLint a3 = glGetAttribLocation(ph.program, "a3");
	EXPECT_EQ(a3, 3);   // Note: implementation specific
	glVertexAttribPointer(a3 + 0, 2, GL_FLOAT, GL_FALSE, 0, &attributes[1][0]);
	glVertexAttribPointer(a3 + 1, 2, GL_FLOAT, GL_FALSE, 0, &attributes[1][2]);
	glVertexAttribDivisor(a3 + 0, 1);
	glVertexAttribDivisor(a3 + 1, 1);
	glEnableVertexAttribArray(a3 + 0);
	glEnableVertexAttribArray(a3 + 1);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLint a5 = glGetAttribLocation(ph.program, "a5");
	EXPECT_EQ(a5, 5);
	glVertexAttribPointer(a5, 4, GL_FLOAT, GL_FALSE, 0, attributes[2]);
	glVertexAttribDivisor(a5, 1);
	glEnableVertexAttribArray(a5);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLint a1 = glGetAttribLocation(ph.program, "a1");
	EXPECT_EQ(a1, 1);   // Note: implementation specific
	glVertexAttribPointer(a1, 4, GL_FLOAT, GL_FALSE, 0, attributes[3]);
	glVertexAttribDivisor(a1, 1);
	glEnableVertexAttribArray(a1);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLint a6 = glGetAttribLocation(ph.program, "a6");
	EXPECT_EQ(a6, 6);   // Note: implementation specific
	glVertexAttribPointer(a6, 4, GL_FLOAT, GL_FALSE, 0, attributes[4]);
	glVertexAttribDivisor(a6, 1);
	glEnableVertexAttribArray(a6);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	glUseProgram(ph.program);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	deleteProgram(ph);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Tests clearing of a texture with 'dirty' content.
TEST_F(SwiftShaderTest, ClearDirtyTexture)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, 256, 256, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, nullptr);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

	float dirty_color[3] = { 128 / 255.0f, 64 / 255.0f, 192 / 255.0f };
	GLint dirty_x = 8;
	GLint dirty_y = 12;
	glTexSubImage2D(GL_TEXTURE_2D, 0, dirty_x, dirty_y, 1, 1, GL_RGB, GL_FLOAT, dirty_color);

	const float clear_color[4] = { 1.0f, 32.0f, 0.5f, 1.0f };
	glClearColor(clear_color[0], clear_color[1], clear_color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	expectFramebufferColor(clear_color, dirty_x, dirty_y);

	Uninitialize();
}

// Tests copying between textures of different floating-point formats using a framebuffer object.
TEST_F(SwiftShaderTest, CopyTexImage)
{
	Initialize(3, false);

	GLuint tex1 = 1;
	float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	glBindTexture(GL_TEXTURE_2D, tex1);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 16, 16);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 5, 10, 1, 1, GL_RGBA, GL_FLOAT, &green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint tex2 = 2;
	glBindTexture(GL_TEXTURE_2D, tex2);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 6, 8, 8, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);
	expectFramebufferColor(green, 3, 4);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Tests reading of half-float textures.
TEST_F(SwiftShaderTest, ReadHalfFloat)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 256, 256, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

	const float clear_color[4] = { 1.0f, 32.0f, 0.5f, 1.0f };
	glClearColor(clear_color[0], clear_color[1], clear_color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	uint16_t pixel[3] = { 0x1234, 0x3F80, 0xAAAA };
	GLint x = 6;
	GLint y = 3;
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RGB, GL_HALF_FLOAT, pixel);

	// This relies on GL_HALF_FLOAT being a valid type for read-back,
	// which isn't guaranteed by the spec but is supported by SwiftShader.
	uint16_t read_color[3] = { 0, 0, 0 };
	glReadPixels(x, y, 1, 1, GL_RGB, GL_HALF_FLOAT, &read_color);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	EXPECT_EQ(read_color[0], pixel[0]);
	EXPECT_EQ(read_color[1], pixel[1]);
	EXPECT_EQ(read_color[2], pixel[2]);

	Uninitialize();
}

// Tests construction of a structure containing a single matrix
TEST_F(SwiftShaderTest, MatrixInStruct)
{
	Initialize(2, false);

	const std::string fs =
		"#version 100\n"
		"precision mediump float;\n"
		"struct S\n"
		"{\n"
		"	mat2 rotation;\n"
		"};\n"
		"void main(void)\n"
		"{\n"
		"	float angle = 1.0;\n"
		"	S(mat2(1.0, angle, 1.0, 1.0));\n"
		"}\n";

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const char *fsSource[1] = { fs.c_str() };
	glShaderSource(fragmentShader, 1, fsSource, nullptr);
	glCompileShader(fragmentShader);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	GLint compileStatus = 0;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileStatus);
	EXPECT_NE(compileStatus, 0);

	Uninitialize();
}

// Test sampling from a sampler in a struct as a function argument
TEST_F(SwiftShaderTest, SamplerArrayInStructArrayAsFunctionArg)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_2D, tex);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	unsigned char green[4] = { 0, 255, 0, 255 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"struct SamplerStruct{ sampler2D tex[2]; };\n"
		"vec4 doSample(in SamplerStruct s[2])\n"
		"{\n"
		"	return texture(s[1].tex[1], vec2(0.0));\n"
		"}\n"
		"uniform SamplerStruct samplerStruct[2];\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"	fragColor = doSample(samplerStruct);\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	glUseProgram(ph.program);
	GLint location = glGetUniformLocation(ph.program, "samplerStruct[1].tex[1]");
	ASSERT_NE(-1, location);
	glUniform1i(location, 0);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program, "samplerStruct[1].tex[1]");

	deleteProgram(ph);

	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test sampling from a sampler in a struct as a function argument
TEST_F(SwiftShaderTest, AtanCornerCases)
{
	Initialize(3, false);

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"const float kPI = 3.14159265358979323846;"
		"uniform float positive_value;\n"
		"uniform float negative_value;\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"	// Should yield vec4(0, pi, pi/2, -pi/2)\n"
		"	vec4 result = atan(vec4(0.0, 0.0, positive_value, negative_value),\n"
		"	                   vec4(positive_value, negative_value, 0.0, 0.0));\n"
		"	fragColor = (result / vec4(kPI)) + vec4(0.5, -0.5, 0.0, 1.0) + vec4(0.5 / 255.0);\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	glUseProgram(ph.program);
	GLint positive_value = glGetUniformLocation(ph.program, "positive_value");
	ASSERT_NE(-1, positive_value);
	GLint negative_value = glGetUniformLocation(ph.program, "negative_value");
	ASSERT_NE(-1, negative_value);
	glUniform1f(positive_value,  1.0);
	glUniform1f(negative_value, -1.0);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program, nullptr);

	deleteProgram(ph);

	unsigned char grey[4] = { 128, 128, 128, 128 };
	expectFramebufferColor(grey);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test conditions that should result in a GL_OUT_OF_MEMORY and not crash
TEST_F(SwiftShaderTest, OutOfMemory)
{
	// Image sizes are assumed to fit in a 32-bit signed integer by the renderer,
	// so test that we can't create a 2+ GiB image.
	{
		Initialize(3, false);

		GLuint tex = 1;
		glBindTexture(GL_TEXTURE_3D, tex);

		const int width = 0xC2;
		const int height = 0x541;
		const int depth = 0x404;
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, width, height, depth, 0, GL_RGBA, GL_FLOAT, nullptr);
		EXPECT_GLENUM_EQ(GL_OUT_OF_MEMORY, glGetError());

		// The spec states that the GL is in an undefined state when GL_OUT_OF_MEMORY
		// is returned, and the context must be recreated before attempting more rendering.
		Uninitialize();
	}
}

// Test using TexImage2D to define a rectangle texture

TEST_F(SwiftShaderTest, TextureRectangle_TexImage2D)
{
	Initialize(2, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);

	// Defining level 0 is allowed
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Defining level other than 0 is not allowed
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());

	GLint maxSize = 0;
	glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB, &maxSize);

	// Defining a texture of the max size is allowed
	{
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, maxSize, maxSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		GLenum error = glGetError();
		ASSERT_TRUE(error == GL_NO_ERROR || error == GL_OUT_OF_MEMORY);
	}

	// Defining a texture larger than the max size is disallowed
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, maxSize + 1, maxSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, maxSize, maxSize + 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());

	Uninitialize();
}

// Test using CompressedTexImage2D cannot be used on a retangle texture
TEST_F(SwiftShaderTest, TextureRectangle_CompressedTexImage2DDisallowed)
{
	Initialize(2, false);

	const char data[128] = { 0 };

	// Control case: 2D texture
	{
		GLuint tex = 1;
		glBindTexture(GL_TEXTURE_2D, tex);
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 16, 16, 0, 128, data);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	}

	// Rectangle textures cannot be compressed
	{
		GLuint tex = 2;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
		glCompressedTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 16, 16, 0, 128, data);
		EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	}

	Uninitialize();
}

// Test using TexStorage2D to define a rectangle texture (ES3)
TEST_F(SwiftShaderTest, TextureRectangle_TexStorage2D)
{
	Initialize(3, false);

	// Defining one level is allowed
	{
		GLuint tex = 1;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
		glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA8UI, 16, 16);
		EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	}

	// Having more than one level is not allowed
	{
		GLuint tex = 2;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
		// Use 5 levels because the EXT_texture_storage extension requires a mip chain all the way
		// to a 1x1 mip.
		glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 5, GL_RGBA8UI, 16, 16);
		EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());
	}

	GLint maxSize = 0;
	glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB, &maxSize);

	// Defining a texture of the max size is allowed but still allow for OOM
	{
		GLuint tex = 3;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
		glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA8UI, maxSize, maxSize);
		GLenum error = glGetError();
		ASSERT_TRUE(error == GL_NO_ERROR || error == GL_OUT_OF_MEMORY);
	}

	// Defining a texture larger than the max size is disallowed
	{
		GLuint tex = 4;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
		glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA8UI, maxSize + 1, maxSize);
		EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());
		glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA8UI, maxSize, maxSize + 1);
		EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());
	}

	// Compressed formats are disallowed
	GLuint tex = 5;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	glTexStorage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 16, 16);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());

	Uninitialize();
}

// Test validation of disallowed texture parameters
TEST_F(SwiftShaderTest, TextureRectangle_TexParameterRestriction)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);

	// Only wrap mode CLAMP_TO_EDGE is supported
	// Wrap S
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_REPEAT);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());

	// Wrap T
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_REPEAT);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());

	// Min filter has to be nearest or linear
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	EXPECT_GLENUM_EQ(GL_INVALID_ENUM, glGetError());

	// Base level has to be 0
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_BASE_LEVEL, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_BASE_LEVEL, 1);
	EXPECT_GLENUM_EQ(GL_INVALID_OPERATION, glGetError());

	Uninitialize();
}

// Test validation of "level" in FramebufferTexture2D
TEST_F(SwiftShaderTest, TextureRectangle_FramebufferTexture2DLevel)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Using level 0 of a rectangle texture is valid.
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex, 0);
	EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Setting level != 0 is invalid
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex, 1);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());

	Uninitialize();
}

// Test sampling from a rectangle texture
TEST_F(SwiftShaderTest, TextureRectangle_SamplingFromRectangle)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	unsigned char green[4] = { 0, 255, 0, 255 };
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	const std::string vs =
		"attribute vec4 position;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#extension GL_ARB_texture_rectangle : require\n"
		"precision mediump float;\n"
		"uniform sampler2DRect tex;\n"
		"void main()\n"
		"{\n"
		"    gl_FragColor = texture2DRect(tex, vec2(0, 0));\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	glUseProgram(ph.program);
	GLint location = glGetUniformLocation(ph.program, "tex");
	ASSERT_NE(-1, location);
	glUniform1i(location, 0);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program, "tex");

	deleteProgram(ph);

	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test sampling from a rectangle texture
TEST_F(SwiftShaderTest, TextureRectangle_SamplingFromRectangleESSL3)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	unsigned char green[4] = { 0, 255, 0, 255 };
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	const std::string vs =
		"#version 300 es\n"
		"in vec4 position;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
		"}\n";

	const std::string fs =
		"#version 300 es\n"
		"#extension GL_ARB_texture_rectangle : require\n"
		"precision mediump float;\n"
		"uniform sampler2DRect tex;\n"
		"out vec4 fragColor;\n"
		"void main()\n"
		"{\n"
		"    fragColor = texture(tex, vec2(0, 0));\n"
		"}\n";

	const ProgramHandles ph = createProgram(vs, fs);

	glUseProgram(ph.program);
	GLint location = glGetUniformLocation(ph.program, "tex");
	ASSERT_NE(-1, location);
	glUniform1i(location, 0);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	drawQuad(ph.program, "tex");

	deleteProgram(ph);

	expectFramebufferColor(green);

	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test attaching a rectangle texture and rendering to it.
TEST_F(SwiftShaderTest, TextureRectangle_RenderToRectangle)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	unsigned char black[4] = { 0, 0, 0, 255 };
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex, 0);
	EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Clearing a texture is just as good as checking we can render to it, right?
	glClearColor(0.0, 1.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

TEST_F(SwiftShaderTest, TextureRectangle_DefaultSamplerParameters)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);

	GLint minFilter = 0;
	glGetTexParameteriv(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, &minFilter);
	EXPECT_GLENUM_EQ(GL_LINEAR, minFilter);

	GLint wrapS = 0;
	glGetTexParameteriv(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, &wrapS);
	EXPECT_GLENUM_EQ(GL_CLAMP_TO_EDGE, wrapS);

	GLint wrapT = 0;
	glGetTexParameteriv(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, &wrapT);
	EXPECT_GLENUM_EQ(GL_CLAMP_TO_EDGE, wrapT);

	Uninitialize();
}

// Test glCopyTexImage with rectangle textures (ES3)
TEST_F(SwiftShaderTest, TextureRectangle_CopyTexImage)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0, 1, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Error case: level != 0
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 1, GL_RGBA8, 0, 0, 1, 1, 0);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());

	// level = 0 works and defines the texture.
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, 0, 0, 1, 1, 0);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex, 0);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

// Test glCopyTexSubImage with rectangle textures (ES3)
TEST_F(SwiftShaderTest, TextureRectangle_CopyTexSubImage)
{
	Initialize(3, false);

	GLuint tex = 1;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
	unsigned char black[4] = { 0, 0, 0, 255 };
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0, 1, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	// Error case: level != 0
	glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 1, 0, 0, 0, 0, 1, 1);
	EXPECT_GLENUM_EQ(GL_INVALID_VALUE, glGetError());

	// level = 0 works and defines the texture.
	glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, 1, 1);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	GLuint fbo = 1;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex, 0);

	unsigned char green[4] = { 0, 255, 0, 255 };
	expectFramebufferColor(green);
	EXPECT_GLENUM_EQ(GL_NONE, glGetError());

	Uninitialize();
}

#ifndef EGL_ANGLE_iosurface_client_buffer
#define EGL_ANGLE_iosurface_client_buffer 1
#define EGL_IOSURFACE_ANGLE 0x3454
#define EGL_IOSURFACE_PLANE_ANGLE 0x345A
#define EGL_TEXTURE_RECTANGLE_ANGLE 0x345B
#define EGL_TEXTURE_TYPE_ANGLE 0x345C
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#endif /* EGL_ANGLE_iosurface_client_buffer */

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>

namespace
{
	void AddIntegerValue(CFMutableDictionaryRef dictionary, const CFStringRef key, int32_t value)
	{
		CFNumberRef number = CFNumberCreate(nullptr, kCFNumberSInt32Type, &value);
		CFDictionaryAddValue(dictionary, key, number);
		CFRelease(number);
	}
}  // anonymous namespace

class EGLClientBufferWrapper
{
public:
	EGLClientBufferWrapper(int width = 1, int height = 1)
	{
		// Create a 1 by 1 BGRA8888 IOSurface
		ioSurface = nullptr;

		CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
			kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		AddIntegerValue(dict, kIOSurfaceWidth, width);
		AddIntegerValue(dict, kIOSurfaceHeight, height);
		AddIntegerValue(dict, kIOSurfacePixelFormat, 'BGRA');
		AddIntegerValue(dict, kIOSurfaceBytesPerElement, 4);

		ioSurface = IOSurfaceCreate(dict);
		CFRelease(dict);

		EXPECT_NE(nullptr, ioSurface);
	}

	~EGLClientBufferWrapper()
	{
		IOSurfaceUnlock(ioSurface, kIOSurfaceLockReadOnly, nullptr);

		CFRelease(ioSurface);
	}

	EGLClientBuffer getClientBuffer() const
	{
		return ioSurface;
	}

	const unsigned char* lockColor()
	{
		IOSurfaceLock(ioSurface, kIOSurfaceLockReadOnly, nullptr);
		return reinterpret_cast<const unsigned char*>(IOSurfaceGetBaseAddress(ioSurface));
	}

	void unlockColor()
	{
		IOSurfaceUnlock(ioSurface, kIOSurfaceLockReadOnly, nullptr);
	}

	void writeColor(void* data, size_t dataSize)
	{
		// Write the data to the IOSurface
		IOSurfaceLock(ioSurface, 0, nullptr);
		memcpy(IOSurfaceGetBaseAddress(ioSurface), data, dataSize);
		IOSurfaceUnlock(ioSurface, 0, nullptr);
	}
private:
	IOSurfaceRef ioSurface;
};

#else // __APPLE__

class EGLClientBufferWrapper
{
public:
	EGLClientBufferWrapper(int width = 1, int height = 1)
	{
		clientBuffer = new unsigned char[4 * width * height];
	}

	~EGLClientBufferWrapper()
	{
		delete[] clientBuffer;
	}

	EGLClientBuffer getClientBuffer() const
	{
		return clientBuffer;
	}

	const unsigned char* lockColor()
	{
		return clientBuffer;
	}

	void unlockColor()
	{
	}

	void writeColor(void* data, size_t dataSize)
	{
		memcpy(clientBuffer, data, dataSize);
	}
private:
	unsigned char* clientBuffer;
};

#endif

class IOSurfaceClientBufferTest : public SwiftShaderTest
{
protected:
	EGLSurface createIOSurfacePbuffer(EGLClientBuffer buffer, EGLint width, EGLint height, EGLint plane, GLenum internalFormat, GLenum type) const
	{
		// Make a PBuffer from it using the EGL_ANGLE_iosurface_client_buffer extension
		const EGLint attribs[] = {
			EGL_WIDTH,                         width,
			EGL_HEIGHT,                        height,
			EGL_IOSURFACE_PLANE_ANGLE,         plane,
			EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
			EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, (EGLint)internalFormat,
			EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
			EGL_TEXTURE_TYPE_ANGLE,            (EGLint)type,
			EGL_NONE,                          EGL_NONE,
		};

		EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, buffer, getConfig(), attribs);
		EXPECT_NE(EGL_NO_SURFACE, pbuffer);
		return pbuffer;
	}

	void bindIOSurfaceToTexture(EGLClientBuffer buffer, EGLint width, EGLint height, EGLint plane, GLenum internalFormat, GLenum type, EGLSurface *pbuffer, GLuint *texture) const
	{
		*pbuffer = createIOSurfacePbuffer(buffer, width, height, plane, internalFormat, type);

		// Bind the pbuffer
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, *texture);
		EGLBoolean result = eglBindTexImage(getDisplay(), *pbuffer, EGL_BACK_BUFFER);
		EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
	}

	void doClear(GLenum internalFormat, bool clearToZero)
	{
		if(internalFormat == GL_R16UI)
		{
			GLuint color = clearToZero ? 0 : 257;
			glClearBufferuiv(GL_COLOR, 0, &color);
			EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
		}
		else
		{
			glClearColor(clearToZero ? 0.0f : 1.0f / 255.0f,
				clearToZero ? 0.0f : 2.0f / 255.0f,
				clearToZero ? 0.0f : 3.0f / 255.0f,
				clearToZero ? 0.0f : 4.0f / 255.0f);
			EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
			glClear(GL_COLOR_BUFFER_BIT);
			EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
		}
	}

	void doClearTest(EGLClientBufferWrapper& clientBufferWrapper, GLenum internalFormat, GLenum type, void *data, size_t dataSize)
	{
		ASSERT_TRUE(dataSize <= 4);

		// Bind the IOSurface to a texture and clear it.
		GLuint texture = 1;
		EGLSurface pbuffer;
		bindIOSurfaceToTexture(clientBufferWrapper.getClientBuffer(), 1, 1, 0, internalFormat, type, &pbuffer, &texture);

		// glClear the pbuffer
		GLuint fbo = 2;
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, texture, 0);
		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());
		EXPECT_GLENUM_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());

		doClear(internalFormat, false);

		// Unbind pbuffer and check content.
		EGLBoolean result = eglReleaseTexImage(getDisplay(), pbuffer, EGL_BACK_BUFFER);
		EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());

		const unsigned char* color = clientBufferWrapper.lockColor();
		for(size_t i = 0; i < dataSize; ++i)
		{
			EXPECT_EQ(color[i], reinterpret_cast<unsigned char*>(data)[i]);
		}

		result = eglDestroySurface(getDisplay(), pbuffer);
		EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());
	}

	void doSampleTest(EGLClientBufferWrapper& clientBufferWrapper, GLenum internalFormat, GLenum type, void *data, size_t dataSize)
	{
		ASSERT_TRUE(dataSize <= 4);

		clientBufferWrapper.writeColor(data, dataSize);

		// Bind the IOSurface to a texture and clear it.
		GLuint texture = 1;
		EGLSurface pbuffer;
		bindIOSurfaceToTexture(clientBufferWrapper.getClientBuffer(), 1, 1, 0, internalFormat, type, &pbuffer, &texture);

		doClear(internalFormat, true);

		// Create program and draw quad using it
		const std::string vs =
			"attribute vec4 position;\n"
			"void main()\n"
			"{\n"
			"    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
			"}\n";

		const std::string fs =
			"#extension GL_ARB_texture_rectangle : require\n"
			"precision mediump float;\n"
			"uniform sampler2DRect tex;\n"
			"void main()\n"
			"{\n"
			"    gl_FragColor = texture2DRect(tex, vec2(0, 0));\n"
			"}\n";

		const ProgramHandles ph = createProgram(vs, fs);

		drawQuad(ph.program, "tex");

		deleteProgram(ph);

		EXPECT_GLENUM_EQ(GL_NO_ERROR, glGetError());

		// Unbind pbuffer and check content.
		EGLBoolean result = eglReleaseTexImage(getDisplay(), pbuffer, EGL_BACK_BUFFER);
		EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
		EXPECT_EQ(EGL_SUCCESS, eglGetError());

		const unsigned char* color = clientBufferWrapper.lockColor();
		for(size_t i = 0; i < dataSize; ++i)
		{
			EXPECT_EQ(color[i], reinterpret_cast<unsigned char*>(data)[i]);
		}
		clientBufferWrapper.unlockColor();
	}
};

// Tests for the EGL_ANGLE_iosurface_client_buffer extension
TEST_F(IOSurfaceClientBufferTest, RenderToBGRA8888IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[4] = { 3, 2, 1, 4 };
		doClearTest(clientBufferWrapper, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data, 4);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test reading from BGRA8888 IOSurfaces
TEST_F(IOSurfaceClientBufferTest, ReadFromBGRA8888IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[4] = { 3, 2, 1, 4 };
		doSampleTest(clientBufferWrapper, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data, 4);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test using RG88 IOSurfaces for rendering
TEST_F(IOSurfaceClientBufferTest, RenderToRG88IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[2] = { 1, 2 };
		doClearTest(clientBufferWrapper, GL_RG, GL_UNSIGNED_BYTE, data, 2);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test reading from RG88 IOSurfaces
TEST_F(IOSurfaceClientBufferTest, ReadFromRG88IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[2] = { 1, 2 };
		doSampleTest(clientBufferWrapper, GL_RG, GL_UNSIGNED_BYTE, data, 2);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test using R8 IOSurfaces for rendering
TEST_F(IOSurfaceClientBufferTest, RenderToR8IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[1] = { 1 };
		doClearTest(clientBufferWrapper, GL_RED, GL_UNSIGNED_BYTE, data, 1);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test reading from R8 IOSurfaces
TEST_F(IOSurfaceClientBufferTest, ReadFromR8IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		unsigned char data[1] = { 1 };
		doSampleTest(clientBufferWrapper, GL_RED, GL_UNSIGNED_BYTE, data, 1);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test using R16 IOSurfaces for rendering
TEST_F(IOSurfaceClientBufferTest, RenderToR16IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		uint16_t data[1] = { 257 };
		doClearTest(clientBufferWrapper, GL_R16UI, GL_UNSIGNED_SHORT, data, 2);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test reading from R8 IOSurfaces
TEST_F(IOSurfaceClientBufferTest, ReadFromR16IOSurface)
{
	Initialize(3, false);

	{ // EGLClientBufferWrapper scope
		EGLClientBufferWrapper clientBufferWrapper;
		uint16_t data[1] = { 257 };
		doSampleTest(clientBufferWrapper, GL_R16UI, GL_UNSIGNED_SHORT, data, 1);
	} // end of EGLClientBufferWrapper scope

	Uninitialize();
}

// Test the validation errors for missing attributes for eglCreatePbufferFromClientBuffer with
// IOSurface
TEST_F(IOSurfaceClientBufferTest, NegativeValidationMissingAttributes)
{
	Initialize(3, false);

	{
		EGLClientBufferWrapper clientBufferWrapper(10, 10);

		// Success case
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_NE(EGL_NO_SURFACE, pbuffer);

			EGLBoolean result = eglDestroySurface(getDisplay(), pbuffer);
			EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
		}

		// Missing EGL_WIDTH
		{
			const EGLint attribs[] = {
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
		}

		// Missing EGL_HEIGHT
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
		}

		// Missing EGL_IOSURFACE_PLANE_ANGLE
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
		}

		// Missing EGL_TEXTURE_TARGET - EGL_BAD_MATCH from the base spec of
		// eglCreatePbufferFromClientBuffer
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_MATCH, eglGetError());
		}

		// Missing EGL_TEXTURE_INTERNAL_FORMAT_ANGLE
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
		}

		// Missing EGL_TEXTURE_FORMAT - EGL_BAD_MATCH from the base spec of
		// eglCreatePbufferFromClientBuffer
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_MATCH, eglGetError());
		}

		// Missing EGL_TEXTURE_TYPE_ANGLE
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
		}
	}

	Uninitialize();
}

// Test the validation errors for bad parameters for eglCreatePbufferFromClientBuffer with IOSurface
TEST_F(IOSurfaceClientBufferTest, NegativeValidationBadAttributes)
{
	Initialize(3, false);

	{
		EGLClientBufferWrapper clientBufferWrapper(10, 10);

		// Success case
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_NE(EGL_NO_SURFACE, pbuffer);

			EGLBoolean result = eglDestroySurface(getDisplay(), pbuffer);
			EXPECT_EQ((EGLBoolean)EGL_TRUE, result);
			EXPECT_EQ(EGL_SUCCESS, eglGetError());
		}

		// EGL_TEXTURE_FORMAT must be EGL_TEXTURE_RGBA
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGB,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// EGL_WIDTH must be at least 1
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         0,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// EGL_HEIGHT must be at least 1
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        0,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

#if defined(__APPLE__)
		// EGL_WIDTH must be at most the width of the IOSurface
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         11,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// EGL_HEIGHT must be at most the height of the IOSurface
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        11,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// EGL_IOSURFACE_PLANE_ANGLE must less than the number of planes of the IOSurface
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         1,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}
#endif

		// EGL_TEXTURE_FORMAT must be at EGL_TEXTURE_RECTANGLE_ANGLE
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_2D,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// EGL_IOSURFACE_PLANE_ANGLE must be at least 0
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         -1,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}

		// The internal format / type most be listed in the table
		{
			const EGLint attribs[] = {
				EGL_WIDTH,                         10,
				EGL_HEIGHT,                        10,
				EGL_IOSURFACE_PLANE_ANGLE,         0,
				EGL_TEXTURE_TARGET,                EGL_TEXTURE_RECTANGLE_ANGLE,
				EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_RGBA,
				EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
				EGL_TEXTURE_TYPE_ANGLE,            GL_UNSIGNED_BYTE,
				EGL_NONE,                          EGL_NONE,
			};

			EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(getDisplay(), EGL_IOSURFACE_ANGLE, clientBufferWrapper.getClientBuffer(), getConfig(), attribs);
			EXPECT_EQ(EGL_NO_SURFACE, pbuffer);
			EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
		}
	}

	Uninitialize();
}

// Test IOSurface pbuffers cannot be made current
TEST_F(IOSurfaceClientBufferTest, MakeCurrentDisallowed)
{
	Initialize(3, false);

	{
		EGLClientBufferWrapper clientBufferWrapper(10, 10);

		EGLSurface pbuffer = createIOSurfacePbuffer(clientBufferWrapper.getClientBuffer(), 10, 10, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE);

		EGLBoolean result = eglMakeCurrent(getDisplay(), pbuffer, pbuffer, getContext());
		EXPECT_EQ((EGLBoolean)EGL_FALSE, result);
		EXPECT_EQ(EGL_BAD_SURFACE, eglGetError());
	}

	Uninitialize();
}
