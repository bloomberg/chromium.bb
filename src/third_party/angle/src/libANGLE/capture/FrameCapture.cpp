//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCapture.cpp:
//   ANGLE Frame capture implementation.
//

#include "libANGLE/capture/FrameCapture.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

#include "sys/stat.h"

#include "common/angle_version.h"
#include "common/mathutil.h"
#include "common/serializer/JsonSerializer.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/GLES1Renderer.h"
#include "libANGLE/Query.h"
#include "libANGLE/ResourceMap.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Surface.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/capture/capture_gles_1_0_autogen.h"
#include "libANGLE/capture/capture_gles_2_0_autogen.h"
#include "libANGLE/capture/capture_gles_3_0_autogen.h"
#include "libANGLE/capture/capture_gles_3_1_autogen.h"
#include "libANGLE/capture/capture_gles_3_2_autogen.h"
#include "libANGLE/capture/capture_gles_ext_autogen.h"
#include "libANGLE/capture/frame_capture_utils.h"
#include "libANGLE/capture/gl_enum_utils.h"
#include "libANGLE/entry_points_utils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"
#include "third_party/ceval/ceval.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

#if !ANGLE_CAPTURE_ENABLED
#    error Frame capture must be enabled to include this file.
#endif  // !ANGLE_CAPTURE_ENABLED

namespace angle
{
namespace
{

constexpr char kEnabledVarName[]        = "ANGLE_CAPTURE_ENABLED";
constexpr char kOutDirectoryVarName[]   = "ANGLE_CAPTURE_OUT_DIR";
constexpr char kFrameStartVarName[]     = "ANGLE_CAPTURE_FRAME_START";
constexpr char kFrameEndVarName[]       = "ANGLE_CAPTURE_FRAME_END";
constexpr char kTriggerVarName[]        = "ANGLE_CAPTURE_TRIGGER";
constexpr char kCaptureLabelVarName[]   = "ANGLE_CAPTURE_LABEL";
constexpr char kCompressionVarName[]    = "ANGLE_CAPTURE_COMPRESSION";
constexpr char kSerializeStateVarName[] = "ANGLE_CAPTURE_SERIALIZE_STATE";
constexpr char kValidationVarName[]     = "ANGLE_CAPTURE_VALIDATION";
constexpr char kValidationExprVarName[] = "ANGLE_CAPTURE_VALIDATION_EXPR";
constexpr char kTrimEnabledVarName[]    = "ANGLE_CAPTURE_TRIM_ENABLED";

constexpr size_t kBinaryAlignment   = 16;
constexpr size_t kFunctionSizeLimit = 5000;

// Limit based on MSVC Compiler Error C2026
constexpr size_t kStringLengthLimit = 16380;

// Android debug properties that correspond to the above environment variables
constexpr char kAndroidEnabled[]        = "debug.angle.capture.enabled";
constexpr char kAndroidOutDir[]         = "debug.angle.capture.out_dir";
constexpr char kAndroidFrameStart[]     = "debug.angle.capture.frame_start";
constexpr char kAndroidFrameEnd[]       = "debug.angle.capture.frame_end";
constexpr char kAndroidTrigger[]        = "debug.angle.capture.trigger";
constexpr char kAndroidCaptureLabel[]   = "debug.angle.capture.label";
constexpr char kAndroidCompression[]    = "debug.angle.capture.compression";
constexpr char kAndroidValidation[]     = "debug.angle.capture.validation";
constexpr char kAndroidValidationExpr[] = "debug.angle.capture.validation_expr";
constexpr char kAndroidTrimEnabled[]    = "debug.angle.capture.trim_enabled";

struct FramebufferCaptureFuncs
{
    FramebufferCaptureFuncs(bool isGLES1)
    {
        if (isGLES1)
        {
            framebufferTexture2D    = &gl::CaptureFramebufferTexture2DOES;
            framebufferRenderbuffer = &gl::CaptureFramebufferRenderbufferOES;
            bindFramebuffer         = &gl::CaptureBindFramebufferOES;
            genFramebuffers         = &gl::CaptureGenFramebuffersOES;
            bindRenderbuffer        = &gl::CaptureBindRenderbufferOES;
            genRenderbuffers        = &gl::CaptureGenRenderbuffersOES;
            renderbufferStorage     = &gl::CaptureRenderbufferStorageOES;
        }
        else
        {
            framebufferTexture2D    = &gl::CaptureFramebufferTexture2D;
            framebufferRenderbuffer = &gl::CaptureFramebufferRenderbuffer;
            bindFramebuffer         = &gl::CaptureBindFramebuffer;
            genFramebuffers         = &gl::CaptureGenFramebuffers;
            bindRenderbuffer        = &gl::CaptureBindRenderbuffer;
            genRenderbuffers        = &gl::CaptureGenRenderbuffers;
            renderbufferStorage     = &gl::CaptureRenderbufferStorage;
        }
    }

    decltype(&gl::CaptureFramebufferTexture2D) framebufferTexture2D;
    decltype(&gl::CaptureFramebufferRenderbuffer) framebufferRenderbuffer;
    decltype(&gl::CaptureBindFramebuffer) bindFramebuffer;
    decltype(&gl::CaptureGenFramebuffers) genFramebuffers;
    decltype(&gl::CaptureBindRenderbuffer) bindRenderbuffer;
    decltype(&gl::CaptureGenRenderbuffers) genRenderbuffers;
    decltype(&gl::CaptureRenderbufferStorage) renderbufferStorage;
};

std::string GetDefaultOutDirectory()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    std::string path = "/sdcard/Android/data/";

    // Linux interface to get application id of the running process
    FILE *cmdline = fopen("/proc/self/cmdline", "r");
    char applicationId[512];
    if (cmdline)
    {
        fread(applicationId, 1, sizeof(applicationId), cmdline);
        fclose(cmdline);

        // Some package may have application id as <app_name>:<cmd_name>
        char *colonSep = strchr(applicationId, ':');
        if (colonSep)
        {
            *colonSep = '\0';
        }
    }
    else
    {
        ERR() << "not able to lookup application id";
    }

    constexpr char kAndroidOutputSubdir[] = "/angle_capture/";
    path += std::string(applicationId) + kAndroidOutputSubdir;

    // Check for existence of output path
    struct stat dir_stat;
    if (stat(path.c_str(), &dir_stat) == -1)
    {
        ERR() << "Output directory '" << path
              << "' does not exist.  Create it over adb using mkdir.";
    }

    return path;
#else
    return std::string("./");
#endif  // defined(ANGLE_PLATFORM_ANDROID)
}

std::string GetCaptureTrigger()
{
    return GetEnvironmentVarOrUnCachedAndroidProperty(kTriggerVarName, kAndroidTrigger);
}

std::ostream &operator<<(std::ostream &os, gl::ContextID contextId)
{
    os << static_cast<int>(contextId.value);
    return os;
}

// Used to indicate that "shared" should be used to identify the files.
constexpr gl::ContextID kSharedContextId = {0};
// Used to indicate no context ID should be output.
constexpr gl::ContextID kNoContextId = {std::numeric_limits<uint32_t>::max()};

struct FmtCapturePrefix
{
    FmtCapturePrefix(gl::ContextID contextIdIn, const std::string &captureLabelIn)
        : contextId(contextIdIn), captureLabel(captureLabelIn)
    {}
    gl::ContextID contextId;
    const std::string &captureLabel;
};

std::ostream &operator<<(std::ostream &os, const FmtCapturePrefix &fmt)
{
    if (fmt.captureLabel.empty())
    {
        os << "angle_capture";
    }
    else
    {
        os << fmt.captureLabel;
    }

    if (fmt.contextId == kSharedContextId)
    {
        os << "_shared";
    }
    else if (fmt.contextId != kNoContextId)
    {
        os << "_context" << fmt.contextId;
    }

    return os;
}

enum class ReplayFunc
{
    Replay,
    Setup,
    Reset,
};

constexpr uint32_t kNoPartId = std::numeric_limits<uint32_t>::max();

struct FmtReplayFunction
{
    FmtReplayFunction(gl::ContextID contextIdIn,
                      uint32_t frameIndexIn,
                      uint32_t partIdIn = kNoPartId)
        : contextId(contextIdIn), frameIndex(frameIndexIn), partId(partIdIn)
    {}
    gl::ContextID contextId;
    uint32_t frameIndex;
    uint32_t partId;
};

std::ostream &operator<<(std::ostream &os, const FmtReplayFunction &fmt)
{
    os << "ReplayContext";

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }
    else
    {
        os << fmt.contextId;
    }

    os << "Frame" << fmt.frameIndex;

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }
    os << "()";
    return os;
}

struct FmtSetupFunction
{
    FmtSetupFunction(uint32_t partIdIn, gl::ContextID contextIdIn)
        : partId(partIdIn), contextId(contextIdIn)
    {}

    uint32_t partId;
    gl::ContextID contextId;
};

std::ostream &operator<<(std::ostream &os, const FmtSetupFunction &fmt)
{
    os << "SetupReplayContext";

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }
    else
    {
        os << fmt.contextId;
    }

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }
    os << "()";
    return os;
}

struct FmtResetFunction
{
    FmtResetFunction() {}
};

std::ostream &operator<<(std::ostream &os, const FmtResetFunction &fmt)
{
    os << "ResetReplay()";
    return os;
}

struct FmtFunction
{
    FmtFunction(ReplayFunc funcTypeIn,
                gl::ContextID contextIdIn,
                uint32_t frameIndexIn,
                uint32_t partIdIn)
        : funcType(funcTypeIn), contextId(contextIdIn), frameIndex(frameIndexIn), partId(partIdIn)
    {}

    ReplayFunc funcType;
    gl::ContextID contextId;
    uint32_t frameIndex;
    uint32_t partId;
};

std::ostream &operator<<(std::ostream &os, const FmtFunction &fmt)
{
    switch (fmt.funcType)
    {
        case ReplayFunc::Replay:
            os << FmtReplayFunction(fmt.contextId, fmt.frameIndex, fmt.partId);
            break;

        case ReplayFunc::Setup:
            os << FmtSetupFunction(fmt.partId, fmt.contextId);
            break;

        case ReplayFunc::Reset:
            os << FmtResetFunction();
            break;

        default:
            UNREACHABLE();
            break;
    }

    return os;
}

struct FmtGetSerializedContextStateFunction
{
    FmtGetSerializedContextStateFunction(gl::ContextID contextIdIn, uint32_t frameIndexIn)
        : contextId(contextIdIn), frameIndex(frameIndexIn)
    {}
    gl::ContextID contextId;
    uint32_t frameIndex;
};

std::ostream &operator<<(std::ostream &os, const FmtGetSerializedContextStateFunction &fmt)
{
    os << "GetSerializedContext" << fmt.contextId << "StateFrame" << fmt.frameIndex << "Data()";
    return os;
}

std::string GetCaptureFileName(gl::ContextID contextId,
                               const std::string &captureLabel,
                               uint32_t frameIndex,
                               const char *suffix)
{
    std::stringstream fnameStream;

    if (contextId == kSharedContextId)
    {
        fnameStream << FmtCapturePrefix(contextId, captureLabel) << suffix;
    }
    else
    {
        fnameStream << FmtCapturePrefix(contextId, captureLabel) << "_frame" << std::setfill('0')
                    << std::setw(3) << frameIndex << suffix;
    }

    return fnameStream.str();
}

std::string GetCaptureFilePath(const std::string &outDir,
                               gl::ContextID contextId,
                               const std::string &captureLabel,
                               uint32_t frameIndex,
                               const char *suffix)
{
    return outDir + GetCaptureFileName(contextId, captureLabel, frameIndex, suffix);
}

void WriteParamStaticVarName(const CallCapture &call,
                             const ParamCapture &param,
                             int counter,
                             std::ostream &out)
{
    out << call.name() << "_" << param.name << "_" << counter;
}

void WriteGLFloatValue(std::ostream &out, GLfloat value)
{
    // Check for non-representable values
    ASSERT(std::numeric_limits<float>::has_infinity);
    ASSERT(std::numeric_limits<float>::has_quiet_NaN);

    if (std::isinf(value))
    {
        float negativeInf = -std::numeric_limits<float>::infinity();
        if (value == negativeInf)
        {
            out << "-";
        }
        out << "std::numeric_limits<float>::infinity()";
    }
    else if (std::isnan(value))
    {
        out << "std::numeric_limits<float>::quiet_NaN()";
    }
    else
    {
        out << std::setprecision(16);
        out << value;
    }
}

template <typename T, typename CastT = T>
void WriteInlineData(const std::vector<uint8_t> &vec, std::ostream &out)
{
    const T *data = reinterpret_cast<const T *>(vec.data());
    size_t count  = vec.size() / sizeof(T);

    if (data == nullptr)
    {
        return;
    }

    out << static_cast<CastT>(data[0]);

    for (size_t dataIndex = 1; dataIndex < count; ++dataIndex)
    {
        out << ", " << static_cast<CastT>(data[dataIndex]);
    }
}

template <>
void WriteInlineData<GLchar>(const std::vector<uint8_t> &vec, std::ostream &out)
{
    const GLchar *data = reinterpret_cast<const GLchar *>(vec.data());
    size_t count       = vec.size() / sizeof(GLchar);

    if (data == nullptr || data[0] == '\0')
    {
        return;
    }

    out << "\"";

    for (size_t dataIndex = 0; dataIndex < count; ++dataIndex)
    {
        if (data[dataIndex] == '\0')
            break;

        out << static_cast<GLchar>(data[dataIndex]);
    }

    out << "\"";
}

void WriteStringParamReplay(DataTracker *dataTracker,
                            std::ostream &out,
                            std::ostream &header,
                            const CallCapture &call,
                            const ParamCapture &param,
                            std::vector<uint8_t> *binaryData)
{
    const std::vector<uint8_t> &data = param.data[0];
    // null terminate C style string
    ASSERT(data.size() > 0 && data.back() == '\0');
    std::string str(data.begin(), data.end() - 1);

    constexpr size_t kMaxInlineStringLength = 20000;
    if (str.size() > kMaxInlineStringLength)
    {
        // Store in binary file if the string is too long.
        // Round up to 16-byte boundary for cross ABI safety.
        size_t offset = rx::roundUpPow2(binaryData->size(), kBinaryAlignment);
        binaryData->resize(offset + str.size() + 1);
        memcpy(binaryData->data() + offset, str.data(), str.size() + 1);
        out << "reinterpret_cast<const char *>(&gBinaryData[" << offset << "])";
    }
    else if (str.find('\n') != std::string::npos)
    {
        // Move multi-line strings into the header.
        int counter = dataTracker->getCounters().getAndIncrement(call.entryPoint, param.name);

        header << "const char ";
        WriteParamStaticVarName(call, param, counter, header);
        header << "[] = R\"(" << str << ")\";\n";

        WriteParamStaticVarName(call, param, counter, out);
    }
    else
    {
        out << "\"" << str << "\"";
    }
}

void WriteStringPointerParamReplay(DataTracker *dataTracker,
                                   std::ostream &out,
                                   std::ostream &header,
                                   const CallCapture &call,
                                   const ParamCapture &param)
{
    // Concatenate the strings to ensure we get an accurate counter
    std::vector<std::string> strings;
    for (const std::vector<uint8_t> &data : param.data)
    {
        // null terminate C style string
        ASSERT(data.size() > 0 && data.back() == '\0');
        strings.emplace_back(data.begin(), data.end() - 1);
    }

    int counter = dataTracker->getStringCounters().getStringCounter(strings);
    if (counter == kStringsNotFound)
    {
        // This is a unique set of strings, so set up their declaration and update the counter
        counter = dataTracker->getCounters().getAndIncrement(call.entryPoint, param.name);
        dataTracker->getStringCounters().setStringCounter(strings, counter);

        header << "const char* const ";
        WriteParamStaticVarName(call, param, counter, header);
        header << "[] = { \n";

        for (const std::string &str : strings)
        {
            // Break up long strings for MSVC
            size_t copyLength = 0;
            std::string separator;
            for (size_t i = 0; i < str.length(); i += kStringLengthLimit)
            {
                if ((str.length() - i) <= kStringLengthLimit)
                {
                    copyLength = str.length() - i;
                    separator  = ",";
                }
                else
                {
                    copyLength = kStringLengthLimit;
                    separator  = "";
                }

                header << "    R\"(" << str.substr(i, copyLength) << ")\"" << separator << "\n";
            }
        }

        header << " };\n";
    }

    ASSERT(counter >= 0);
    WriteParamStaticVarName(call, param, counter, out);
}

template <typename ParamT>
void WriteResourceIDPointerParamReplay(DataTracker *dataTracker,
                                       std::ostream &out,
                                       std::ostream &header,
                                       const CallCapture &call,
                                       const ParamCapture &param)
{
    int counter = dataTracker->getCounters().getAndIncrement(call.entryPoint, param.name);

    header << "const GLuint ";
    WriteParamStaticVarName(call, param, counter, header);
    header << "[] = { ";

    const ResourceIDType resourceIDType = GetResourceIDTypeFromParamType(param.type);
    ASSERT(resourceIDType != ResourceIDType::InvalidEnum);
    const char *name = GetResourceIDTypeName(resourceIDType);

    ASSERT(param.dataNElements > 0);
    ASSERT(param.data.size() == 1);

    const ParamT *returnedIDs = reinterpret_cast<const ParamT *>(param.data[0].data());
    for (GLsizei resIndex = 0; resIndex < param.dataNElements; ++resIndex)
    {
        ParamT id = returnedIDs[resIndex];
        if (resIndex > 0)
        {
            header << ", ";
        }
        header << "g" << name << "Map2[" << id.value << "]";
    }

    header << " };\n    ";

    WriteParamStaticVarName(call, param, counter, out);
}

void WriteBinaryParamReplay(DataTracker *dataTracker,
                            std::ostream &out,
                            std::ostream &header,
                            const CallCapture &call,
                            const ParamCapture &param,
                            std::vector<uint8_t> *binaryData)
{
    int counter = dataTracker->getCounters().getAndIncrement(call.entryPoint, param.name);

    ASSERT(param.data.size() == 1);
    const std::vector<uint8_t> &data = param.data[0];

    ParamType overrideType = param.type;
    if (param.type == ParamType::TGLvoidConstPointer || param.type == ParamType::TvoidConstPointer)
    {
        overrideType = ParamType::TGLubyteConstPointer;
    }
    if (overrideType == ParamType::TGLenumConstPointer || overrideType == ParamType::TGLcharPointer)
    {
        // Inline if data are of type string or enum
        std::string paramTypeString = ParamTypeToString(param.type);
        header << paramTypeString.substr(0, paramTypeString.length() - 1);
        WriteParamStaticVarName(call, param, counter, header);
        header << "[] = { ";
        if (overrideType == ParamType::TGLenumConstPointer)
        {
            WriteInlineData<GLuint>(data, header);
        }
        else
        {
            ASSERT(overrideType == ParamType::TGLcharPointer);
            WriteInlineData<GLchar>(data, header);
        }
        header << " };\n";
        WriteParamStaticVarName(call, param, counter, out);
    }
    else
    {
        // Store in binary file if data are not of type string or enum
        // Round up to 16-byte boundary for cross ABI safety
        size_t offset = rx::roundUpPow2(binaryData->size(), kBinaryAlignment);
        binaryData->resize(offset + data.size());
        memcpy(binaryData->data() + offset, data.data(), data.size());
        out << "reinterpret_cast<" << ParamTypeToString(overrideType) << ">(&gBinaryData[" << offset
            << "])";
    }
}

uintptr_t SyncIndexValue(GLsync sync)
{
    return reinterpret_cast<uintptr_t>(sync);
}

void WriteCppReplayForCall(const CallCapture &call,
                           DataTracker *dataTracker,
                           std::ostream &out,
                           std::ostream &header,
                           std::vector<uint8_t> *binaryData)
{
    std::ostringstream callOut;

    if (call.entryPoint == EntryPoint::GLCreateShader ||
        call.entryPoint == EntryPoint::GLCreateProgram ||
        call.entryPoint == EntryPoint::GLCreateShaderProgramv)
    {
        GLuint id = call.params.getReturnValue().value.GLuintVal;
        callOut << "gShaderProgramMap2[" << id << "] = ";
    }

    if (call.entryPoint == EntryPoint::GLFenceSync)
    {
        GLsync sync = call.params.getReturnValue().value.GLsyncVal;
        callOut << "gSyncMap[" << SyncIndexValue(sync) << "] = ";
    }

    // Depending on how a buffer is mapped, we may need to track its location for readback
    bool trackBufferPointer = false;

    if (call.entryPoint == EntryPoint::GLMapBufferRange ||
        call.entryPoint == EntryPoint::GLMapBufferRangeEXT)
    {
        GLbitfield access =
            call.params.getParam("access", ParamType::TGLbitfield, 3).value.GLbitfieldVal;

        trackBufferPointer = access & GL_MAP_WRITE_BIT;
    }

    if (call.entryPoint == EntryPoint::GLMapBuffer || call.entryPoint == EntryPoint::GLMapBufferOES)
    {
        GLenum access = call.params.getParam("access", ParamType::TGLenum, 1).value.GLenumVal;

        trackBufferPointer =
            access == GL_WRITE_ONLY_OES || access == GL_WRITE_ONLY || access == GL_READ_WRITE;
    }

    if (trackBufferPointer)
    {
        // Track the returned pointer so we update its data when unmapped
        gl::BufferID bufferID = call.params.getMappedBufferID();
        callOut << "gMappedBufferData[";
        WriteParamValueReplay<ParamType::TBufferID>(callOut, call, bufferID);
        callOut << "] = ";
    }

    callOut << call.name() << "(";

    bool first = true;
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        if (!first)
        {
            callOut << ", ";
        }

        if (param.arrayClientPointerIndex != -1 && param.value.voidConstPointerVal != nullptr)
        {
            callOut << "gClientArrays[" << param.arrayClientPointerIndex << "]";
        }
        else if (param.readBufferSizeBytes > 0)
        {
            callOut << "reinterpret_cast<" << ParamTypeToString(param.type) << ">(gReadBuffer)";
        }
        else if (param.data.empty())
        {
            if (param.type == ParamType::TGLenum)
            {
                OutputGLenumString(callOut, param.enumGroup, param.value.GLenumVal);
            }
            else if (param.type == ParamType::TGLbitfield)
            {
                OutputGLbitfieldString(callOut, param.enumGroup, param.value.GLbitfieldVal);
            }
            else if (param.type == ParamType::TGLfloat)
            {
                WriteGLFloatValue(callOut, param.value.GLfloatVal);
            }
            else if (param.type == ParamType::TGLsync)
            {
                callOut << "gSyncMap[" << SyncIndexValue(param.value.GLsyncVal) << "]";
            }
            else if (param.type == ParamType::TGLuint64 && param.name == "timeout")
            {
                if (param.value.GLuint64Val == GL_TIMEOUT_IGNORED)
                {
                    callOut << "GL_TIMEOUT_IGNORED";
                }
                else
                {
                    WriteParamCaptureReplay(callOut, call, param);
                }
            }
            else
            {
                WriteParamCaptureReplay(callOut, call, param);
            }
        }
        else
        {
            switch (param.type)
            {
                case ParamType::TGLcharConstPointer:
                    WriteStringParamReplay(dataTracker, callOut, header, call, param, binaryData);
                    break;
                case ParamType::TGLcharConstPointerPointer:
                    WriteStringPointerParamReplay(dataTracker, callOut, header, call, param);
                    break;
                case ParamType::TBufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::BufferID>(dataTracker, callOut, out, call,
                                                                    param);
                    break;
                case ParamType::TFenceNVIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::FenceNVID>(dataTracker, callOut, out,
                                                                     call, param);
                    break;
                case ParamType::TFramebufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::FramebufferID>(dataTracker, callOut, out,
                                                                         call, param);
                    break;
                case ParamType::TMemoryObjectIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::MemoryObjectID>(dataTracker, callOut, out,
                                                                          call, param);
                    break;
                case ParamType::TProgramPipelineIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::ProgramPipelineID>(dataTracker, callOut,
                                                                             out, call, param);
                    break;
                case ParamType::TQueryIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::QueryID>(dataTracker, callOut, out, call,
                                                                   param);
                    break;
                case ParamType::TRenderbufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::RenderbufferID>(dataTracker, callOut, out,
                                                                          call, param);
                    break;
                case ParamType::TSamplerIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::SamplerID>(dataTracker, callOut, out,
                                                                     call, param);
                    break;
                case ParamType::TSemaphoreIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::SemaphoreID>(dataTracker, callOut, out,
                                                                       call, param);
                    break;
                case ParamType::TTextureIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::TextureID>(dataTracker, callOut, out,
                                                                     call, param);
                    break;
                case ParamType::TTransformFeedbackIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::TransformFeedbackID>(dataTracker, callOut,
                                                                               out, call, param);
                    break;
                case ParamType::TVertexArrayIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::VertexArrayID>(dataTracker, callOut, out,
                                                                         call, param);
                    break;
                default:
                    WriteBinaryParamReplay(dataTracker, callOut, header, call, param, binaryData);
                    break;
            }
        }

        first = false;
    }

    callOut << ")";

    out << callOut.str();
}

size_t MaxClientArraySize(const gl::AttribArray<size_t> &clientArraySizes)
{
    size_t found = 0;
    for (size_t size : clientArraySizes)
    {
        if (size > found)
        {
            found = size;
        }
    }

    return found;
}

CallCapture CaptureMakeCurrent(EGLDisplay display,
                               EGLSurface draw,
                               EGLSurface read,
                               EGLContext context)
{
    ParamBuffer paramBuffer;
    paramBuffer.addValueParam("display", ParamType::TEGLDisplay, display);
    paramBuffer.addValueParam("draw", ParamType::TEGLSurface, draw);
    paramBuffer.addValueParam("read", ParamType::TEGLSurface, read);
    paramBuffer.addValueParam("context", ParamType::TEGLContext, context);

    return CallCapture(angle::EntryPoint::EGLMakeCurrent, std::move(paramBuffer));
}

std::string GetBinaryDataFilePath(bool compression, const std::string &captureLabel)
{
    std::stringstream fnameStream;
    fnameStream << FmtCapturePrefix(kNoContextId, captureLabel) << ".angledata";
    if (compression)
    {
        fnameStream << ".gz";
    }
    return fnameStream.str();
}

void SaveBinaryData(bool compression,
                    const std::string &outDir,
                    gl::ContextID contextId,
                    const std::string &captureLabel,
                    const std::vector<uint8_t> &binaryData)
{
    std::string binaryDataFileName = GetBinaryDataFilePath(compression, captureLabel);
    std::string dataFilepath       = outDir + binaryDataFileName;

    SaveFileHelper saveData(dataFilepath);

    if (compression)
    {
        // Save compressed data.
        uLong uncompressedSize       = static_cast<uLong>(binaryData.size());
        uLong expectedCompressedSize = zlib_internal::GzipExpectedCompressedSize(uncompressedSize);

        std::vector<uint8_t> compressedData(expectedCompressedSize, 0);

        uLong compressedSize = expectedCompressedSize;
        int zResult = zlib_internal::GzipCompressHelper(compressedData.data(), &compressedSize,
                                                        binaryData.data(), uncompressedSize,
                                                        nullptr, nullptr);

        if (zResult != Z_OK)
        {
            FATAL() << "Error compressing binary data: " << zResult;
        }

        saveData.write(compressedData.data(), compressedSize);
    }
    else
    {
        saveData.write(binaryData.data(), binaryData.size());
    }
}

void WriteInitReplayCall(bool compression,
                         std::ostream &out,
                         gl::ContextID contextId,
                         const std::string &captureLabel,
                         size_t maxClientArraySize,
                         size_t readBufferSize,
                         const PackedEnumMap<ResourceIDType, uint32_t> &maxIDs)
{
    for (ResourceIDType resourceID : AllEnums<ResourceIDType>())
    {
        const char *name = GetResourceIDTypeName(resourceID);
        out << "    uint32_t kMax" << name << " = " << maxIDs[resourceID] << ";\n";
    }

    std::string binaryDataFileName = GetBinaryDataFilePath(compression, captureLabel);
    out << "    InitializeReplay2(\"" << binaryDataFileName << "\", " << maxClientArraySize << ", "
        << readBufferSize;

    for (ResourceIDType resourceID : AllEnums<ResourceIDType>())
    {
        out << ", kMax" << GetResourceIDTypeName(resourceID);
    }

    out << ");\n";
}

// TODO (http://anglebug.com/4599): Reset more state on frame loop
void MaybeResetResources(std::stringstream &out,
                         ResourceIDType resourceIDType,
                         DataTracker *dataTracker,
                         std::stringstream &header,
                         ResourceTracker *resourceTracker,
                         std::vector<uint8_t> *binaryData)
{
    switch (resourceIDType)
    {
        case ResourceIDType::Buffer:
        {
            TrackedResource &trackedBuffers =
                resourceTracker->getTrackedResource(ResourceIDType::Buffer);
            ResourceSet &newBuffers           = trackedBuffers.getNewResources();
            ResourceCalls &bufferRegenCalls   = trackedBuffers.getResourceRegenCalls();
            ResourceCalls &bufferRestoreCalls = trackedBuffers.getResourceRestoreCalls();

            BufferCalls &bufferMapCalls   = resourceTracker->getBufferMapCalls();
            BufferCalls &bufferUnmapCalls = resourceTracker->getBufferUnmapCalls();

            // If we have any new buffers generated and not deleted during the run, delete them now
            if (!newBuffers.empty())
            {
                out << "    const GLuint deleteBuffers[] = {";
                ResourceSet::iterator bufferIter = newBuffers.begin();
                for (size_t i = 0; bufferIter != newBuffers.end(); ++i, ++bufferIter)
                {
                    if (i > 0)
                    {
                        out << ", ";
                    }
                    if ((i % 4) == 0)
                    {
                        out << "\n        ";
                    }
                    out << "gBufferMap[" << *bufferIter << "]";
                }
                out << "};\n";
                out << "    glDeleteBuffers(" << newBuffers.size() << ", deleteBuffers);\n";
            }

            // If any of our starting buffers were deleted during the run, recreate them
            ResourceSet &buffersToRegen = trackedBuffers.getResourcesToRegen();
            for (GLuint id : buffersToRegen)
            {
                // Emit their regen calls
                for (CallCapture &call : bufferRegenCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                    out << ";\n";
                }
            }

            // If any of our starting buffers were modified during the run, restore their contents
            ResourceSet &buffersToRestore = trackedBuffers.getResourcesToRestore();
            for (GLuint id : buffersToRestore)
            {
                if (resourceTracker->getStartingBuffersMappedCurrent(id))
                {
                    // Some drivers require the buffer to be unmapped before you can update data,
                    // which violates the spec. See gl::Buffer::bufferDataImpl().
                    for (CallCapture &call : bufferUnmapCalls[id])
                    {
                        out << "    ";
                        WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                        out << ";\n";
                    }
                }

                // Emit their restore calls
                for (CallCapture &call : bufferRestoreCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                    out << ";\n";

                    // Also note that this buffer has been implicitly unmapped by this call
                    resourceTracker->setBufferUnmapped(id);
                }
            }

            // Update the map/unmap of buffers to match the starting state
            ResourceSet startingBuffers = trackedBuffers.getStartingResources();
            for (GLuint id : startingBuffers)
            {
                // If the buffer was mapped at the start, but is not mapped now, we need to map
                if (resourceTracker->getStartingBuffersMappedInitial(id) &&
                    !resourceTracker->getStartingBuffersMappedCurrent(id))
                {
                    // Emit their map calls
                    for (CallCapture &call : bufferMapCalls[id])
                    {
                        out << "    ";
                        WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                        out << ";\n";
                    }
                }
                // If the buffer was unmapped at the start, but is mapped now, we need to unmap
                if (!resourceTracker->getStartingBuffersMappedInitial(id) &&
                    resourceTracker->getStartingBuffersMappedCurrent(id))
                {
                    // Emit their unmap calls
                    for (CallCapture &call : bufferUnmapCalls[id])
                    {
                        out << "    ";
                        WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                        out << ";\n";
                    }
                }
            }

            // Restore buffer bindings as seen during MEC
            std::vector<CallCapture> &bufferBindingCalls = resourceTracker->getBufferBindingCalls();
            for (CallCapture &call : bufferBindingCalls)
            {
                out << "    ";
                WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                out << ";\n";
            }

            break;
        }
        case ResourceIDType::ShaderProgram:
        {
            ResourceSet &newPrograms =
                resourceTracker->getTrackedResource(ResourceIDType::ShaderProgram)
                    .getNewResources();

            // If we have any new programs created and not deleted during the run, delete them now
            for (const auto &newProgram : newPrograms)
            {
                out << "    glDeleteProgram(gShaderProgramMap2[" << newProgram << "]);\n";
            }

            // TODO (http://anglebug.com/5968): Handle programs that need regen
            // This would only happen if a starting program was deleted during the run
            ASSERT(resourceTracker->getTrackedResource(ResourceIDType::ShaderProgram)
                       .getResourcesToRegen()
                       .empty());
            break;
        }
        case ResourceIDType::Texture:
        {
            TrackedResource &trackedTextures =
                resourceTracker->getTrackedResource(ResourceIDType::Texture);
            ResourceSet &newTextures           = trackedTextures.getNewResources();
            ResourceCalls &textureRegenCalls   = trackedTextures.getResourceRegenCalls();
            ResourceCalls &textureRestoreCalls = trackedTextures.getResourceRestoreCalls();

            // If we have any new textures generated and not deleted during the run, delete them now
            if (!newTextures.empty())
            {
                out << "    const GLuint deleteTextures[] = {";
                ResourceSet::iterator textureIter = newTextures.begin();
                for (size_t i = 0; textureIter != newTextures.end(); ++i, ++textureIter)
                {
                    if (i > 0)
                    {
                        out << ", ";
                    }
                    if ((i % 4) == 0)
                    {
                        out << "\n        ";
                    }
                    out << "gTextureMap[" << *textureIter << "]";
                }
                out << "};\n";
                out << "    glDeleteTextures(" << newTextures.size() << ", deleteTextures);\n";
            }

            // If any of our starting textures were deleted during the run, regen them
            ResourceSet &texturesToRegen = trackedTextures.getResourcesToRegen();
            for (GLuint id : texturesToRegen)
            {
                // Emit their regen calls
                for (CallCapture &call : textureRegenCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                    out << ";\n";
                }
            }

            // If any of our starting textures were modified during the run, restore their contents
            ResourceSet &texturesToRestore = trackedTextures.getResourcesToRestore();
            for (GLuint id : texturesToRestore)
            {
                // Emit their restore calls
                for (CallCapture &call : textureRestoreCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
                    out << ";\n";
                }
            }
            break;
        }
        default:
            // TODO (http://anglebug.com/4599): Reset more than just buffers
            break;
    }
}

void MaybeResetFenceSyncObjects(std::stringstream &out,
                                DataTracker *dataTracker,
                                std::stringstream &header,
                                ResourceTracker *resourceTracker,
                                std::vector<uint8_t> *binaryData)
{
    FenceSyncCalls &fenceSyncRegenCalls = resourceTracker->getFenceSyncRegenCalls();

    // If any of our starting fence sync objects were deleted during the run, recreate them
    FenceSyncSet &fenceSyncsToRegen = resourceTracker->getFenceSyncsToRegen();
    for (const GLsync sync : fenceSyncsToRegen)
    {
        // Emit their regen calls
        for (CallCapture &call : fenceSyncRegenCalls[sync])
        {
            out << "    ";
            WriteCppReplayForCall(call, dataTracker, out, header, binaryData);
            out << ";\n";
        }
    }
}

void MaybeResetOpaqueTypeObjects(std::stringstream &out,
                                 DataTracker *dataTracker,
                                 std::stringstream &header,
                                 ResourceTracker *resourceTracker,
                                 std::vector<uint8_t> *binaryData)
{
    MaybeResetFenceSyncObjects(out, dataTracker, header, resourceTracker, binaryData);
}

bool FindShaderProgramIDInCall(const CallCapture &call, gl::ShaderProgramID *idOut)
{
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        // Only checking for programs right now, but could be expanded to all ResourceTypes
        if (param.type == ParamType::TShaderProgramID)
        {
            *idOut = param.value.ShaderProgramIDVal;
            return true;
        }
    }

    return false;
}

void MarkResourceIDActive(ResourceIDType resourceType,
                          GLuint id,
                          std::vector<CallCapture> *setupCalls,
                          const ResourceIDToSetupCallsMap *resourceIDToSetupCallsMap)
{
    const std::map<GLuint, gl::Range<size_t>> &resourceSetupCalls =
        (*resourceIDToSetupCallsMap)[resourceType];
    const auto iter = resourceSetupCalls.find(id);
    if (iter == resourceSetupCalls.end())
    {
        return;
    }

    // Mark all of the calls that were used to initialize this resource as ACTIVE
    const gl::Range<size_t> &calls = iter->second;
    for (size_t index : calls)
    {
        (*setupCalls)[index].isActive = true;
    }
}

void WriteCppReplayFunctionWithParts(const gl::ContextID contextID,
                                     ReplayFunc replayFunc,
                                     DataTracker *dataTracker,
                                     uint32_t frameIndex,
                                     std::vector<uint8_t> *binaryData,
                                     const std::vector<CallCapture> &calls,
                                     std::stringstream &header,
                                     std::stringstream &callStream,
                                     std::stringstream &out)
{
    std::stringstream callStreamParts;

    int callCount = 0;
    int partCount = 0;

    // Setup can get quite large. If over a certain size, break up the function to avoid
    // overflowing the stack
    if (calls.size() > kFunctionSizeLimit)
    {
        callStreamParts << "void " << FmtFunction(replayFunc, contextID, frameIndex, ++partCount)
                        << "\n";
        callStreamParts << "{\n";
    }

    for (const CallCapture &call : calls)
    {
        if (!call.isActive)
        {
            // Don't write setup calls for inactive resources
            continue;
        }

        callStreamParts << "    ";
        WriteCppReplayForCall(call, dataTracker, callStreamParts, header, binaryData);
        callStreamParts << ";\n";

        if (partCount > 0 && ++callCount % kFunctionSizeLimit == 0)
        {
            callStreamParts << "}\n";
            callStreamParts << "\n";
            callStreamParts << "void "
                            << FmtFunction(replayFunc, contextID, frameIndex, ++partCount) << "\n";
            callStreamParts << "{\n";
        }
    }

    if (partCount > 0)
    {
        callStreamParts << "}\n";
        callStreamParts << "\n";

        // Write out the parts
        out << callStreamParts.str();

        // Write out the calls to the parts
        for (int i = 1; i <= partCount; i++)
        {
            callStream << "    " << FmtFunction(replayFunc, contextID, frameIndex, i) << ";\n";
        }
    }
    else
    {
        // If we didn't chunk it up, write all the calls directly to SetupContext
        callStream << callStreamParts.str();
    }
}

// Auxiliary contexts are other contexts in the share group that aren't the context calling
// eglSwapBuffers().
void WriteAuxiliaryContextCppSetupReplay(bool compression,
                                         const std::string &outDir,
                                         const gl::Context *context,
                                         const std::string &captureLabel,
                                         uint32_t frameIndex,
                                         const std::vector<CallCapture> &setupCalls,
                                         std::vector<uint8_t> *binaryData,
                                         bool serializeStateEnabled,
                                         const FrameCaptureShared &frameCaptureShared)
{
    ASSERT(frameCaptureShared.getWindowSurfaceContextID() != context->id());

    DataTracker dataTracker;

    std::stringstream out;
    std::stringstream include;
    std::stringstream header;

    include << "#include \""
            << FmtCapturePrefix(frameCaptureShared.getWindowSurfaceContextID(), captureLabel)
            << ".h\"\n";
    include << "#include \"angle_trace_gl.h\"\n";
    include << "";
    include << "\n";
    include << "namespace\n";
    include << "{\n";

    if (!captureLabel.empty())
    {
        header << "namespace " << captureLabel << "\n";
        header << "{\n";
        out << "namespace " << captureLabel << "\n";
        out << "{\n";
    }

    std::stringstream setupCallStream;

    header << "void " << FmtSetupFunction(kNoPartId, context->id()) << ";\n";
    setupCallStream << "void " << FmtSetupFunction(kNoPartId, context->id()) << "\n";
    setupCallStream << "{\n";

    WriteCppReplayFunctionWithParts(context->id(), ReplayFunc::Setup, &dataTracker, frameIndex,
                                    binaryData, setupCalls, include, setupCallStream, out);

    out << setupCallStream.str();
    out << "}\n";
    out << "\n";

    if (!captureLabel.empty())
    {
        header << "} // namespace " << captureLabel << "\n";
        out << "} // namespace " << captureLabel << "\n";
    }

    include << "}  // namespace\n";

    // Write out the source file.
    {
        std::string outString    = out.str();
        std::string headerString = include.str();

        std::string cppFilePath =
            GetCaptureFilePath(outDir, context->id(), captureLabel, frameIndex, ".cpp");

        SaveFileHelper saveCpp(cppFilePath);
        saveCpp << headerString << "\n" << outString;
    }

    // Write out the header file.
    {
        std::string headerContents = header.str();

        std::stringstream headerPathStream;
        headerPathStream << outDir << FmtCapturePrefix(context->id(), captureLabel) << ".h";
        std::string headerPath = headerPathStream.str();

        SaveFileHelper saveHeader(headerPath);
        saveHeader << headerContents;
    }
}

void WriteShareGroupCppSetupReplay(bool compression,
                                   const std::string &outDir,
                                   const std::string &captureLabel,
                                   uint32_t frameIndex,
                                   uint32_t frameCount,
                                   const std::vector<CallCapture> &setupCalls,
                                   ResourceTracker *resourceTracker,
                                   std::vector<uint8_t> *binaryData,
                                   bool serializeStateEnabled,
                                   const FrameCaptureShared &frameCaptureShared)
{
    DataTracker dataTracker;

    std::stringstream out;
    std::stringstream include;

    include << "#include \"angle_trace_gl.h\"\n";
    include << "\n";
    include << "namespace\n";
    include << "{\n";

    if (!captureLabel.empty())
    {
        out << "namespace " << captureLabel << "\n";
        out << "{\n";
    }

    std::stringstream setupCallStream;

    setupCallStream << "void " << FmtSetupFunction(kNoPartId, kSharedContextId) << "\n";
    setupCallStream << "{\n";

    WriteCppReplayFunctionWithParts(kSharedContextId, ReplayFunc::Setup, &dataTracker, frameIndex,
                                    binaryData, setupCalls, include, setupCallStream, out);

    out << setupCallStream.str();
    out << "}\n";
    out << "\n";

    if (!captureLabel.empty())
    {
        out << "} // namespace " << captureLabel << "\n";
    }

    include << "}  // namespace\n";

    // Write out the source file.
    {
        std::string outString    = out.str();
        std::string headerString = include.str();

        std::string cppFilePath =
            GetCaptureFilePath(outDir, kSharedContextId, captureLabel, frameIndex, ".cpp");

        SaveFileHelper saveCpp(cppFilePath);
        saveCpp << headerString << "\n" << outString;
    }
}

ProgramSources GetAttachedProgramSources(const gl::Program *program)
{
    ProgramSources sources;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const gl::Shader *shader = program->getAttachedShader(shaderType);
        if (shader)
        {
            sources[shaderType] = shader->getSourceString();
        }
    }
    return sources;
}

template <typename IDType>
void CaptureUpdateResourceIDs(const CallCapture &call,
                              const ParamCapture &param,
                              std::vector<CallCapture> *callsOut)
{
    GLsizei n = call.params.getParamFlexName("n", "count", ParamType::TGLsizei, 0).value.GLsizeiVal;
    ASSERT(param.data.size() == 1);
    ResourceIDType resourceIDType = GetResourceIDTypeFromParamType(param.type);
    ASSERT(resourceIDType != ResourceIDType::InvalidEnum &&
           resourceIDType != ResourceIDType::ShaderProgram);
    const char *resourceName = GetResourceIDTypeName(resourceIDType);

    std::stringstream updateFuncNameStr;
    updateFuncNameStr << "Update" << resourceName << "ID2";
    std::string updateFuncName = updateFuncNameStr.str();

    const IDType *returnedIDs = reinterpret_cast<const IDType *>(param.data[0].data());

    for (GLsizei idIndex = 0; idIndex < n; ++idIndex)
    {
        IDType id                = returnedIDs[idIndex];
        GLsizei readBufferOffset = idIndex * sizeof(gl::RenderbufferID);
        ParamBuffer params;
        params.addValueParam("id", ParamType::TGLuint, id.value);
        params.addValueParam("readBufferOffset", ParamType::TGLsizei, readBufferOffset);
        callsOut->emplace_back(updateFuncName, std::move(params));
    }
}

void CaptureUpdateUniformLocations(const gl::Program *program, std::vector<CallCapture> *callsOut)
{
    const std::vector<gl::LinkedUniform> &uniforms     = program->getState().getUniforms();
    const std::vector<gl::VariableLocation> &locations = program->getUniformLocations();

    for (GLint location = 0; location < static_cast<GLint>(locations.size()); ++location)
    {
        const gl::VariableLocation &locationVar = locations[location];

        // This handles the case where the application calls glBindUniformLocationCHROMIUM
        // on an unused uniform. We must still store a -1 into gUniformLocations in case the
        // application attempts to call a glUniform* call. To do this we'll pass in a blank name to
        // force glGetUniformLocation to return -1.
        std::string name;
        int count = 1;
        ParamBuffer params;
        params.addValueParam("program", ParamType::TShaderProgramID, program->id());

        if (locationVar.index >= uniforms.size())
        {
            name = "";
        }
        else
        {
            const gl::LinkedUniform &uniform = uniforms[locationVar.index];

            name = uniform.name;

            if (uniform.isArray())
            {
                if (locationVar.arrayIndex > 0)
                {
                    // Non-sequential array uniform locations are not currently handled.
                    // In practice array locations shouldn't ever be non-sequential.
                    ASSERT(uniform.location == -1 ||
                           location == uniform.location + static_cast<int>(locationVar.arrayIndex));
                    continue;
                }

                if (uniform.isArrayOfArrays())
                {
                    UNIMPLEMENTED();
                }

                name  = gl::StripLastArrayIndex(name);
                count = uniform.arraySizes[0];
            }
        }

        ParamCapture nameParam("name", ParamType::TGLcharConstPointer);
        CaptureString(name.c_str(), &nameParam);
        params.addParam(std::move(nameParam));
        params.addValueParam("location", ParamType::TGLint, location);
        params.addValueParam("count", ParamType::TGLint, static_cast<GLint>(count));
        callsOut->emplace_back("UpdateUniformLocation2", std::move(params));
    }
}

void CaptureValidateSerializedState(const gl::Context *context, std::vector<CallCapture> *callsOut)
{
    INFO() << "Capturing validation checkpoint at position " << callsOut->size();

    context->finishImmutable();

    std::string serializedState;
    angle::Result result = angle::SerializeContextToString(context, &serializedState);
    if (result != angle::Result::Continue)
    {
        ERR() << "Internal error serializing context state.";
        return;
    }
    ParamCapture serializedStateParam("serializedState", ParamType::TGLcharConstPointer);
    CaptureString(serializedState.c_str(), &serializedStateParam);

    ParamBuffer params;
    params.addParam(std::move(serializedStateParam));

    callsOut->emplace_back("VALIDATE_CHECKPOINT", std::move(params));
}

void CaptureUpdateUniformBlockIndexes(const gl::Program *program,
                                      std::vector<CallCapture> *callsOut)
{
    const std::vector<gl::InterfaceBlock> &uniformBlocks = program->getState().getUniformBlocks();

    for (GLuint index = 0; index < uniformBlocks.size(); ++index)
    {
        ParamBuffer params;

        std::string name;
        params.addValueParam("program", ParamType::TShaderProgramID, program->id());

        ParamCapture nameParam("name", ParamType::TGLcharConstPointer);
        CaptureString(uniformBlocks[index].name.c_str(), &nameParam);
        params.addParam(std::move(nameParam));

        params.addValueParam("index", ParamType::TGLuint, index);
        callsOut->emplace_back("UpdateUniformBlockIndex", std::move(params));
    }
}

void CaptureDeleteUniformLocations(gl::ShaderProgramID program, std::vector<CallCapture> *callsOut)
{
    ParamBuffer params;
    params.addValueParam("program", ParamType::TShaderProgramID, program);
    callsOut->emplace_back("DeleteUniformLocations2", std::move(params));
}

void MaybeCaptureUpdateResourceIDs(std::vector<CallCapture> *callsOut)
{
    const CallCapture &call = callsOut->back();

    switch (call.entryPoint)
    {
        case EntryPoint::GLGenBuffers:
        {
            const ParamCapture &buffers =
                call.params.getParam("buffersPacked", ParamType::TBufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::BufferID>(call, buffers, callsOut);
            break;
        }

        case EntryPoint::GLGenFencesNV:
        {
            const ParamCapture &fences =
                call.params.getParam("fencesPacked", ParamType::TFenceNVIDPointer, 1);
            CaptureUpdateResourceIDs<gl::FenceNVID>(call, fences, callsOut);
            break;
        }

        case EntryPoint::GLGenFramebuffers:
        case EntryPoint::GLGenFramebuffersOES:
        {
            const ParamCapture &framebuffers =
                call.params.getParam("framebuffersPacked", ParamType::TFramebufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::FramebufferID>(call, framebuffers, callsOut);
            break;
        }

        case EntryPoint::GLGenProgramPipelines:
        {
            const ParamCapture &pipelines =
                call.params.getParam("pipelinesPacked", ParamType::TProgramPipelineIDPointer, 1);
            CaptureUpdateResourceIDs<gl::ProgramPipelineID>(call, pipelines, callsOut);
            break;
        }

        case EntryPoint::GLGenQueries:
        case EntryPoint::GLGenQueriesEXT:
        {
            const ParamCapture &queries =
                call.params.getParam("idsPacked", ParamType::TQueryIDPointer, 1);
            CaptureUpdateResourceIDs<gl::QueryID>(call, queries, callsOut);
            break;
        }

        case EntryPoint::GLGenRenderbuffers:
        case EntryPoint::GLGenRenderbuffersOES:
        {
            const ParamCapture &renderbuffers =
                call.params.getParam("renderbuffersPacked", ParamType::TRenderbufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::RenderbufferID>(call, renderbuffers, callsOut);
            break;
        }

        case EntryPoint::GLGenSamplers:
        {
            const ParamCapture &samplers =
                call.params.getParam("samplersPacked", ParamType::TSamplerIDPointer, 1);
            CaptureUpdateResourceIDs<gl::SamplerID>(call, samplers, callsOut);
            break;
        }

        case EntryPoint::GLGenSemaphoresEXT:
        {
            const ParamCapture &semaphores =
                call.params.getParam("semaphoresPacked", ParamType::TSemaphoreIDPointer, 1);
            CaptureUpdateResourceIDs<gl::SemaphoreID>(call, semaphores, callsOut);
            break;
        }

        case EntryPoint::GLGenTextures:
        {
            const ParamCapture &textures =
                call.params.getParam("texturesPacked", ParamType::TTextureIDPointer, 1);
            CaptureUpdateResourceIDs<gl::TextureID>(call, textures, callsOut);
            break;
        }

        case EntryPoint::GLGenTransformFeedbacks:
        {
            const ParamCapture &xfbs =
                call.params.getParam("idsPacked", ParamType::TTransformFeedbackIDPointer, 1);
            CaptureUpdateResourceIDs<gl::TransformFeedbackID>(call, xfbs, callsOut);
            break;
        }

        case EntryPoint::GLGenVertexArrays:
        case EntryPoint::GLGenVertexArraysOES:
        {
            const ParamCapture &vertexArrays =
                call.params.getParam("arraysPacked", ParamType::TVertexArrayIDPointer, 1);
            CaptureUpdateResourceIDs<gl::VertexArrayID>(call, vertexArrays, callsOut);
            break;
        }

        case EntryPoint::GLCreateMemoryObjectsEXT:
        {
            const ParamCapture &memoryObjects =
                call.params.getParam("memoryObjectsPacked", ParamType::TMemoryObjectIDPointer, 1);
            CaptureUpdateResourceIDs<gl::MemoryObjectID>(call, memoryObjects, callsOut);
            break;
        }

        default:
            break;
    }
}

void CaptureUpdateCurrentProgram(const CallCapture &call, std::vector<CallCapture> *callsOut)
{
    const ParamCapture &param =
        call.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
    gl::ShaderProgramID programID = param.value.ShaderProgramIDVal;

    ParamBuffer paramBuffer;
    paramBuffer.addValueParam("program", ParamType::TShaderProgramID, programID);

    callsOut->emplace_back("UpdateCurrentProgram", std::move(paramBuffer));
}

bool IsDefaultCurrentValue(const gl::VertexAttribCurrentValueData &currentValue)
{
    if (currentValue.Type != gl::VertexAttribType::Float)
        return false;

    return currentValue.Values.FloatValues[0] == 0.0f &&
           currentValue.Values.FloatValues[1] == 0.0f &&
           currentValue.Values.FloatValues[2] == 0.0f && currentValue.Values.FloatValues[3] == 1.0f;
}

bool IsQueryActive(const gl::State &glState, gl::QueryID &queryID)
{
    const gl::ActiveQueryMap &activeQueries = glState.getActiveQueriesForCapture();
    for (const auto &activeQueryIter : activeQueries)
    {
        const gl::Query *activeQuery = activeQueryIter.get();
        if (activeQuery && activeQuery->id() == queryID)
        {
            return true;
        }
    }

    return false;
}

bool IsTextureUpdate(CallCapture &call)
{
    switch (call.entryPoint)
    {
        case EntryPoint::GLCompressedCopyTextureCHROMIUM:
        case EntryPoint::GLCompressedTexImage1D:
        case EntryPoint::GLCompressedTexImage2D:
        case EntryPoint::GLCompressedTexImage2DRobustANGLE:
        case EntryPoint::GLCompressedTexImage3D:
        case EntryPoint::GLCompressedTexImage3DOES:
        case EntryPoint::GLCompressedTexImage3DRobustANGLE:
        case EntryPoint::GLCompressedTexSubImage1D:
        case EntryPoint::GLCompressedTexSubImage2D:
        case EntryPoint::GLCompressedTexSubImage2DRobustANGLE:
        case EntryPoint::GLCompressedTexSubImage3D:
        case EntryPoint::GLCompressedTexSubImage3DOES:
        case EntryPoint::GLCompressedTexSubImage3DRobustANGLE:
        case EntryPoint::GLCompressedTextureSubImage1D:
        case EntryPoint::GLCompressedTextureSubImage2D:
        case EntryPoint::GLCompressedTextureSubImage3D:
        case EntryPoint::GLCopyTexImage1D:
        case EntryPoint::GLCopyTexImage2D:
        case EntryPoint::GLCopyTexSubImage1D:
        case EntryPoint::GLCopyTexSubImage2D:
        case EntryPoint::GLCopyTexSubImage3D:
        case EntryPoint::GLCopyTexSubImage3DOES:
        case EntryPoint::GLCopyTexture3DANGLE:
        case EntryPoint::GLCopyTextureCHROMIUM:
        case EntryPoint::GLCopyTextureSubImage1D:
        case EntryPoint::GLCopyTextureSubImage2D:
        case EntryPoint::GLCopyTextureSubImage3D:
        case EntryPoint::GLTexImage1D:
        case EntryPoint::GLTexImage2D:
        case EntryPoint::GLTexImage2DExternalANGLE:
        case EntryPoint::GLTexImage2DMultisample:
        case EntryPoint::GLTexImage2DRobustANGLE:
        case EntryPoint::GLTexImage3D:
        case EntryPoint::GLTexImage3DMultisample:
        case EntryPoint::GLTexImage3DOES:
        case EntryPoint::GLTexImage3DRobustANGLE:
        case EntryPoint::GLTexSubImage1D:
        case EntryPoint::GLTexSubImage2D:
        case EntryPoint::GLTexSubImage2DRobustANGLE:
        case EntryPoint::GLTexSubImage3D:
        case EntryPoint::GLTexSubImage3DOES:
        case EntryPoint::GLTexSubImage3DRobustANGLE:
        case EntryPoint::GLTextureSubImage1D:
        case EntryPoint::GLTextureSubImage2D:
        case EntryPoint::GLTextureSubImage3D:
            return true;

        // Note: CopyImageSubData is handled specially in copyCompressedTextureData
        case EntryPoint::GLCopyImageSubData:
        case EntryPoint::GLCopyImageSubDataEXT:
        case EntryPoint::GLCopyImageSubDataOES:
            return false;

        default:
            return false;
    }
}

void Capture(std::vector<CallCapture> *setupCalls, CallCapture &&call)
{
    setupCalls->emplace_back(std::move(call));
}

void CaptureFramebufferAttachment(std::vector<CallCapture> *setupCalls,
                                  const gl::State &replayState,
                                  const FramebufferCaptureFuncs &framebufferFuncs,
                                  const gl::FramebufferAttachment &attachment)
{
    GLuint resourceID = attachment.getResource()->getId();

    // TODO(jmadill): Layer attachments. http://anglebug.com/3662
    if (attachment.type() == GL_TEXTURE)
    {
        gl::ImageIndex index = attachment.getTextureImageIndex();

        Capture(setupCalls, framebufferFuncs.framebufferTexture2D(
                                replayState, true, GL_FRAMEBUFFER, attachment.getBinding(),
                                index.getTarget(), {resourceID}, index.getLevelIndex()));
    }
    else
    {
        ASSERT(attachment.type() == GL_RENDERBUFFER);
        Capture(setupCalls, framebufferFuncs.framebufferRenderbuffer(
                                replayState, true, GL_FRAMEBUFFER, attachment.getBinding(),
                                GL_RENDERBUFFER, {resourceID}));
    }
}

void CaptureUpdateUniformValues(const gl::State &replayState,
                                const gl::Context *context,
                                gl::Program *program,
                                std::vector<CallCapture> *callsOut)
{
    if (!program->isLinked())
    {
        // We can't populate uniforms if the program hasn't been linked
        return;
    }

    // We need to bind the program and update its uniforms
    if (!replayState.getProgram() || replayState.getProgram()->id() != program->id())
    {
        Capture(callsOut, CaptureUseProgram(replayState, true, program->id()));
        CaptureUpdateCurrentProgram(callsOut->back(), callsOut);
    }

    const std::vector<gl::LinkedUniform> &uniforms = program->getState().getUniforms();

    for (const gl::LinkedUniform &uniform : uniforms)
    {
        std::string uniformName = uniform.name;

        int uniformCount = 1;
        if (uniform.isArray())
        {
            if (uniform.isArrayOfArrays())
            {
                UNIMPLEMENTED();
                continue;
            }

            uniformCount = uniform.arraySizes[0];
            uniformName  = gl::StripLastArrayIndex(uniformName);
        }

        gl::UniformLocation uniformLoc      = program->getUniformLocation(uniformName);
        const gl::UniformTypeInfo *typeInfo = uniform.typeInfo;
        int componentCount                  = typeInfo->componentCount;
        int uniformSize                     = uniformCount * componentCount;

        // For arrayed uniforms, we'll need to increment a read location
        gl::UniformLocation readLoc = uniformLoc;

        // If the uniform is unused, just continue
        if (readLoc.value == -1)
        {
            continue;
        }

        // Image uniforms are special and cannot be set this way
        if (typeInfo->isImageType)
        {
            continue;
        }

        // Samplers should be populated with GL_INT, regardless of return type
        if (typeInfo->isSampler)
        {
            std::vector<GLint> uniformBuffer(uniformSize);
            for (int index = 0; index < uniformCount; index++, readLoc.value++)
            {
                program->getUniformiv(context, readLoc,
                                      uniformBuffer.data() + index * componentCount);
            }

            Capture(callsOut, CaptureUniform1iv(replayState, true, uniformLoc, uniformCount,
                                                uniformBuffer.data()));

            continue;
        }

        switch (typeInfo->componentType)
        {
            case GL_FLOAT:
            {
                std::vector<GLfloat> uniformBuffer(uniformSize);
                for (int index = 0; index < uniformCount; index++, readLoc.value++)
                {
                    program->getUniformfv(context, readLoc,
                                          uniformBuffer.data() + index * componentCount);
                }
                switch (typeInfo->type)
                {
                    // Note: All matrix uniforms are populated without transpose
                    case GL_FLOAT_MAT4x3:
                        Capture(callsOut, CaptureUniformMatrix4x3fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT4x2:
                        Capture(callsOut, CaptureUniformMatrix4x2fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT4:
                        Capture(callsOut,
                                CaptureUniformMatrix4fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3x4:
                        Capture(callsOut, CaptureUniformMatrix3x4fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3x2:
                        Capture(callsOut, CaptureUniformMatrix3x2fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3:
                        Capture(callsOut,
                                CaptureUniformMatrix3fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2x4:
                        Capture(callsOut, CaptureUniformMatrix2x4fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2x3:
                        Capture(callsOut, CaptureUniformMatrix2x3fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2:
                        Capture(callsOut,
                                CaptureUniformMatrix2fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC4:
                        Capture(callsOut, CaptureUniform4fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC3:
                        Capture(callsOut, CaptureUniform3fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC2:
                        Capture(callsOut, CaptureUniform2fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT:
                        Capture(callsOut, CaptureUniform1fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            case GL_INT:
            {
                std::vector<GLint> uniformBuffer(uniformSize);
                for (int index = 0; index < uniformCount; index++, readLoc.value++)
                {
                    program->getUniformiv(context, readLoc,
                                          uniformBuffer.data() + index * componentCount);
                }
                switch (componentCount)
                {
                    case 4:
                        Capture(callsOut, CaptureUniform4iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 3:
                        Capture(callsOut, CaptureUniform3iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 2:
                        Capture(callsOut, CaptureUniform2iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 1:
                        Capture(callsOut, CaptureUniform1iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            case GL_BOOL:
            case GL_UNSIGNED_INT:
            {
                std::vector<GLuint> uniformBuffer(uniformSize);
                for (int index = 0; index < uniformCount; index++, readLoc.value++)
                {
                    program->getUniformuiv(context, readLoc,
                                           uniformBuffer.data() + index * componentCount);
                }
                switch (componentCount)
                {
                    case 4:
                        Capture(callsOut, CaptureUniform4uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 3:
                        Capture(callsOut, CaptureUniform3uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 2:
                        Capture(callsOut, CaptureUniform2uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 1:
                        Capture(callsOut, CaptureUniform1uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            default:
                UNIMPLEMENTED();
                break;
        }
    }
}

void CaptureVertexPointerES1(std::vector<CallCapture> *setupCalls,
                             gl::State *replayState,
                             GLuint attribIndex,
                             const gl::VertexAttribute &attrib,
                             const gl::VertexBinding &binding)
{
    switch (gl::GLES1Renderer::VertexArrayType(attribIndex))
    {
        case gl::ClientVertexArrayType::Vertex:
            Capture(setupCalls,
                    CaptureVertexPointer(*replayState, true, attrib.format->channelCount,
                                         attrib.format->vertexAttribType, binding.getStride(),
                                         attrib.pointer));
            break;
        case gl::ClientVertexArrayType::Normal:
            Capture(setupCalls,
                    CaptureNormalPointer(*replayState, true, attrib.format->vertexAttribType,
                                         binding.getStride(), attrib.pointer));
            break;
        case gl::ClientVertexArrayType::Color:
            Capture(setupCalls, CaptureColorPointer(*replayState, true, attrib.format->channelCount,
                                                    attrib.format->vertexAttribType,
                                                    binding.getStride(), attrib.pointer));
            break;
        case gl::ClientVertexArrayType::PointSize:
            Capture(setupCalls,
                    CapturePointSizePointerOES(*replayState, true, attrib.format->vertexAttribType,
                                               binding.getStride(), attrib.pointer));
            break;
        case gl::ClientVertexArrayType::TextureCoord:
            Capture(setupCalls,
                    CaptureTexCoordPointer(*replayState, true, attrib.format->channelCount,
                                           attrib.format->vertexAttribType, binding.getStride(),
                                           attrib.pointer));
            break;
        default:
            UNREACHABLE();
    }
}

bool VertexBindingMatchesAttribStride(const gl::VertexAttribute &attrib,
                                      const gl::VertexBinding &binding)
{
    if (attrib.vertexAttribArrayStride == 0 &&
        binding.getStride() == ComputeVertexAttributeTypeSize(attrib))
    {
        return true;
    }

    return attrib.vertexAttribArrayStride == binding.getStride();
}

void CaptureVertexArrayState(std::vector<CallCapture> *setupCalls,
                             const gl::Context *context,
                             const gl::VertexArray *vertexArray,
                             gl::State *replayState)
{
    const std::vector<gl::VertexAttribute> &vertexAttribs = vertexArray->getVertexAttributes();
    const std::vector<gl::VertexBinding> &vertexBindings  = vertexArray->getVertexBindings();

    gl::AttributesMask vertexPointerBindings;

    for (GLuint attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        const gl::VertexAttribute defaultAttrib(attribIndex);
        const gl::VertexBinding defaultBinding;

        const gl::VertexAttribute &attrib = vertexAttribs[attribIndex];
        const gl::VertexBinding &binding  = vertexBindings[attrib.bindingIndex];

        if (attrib.enabled != defaultAttrib.enabled)
        {
            if (context->isGLES1())
            {
                Capture(setupCalls,
                        CaptureEnableClientState(*replayState, false,
                                                 gl::GLES1Renderer::VertexArrayType(attribIndex)));
            }
            else
            {
                Capture(setupCalls,
                        CaptureEnableVertexAttribArray(*replayState, false, attribIndex));
            }
        }

        if (attrib.format != defaultAttrib.format || attrib.pointer != defaultAttrib.pointer ||
            binding.getStride() != defaultBinding.getStride() ||
            binding.getBuffer().get() != nullptr)
        {
            // Each attribute can pull from a separate buffer, so check the binding
            gl::Buffer *buffer = binding.getBuffer().get();
            if (buffer && buffer != replayState->getArrayBuffer())
            {
                replayState->setBufferBinding(context, gl::BufferBinding::Array, buffer);

                Capture(setupCalls, CaptureBindBuffer(*replayState, true, gl::BufferBinding::Array,
                                                      buffer->id()));
            }

            // Establish the relationship between currently bound buffer and the VAO
            if (context->isGLES1())
            {
                // Track indexes that used ES1 calls
                vertexPointerBindings.set(attribIndex);

                CaptureVertexPointerES1(setupCalls, replayState, attribIndex, attrib, binding);
            }
            else if (attrib.bindingIndex == attribIndex &&
                     VertexBindingMatchesAttribStride(attrib, binding) &&
                     (!buffer || binding.getOffset() == reinterpret_cast<GLintptr>(attrib.pointer)))
            {
                // Check if we can use strictly ES2 semantics, and track indexes that do.
                vertexPointerBindings.set(attribIndex);

                Capture(setupCalls,
                        CaptureVertexAttribPointer(
                            *replayState, true, attribIndex, attrib.format->channelCount,
                            attrib.format->vertexAttribType, attrib.format->isNorm(),
                            attrib.vertexAttribArrayStride, attrib.pointer));

                if (binding.getDivisor() != 0)
                {
                    Capture(setupCalls, CaptureVertexAttribDivisor(*replayState, true, attribIndex,
                                                                   binding.getDivisor()));
                }
            }
            else
            {
                ASSERT(context->getClientVersion() >= gl::ES_3_1);

                Capture(setupCalls,
                        CaptureVertexAttribFormat(*replayState, true, attribIndex,
                                                  attrib.format->channelCount,
                                                  attrib.format->vertexAttribType,
                                                  attrib.format->isNorm(), attrib.relativeOffset));
                Capture(setupCalls, CaptureVertexAttribBinding(*replayState, true, attribIndex,
                                                               attrib.bindingIndex));
            }
        }
    }

    // The loop below expects attribs and bindings to have equal counts
    static_assert(gl::MAX_VERTEX_ATTRIBS == gl::MAX_VERTEX_ATTRIB_BINDINGS,
                  "Max vertex attribs and bindings count mismatch");

    // Loop through binding indices that weren't used by VertexAttribPointer
    for (size_t bindingIndex : vertexPointerBindings.flip())
    {
        const gl::VertexBinding &binding = vertexBindings[bindingIndex];

        if (binding.getBuffer().id().value != 0)
        {
            Capture(setupCalls,
                    CaptureBindVertexBuffer(*replayState, true, static_cast<GLuint>(bindingIndex),
                                            binding.getBuffer().id(), binding.getOffset(),
                                            binding.getStride()));
        }

        if (binding.getDivisor() != 0)
        {
            Capture(setupCalls, CaptureVertexBindingDivisor(*replayState, true,
                                                            static_cast<GLuint>(bindingIndex),
                                                            binding.getDivisor()));
        }
    }

    // The element array buffer is not per attribute, but per VAO
    gl::Buffer *elementArrayBuffer = vertexArray->getElementArrayBuffer();
    if (elementArrayBuffer)
    {
        Capture(setupCalls, CaptureBindBuffer(*replayState, true, gl::BufferBinding::ElementArray,
                                              elementArrayBuffer->id()));
    }
}

void CaptureTextureStorage(std::vector<CallCapture> *setupCalls,
                           gl::State *replayState,
                           const gl::Texture *texture)
{
    // Use mip-level 0 for the base dimensions
    gl::ImageIndex imageIndex = gl::ImageIndex::MakeFromType(texture->getType(), 0);
    const gl::ImageDesc &desc = texture->getTextureState().getImageDesc(imageIndex);

    switch (texture->getType())
    {
        case gl::TextureType::_2D:
        case gl::TextureType::CubeMap:
        {
            Capture(setupCalls, CaptureTexStorage2D(*replayState, true, texture->getType(),
                                                    texture->getImmutableLevels(),
                                                    desc.format.info->internalFormat,
                                                    desc.size.width, desc.size.height));
            break;
        }
        case gl::TextureType::_3D:
        case gl::TextureType::_2DArray:
        case gl::TextureType::CubeMapArray:
        {
            Capture(setupCalls, CaptureTexStorage3D(
                                    *replayState, true, texture->getType(),
                                    texture->getImmutableLevels(), desc.format.info->internalFormat,
                                    desc.size.width, desc.size.height, desc.size.depth));
            break;
        }
        case gl::TextureType::Buffer:
        {
            // Do nothing. This will already be captured as a buffer.
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }
}

void CaptureTextureContents(std::vector<CallCapture> *setupCalls,
                            gl::State *replayState,
                            const gl::Texture *texture,
                            const gl::ImageIndex &index,
                            const gl::ImageDesc &desc,
                            GLuint size,
                            const void *data)
{
    const gl::InternalFormat &format = *desc.format.info;

    if (index.getType() == gl::TextureType::Buffer)
    {
        // Zero binding size indicates full buffer bound
        if (texture->getBuffer().getSize() == 0)
        {
            Capture(setupCalls,
                    CaptureTexBufferEXT(*replayState, true, index.getType(), format.internalFormat,
                                        texture->getBuffer().get()->id()));
        }
        else
        {
            Capture(setupCalls, CaptureTexBufferRangeEXT(*replayState, true, index.getType(),
                                                         format.internalFormat,
                                                         texture->getBuffer().get()->id(),
                                                         texture->getBuffer().getOffset(),
                                                         texture->getBuffer().getSize()));
        }

        // For buffers, we're done
        return;
    }

    bool is3D =
        (index.getType() == gl::TextureType::_3D || index.getType() == gl::TextureType::_2DArray ||
         index.getType() == gl::TextureType::CubeMapArray);

    if (format.compressed)
    {
        if (is3D)
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureCompressedTexSubImage3D(
                            *replayState, true, index.getTarget(), index.getLevelIndex(), 0, 0, 0,
                            desc.size.width, desc.size.height, desc.size.depth,
                            format.internalFormat, size, data));
            }
            else
            {
                Capture(setupCalls,
                        CaptureCompressedTexImage3D(*replayState, true, index.getTarget(),
                                                    index.getLevelIndex(), format.internalFormat,
                                                    desc.size.width, desc.size.height,
                                                    desc.size.depth, 0, size, data));
            }
        }
        else
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureCompressedTexSubImage2D(
                            *replayState, true, index.getTarget(), index.getLevelIndex(), 0, 0,
                            desc.size.width, desc.size.height, format.internalFormat, size, data));
            }
            else
            {
                Capture(setupCalls, CaptureCompressedTexImage2D(
                                        *replayState, true, index.getTarget(),
                                        index.getLevelIndex(), format.internalFormat,
                                        desc.size.width, desc.size.height, 0, size, data));
            }
        }
    }
    else
    {
        if (is3D)
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureTexSubImage3D(*replayState, true, index.getTarget(),
                                             index.getLevelIndex(), 0, 0, 0, desc.size.width,
                                             desc.size.height, desc.size.depth, format.format,
                                             format.type, data));
            }
            else
            {
                Capture(
                    setupCalls,
                    CaptureTexImage3D(*replayState, true, index.getTarget(), index.getLevelIndex(),
                                      format.internalFormat, desc.size.width, desc.size.height,
                                      desc.size.depth, 0, format.format, format.type, data));
            }
        }
        else
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureTexSubImage2D(*replayState, true, index.getTarget(),
                                             index.getLevelIndex(), 0, 0, desc.size.width,
                                             desc.size.height, format.format, format.type, data));
            }
            else
            {
                Capture(setupCalls, CaptureTexImage2D(*replayState, true, index.getTarget(),
                                                      index.getLevelIndex(), format.internalFormat,
                                                      desc.size.width, desc.size.height, 0,
                                                      format.format, format.type, data));
            }
        }
    }
}

void GenerateLinkedProgram(const gl::Context *context,
                           const gl::State &replayState,
                           ResourceTracker *resourceTracker,
                           std::vector<CallCapture> *setupCalls,
                           gl::Program *program,
                           gl::ShaderProgramID id,
                           const ProgramSources &linkedSources)
{
    // Bump up our max program ID before creating temp shaders
    resourceTracker->onShaderProgramAccess(id);

    // Use max ID as a temporary shader ID.
    // Note this isn't the real ID of the shader, just a lookup into the gShaderProgram map.
    gl::ShaderProgramID tempShaderID = {resourceTracker->getMaxShaderPrograms()};

    // Compile with last linked sources.
    for (gl::ShaderType shaderType : program->getExecutable().getLinkedShaderStages())
    {
        const std::string &sourceString = linkedSources[shaderType];
        const char *sourcePointer       = sourceString.c_str();

        if (sourceString.empty())
        {
            // If we don't have source for this shader, that means it was populated by the app
            // using glProgramBinary.  We need to look it up from our cached copy.
            const ProgramSources &cachedLinkedSources =
                context->getShareGroup()->getFrameCaptureShared()->getProgramSources(id);

            const std::string &cachedSourceString = cachedLinkedSources[shaderType];
            sourcePointer                         = cachedSourceString.c_str();
            ASSERT(!cachedSourceString.empty());
        }

        // Compile and attach the temporary shader. Then free it immediately.
        Capture(setupCalls, CaptureCreateShader(replayState, true, shaderType, tempShaderID.value));
        Capture(setupCalls,
                CaptureShaderSource(replayState, true, tempShaderID, 1, &sourcePointer, nullptr));
        Capture(setupCalls, CaptureCompileShader(replayState, true, tempShaderID));
        Capture(setupCalls, CaptureAttachShader(replayState, true, id, tempShaderID));
        Capture(setupCalls, CaptureDeleteShader(replayState, true, tempShaderID));
    }

    // Gather XFB varyings
    std::vector<std::string> xfbVaryings;
    for (const gl::TransformFeedbackVarying &xfbVarying :
         program->getState().getLinkedTransformFeedbackVaryings())
    {
        xfbVaryings.push_back(xfbVarying.nameWithArrayIndex());
    }

    if (!xfbVaryings.empty())
    {
        std::vector<const char *> varyingsStrings;
        for (const std::string &varyingString : xfbVaryings)
        {
            varyingsStrings.push_back(varyingString.data());
        }

        GLenum xfbMode = program->getState().getTransformFeedbackBufferMode();
        Capture(setupCalls, CaptureTransformFeedbackVaryings(replayState, true, id,
                                                             static_cast<GLint>(xfbVaryings.size()),
                                                             varyingsStrings.data(), xfbMode));
    }

    // Force the attributes to be bound the same way as in the existing program.
    // This can affect attributes that are optimized out in some implementations.
    for (const sh::ShaderVariable &attrib : program->getState().getProgramInputs())
    {
        if (gl::IsBuiltInName(attrib.name))
        {
            // Don't try to bind built-in attributes
            continue;
        }

        // Separable programs may not have a VS, meaning it may not have attributes.
        if (program->getExecutable().hasLinkedShaderStage(gl::ShaderType::Vertex))
        {
            ASSERT(attrib.location != -1);
            Capture(setupCalls, CaptureBindAttribLocation(replayState, true, id,
                                                          static_cast<GLuint>(attrib.location),
                                                          attrib.name.c_str()));
        }
    }

    if (program->isSeparable())
    {
        // MEC manually recreates separable programs, rather than attempting to recreate a call
        // to glCreateShaderProgramv(), so insert a call to mark it separable.
        Capture(setupCalls,
                CaptureProgramParameteri(replayState, true, id, GL_PROGRAM_SEPARABLE, GL_TRUE));
    }

    Capture(setupCalls, CaptureLinkProgram(replayState, true, id));
    CaptureUpdateUniformLocations(program, setupCalls);
    CaptureUpdateUniformValues(replayState, context, program, setupCalls);
    CaptureUpdateUniformBlockIndexes(program, setupCalls);

    // Capture uniform block bindings for each program
    for (unsigned int uniformBlockIndex = 0;
         uniformBlockIndex < program->getActiveUniformBlockCount(); uniformBlockIndex++)
    {
        GLuint blockBinding = program->getUniformBlockBinding(uniformBlockIndex);
        Capture(setupCalls, CaptureUniformBlockBinding(replayState, true, id, {uniformBlockIndex},
                                                       blockBinding));
    }
}

// TODO(http://anglebug.com/4599): Improve reset/restore call generation
// There are multiple ways to track reset calls for individual resources. For now, we are tracking
// separate lists of instructions that mirror the calls created during mid-execution setup. Other
// methods could involve passing the original CallCaptures to this function, or tracking the
// indices of original setup calls.
void CaptureBufferResetCalls(const gl::State &replayState,
                             ResourceTracker *resourceTracker,
                             gl::BufferID *id,
                             const gl::Buffer *buffer)
{
    GLuint bufferID = (*id).value;

    // Track this as a starting resource that may need to be restored.
    TrackedResource &trackedBuffers = resourceTracker->getTrackedResource(ResourceIDType::Buffer);
    ResourceSet &startingBuffers    = trackedBuffers.getStartingResources();
    startingBuffers.insert(bufferID);

    // Track calls to regenerate a given buffer
    ResourceCalls &bufferRegenCalls = trackedBuffers.getResourceRegenCalls();
    Capture(&bufferRegenCalls[bufferID], CaptureDeleteBuffers(replayState, true, 1, id));
    Capture(&bufferRegenCalls[bufferID], CaptureGenBuffers(replayState, true, 1, id));
    MaybeCaptureUpdateResourceIDs(&bufferRegenCalls[bufferID]);

    // Track calls to restore a given buffer's contents
    ResourceCalls &bufferRestoreCalls = trackedBuffers.getResourceRestoreCalls();
    Capture(&bufferRestoreCalls[bufferID],
            CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, *id));
    Capture(&bufferRestoreCalls[bufferID],
            CaptureBufferData(replayState, true, gl::BufferBinding::Array,
                              static_cast<GLsizeiptr>(buffer->getSize()), buffer->getMapPointer(),
                              buffer->getUsage()));

    if (buffer->isMapped())
    {
        // Track calls to remap a buffer that started as mapped
        BufferCalls &bufferMapCalls = resourceTracker->getBufferMapCalls();

        Capture(&bufferMapCalls[bufferID],
                CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, *id));

        void *dontCare = nullptr;
        Capture(&bufferMapCalls[bufferID],
                CaptureMapBufferRange(replayState, true, gl::BufferBinding::Array,
                                      static_cast<GLsizeiptr>(buffer->getMapOffset()),
                                      static_cast<GLsizeiptr>(buffer->getMapLength()),
                                      buffer->getAccessFlags(), dontCare));

        // Track the bufferID that was just mapped
        bufferMapCalls[bufferID].back().params.setMappedBufferID(buffer->id());
    }

    // Track calls unmap a buffer that started as unmapped
    BufferCalls &bufferUnmapCalls = resourceTracker->getBufferUnmapCalls();
    Capture(&bufferUnmapCalls[bufferID],
            CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, *id));
    Capture(&bufferUnmapCalls[bufferID],
            CaptureUnmapBuffer(replayState, true, gl::BufferBinding::Array, GL_TRUE));
}

void CaptureFenceSyncResetCalls(const gl::State &replayState,
                                ResourceTracker *resourceTracker,
                                GLsync syncID,
                                const gl::Sync *sync)
{
    // Track calls to regenerate a given fence sync
    FenceSyncCalls &fenceSyncRegenCalls = resourceTracker->getFenceSyncRegenCalls();
    Capture(&fenceSyncRegenCalls[syncID],
            CaptureFenceSync(replayState, true, sync->getCondition(), sync->getFlags(), syncID));
    MaybeCaptureUpdateResourceIDs(&fenceSyncRegenCalls[syncID]);
}

void CaptureBufferBindingResetCalls(const gl::State &replayState,
                                    ResourceTracker *resourceTracker,
                                    gl::BufferBinding binding,
                                    gl::BufferID id)
{
    std::vector<CallCapture> &bufferBindingCalls = resourceTracker->getBufferBindingCalls();
    Capture(&bufferBindingCalls, CaptureBindBuffer(replayState, true, binding, id));
}

void CaptureIndexedBuffers(const gl::State &glState,
                           const gl::BufferVector &indexedBuffers,
                           gl::BufferBinding binding,
                           std::vector<CallCapture> *setupCalls)
{
    for (unsigned int index = 0; index < indexedBuffers.size(); ++index)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &buffer = indexedBuffers[index];

        if (buffer.get() == nullptr)
        {
            continue;
        }

        GLintptr offset       = buffer.getOffset();
        GLsizeiptr size       = buffer.getSize();
        gl::BufferID bufferID = buffer.get()->id();

        // Context::bindBufferBase() calls Context::bindBufferRange() with size and offset = 0.
        if ((offset == 0) && (size == 0))
        {
            Capture(setupCalls, CaptureBindBufferBase(glState, true, binding, index, bufferID));
        }
        else
        {
            Capture(setupCalls,
                    CaptureBindBufferRange(glState, true, binding, index, bufferID, offset, size));
        }
    }
}

void CaptureDefaultVertexAttribs(const gl::State &replayState,
                                 const gl::State &apiState,
                                 std::vector<CallCapture> *setupCalls)
{
    const std::vector<gl::VertexAttribCurrentValueData> &currentValues =
        apiState.getVertexAttribCurrentValues();

    for (GLuint attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        const gl::VertexAttribCurrentValueData &defaultValue = currentValues[attribIndex];
        if (!IsDefaultCurrentValue(defaultValue))
        {
            Capture(setupCalls, CaptureVertexAttrib4fv(replayState, true, attribIndex,
                                                       defaultValue.Values.FloatValues));
        }
    }
}

// Capture the setup of the state that's shared by all of the contexts in the share group:
// OpenGL ES Version 3.2 (October 22, 2019)
// Chapter 5 Shared Objects and Multiple Contexts
//     Objects that can be shared between contexts include buffer objects, program
//   and shader objects, renderbuffer objects, sampler objects, sync objects, and texture
//   objects (except for the texture objects named zero).
//     Objects which contain references to other objects include framebuffer, program
//   pipeline, transform feedback, and vertex array objects. Such objects are called
//   container objects and are not shared.
void CaptureShareGroupMidExecutionSetup(const gl::Context *context,
                                        std::vector<CallCapture> *setupCalls,
                                        ResourceTracker *resourceTracker,
                                        gl::State &replayState)
{

    FrameCaptureShared *frameCaptureShared = context->getShareGroup()->getFrameCaptureShared();
    const gl::State &apiState              = context->getState();

    // Small helper function to make the code more readable.
    auto cap = [setupCalls](CallCapture &&call) { setupCalls->emplace_back(std::move(call)); };

    // Capture Buffer data.
    const gl::BufferManager &buffers = apiState.getBufferManagerForCapture();
    for (const auto &bufferIter : buffers)
    {
        gl::BufferID id    = {bufferIter.first};
        gl::Buffer *buffer = bufferIter.second;

        if (id.value == 0)
        {
            continue;
        }

        // glBufferData. Would possibly be better implemented using a getData impl method.
        // Saving buffers that are mapped during a swap is not yet handled.
        if (buffer->getSize() == 0)
        {
            continue;
        }

        // Remember if the buffer was already mapped
        GLboolean bufferMapped = buffer->isMapped();

        // If needed, map the buffer so we can capture its contents
        if (!bufferMapped)
        {
            (void)buffer->mapRange(context, 0, static_cast<GLsizeiptr>(buffer->getSize()),
                                   GL_MAP_READ_BIT);
        }

        // Generate binding.
        cap(CaptureGenBuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        // Always use the array buffer binding point to upload data to keep things simple.
        if (buffer != replayState.getArrayBuffer())
        {
            replayState.setBufferBinding(context, gl::BufferBinding::Array, buffer);
            cap(CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, id));
        }

        if (buffer->isImmutable())
        {
            cap(CaptureBufferStorageEXT(replayState, true, gl::BufferBinding::Array,
                                        static_cast<GLsizeiptr>(buffer->getSize()),
                                        buffer->getMapPointer(),
                                        buffer->getStorageExtUsageFlags()));
        }
        else
        {
            cap(CaptureBufferData(replayState, true, gl::BufferBinding::Array,
                                  static_cast<GLsizeiptr>(buffer->getSize()),
                                  buffer->getMapPointer(), buffer->getUsage()));
        }

        if (bufferMapped)
        {
            void *dontCare = nullptr;
            Capture(setupCalls,
                    CaptureMapBufferRange(replayState, true, gl::BufferBinding::Array,
                                          static_cast<GLsizeiptr>(buffer->getMapOffset()),
                                          static_cast<GLsizeiptr>(buffer->getMapLength()),
                                          buffer->getAccessFlags(), dontCare));

            resourceTracker->setStartingBufferMapped(buffer->id().value, true);

            frameCaptureShared->trackBufferMapping(
                &setupCalls->back(), buffer->id(), static_cast<GLsizeiptr>(buffer->getMapOffset()),
                static_cast<GLsizeiptr>(buffer->getMapLength()),
                (buffer->getAccessFlags() & GL_MAP_WRITE_BIT) != 0);
        }
        else
        {
            resourceTracker->setStartingBufferMapped(buffer->id().value, false);
        }

        // Generate the calls needed to restore this buffer to original state for frame looping
        CaptureBufferResetCalls(replayState, resourceTracker, &id, buffer);

        // Unmap the buffer if it wasn't already mapped
        if (!bufferMapped)
        {
            GLboolean dontCare;
            (void)buffer->unmap(context, &dontCare);
        }
    }

    // Set a unpack alignment of 1. Otherwise, computeRowPitch() will compute the wrong value,
    // leading to a crash in memcpy() when capturing the texture contents.
    gl::PixelUnpackState &currentUnpackState = replayState.getUnpackState();
    if (currentUnpackState.alignment != 1)
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ALIGNMENT, 1));
        currentUnpackState.alignment = 1;
    }

    // Capture Texture setup and data.
    const gl::TextureManager &textures = apiState.getTextureManagerForCapture();

    for (const auto &textureIter : textures)
    {
        gl::TextureID id     = {textureIter.first};
        gl::Texture *texture = textureIter.second;

        if (id.value == 0)
        {
            continue;
        }

        // Track this as a starting resource that may need to be restored.
        TrackedResource &trackedTextures =
            resourceTracker->getTrackedResource(ResourceIDType::Texture);
        ResourceSet &startingTextures = trackedTextures.getStartingResources();
        startingTextures.insert(id.value);

        // For the initial texture creation calls, track in the generate list
        ResourceCalls &textureRegenCalls = trackedTextures.getResourceRegenCalls();
        CallVector texGenCalls({setupCalls, &textureRegenCalls[id.value]});

        // For reset only, delete the texture before genning
        Capture(&textureRegenCalls[id.value], CaptureDeleteTextures(replayState, true, 1, &id));

        // Gen the Texture.
        for (std::vector<CallCapture> *calls : texGenCalls)
        {
            Capture(calls, CaptureGenTextures(replayState, true, 1, &id));
            MaybeCaptureUpdateResourceIDs(calls);
        }

        // For the remaining texture setup calls, track in the restore list
        ResourceCalls &textureRestoreCalls = trackedTextures.getResourceRestoreCalls();
        CallVector texSetupCalls({setupCalls, &textureRestoreCalls[id.value]});

        for (std::vector<CallCapture> *calls : texSetupCalls)
        {
            Capture(calls, CaptureBindTexture(replayState, true, texture->getType(), id));
        }
        replayState.setSamplerTexture(context, texture->getType(), texture);

        // Capture sampler parameter states.
        // TODO(jmadill): More sampler / texture states. http://anglebug.com/3662
        gl::SamplerState defaultSamplerState =
            gl::SamplerState::CreateDefaultForTarget(texture->getType());
        const gl::SamplerState &textureSamplerState = texture->getSamplerState();

        auto capTexParam = [&replayState, texture, &texSetupCalls](GLenum pname, GLint param) {
            for (std::vector<CallCapture> *calls : texSetupCalls)
            {
                Capture(calls,
                        CaptureTexParameteri(replayState, true, texture->getType(), pname, param));
            }
        };

        auto capTexParamf = [&replayState, texture, &texSetupCalls](GLenum pname, GLfloat param) {
            for (std::vector<CallCapture> *calls : texSetupCalls)
            {
                Capture(calls,
                        CaptureTexParameterf(replayState, true, texture->getType(), pname, param));
            }
        };

        if (textureSamplerState.getMinFilter() != defaultSamplerState.getMinFilter())
        {
            capTexParam(GL_TEXTURE_MIN_FILTER, textureSamplerState.getMinFilter());
        }

        if (textureSamplerState.getMagFilter() != defaultSamplerState.getMagFilter())
        {
            capTexParam(GL_TEXTURE_MAG_FILTER, textureSamplerState.getMagFilter());
        }

        if (textureSamplerState.getWrapR() != defaultSamplerState.getWrapR())
        {
            capTexParam(GL_TEXTURE_WRAP_R, textureSamplerState.getWrapR());
        }

        if (textureSamplerState.getWrapS() != defaultSamplerState.getWrapS())
        {
            capTexParam(GL_TEXTURE_WRAP_S, textureSamplerState.getWrapS());
        }

        if (textureSamplerState.getWrapT() != defaultSamplerState.getWrapT())
        {
            capTexParam(GL_TEXTURE_WRAP_T, textureSamplerState.getWrapT());
        }

        if (textureSamplerState.getMinLod() != defaultSamplerState.getMinLod())
        {
            capTexParamf(GL_TEXTURE_MIN_LOD, textureSamplerState.getMinLod());
        }

        if (textureSamplerState.getMaxLod() != defaultSamplerState.getMaxLod())
        {
            capTexParamf(GL_TEXTURE_MAX_LOD, textureSamplerState.getMaxLod());
        }

        if (textureSamplerState.getCompareMode() != defaultSamplerState.getCompareMode())
        {
            capTexParam(GL_TEXTURE_COMPARE_MODE, textureSamplerState.getCompareMode());
        }

        if (textureSamplerState.getCompareFunc() != defaultSamplerState.getCompareFunc())
        {
            capTexParam(GL_TEXTURE_COMPARE_FUNC, textureSamplerState.getCompareFunc());
        }

        // Texture parameters
        if (texture->getSwizzleRed() != GL_RED)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_R, texture->getSwizzleRed());
        }

        if (texture->getSwizzleGreen() != GL_GREEN)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_G, texture->getSwizzleGreen());
        }

        if (texture->getSwizzleBlue() != GL_BLUE)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_B, texture->getSwizzleBlue());
        }

        if (texture->getSwizzleAlpha() != GL_ALPHA)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_A, texture->getSwizzleAlpha());
        }

        if (texture->getBaseLevel() != 0)
        {
            capTexParam(GL_TEXTURE_BASE_LEVEL, texture->getBaseLevel());
        }

        if (texture->getMaxLevel() != 1000)
        {
            capTexParam(GL_TEXTURE_MAX_LEVEL, texture->getMaxLevel());
        }

        // If the texture is immutable, initialize it with TexStorage
        if (texture->getImmutableFormat())
        {
            // We can only call TexStorage *once* on an immutable texture, so it needs special
            // handling. To solve this, immutable textures will have a BindTexture and TexStorage as
            // part of their textureRegenCalls. The resulting regen sequence will be:
            //
            //    const GLuint glDeleteTextures_texturesPacked_0[] = { gTextureMap[52] };
            //    glDeleteTextures(1, glDeleteTextures_texturesPacked_0);
            //    glGenTextures(1, reinterpret_cast<GLuint *>(gReadBuffer));
            //    UpdateTextureID(52, 0);
            //    glBindTexture(GL_TEXTURE_2D, gTextureMap[52]);
            //    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 256, 512);

            // Bind the texture first just for textureRegenCalls
            Capture(&textureRegenCalls[id.value],
                    CaptureBindTexture(replayState, true, texture->getType(), id));

            // Then add TexStorage to texGenCalls instead of texSetupCalls
            for (std::vector<CallCapture> *calls : texGenCalls)
            {
                CaptureTextureStorage(calls, &replayState, texture);
            }
        }

        // Iterate texture levels and layers.
        gl::ImageIndexIterator imageIter = gl::ImageIndexIterator::MakeGeneric(
            texture->getType(), 0, texture->getMipmapMaxLevel() + 1, gl::ImageIndex::kEntireLevel,
            gl::ImageIndex::kEntireLevel);
        while (imageIter.hasNext())
        {
            gl::ImageIndex index = imageIter.next();

            const gl::ImageDesc &desc = texture->getTextureState().getImageDesc(index);

            if (desc.size.empty())
            {
                continue;
            }

            const gl::InternalFormat &format = *desc.format.info;

            bool supportedType = (index.getType() == gl::TextureType::_2D ||
                                  index.getType() == gl::TextureType::_3D ||
                                  index.getType() == gl::TextureType::_2DArray ||
                                  index.getType() == gl::TextureType::Buffer ||
                                  index.getType() == gl::TextureType::CubeMap ||
                                  index.getType() == gl::TextureType::CubeMapArray);

            // Check for supported textures
            if (!supportedType)
            {
                ERR() << "Unsupported texture type: " << index.getType();
                UNREACHABLE();
            }

            if (index.getType() == gl::TextureType::Buffer)
            {
                // The buffer contents are already backed up, but we need to emit the TexBuffer
                // binding calls
                for (std::vector<CallCapture> *calls : texSetupCalls)
                {
                    CaptureTextureContents(calls, &replayState, texture, index, desc, 0, 0);
                }
                continue;
            }

            if (format.compressed)
            {
                // For compressed images, we've tracked a copy of the incoming data, so we can
                // use that rather than try to read data back that may have been converted.
                const std::vector<uint8_t> &capturedTextureLevel =
                    context->getShareGroup()->getFrameCaptureShared()->retrieveCachedTextureLevel(
                        texture->id(), index.getTarget(), index.getLevelIndex());

                // Use the shadow copy of the data to populate the call
                for (std::vector<CallCapture> *calls : texSetupCalls)
                {
                    CaptureTextureContents(calls, &replayState, texture, index, desc,
                                           static_cast<GLuint>(capturedTextureLevel.size()),
                                           capturedTextureLevel.data());
                }
            }
            else
            {
                // Use ANGLE_get_image to read back pixel data.
                if (context->getExtensions().getImageANGLE)
                {
                    GLenum getFormat = format.format;
                    GLenum getType   = format.type;

                    angle::MemoryBuffer data;

                    const gl::Extents size(desc.size.width, desc.size.height, desc.size.depth);
                    const gl::PixelUnpackState &unpack = apiState.getUnpackState();

                    GLuint endByte = 0;
                    bool unpackSize =
                        format.computePackUnpackEndByte(getType, size, unpack, true, &endByte);
                    ASSERT(unpackSize);

                    bool result = data.resize(endByte);
                    ASSERT(result);

                    gl::PixelPackState packState;
                    packState.alignment = 1;

                    (void)texture->getTexImage(context, packState, nullptr, index.getTarget(),
                                               index.getLevelIndex(), getFormat, getType,
                                               data.data());

                    for (std::vector<CallCapture> *calls : texSetupCalls)
                    {
                        CaptureTextureContents(calls, &replayState, texture, index, desc,
                                               static_cast<GLuint>(data.size()), data.data());
                    }
                }
                else
                {
                    for (std::vector<CallCapture> *calls : texSetupCalls)
                    {
                        CaptureTextureContents(calls, &replayState, texture, index, desc, 0,
                                               nullptr);
                    }
                }
            }
        }
    }

    // Capture Renderbuffers.
    const gl::RenderbufferManager &renderbuffers = apiState.getRenderbufferManagerForCapture();
    FramebufferCaptureFuncs framebufferFuncs(context->isGLES1());

    for (const auto &renderbufIter : renderbuffers)
    {
        gl::RenderbufferID id                = {renderbufIter.first};
        const gl::Renderbuffer *renderbuffer = renderbufIter.second;

        // Generate renderbuffer id.
        cap(framebufferFuncs.genRenderbuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);
        cap(framebufferFuncs.bindRenderbuffer(replayState, true, GL_RENDERBUFFER, id));

        GLenum internalformat = renderbuffer->getFormat().info->internalFormat;

        if (renderbuffer->getSamples() > 0)
        {
            // Note: We could also use extensions if available.
            cap(CaptureRenderbufferStorageMultisample(
                replayState, true, GL_RENDERBUFFER, renderbuffer->getSamples(), internalformat,
                renderbuffer->getWidth(), renderbuffer->getHeight()));
        }
        else
        {
            cap(framebufferFuncs.renderbufferStorage(replayState, true, GL_RENDERBUFFER,
                                                     internalformat, renderbuffer->getWidth(),
                                                     renderbuffer->getHeight()));
        }

        // TODO(jmadill): Capture renderbuffer contents. http://anglebug.com/3662
    }

    // Capture Shaders and Programs.
    const gl::ShaderProgramManager &shadersAndPrograms =
        apiState.getShaderProgramManagerForCapture();
    const gl::ResourceMap<gl::Shader, gl::ShaderProgramID> &shaders =
        shadersAndPrograms.getShadersForCapture();
    const gl::ResourceMap<gl::Program, gl::ShaderProgramID> &programs =
        shadersAndPrograms.getProgramsForCaptureAndPerf();

    // Capture Program binary state.
    for (const auto &programIter : programs)
    {
        gl::ShaderProgramID id = {programIter.first};
        gl::Program *program   = programIter.second;

        // Unlinked programs don't have an executable. Thus they don't need to be captured.
        // Programs are shared by contexts in the share group and only need to be captured once.
        if (!program->isLinked())
        {
            continue;
        }

        size_t programSetupStart = setupCalls->size();

        // Get last linked shader source.
        const ProgramSources &linkedSources =
            context->getShareGroup()->getFrameCaptureShared()->getProgramSources(id);

        cap(CaptureCreateProgram(replayState, true, id.value));

        GenerateLinkedProgram(context, replayState, resourceTracker, setupCalls, program, id,
                              linkedSources);

        // Update the program in replayState
        if (!replayState.getProgram() || replayState.getProgram()->id() != program->id())
        {
            // Note: We don't do this in GenerateLinkedProgram because it can't modify state
            (void)replayState.setProgram(context, program);
        }

        resourceTracker->getTrackedResource(ResourceIDType::ShaderProgram)
            .getStartingResources()
            .insert(id.value);

        size_t programSetupEnd = setupCalls->size();

        // Mark the range of calls used to setup this program
        frameCaptureShared->markResourceSetupCallsInactive(
            setupCalls, ResourceIDType::ShaderProgram, id.value,
            gl::Range<size_t>(programSetupStart, programSetupEnd));
    }

    // Handle shaders.
    for (const auto &shaderIter : shaders)
    {
        gl::ShaderProgramID id = {shaderIter.first};
        gl::Shader *shader     = shaderIter.second;

        // Skip shaders scheduled for deletion.
        // Shaders are shared by contexts in the share group and only need to be captured once.
        if (shader->hasBeenDeleted())
        {
            continue;
        }

        size_t shaderSetupStart = setupCalls->size();

        cap(CaptureCreateShader(replayState, true, shader->getType(), id.value));

        std::string shaderSource  = shader->getSourceString();
        const char *sourcePointer = shaderSource.empty() ? nullptr : shaderSource.c_str();

        // This does not handle some more tricky situations like attaching shaders to a non-linked
        // program. Or attaching uncompiled shaders. Or attaching and then deleting a shader.
        // TODO(jmadill): Handle trickier program uses. http://anglebug.com/3662
        if (shader->isCompiled())
        {
            const std::string &capturedSource =
                context->getShareGroup()->getFrameCaptureShared()->getShaderSource(id);
            if (capturedSource != shaderSource)
            {
                ASSERT(!capturedSource.empty());
                sourcePointer = capturedSource.c_str();
            }

            cap(CaptureShaderSource(replayState, true, id, 1, &sourcePointer, nullptr));
            cap(CaptureCompileShader(replayState, true, id));
        }

        if (sourcePointer && (!shader->isCompiled() || sourcePointer != shaderSource.c_str()))
        {
            cap(CaptureShaderSource(replayState, true, id, 1, &sourcePointer, nullptr));
        }

        size_t shaderSetupEnd = setupCalls->size();

        // Mark the range of calls used to setup this shader
        frameCaptureShared->markResourceSetupCallsInactive(
            setupCalls, ResourceIDType::ShaderProgram, id.value,
            gl::Range<size_t>(shaderSetupStart, shaderSetupEnd));
    }

    // Capture Sampler Objects
    const gl::SamplerManager &samplers = apiState.getSamplerManagerForCapture();
    for (const auto &samplerIter : samplers)
    {
        gl::SamplerID samplerID = {samplerIter.first};

        // Don't gen the sampler if we've seen it before, since they are shared across the context
        // share group.
        cap(CaptureGenSamplers(replayState, true, 1, &samplerID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::Sampler *sampler = samplerIter.second;
        if (!sampler)
        {
            continue;
        }

        gl::SamplerState defaultSamplerState;
        if (sampler->getMinFilter() != defaultSamplerState.getMinFilter())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_MIN_FILTER,
                                         sampler->getMinFilter()));
        }
        if (sampler->getMagFilter() != defaultSamplerState.getMagFilter())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_MAG_FILTER,
                                         sampler->getMagFilter()));
        }
        if (sampler->getWrapS() != defaultSamplerState.getWrapS())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_S,
                                         sampler->getWrapS()));
        }
        if (sampler->getWrapR() != defaultSamplerState.getWrapR())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_R,
                                         sampler->getWrapR()));
        }
        if (sampler->getWrapT() != defaultSamplerState.getWrapT())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_T,
                                         sampler->getWrapT()));
        }
        if (sampler->getMinLod() != defaultSamplerState.getMinLod())
        {
            cap(CaptureSamplerParameterf(replayState, true, samplerID, GL_TEXTURE_MIN_LOD,
                                         sampler->getMinLod()));
        }
        if (sampler->getMaxLod() != defaultSamplerState.getMaxLod())
        {
            cap(CaptureSamplerParameterf(replayState, true, samplerID, GL_TEXTURE_MAX_LOD,
                                         sampler->getMaxLod()));
        }
        if (sampler->getCompareMode() != defaultSamplerState.getCompareMode())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_COMPARE_MODE,
                                         sampler->getCompareMode()));
        }
        if (sampler->getCompareFunc() != defaultSamplerState.getCompareFunc())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_COMPARE_FUNC,
                                         sampler->getCompareFunc()));
        }
    }

    // Capture Sync Objects
    const gl::SyncManager &syncs = apiState.getSyncManagerForCapture();
    for (const auto &syncIter : syncs)
    {
        GLsync syncID        = gl::bitCast<GLsync>(static_cast<size_t>(syncIter.first));
        const gl::Sync *sync = syncIter.second;

        if (!sync)
        {
            continue;
        }
        cap(CaptureFenceSync(replayState, true, sync->getCondition(), sync->getFlags(), syncID));
        CaptureFenceSyncResetCalls(replayState, resourceTracker, syncID, sync);
        resourceTracker->getStartingFenceSyncs().insert(syncID);
    }

    // Allow the replayState object to be destroyed conveniently.
    replayState.setBufferBinding(context, gl::BufferBinding::Array, nullptr);
}

void CaptureMidExecutionSetup(const gl::Context *context,
                              std::vector<CallCapture> *setupCalls,
                              std::vector<CallCapture> *shareGroupSetupCalls,
                              ResourceIDToSetupCallsMap *resourceIDToSetupCalls,
                              ResourceTracker *resourceTracker,
                              gl::State &replayState,
                              bool validationEnabled)
{
    const gl::State &apiState = context->getState();

    // Small helper function to make the code more readable.
    auto cap = [setupCalls](CallCapture &&call) { setupCalls->emplace_back(std::move(call)); };

    // Need to go from uint32 -> uint64 -> EGLContext (void*) to handle MSVC compiler
    // warning on 64b systems:
    //   error C4312: 'reinterpret_cast': conversion from 'uint32_t' to 'EGLContext' of
    //   greater size
    uint64_t contextID    = static_cast<uint64_t>(context->id().value);
    EGLContext eglContext = reinterpret_cast<EGLContext>(contextID);
    cap(CaptureMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext));

    // Vertex input states. Must happen after buffer data initialization. Do not capture on GLES1.
    if (!context->isGLES1())
    {
        CaptureDefaultVertexAttribs(replayState, apiState, setupCalls);
    }

    // Capture vertex array objects
    const gl::VertexArrayMap &vertexArrayMap = context->getVertexArraysForCapture();
    gl::VertexArrayID boundVertexArrayID     = {0};
    for (const auto &vertexArrayIter : vertexArrayMap)
    {
        gl::VertexArrayID vertexArrayID = {vertexArrayIter.first};
        if (vertexArrayID.value != 0)
        {
            cap(CaptureGenVertexArrays(replayState, true, 1, &vertexArrayID));
            MaybeCaptureUpdateResourceIDs(setupCalls);
        }

        if (vertexArrayIter.second)
        {
            const gl::VertexArray *vertexArray = vertexArrayIter.second;

            // Bind the vertexArray (unless default) and populate it
            if (vertexArrayID.value != 0)
            {
                cap(CaptureBindVertexArray(replayState, true, vertexArrayID));
                boundVertexArrayID = vertexArrayID;
            }
            CaptureVertexArrayState(setupCalls, context, vertexArray, &replayState);
        }
    }

    // Bind the current vertex array
    const gl::VertexArray *currentVertexArray = apiState.getVertexArray();
    if (currentVertexArray->id() != boundVertexArrayID)
    {
        cap(CaptureBindVertexArray(replayState, true, currentVertexArray->id()));
    }

    // Capture indexed buffer bindings.
    const gl::BufferVector &uniformIndexedBuffers =
        apiState.getOffsetBindingPointerUniformBuffers();
    const gl::BufferVector &atomicCounterIndexedBuffers =
        apiState.getOffsetBindingPointerAtomicCounterBuffers();
    const gl::BufferVector &shaderStorageIndexedBuffers =
        apiState.getOffsetBindingPointerShaderStorageBuffers();
    CaptureIndexedBuffers(replayState, uniformIndexedBuffers, gl::BufferBinding::Uniform,
                          setupCalls);
    CaptureIndexedBuffers(replayState, atomicCounterIndexedBuffers,
                          gl::BufferBinding::AtomicCounter, setupCalls);
    CaptureIndexedBuffers(replayState, shaderStorageIndexedBuffers,
                          gl::BufferBinding::ShaderStorage, setupCalls);

    // Capture Buffer bindings.
    const gl::BoundBufferMap &boundBuffers = apiState.getBoundBuffersForCapture();
    for (gl::BufferBinding binding : angle::AllEnums<gl::BufferBinding>())
    {
        gl::BufferID bufferID = boundBuffers[binding].id();

        // Filter out redundant buffer binding commands. Note that the code in the previous section
        // only binds to ARRAY_BUFFER. Therefore we only check the array binding against the binding
        // we set earlier.
        bool isArray                  = binding == gl::BufferBinding::Array;
        const gl::Buffer *arrayBuffer = replayState.getArrayBuffer();
        if ((isArray && arrayBuffer && arrayBuffer->id() != bufferID) ||
            (!isArray && bufferID.value != 0))
        {
            cap(CaptureBindBuffer(replayState, true, binding, bufferID));
        }

        // Restore all buffer bindings for Reset
        if (bufferID.value != 0)
        {
            CaptureBufferBindingResetCalls(replayState, resourceTracker, binding, bufferID);
        }
    }

    // Set a unpack alignment of 1. Otherwise, computeRowPitch() will compute the wrong value,
    // leading to a crash in memcpy() when capturing the texture contents.
    gl::PixelUnpackState &currentUnpackState = replayState.getUnpackState();
    if (currentUnpackState.alignment != 1)
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ALIGNMENT, 1));
        currentUnpackState.alignment = 1;
    }

    // Capture Texture setup and data.
    const gl::TextureBindingMap &apiBoundTextures = apiState.getBoundTexturesForCapture();

    // Set Texture bindings.
    for (gl::TextureType textureType : angle::AllEnums<gl::TextureType>())
    {
        const gl::TextureBindingVector &apiBindings = apiBoundTextures[textureType];
        const gl::TextureBindingVector &replayBindings =
            replayState.getBoundTexturesForCapture()[textureType];
        ASSERT(apiBindings.size() == replayBindings.size());
        for (size_t bindingIndex = 0; bindingIndex < apiBindings.size(); ++bindingIndex)
        {
            gl::TextureID apiTextureID    = apiBindings[bindingIndex].id();
            gl::TextureID replayTextureID = replayBindings[bindingIndex].id();

            if (apiTextureID != replayTextureID)
            {
                if (replayState.getActiveSampler() != bindingIndex)
                {
                    cap(CaptureActiveTexture(replayState, true,
                                             GL_TEXTURE0 + static_cast<GLenum>(bindingIndex)));
                    replayState.setActiveSampler(static_cast<unsigned int>(bindingIndex));
                }

                cap(CaptureBindTexture(replayState, true, textureType, apiTextureID));
                replayState.setSamplerTexture(context, textureType,
                                              apiBindings[bindingIndex].get());
            }
        }
    }

    // Set active Texture.
    if (replayState.getActiveSampler() != apiState.getActiveSampler())
    {
        cap(CaptureActiveTexture(replayState, true,
                                 GL_TEXTURE0 + static_cast<GLenum>(apiState.getActiveSampler())));
        replayState.setActiveSampler(apiState.getActiveSampler());
    }

    // Set Renderbuffer binding.
    const gl::RenderbufferManager &renderbuffers = apiState.getRenderbufferManagerForCapture();
    gl::RenderbufferID currentRenderbuffer       = {0};
    for (const auto &renderbufIter : renderbuffers)
    {
        currentRenderbuffer = renderbufIter.second->id();
    }

    if (currentRenderbuffer != apiState.getRenderbufferId())
    {
        cap(CaptureBindRenderbuffer(replayState, true, GL_RENDERBUFFER,
                                    apiState.getRenderbufferId()));
    }

    // Capture Framebuffers.
    const gl::FramebufferManager &framebuffers = apiState.getFramebufferManagerForCapture();
    FramebufferCaptureFuncs framebufferFuncs(context->isGLES1());

    gl::FramebufferID currentDrawFramebuffer = {0};
    gl::FramebufferID currentReadFramebuffer = {0};

    for (const auto &framebufferIter : framebuffers)
    {
        gl::FramebufferID id               = {framebufferIter.first};
        const gl::Framebuffer *framebuffer = framebufferIter.second;

        // The default Framebuffer exists (by default).
        if (framebuffer->isDefault())
        {
            continue;
        }

        cap(framebufferFuncs.genFramebuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);
        cap(framebufferFuncs.bindFramebuffer(replayState, true, GL_FRAMEBUFFER, id));
        currentDrawFramebuffer = currentReadFramebuffer = id;

        // Color Attachments.
        for (const gl::FramebufferAttachment &colorAttachment : framebuffer->getColorAttachments())
        {
            if (!colorAttachment.isAttached())
            {
                continue;
            }

            CaptureFramebufferAttachment(setupCalls, replayState, framebufferFuncs,
                                         colorAttachment);
        }

        const gl::FramebufferAttachment *depthAttachment = framebuffer->getDepthAttachment();
        if (depthAttachment)
        {
            ASSERT(depthAttachment->getBinding() == GL_DEPTH_ATTACHMENT ||
                   depthAttachment->getBinding() == GL_DEPTH_STENCIL_ATTACHMENT);
            CaptureFramebufferAttachment(setupCalls, replayState, framebufferFuncs,
                                         *depthAttachment);
        }

        const gl::FramebufferAttachment *stencilAttachment = framebuffer->getStencilAttachment();
        if (stencilAttachment)
        {
            ASSERT(stencilAttachment->getBinding() == GL_STENCIL_ATTACHMENT ||
                   depthAttachment->getBinding() == GL_DEPTH_STENCIL_ATTACHMENT);
            CaptureFramebufferAttachment(setupCalls, replayState, framebufferFuncs,
                                         *stencilAttachment);
        }

        gl::FramebufferState defaultFramebufferState(
            context->getCaps(), framebuffer->getState().id(),
            framebuffer->getState().getFramebufferSerial());
        const std::vector<GLenum> &defaultDrawBufferStates =
            defaultFramebufferState.getDrawBufferStates();
        const std::vector<GLenum> &drawBufferStates = framebuffer->getDrawBufferStates();

        if (drawBufferStates != defaultDrawBufferStates)
        {
            cap(CaptureDrawBuffers(replayState, true, static_cast<GLsizei>(drawBufferStates.size()),
                                   drawBufferStates.data()));
        }
    }

    // Capture framebuffer bindings.
    gl::FramebufferID stateReadFramebuffer = apiState.getReadFramebuffer()->id();
    gl::FramebufferID stateDrawFramebuffer = apiState.getDrawFramebuffer()->id();
    if (stateDrawFramebuffer == stateReadFramebuffer)
    {
        if (currentDrawFramebuffer != stateDrawFramebuffer ||
            currentReadFramebuffer != stateReadFramebuffer)
        {
            cap(framebufferFuncs.bindFramebuffer(replayState, true, GL_FRAMEBUFFER,
                                                 stateDrawFramebuffer));
            currentDrawFramebuffer = currentReadFramebuffer = stateDrawFramebuffer;
        }
    }
    else
    {
        if (currentDrawFramebuffer != stateDrawFramebuffer)
        {
            cap(framebufferFuncs.bindFramebuffer(replayState, true, GL_DRAW_FRAMEBUFFER,
                                                 currentDrawFramebuffer));
            currentDrawFramebuffer = stateDrawFramebuffer;
        }

        if (currentReadFramebuffer != stateReadFramebuffer)
        {
            cap(framebufferFuncs.bindFramebuffer(replayState, true, GL_READ_FRAMEBUFFER,
                                                 replayState.getReadFramebuffer()->id()));
            currentReadFramebuffer = stateReadFramebuffer;
        }
    }

    // Capture Program Pipelines
    const gl::ProgramPipelineManager *programPipelineManager =
        apiState.getProgramPipelineManagerForCapture();

    for (const auto &ppoIterator : *programPipelineManager)
    {
        gl::ProgramPipeline *pipeline = ppoIterator.second;
        gl::ProgramPipelineID id      = {ppoIterator.first};
        cap(CaptureGenProgramPipelines(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        // PPOs can contain graphics and compute programs, so loop through all shader types rather
        // than just the linked ones since getLinkedShaderStages() will return either only graphics
        // or compute stages.
        for (gl::ShaderType shaderType : gl::AllShaderTypes())
        {
            gl::Program *program = pipeline->getShaderProgram(shaderType);
            if (!program)
            {
                continue;
            }
            ASSERT(program->isLinked());
            GLbitfield gLbitfield = GetBitfieldFromShaderType(shaderType);
            cap(CaptureUseProgramStages(replayState, true, pipeline->id(), gLbitfield,
                                        program->id()));

            // Set this program as active so it will be generated in Setup
            // Note: We aren't filtering ProgramPipelines, so this could be setting programs
            // active that aren't actually used.
            MarkResourceIDActive(ResourceIDType::ShaderProgram, program->id().value,
                                 shareGroupSetupCalls, resourceIDToSetupCalls);
        }

        gl::Program *program = pipeline->getActiveShaderProgram();
        if (program)
        {
            cap(CaptureActiveShaderProgram(replayState, true, id, program->id()));
        }
    }

    // For now we assume the installed program executable is the same as the current program.
    // TODO(jmadill): Handle installed program executable. http://anglebug.com/3662
    if (!context->isGLES1())
    {
        // If we have a program bound in the API, or if there is no program bound to the API at
        // time of capture and we bound a program for uniform updates during MEC, we must add
        // a set program call to replay the correct states.
        if (apiState.getProgram())
        {
            cap(CaptureUseProgram(replayState, true, apiState.getProgram()->id()));
            CaptureUpdateCurrentProgram(setupCalls->back(), setupCalls);
            (void)replayState.setProgram(context, apiState.getProgram());

            // Set this program as active so it will be generated in Setup
            MarkResourceIDActive(ResourceIDType::ShaderProgram, apiState.getProgram()->id().value,
                                 shareGroupSetupCalls, resourceIDToSetupCalls);
        }
        else if (replayState.getProgram())
        {
            cap(CaptureUseProgram(replayState, true, {0}));
            CaptureUpdateCurrentProgram(setupCalls->back(), setupCalls);
            (void)replayState.setProgram(context, nullptr);
        }

        // Same for program pipelines as for programs, see comment above.
        if (apiState.getProgramPipeline())
        {
            cap(CaptureBindProgramPipeline(replayState, true, apiState.getProgramPipeline()->id()));
        }
        else if (replayState.getProgramPipeline())
        {
            cap(CaptureBindProgramPipeline(replayState, true, {0}));
        }
    }

    // Create existing queries. Note that queries may be genned and not yet started. In that
    // case the queries will exist in the query map as nullptr entries.
    const gl::QueryMap &queryMap = context->getQueriesForCapture();
    for (gl::QueryMap::Iterator queryIter = queryMap.beginWithNull();
         queryIter != queryMap.endWithNull(); ++queryIter)
    {
        ASSERT(queryIter->first);
        gl::QueryID queryID = {queryIter->first};

        cap(CaptureGenQueries(replayState, true, 1, &queryID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::Query *query = queryIter->second;
        if (query)
        {
            gl::QueryType queryType = query->getType();

            // Begin the query to generate the object
            cap(CaptureBeginQuery(replayState, true, queryType, queryID));

            // End the query if it was not active
            if (!IsQueryActive(apiState, queryID))
            {
                cap(CaptureEndQuery(replayState, true, queryType));
            }
        }
    }

    // Transform Feedback
    const gl::TransformFeedbackMap &xfbMap = context->getTransformFeedbacksForCapture();
    for (const auto &xfbIter : xfbMap)
    {
        gl::TransformFeedbackID xfbID = {xfbIter.first};

        // Do not capture the default XFB object.
        if (xfbID.value == 0)
        {
            continue;
        }

        cap(CaptureGenTransformFeedbacks(replayState, true, 1, &xfbID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::TransformFeedback *xfb = xfbIter.second;
        if (!xfb)
        {
            // The object was never created
            continue;
        }

        // Bind XFB to create the object
        cap(CaptureBindTransformFeedback(replayState, true, GL_TRANSFORM_FEEDBACK, xfbID));

        // Bind the buffers associated with this XFB object
        for (size_t i = 0; i < xfb->getIndexedBufferCount(); ++i)
        {
            const gl::OffsetBindingPointer<gl::Buffer> &xfbBuffer = xfb->getIndexedBuffer(i);

            // Note: Buffers bound with BindBufferBase can be used with BindBuffer
            cap(CaptureBindBufferRange(replayState, true, gl::BufferBinding::TransformFeedback, 0,
                                       xfbBuffer.id(), xfbBuffer.getOffset(), xfbBuffer.getSize()));
        }

        if (xfb->isActive() || xfb->isPaused())
        {
            // We don't support active XFB in MEC yet
            UNIMPLEMENTED();
        }
    }

    // Bind the current XFB buffer after populating XFB objects
    gl::TransformFeedback *currentXFB = apiState.getCurrentTransformFeedback();
    if (currentXFB)
    {
        cap(CaptureBindTransformFeedback(replayState, true, GL_TRANSFORM_FEEDBACK,
                                         currentXFB->id()));
    }

    // Bind samplers
    const gl::SamplerBindingVector &samplerBindings = apiState.getSamplers();
    for (GLuint bindingIndex = 0; bindingIndex < static_cast<GLuint>(samplerBindings.size());
         ++bindingIndex)
    {
        gl::SamplerID samplerID = samplerBindings[bindingIndex].id();
        if (samplerID.value != 0)
        {
            cap(CaptureBindSampler(replayState, true, bindingIndex, samplerID));
        }
    }

    // Capture Image Texture bindings
    const std::vector<gl::ImageUnit> &imageUnits = apiState.getImageUnits();
    for (GLuint bindingIndex = 0; bindingIndex < static_cast<GLuint>(imageUnits.size());
         ++bindingIndex)
    {
        const gl::ImageUnit &imageUnit = imageUnits[bindingIndex];

        if (imageUnit.texture == 0)
        {
            continue;
        }

        cap(CaptureBindImageTexture(replayState, true, bindingIndex, imageUnit.texture.id(),
                                    imageUnit.level, imageUnit.layered, imageUnit.layer,
                                    imageUnit.access, imageUnit.format));
    }

    // Capture GL Context states.
    auto capCap = [cap, &replayState](GLenum capEnum, bool capValue) {
        if (capValue)
        {
            cap(CaptureEnable(replayState, true, capEnum));
        }
        else
        {
            cap(CaptureDisable(replayState, true, capEnum));
        }
    };

    // Capture GLES1 context states.
    if (context->isGLES1())
    {
        const bool currentTextureState = apiState.getEnableFeature(GL_TEXTURE_2D);
        const bool defaultTextureState = replayState.getEnableFeature(GL_TEXTURE_2D);
        if (currentTextureState != defaultTextureState)
        {
            capCap(GL_TEXTURE_2D, currentTextureState);
        }

        cap(CaptureMatrixMode(replayState, true, gl::MatrixType::Projection));
        for (angle::Mat4 projectionMatrix :
             apiState.gles1().getMatrixStack(gl::MatrixType::Projection))
        {
            cap(CapturePushMatrix(replayState, true));
            cap(CaptureLoadMatrixf(replayState, true, projectionMatrix.elements().data()));
        }

        cap(CaptureMatrixMode(replayState, true, gl::MatrixType::Modelview));
        for (angle::Mat4 modelViewMatrix :
             apiState.gles1().getMatrixStack(gl::MatrixType::Modelview))
        {
            cap(CapturePushMatrix(replayState, true));
            cap(CaptureLoadMatrixf(replayState, true, modelViewMatrix.elements().data()));
        }

        gl::MatrixType currentMatrixMode = apiState.gles1().getMatrixMode();
        if (currentMatrixMode != gl::MatrixType::Modelview)
        {
            cap(CaptureMatrixMode(replayState, true, currentMatrixMode));
        }
    }

    // Rasterizer state. Missing ES 3.x features.
    const gl::RasterizerState &defaultRasterState = replayState.getRasterizerState();
    const gl::RasterizerState &currentRasterState = apiState.getRasterizerState();
    if (currentRasterState.cullFace != defaultRasterState.cullFace)
    {
        capCap(GL_CULL_FACE, currentRasterState.cullFace);
    }

    if (currentRasterState.cullMode != defaultRasterState.cullMode)
    {
        cap(CaptureCullFace(replayState, true, currentRasterState.cullMode));
    }

    if (currentRasterState.frontFace != defaultRasterState.frontFace)
    {
        cap(CaptureFrontFace(replayState, true, currentRasterState.frontFace));
    }

    if (currentRasterState.polygonOffsetFill != defaultRasterState.polygonOffsetFill)
    {
        capCap(GL_POLYGON_OFFSET_FILL, currentRasterState.polygonOffsetFill);
    }

    if (currentRasterState.polygonOffsetFactor != defaultRasterState.polygonOffsetFactor ||
        currentRasterState.polygonOffsetUnits != defaultRasterState.polygonOffsetUnits)
    {
        cap(CapturePolygonOffset(replayState, true, currentRasterState.polygonOffsetFactor,
                                 currentRasterState.polygonOffsetUnits));
    }

    // pointDrawMode/multiSample are only used in the D3D back-end right now.

    if (currentRasterState.rasterizerDiscard != defaultRasterState.rasterizerDiscard)
    {
        capCap(GL_RASTERIZER_DISCARD, currentRasterState.rasterizerDiscard);
    }

    if (currentRasterState.dither != defaultRasterState.dither)
    {
        capCap(GL_DITHER, currentRasterState.dither);
    }

    // Depth/stencil state.
    const gl::DepthStencilState &defaultDSState = replayState.getDepthStencilState();
    const gl::DepthStencilState &currentDSState = apiState.getDepthStencilState();
    if (defaultDSState.depthFunc != currentDSState.depthFunc)
    {
        cap(CaptureDepthFunc(replayState, true, currentDSState.depthFunc));
    }

    if (defaultDSState.depthMask != currentDSState.depthMask)
    {
        cap(CaptureDepthMask(replayState, true, gl::ConvertToGLBoolean(currentDSState.depthMask)));
    }

    if (defaultDSState.depthTest != currentDSState.depthTest)
    {
        capCap(GL_DEPTH_TEST, currentDSState.depthTest);
    }

    if (defaultDSState.stencilTest != currentDSState.stencilTest)
    {
        capCap(GL_STENCIL_TEST, currentDSState.stencilTest);
    }

    if (currentDSState.stencilFunc == currentDSState.stencilBackFunc &&
        currentDSState.stencilMask == currentDSState.stencilBackMask)
    {
        // Front and back are equal
        if (defaultDSState.stencilFunc != currentDSState.stencilFunc ||
            defaultDSState.stencilMask != currentDSState.stencilMask ||
            apiState.getStencilRef() != 0)
        {
            cap(CaptureStencilFunc(replayState, true, currentDSState.stencilFunc,
                                   apiState.getStencilRef(), currentDSState.stencilMask));
        }
    }
    else
    {
        // Front and back are separate
        if (defaultDSState.stencilFunc != currentDSState.stencilFunc ||
            defaultDSState.stencilMask != currentDSState.stencilMask ||
            apiState.getStencilRef() != 0)
        {
            cap(CaptureStencilFuncSeparate(replayState, true, GL_FRONT, currentDSState.stencilFunc,
                                           apiState.getStencilRef(), currentDSState.stencilMask));
        }

        if (defaultDSState.stencilBackFunc != currentDSState.stencilBackFunc ||
            defaultDSState.stencilBackMask != currentDSState.stencilBackMask ||
            apiState.getStencilBackRef() != 0)
        {
            cap(CaptureStencilFuncSeparate(
                replayState, true, GL_BACK, currentDSState.stencilBackFunc,
                apiState.getStencilBackRef(), currentDSState.stencilBackMask));
        }
    }

    if (currentDSState.stencilFail == currentDSState.stencilBackFail &&
        currentDSState.stencilPassDepthFail == currentDSState.stencilBackPassDepthFail &&
        currentDSState.stencilPassDepthPass == currentDSState.stencilBackPassDepthPass)
    {
        // Front and back are equal
        if (defaultDSState.stencilFail != currentDSState.stencilFail ||
            defaultDSState.stencilPassDepthFail != currentDSState.stencilPassDepthFail ||
            defaultDSState.stencilPassDepthPass != currentDSState.stencilPassDepthPass)
        {
            cap(CaptureStencilOp(replayState, true, currentDSState.stencilFail,
                                 currentDSState.stencilPassDepthFail,
                                 currentDSState.stencilPassDepthPass));
        }
    }
    else
    {
        // Front and back are separate
        if (defaultDSState.stencilFail != currentDSState.stencilFail ||
            defaultDSState.stencilPassDepthFail != currentDSState.stencilPassDepthFail ||
            defaultDSState.stencilPassDepthPass != currentDSState.stencilPassDepthPass)
        {
            cap(CaptureStencilOpSeparate(replayState, true, GL_FRONT, currentDSState.stencilFail,
                                         currentDSState.stencilPassDepthFail,
                                         currentDSState.stencilPassDepthPass));
        }

        if (defaultDSState.stencilBackFail != currentDSState.stencilBackFail ||
            defaultDSState.stencilBackPassDepthFail != currentDSState.stencilBackPassDepthFail ||
            defaultDSState.stencilBackPassDepthPass != currentDSState.stencilBackPassDepthPass)
        {
            cap(CaptureStencilOpSeparate(replayState, true, GL_BACK, currentDSState.stencilBackFail,
                                         currentDSState.stencilBackPassDepthFail,
                                         currentDSState.stencilBackPassDepthPass));
        }
    }

    if (currentDSState.stencilWritemask == currentDSState.stencilBackWritemask)
    {
        // Front and back are equal
        if (defaultDSState.stencilWritemask != currentDSState.stencilWritemask)
        {
            cap(CaptureStencilMask(replayState, true, currentDSState.stencilWritemask));
        }
    }
    else
    {
        // Front and back are separate
        if (defaultDSState.stencilWritemask != currentDSState.stencilWritemask)
        {
            cap(CaptureStencilMaskSeparate(replayState, true, GL_FRONT,
                                           currentDSState.stencilWritemask));
        }

        if (defaultDSState.stencilBackWritemask != currentDSState.stencilBackWritemask)
        {
            cap(CaptureStencilMaskSeparate(replayState, true, GL_BACK,
                                           currentDSState.stencilBackWritemask));
        }
    }

    // Blend state.
    const gl::BlendState &defaultBlendState = replayState.getBlendState();
    const gl::BlendState &currentBlendState = apiState.getBlendState();

    if (currentBlendState.blend != defaultBlendState.blend)
    {
        capCap(GL_BLEND, currentBlendState.blend);
    }

    if (currentBlendState.sourceBlendRGB != defaultBlendState.sourceBlendRGB ||
        currentBlendState.destBlendRGB != defaultBlendState.destBlendRGB ||
        currentBlendState.sourceBlendAlpha != defaultBlendState.sourceBlendAlpha ||
        currentBlendState.destBlendAlpha != defaultBlendState.destBlendAlpha)
    {
        if (currentBlendState.sourceBlendRGB == currentBlendState.sourceBlendAlpha &&
            currentBlendState.destBlendRGB == currentBlendState.destBlendAlpha)
        {
            // Color and alpha are equal
            cap(CaptureBlendFunc(replayState, true, currentBlendState.sourceBlendRGB,
                                 currentBlendState.destBlendRGB));
        }
        else
        {
            // Color and alpha are separate
            cap(CaptureBlendFuncSeparate(
                replayState, true, currentBlendState.sourceBlendRGB, currentBlendState.destBlendRGB,
                currentBlendState.sourceBlendAlpha, currentBlendState.destBlendAlpha));
        }
    }

    if (currentBlendState.blendEquationRGB != defaultBlendState.blendEquationRGB ||
        currentBlendState.blendEquationAlpha != defaultBlendState.blendEquationAlpha)
    {
        cap(CaptureBlendEquationSeparate(replayState, true, currentBlendState.blendEquationRGB,
                                         currentBlendState.blendEquationAlpha));
    }

    if (currentBlendState.colorMaskRed != defaultBlendState.colorMaskRed ||
        currentBlendState.colorMaskGreen != defaultBlendState.colorMaskGreen ||
        currentBlendState.colorMaskBlue != defaultBlendState.colorMaskBlue ||
        currentBlendState.colorMaskAlpha != defaultBlendState.colorMaskAlpha)
    {
        cap(CaptureColorMask(replayState, true,
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskRed),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskGreen),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskBlue),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskAlpha)));
    }

    const gl::ColorF &currentBlendColor = apiState.getBlendColor();
    if (currentBlendColor != gl::ColorF())
    {
        cap(CaptureBlendColor(replayState, true, currentBlendColor.red, currentBlendColor.green,
                              currentBlendColor.blue, currentBlendColor.alpha));
    }

    // Pixel storage states.
    gl::PixelPackState &currentPackState = replayState.getPackState();
    if (currentPackState.alignment != apiState.getPackAlignment())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_ALIGNMENT, apiState.getPackAlignment()));
        currentPackState.alignment = apiState.getPackAlignment();
    }

    if (currentPackState.rowLength != apiState.getPackRowLength())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_ROW_LENGTH, apiState.getPackRowLength()));
        currentPackState.rowLength = apiState.getPackRowLength();
    }

    if (currentPackState.skipRows != apiState.getPackSkipRows())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_SKIP_ROWS, apiState.getPackSkipRows()));
        currentPackState.skipRows = apiState.getPackSkipRows();
    }

    if (currentPackState.skipPixels != apiState.getPackSkipPixels())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_SKIP_PIXELS,
                               apiState.getPackSkipPixels()));
        currentPackState.skipPixels = apiState.getPackSkipPixels();
    }

    // We set unpack alignment above, no need to change it here
    ASSERT(currentUnpackState.alignment == 1);
    if (currentUnpackState.rowLength != apiState.getUnpackRowLength())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ROW_LENGTH,
                               apiState.getUnpackRowLength()));
        currentUnpackState.rowLength = apiState.getUnpackRowLength();
    }

    if (currentUnpackState.skipRows != apiState.getUnpackSkipRows())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_ROWS,
                               apiState.getUnpackSkipRows()));
        currentUnpackState.skipRows = apiState.getUnpackSkipRows();
    }

    if (currentUnpackState.skipPixels != apiState.getUnpackSkipPixels())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_PIXELS,
                               apiState.getUnpackSkipPixels()));
        currentUnpackState.skipPixels = apiState.getUnpackSkipPixels();
    }

    if (currentUnpackState.imageHeight != apiState.getUnpackImageHeight())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_IMAGE_HEIGHT,
                               apiState.getUnpackImageHeight()));
        currentUnpackState.imageHeight = apiState.getUnpackImageHeight();
    }

    if (currentUnpackState.skipImages != apiState.getUnpackSkipImages())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_IMAGES,
                               apiState.getUnpackSkipImages()));
        currentUnpackState.skipImages = apiState.getUnpackSkipImages();
    }

    // Clear state. Missing ES 3.x features.
    // TODO(http://anglebug.com/3662): Complete state capture.
    const gl::ColorF &currentClearColor = apiState.getColorClearValue();
    if (currentClearColor != gl::ColorF())
    {
        cap(CaptureClearColor(replayState, true, currentClearColor.red, currentClearColor.green,
                              currentClearColor.blue, currentClearColor.alpha));
    }

    if (apiState.getDepthClearValue() != 1.0f)
    {
        cap(CaptureClearDepthf(replayState, true, apiState.getDepthClearValue()));
    }

    if (apiState.getStencilClearValue() != 0)
    {
        cap(CaptureClearStencil(replayState, true, apiState.getStencilClearValue()));
    }

    // Viewport / scissor / clipping planes.
    const gl::Rectangle &currentViewport = apiState.getViewport();
    if (currentViewport != gl::Rectangle())
    {
        cap(CaptureViewport(replayState, true, currentViewport.x, currentViewport.y,
                            currentViewport.width, currentViewport.height));
    }

    if (apiState.getNearPlane() != 0.0f || apiState.getFarPlane() != 1.0f)
    {
        cap(CaptureDepthRangef(replayState, true, apiState.getNearPlane(), apiState.getFarPlane()));
    }

    if (apiState.isScissorTestEnabled())
    {
        capCap(GL_SCISSOR_TEST, apiState.isScissorTestEnabled());
    }

    const gl::Rectangle &currentScissor = apiState.getScissor();
    if (currentScissor != gl::Rectangle())
    {
        cap(CaptureScissor(replayState, true, currentScissor.x, currentScissor.y,
                           currentScissor.width, currentScissor.height));
    }

    // Allow the replayState object to be destroyed conveniently.
    replayState.setBufferBinding(context, gl::BufferBinding::Array, nullptr);

    // Clean up the replay state.
    replayState.reset(context);

    if (validationEnabled)
    {
        CaptureValidateSerializedState(context, setupCalls);
    }
}

bool SkipCall(EntryPoint entryPoint)
{
    switch (entryPoint)
    {
        case EntryPoint::GLDebugMessageCallback:
        case EntryPoint::GLDebugMessageCallbackKHR:
        case EntryPoint::GLDebugMessageControl:
        case EntryPoint::GLDebugMessageControlKHR:
        case EntryPoint::GLDebugMessageInsert:
        case EntryPoint::GLDebugMessageInsertKHR:
        case EntryPoint::GLGetDebugMessageLog:
        case EntryPoint::GLGetDebugMessageLogKHR:
        case EntryPoint::GLGetObjectLabelEXT:
        case EntryPoint::GLGetObjectLabelKHR:
        case EntryPoint::GLGetObjectPtrLabelKHR:
        case EntryPoint::GLGetPointervKHR:
        case EntryPoint::GLInsertEventMarkerEXT:
        case EntryPoint::GLLabelObjectEXT:
        case EntryPoint::GLObjectLabelKHR:
        case EntryPoint::GLObjectPtrLabelKHR:
        case EntryPoint::GLPopDebugGroupKHR:
        case EntryPoint::GLPopGroupMarkerEXT:
        case EntryPoint::GLPushDebugGroupKHR:
        case EntryPoint::GLPushGroupMarkerEXT:
            // Purposefully skip entry points from:
            // - KHR_debug
            // - EXT_debug_label
            // - EXT_debug_marker
            // There is no need to capture these for replaying a trace in our harness
            return true;

        case EntryPoint::GLGetActiveUniform:
        case EntryPoint::GLGetActiveUniformsiv:
            // Skip these calls because:
            // - We don't use the return values.
            // - Active uniform counts can vary between platforms due to cross stage optimizations
            //   and asking about uniforms above GL_ACTIVE_UNIFORMS triggers errors.
            return true;

        default:
            break;
    }

    return false;
}

GLint GetAdjustedTextureCacheLevel(gl::TextureTarget target, GLint level)
{
    GLint adjustedLevel = level;

    // If target is a cube, we need to maintain 6 images per level
    if (IsCubeMapFaceTarget(target))
    {
        adjustedLevel *= 6;
        adjustedLevel += CubeMapTextureTargetToFaceIndex(target);
    }

    return adjustedLevel;
}
}  // namespace

ParamCapture::ParamCapture() : type(ParamType::TGLenum), enumGroup(gl::GLenumGroup::DefaultGroup) {}

ParamCapture::ParamCapture(const char *nameIn, ParamType typeIn)
    : name(nameIn), type(typeIn), enumGroup(gl::GLenumGroup::DefaultGroup)
{}

ParamCapture::~ParamCapture() = default;

ParamCapture::ParamCapture(ParamCapture &&other)
    : type(ParamType::TGLenum), enumGroup(gl::GLenumGroup::DefaultGroup)
{
    *this = std::move(other);
}

ParamCapture &ParamCapture::operator=(ParamCapture &&other)
{
    std::swap(name, other.name);
    std::swap(type, other.type);
    std::swap(value, other.value);
    std::swap(enumGroup, other.enumGroup);
    std::swap(data, other.data);
    std::swap(arrayClientPointerIndex, other.arrayClientPointerIndex);
    std::swap(readBufferSizeBytes, other.readBufferSizeBytes);
    std::swap(dataNElements, other.dataNElements);
    return *this;
}

ParamBuffer::ParamBuffer() {}

ParamBuffer::~ParamBuffer() = default;

ParamBuffer::ParamBuffer(ParamBuffer &&other)
{
    *this = std::move(other);
}

ParamBuffer &ParamBuffer::operator=(ParamBuffer &&other)
{
    std::swap(mParamCaptures, other.mParamCaptures);
    std::swap(mClientArrayDataParam, other.mClientArrayDataParam);
    std::swap(mReadBufferSize, other.mReadBufferSize);
    std::swap(mReturnValueCapture, other.mReturnValueCapture);
    std::swap(mMappedBufferID, other.mMappedBufferID);
    return *this;
}

ParamCapture &ParamBuffer::getParam(const char *paramName, ParamType paramType, int index)
{
    ParamCapture &capture = mParamCaptures[index];
    ASSERT(capture.name == paramName);
    ASSERT(capture.type == paramType);
    return capture;
}

const ParamCapture &ParamBuffer::getParam(const char *paramName,
                                          ParamType paramType,
                                          int index) const
{
    return const_cast<ParamBuffer *>(this)->getParam(paramName, paramType, index);
}

ParamCapture &ParamBuffer::getParamFlexName(const char *paramName1,
                                            const char *paramName2,
                                            ParamType paramType,
                                            int index)
{
    ParamCapture &capture = mParamCaptures[index];
    ASSERT(capture.name == paramName1 || capture.name == paramName2);
    ASSERT(capture.type == paramType);
    return capture;
}

const ParamCapture &ParamBuffer::getParamFlexName(const char *paramName1,
                                                  const char *paramName2,
                                                  ParamType paramType,
                                                  int index) const
{
    return const_cast<ParamBuffer *>(this)->getParamFlexName(paramName1, paramName2, paramType,
                                                             index);
}

void ParamBuffer::addParam(ParamCapture &&param)
{
    if (param.arrayClientPointerIndex != -1)
    {
        ASSERT(mClientArrayDataParam == -1);
        mClientArrayDataParam = static_cast<int>(mParamCaptures.size());
    }

    mReadBufferSize = std::max(param.readBufferSizeBytes, mReadBufferSize);
    mParamCaptures.emplace_back(std::move(param));
}

void ParamBuffer::addReturnValue(ParamCapture &&returnValue)
{
    mReturnValueCapture = std::move(returnValue);
}

ParamCapture &ParamBuffer::getClientArrayPointerParameter()
{
    ASSERT(hasClientArrayData());
    return mParamCaptures[mClientArrayDataParam];
}

CallCapture::CallCapture(EntryPoint entryPointIn, ParamBuffer &&paramsIn)
    : entryPoint(entryPointIn), params(std::move(paramsIn))
{}

CallCapture::CallCapture(const std::string &customFunctionNameIn, ParamBuffer &&paramsIn)
    : entryPoint(EntryPoint::GLInvalid),
      customFunctionName(customFunctionNameIn),
      params(std::move(paramsIn))
{}

CallCapture::~CallCapture() = default;

CallCapture::CallCapture(CallCapture &&other)
{
    *this = std::move(other);
}

CallCapture &CallCapture::operator=(CallCapture &&other)
{
    std::swap(entryPoint, other.entryPoint);
    std::swap(customFunctionName, other.customFunctionName);
    std::swap(params, other.params);
    std::swap(isActive, other.isActive);
    return *this;
}

const char *CallCapture::name() const
{
    if (entryPoint == EntryPoint::GLInvalid)
    {
        ASSERT(!customFunctionName.empty());
        return customFunctionName.c_str();
    }

    return angle::GetEntryPointName(entryPoint);
}

ReplayContext::ReplayContext(size_t readBufferSizebytes,
                             const gl::AttribArray<size_t> &clientArraysSizebytes)
{
    mReadBuffer.resize(readBufferSizebytes);

    for (uint32_t i = 0; i < clientArraysSizebytes.size(); i++)
    {
        mClientArraysBuffer[i].resize(clientArraysSizebytes[i]);
    }
}
ReplayContext::~ReplayContext() {}

FrameCapture::FrameCapture()  = default;
FrameCapture::~FrameCapture() = default;

void FrameCapture::reset()
{
    mSetupCalls.clear();
}

FrameCaptureShared::FrameCaptureShared()
    : mLastContextId{0},
      mEnabled(true),
      mSerializeStateEnabled(false),
      mCompression(true),
      mClientVertexArrayMap{},
      mFrameIndex(1),
      mCaptureStartFrame(1),
      mCaptureEndFrame(0),
      mClientArraySizes{},
      mReadBufferSize(0),
      mHasResourceType{},
      mResourceIDToSetupCalls{},
      mMaxAccessedResourceIDs{},
      mCaptureTrigger(0),
      mCaptureActive(false),
      mWindowSurfaceContextID({0})
{
    reset();

    std::string enabledFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kEnabledVarName, kAndroidEnabled);
    if (enabledFromEnv == "0")
    {
        mEnabled = false;
    }

    std::string pathFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kOutDirectoryVarName, kAndroidOutDir);
    if (pathFromEnv.empty())
    {
        mOutDirectory = GetDefaultOutDirectory();
    }
    else
    {
        mOutDirectory = pathFromEnv;
    }

    // Ensure the capture path ends with a slash.
    if (mOutDirectory.back() != '\\' && mOutDirectory.back() != '/')
    {
        mOutDirectory += '/';
    }

    std::string startFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameStartVarName, kAndroidFrameStart);
    if (!startFromEnv.empty())
    {
        mCaptureStartFrame = atoi(startFromEnv.c_str());
    }
    if (mCaptureStartFrame < 1)
    {
        WARN() << "Cannot use a capture start frame less than 1.";
        mCaptureStartFrame = 1;
    }

    std::string endFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameEndVarName, kAndroidFrameEnd);
    if (!endFromEnv.empty())
    {
        mCaptureEndFrame = atoi(endFromEnv.c_str());
    }

    std::string captureTriggerFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kTriggerVarName, kAndroidTrigger);
    if (!captureTriggerFromEnv.empty())
    {
        mCaptureTrigger = atoi(captureTriggerFromEnv.c_str());

        // If the trigger has been populated, ignore the other frame range variables by setting them
        // to unreasonable values. This isn't perfect, but it is effective.
        mCaptureStartFrame = mCaptureEndFrame = std::numeric_limits<uint32_t>::max();
        INFO() << "Capture trigger detected, disabling capture start/end frame.";
    }

    std::string labelFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kCaptureLabelVarName, kAndroidCaptureLabel);
    if (!labelFromEnv.empty())
    {
        // Optional label to provide unique file names and namespaces
        mCaptureLabel = labelFromEnv;
    }

    std::string compressionFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kCompressionVarName, kAndroidCompression);
    if (compressionFromEnv == "0")
    {
        mCompression = false;
    }
    std::string serializeStateFromEnv = angle::GetEnvironmentVar(kSerializeStateVarName);
    if (serializeStateFromEnv == "1")
    {
        mSerializeStateEnabled = true;
    }

    std::string validateSerialiedStateFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kValidationVarName, kAndroidValidation);
    if (validateSerialiedStateFromEnv == "1")
    {
        mValidateSerializedState = true;
    }

    mValidationExpression =
        GetEnvironmentVarOrUnCachedAndroidProperty(kValidationExprVarName, kAndroidValidationExpr);

    if (!mValidationExpression.empty())
    {
        INFO() << "Validation expression is " << kValidationExprVarName;
    }

    std::string trimEnabledFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kTrimEnabledVarName, kAndroidTrimEnabled);
    if (trimEnabledFromEnv == "0")
    {
        mTrimEnabled = false;
    }

    if (mFrameIndex == mCaptureStartFrame)
    {
        // Capture is starting from the first frame, so set the capture active to ensure all GLES
        // commands issued are handled correctly by maybeCapturePreCallUpdates() and
        // maybeCapturePostCallUpdates().
        setCaptureActive();
    }

    if (mCaptureEndFrame < mCaptureStartFrame)
    {
        mEnabled = false;
    }
}

FrameCaptureShared::~FrameCaptureShared() = default;

void FrameCaptureShared::copyCompressedTextureData(const gl::Context *context,
                                                   const CallCapture &call)
{
    // For compressed textures, we need to copy the source data that was already captured into a new
    // cached texture entry for use during mid-execution capture, rather than reading it back with
    // ANGLE_get_image.

    GLenum srcTarget = call.params.getParam("srcTarget", ParamType::TGLenum, 1).value.GLenumVal;
    GLenum dstTarget = call.params.getParam("dstTarget", ParamType::TGLenum, 7).value.GLenumVal;

    // TODO(anglebug.com/6104): Type of incoming ID varies based on target type, but we're only
    // handling textures for now. If either of these asserts fire, then we need to add renderbuffer
    // support.
    ASSERT(srcTarget == GL_TEXTURE_2D || srcTarget == GL_TEXTURE_2D_ARRAY ||
           srcTarget == GL_TEXTURE_3D || srcTarget == GL_TEXTURE_CUBE_MAP);
    ASSERT(dstTarget == GL_TEXTURE_2D || dstTarget == GL_TEXTURE_2D_ARRAY ||
           dstTarget == GL_TEXTURE_3D || dstTarget == GL_TEXTURE_CUBE_MAP);

    gl::TextureID srcName =
        call.params.getParam("srcName", ParamType::TTextureID, 0).value.TextureIDVal;
    GLint srcLevel = call.params.getParam("srcLevel", ParamType::TGLint, 2).value.GLintVal;
    gl::TextureID dstName =
        call.params.getParam("dstName", ParamType::TTextureID, 6).value.TextureIDVal;
    GLint dstLevel = call.params.getParam("dstLevel", ParamType::TGLint, 8).value.GLintVal;

    // Inspect the dest texture to see if this is compressed in case we need to back it up
    gl::Texture *dstTexture = context->getTexture(dstName);
    ASSERT(dstTexture);

    // Look up its target using dstZ as the slice in case of 3D/Cube/Array
    GLint dstZ = call.params.getParam("dstZ", ParamType::TGLint, 11).value.GLintVal;
    gl::TextureTarget dstTargetPacked = gl::TextureTypeToTarget(dstTexture->getType(), dstZ);

    // Pull the info and check
    const gl::InternalFormat &dstFormat = *dstTexture->getFormat(dstTargetPacked, dstLevel).info;
    if (dstFormat.compressed)
    {
        context->getShareGroup()->getFrameCaptureShared()->copyCachedTextureLevel(
            context, srcName, srcLevel, dstName, dstLevel, call);
    }

    // Also track that the destination texture has been updated
    mResourceTracker.getTrackedResource(ResourceIDType::Texture).setModifiedResource(dstName.value);
}

void FrameCaptureShared::captureCompressedTextureData(const gl::Context *context,
                                                      const CallCapture &call)
{
    // For compressed textures, track a shadow copy of the data
    // for use during mid-execution capture, rather than reading it back
    // with ANGLE_get_image

    // Storing the compressed data is handled the same for all entry points,
    // they just have slightly different parameter locations
    int dataParamOffset    = -1;
    int xoffsetParamOffset = -1;
    int yoffsetParamOffset = -1;
    int zoffsetParamOffset = -1;
    int widthParamOffset   = -1;
    int heightParamOffset  = -1;
    int depthParamOffset   = -1;
    switch (call.entryPoint)
    {
        case EntryPoint::GLCompressedTexSubImage3D:
            xoffsetParamOffset = 2;
            yoffsetParamOffset = 3;
            zoffsetParamOffset = 4;
            widthParamOffset   = 5;
            heightParamOffset  = 6;
            depthParamOffset   = 7;
            dataParamOffset    = 10;
            break;
        case EntryPoint::GLCompressedTexImage3D:
            widthParamOffset  = 3;
            heightParamOffset = 4;
            depthParamOffset  = 5;
            dataParamOffset   = 8;
            break;
        case EntryPoint::GLCompressedTexSubImage2D:
            xoffsetParamOffset = 2;
            yoffsetParamOffset = 3;
            widthParamOffset   = 4;
            heightParamOffset  = 5;
            dataParamOffset    = 8;
            break;
        case EntryPoint::GLCompressedTexImage2D:
            widthParamOffset  = 3;
            heightParamOffset = 4;
            dataParamOffset   = 7;
            break;
        default:
            // There should be no other callers of this function
            ASSERT(0);
            break;
    }

    gl::Buffer *pixelUnpackBuffer =
        context->getState().getTargetBuffer(gl::BufferBinding::PixelUnpack);

    const uint8_t *data = static_cast<const uint8_t *>(
        call.params.getParam("data", ParamType::TvoidConstPointer, dataParamOffset)
            .value.voidConstPointerVal);

    GLsizei imageSize = call.params.getParam("imageSize", ParamType::TGLsizei, dataParamOffset - 1)
                            .value.GLsizeiVal;

    const uint8_t *pixelData = nullptr;

    if (pixelUnpackBuffer)
    {
        // If using pixel unpack buffer, map the buffer and track its data
        ASSERT(!pixelUnpackBuffer->isMapped());
        (void)pixelUnpackBuffer->mapRange(context, reinterpret_cast<GLintptr>(data), imageSize,
                                          GL_MAP_READ_BIT);

        pixelData = reinterpret_cast<const uint8_t *>(pixelUnpackBuffer->getMapPointer());
    }
    else
    {
        pixelData = data;
    }

    if (!pixelData)
    {
        // If no pointer was provided and we weren't able to map the buffer, there is no data to
        // capture
        return;
    }

    // Look up the texture type
    gl::TextureTarget targetPacked =
        call.params.getParam("targetPacked", ParamType::TTextureTarget, 0).value.TextureTargetVal;
    gl::TextureType textureType = gl::TextureTargetToType(targetPacked);

    // Create a copy of the incoming data
    std::vector<uint8_t> compressedData;
    compressedData.assign(pixelData, pixelData + imageSize);

    // Look up the currently bound texture
    gl::Texture *texture = context->getState().getTargetTexture(textureType);
    ASSERT(texture);

    // Record the data, indexed by textureID and level
    GLint level = call.params.getParam("level", ParamType::TGLint, 1).value.GLintVal;
    std::vector<uint8_t> &levelData =
        context->getShareGroup()->getFrameCaptureShared()->getCachedTextureLevelData(
            texture, targetPacked, level, call.entryPoint);

    // Unpack the various pixel rectangle parameters.
    ASSERT(widthParamOffset != -1);
    ASSERT(heightParamOffset != -1);
    GLsizei pixelWidth =
        call.params.getParam("width", ParamType::TGLsizei, widthParamOffset).value.GLsizeiVal;
    GLsizei pixelHeight =
        call.params.getParam("height", ParamType::TGLsizei, heightParamOffset).value.GLsizeiVal;
    GLsizei pixelDepth = 1;
    if (depthParamOffset != -1)
    {
        pixelDepth =
            call.params.getParam("depth", ParamType::TGLsizei, depthParamOffset).value.GLsizeiVal;
    }

    GLint xoffset = 0;
    GLint yoffset = 0;
    GLint zoffset = 0;

    if (xoffsetParamOffset != -1)
    {
        xoffset =
            call.params.getParam("xoffset", ParamType::TGLint, xoffsetParamOffset).value.GLintVal;
    }

    if (yoffsetParamOffset != -1)
    {
        yoffset =
            call.params.getParam("yoffset", ParamType::TGLint, yoffsetParamOffset).value.GLintVal;
    }

    if (zoffsetParamOffset != -1)
    {
        zoffset =
            call.params.getParam("zoffset", ParamType::TGLint, zoffsetParamOffset).value.GLintVal;
    }

    // Get the format of the texture for use with the compressed block size math.
    const gl::InternalFormat &format = *texture->getFormat(targetPacked, level).info;

    // Divide dimensions according to block size.
    const gl::Extents &levelExtents = texture->getExtents(targetPacked, level);

    // Scale down the width/height pixel offsets to reflect block size
    int blockWidth  = static_cast<int>(format.compressedBlockWidth);
    int blockHeight = static_cast<int>(format.compressedBlockHeight);
    ASSERT(format.compressedBlockDepth == 1);

    // Round the incoming width and height up to align with block size
    pixelWidth  = rx::roundUp(pixelWidth, blockWidth);
    pixelHeight = rx::roundUp(pixelHeight, blockHeight);

    // Scale the width, height, and offsets
    pixelWidth /= blockWidth;
    pixelHeight /= blockHeight;
    xoffset /= blockWidth;
    yoffset /= blockHeight;

    GLint pixelBytes = static_cast<GLint>(format.pixelBytes);

    // Also round the texture's width and height up to reflect block size
    int levelWidth  = rx::roundUp(levelExtents.width, blockWidth);
    int levelHeight = rx::roundUp(levelExtents.height, blockHeight);

    GLint pixelRowPitch   = pixelWidth * pixelBytes;
    GLint pixelDepthPitch = pixelRowPitch * pixelHeight;
    GLint levelRowPitch   = (levelWidth / blockWidth) * pixelBytes;
    GLint levelDepthPitch = (levelHeight / blockHeight) * levelRowPitch;

    for (GLint zindex = 0; zindex < pixelDepth; ++zindex)
    {
        GLint z = zindex + zoffset;
        for (GLint yindex = 0; yindex < pixelHeight; ++yindex)
        {
            GLint y           = yindex + yoffset;
            GLint pixelOffset = zindex * pixelDepthPitch + yindex * pixelRowPitch;
            GLint levelOffset = z * levelDepthPitch + y * levelRowPitch + xoffset * pixelBytes;
            ASSERT(static_cast<size_t>(levelOffset + pixelRowPitch) <= levelData.size());
            memcpy(&levelData[levelOffset], &pixelData[pixelOffset], pixelRowPitch);
        }
    }

    if (pixelUnpackBuffer)
    {
        GLboolean success;
        (void)pixelUnpackBuffer->unmap(context, &success);
        ASSERT(success);
    }
}

void FrameCaptureShared::trackBufferMapping(CallCapture *call,
                                            gl::BufferID id,
                                            GLintptr offset,
                                            GLsizeiptr length,
                                            bool writable)
{
    // Track that the buffer was mapped
    mResourceTracker.setBufferMapped(id.value);

    if (writable)
    {
        // If this buffer was mapped writable, we don't have any visibility into what
        // happens to it. Therefore, remember the details about it, and we'll read it back
        // on Unmap to repopulate it during replay.
        mBufferDataMap[id] = std::make_pair(offset, length);

        // Track that this buffer was potentially modified
        mResourceTracker.getTrackedResource(ResourceIDType::Buffer).setModifiedResource(id.value);

        // Track the bufferID that was just mapped for use when writing return value
        call->params.setMappedBufferID(id);
    }
}

void FrameCaptureShared::trackTextureUpdate(const gl::Context *context, const CallCapture &call)
{
    int index             = 0;
    std::string paramName = "targetPacked";

    // Some calls provide the textureID directly
    switch (call.entryPoint)
    {
        case EntryPoint::GLCompressedCopyTextureCHROMIUM:
            index     = 1;
            paramName = "destIdPacked";
            break;
        case EntryPoint::GLCopyTextureCHROMIUM:
        case EntryPoint::GLCopySubTextureCHROMIUM:
        case EntryPoint::GLCopyTexture3DANGLE:
            index     = 3;
            paramName = "destIdPacked";
            break;
        default:
            break;
    }

    // For the rest, look it up based on the currently bound texture
    GLuint id = 0;
    if (index == 0)
    {
        gl::TextureTarget targetPacked =
            call.params.getParam(paramName.c_str(), ParamType::TTextureTarget, index)
                .value.TextureTargetVal;
        gl::TextureType textureType = gl::TextureTargetToType(targetPacked);
        gl::Texture *texture        = context->getState().getTargetTexture(textureType);
        id                          = texture->id().value;
    }
    else
    {
        gl::TextureID destIDPacked =
            call.params.getParam(paramName.c_str(), ParamType::TTextureID, index)
                .value.TextureIDVal;
        id = destIDPacked.value;
    }

    // Mark it as modified
    mResourceTracker.getTrackedResource(ResourceIDType::Texture).setModifiedResource(id);
}

void FrameCaptureShared::updateCopyImageSubData(CallCapture &call)
{
    // This call modifies srcName and dstName to no longer be object IDs (GLuint), but actual
    // packed types that can remapped using gTextureMap and gRenderbufferMap

    GLint srcName    = call.params.getParam("srcName", ParamType::TGLuint, 0).value.GLuintVal;
    GLenum srcTarget = call.params.getParam("srcTarget", ParamType::TGLenum, 1).value.GLenumVal;
    switch (srcTarget)
    {
        case GL_RENDERBUFFER:
        {
            // Convert the GLuint to RenderbufferID
            gl::RenderbufferID srcRenderbufferID = {static_cast<GLuint>(srcName)};
            call.params.setValueParamAtIndex("srcName", ParamType::TRenderbufferID,
                                             srcRenderbufferID, 0);
            break;
        }
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        {
            // Convert the GLuint to TextureID
            gl::TextureID srcTextureID = {static_cast<GLuint>(srcName)};
            call.params.setValueParamAtIndex("srcName", ParamType::TTextureID, srcTextureID, 0);
            break;
        }
        default:
            ERR() << "Unhandled srcTarget = " << srcTarget;
            UNREACHABLE();
            break;
    }

    // Change dstName to the appropriate type based on dstTarget
    GLint dstName    = call.params.getParam("dstName", ParamType::TGLuint, 6).value.GLuintVal;
    GLenum dstTarget = call.params.getParam("dstTarget", ParamType::TGLenum, 7).value.GLenumVal;
    switch (dstTarget)
    {
        case GL_RENDERBUFFER:
        {
            // Convert the GLuint to RenderbufferID
            gl::RenderbufferID dstRenderbufferID = {static_cast<GLuint>(dstName)};
            call.params.setValueParamAtIndex("dstName", ParamType::TRenderbufferID,
                                             dstRenderbufferID, 6);
            break;
        }
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        {
            // Convert the GLuint to TextureID
            gl::TextureID dstTextureID = {static_cast<GLuint>(dstName)};
            call.params.setValueParamAtIndex("dstName", ParamType::TTextureID, dstTextureID, 6);
            break;
        }
        default:
            ERR() << "Unhandled dstTarget = " << dstTarget;
            UNREACHABLE();
            break;
    }
}

void FrameCaptureShared::overrideProgramBinary(const gl::Context *context,
                                               CallCapture &inCall,
                                               std::vector<CallCapture> &outCalls)
{
    // Program binaries are inherently non-portable, even between two ANGLE builds.
    // If an application is using glProgramBinary in the middle of a trace, we need to replace
    // those calls with an equivalent sequence of portable calls.
    //
    // For example, here is a sequence an app could use for glProgramBinary:
    //
    //   gShaderProgramMap[42] = glCreateProgram();
    //   glProgramBinary(gShaderProgramMap[42], GL_PROGRAM_BINARY_ANGLE, gBinaryData[x], 1000);
    //   glGetProgramiv(gShaderProgramMap[42], GL_LINK_STATUS, gReadBuffer);
    //   glGetProgramiv(gShaderProgramMap[42], GL_PROGRAM_BINARY_LENGTH, gReadBuffer);
    //
    // With this override, the glProgramBinary call will be replaced like so:
    //
    //   gShaderProgramMap[42] = glCreateProgram();
    //   === Begin override ===
    //   gShaderProgramMap[43] = glCreateShader(GL_VERTEX_SHADER);
    //   glShaderSource(gShaderProgramMap[43], 1, string_0, &gBinaryData[100]);
    //   glCompileShader(gShaderProgramMap[43]);
    //   glAttachShader(gShaderProgramMap[42], gShaderProgramMap[43]);
    //   glDeleteShader(gShaderProgramMap[43]);
    //   gShaderProgramMap[43] = glCreateShader(GL_FRAGMENT_SHADER);
    //   glShaderSource(gShaderProgramMap[43], 1, string_1, &gBinaryData[200]);
    //   glCompileShader(gShaderProgramMap[43]);
    //   glAttachShader(gShaderProgramMap[42], gShaderProgramMap[43]);
    //   glDeleteShader(gShaderProgramMap[43]);
    //   glBindAttribLocation(gShaderProgramMap[42], 0, "attrib1");
    //   glBindAttribLocation(gShaderProgramMap[42], 1, "attrib2");
    //   glLinkProgram(gShaderProgramMap[42]);
    //   UpdateUniformLocation(gShaderProgramMap[42], "foo", 0, 20);
    //   UpdateUniformLocation(gShaderProgramMap[42], "bar", 72, 1);
    //   glUseProgram(gShaderProgramMap[42]);
    //   UpdateCurrentProgram(gShaderProgramMap[42]);
    //   glUniform4fv(gUniformLocations[gCurrentProgram][0], 20, &gBinaryData[300]);
    //   glUniform1iv(gUniformLocations[gCurrentProgram][72], 1, &gBinaryData[400]);
    //   === End override ===
    //   glGetProgramiv(gShaderProgramMap[42], GL_LINK_STATUS, gReadBuffer);
    //   glGetProgramiv(gShaderProgramMap[42], GL_PROGRAM_BINARY_LENGTH, gReadBuffer);
    //
    // To facilitate this override, we are serializing each shader stage source into the binary
    // itself.  See Program::serialize and Program::deserialize.  Once extracted from the binary,
    // they will be available via getProgramSources.

    gl::ShaderProgramID id = inCall.params.getParam("programPacked", ParamType::TShaderProgramID, 0)
                                 .value.ShaderProgramIDVal;

    gl::Program *program = context->getProgramResolveLink(id);
    ASSERT(program);

    GenerateLinkedProgram(context, context->getState(), &mResourceTracker, &outCalls, program, id,
                          getProgramSources(id));
}

void FrameCaptureShared::maybeOverrideEntryPoint(const gl::Context *context,
                                                 CallCapture &inCall,
                                                 std::vector<CallCapture> &outCalls)
{
    switch (inCall.entryPoint)
    {
        case EntryPoint::GLEGLImageTargetTexture2DOES:
        {
            // We don't support reading EGLImages. Instead, just pull from a tiny null texture.
            // TODO (anglebug.com/4964): Read back the image data and populate the texture.
            std::vector<uint8_t> pixelData = {0, 0, 0, 0};
            outCalls.emplace_back(
                CaptureTexSubImage2D(context->getState(), true, gl::TextureTarget::_2D, 0, 0, 0, 1,
                                     1, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data()));
            break;
        }
        case EntryPoint::GLEGLImageTargetRenderbufferStorageOES:
        {
            UNIMPLEMENTED();
            break;
        }
        case EntryPoint::GLCopyImageSubData:
        case EntryPoint::GLCopyImageSubDataEXT:
        case EntryPoint::GLCopyImageSubDataOES:
        {
            // We must look at the src and dst target types to determine which remap table to use
            updateCopyImageSubData(inCall);
            outCalls.emplace_back(std::move(inCall));
            break;
        }
        case EntryPoint::GLProgramBinary:
        case EntryPoint::GLProgramBinaryOES:
        {
            // Binary formats are not portable at all, so replace the calls with full linking
            // sequence
            overrideProgramBinary(context, inCall, outCalls);
            break;
        }
        default:
        {
            // Pass the single call through
            outCalls.emplace_back(std::move(inCall));
            break;
        }
    }
}

void FrameCaptureShared::maybeCaptureDrawArraysClientData(const gl::Context *context,
                                                          CallCapture &call,
                                                          size_t instanceCount)
{
    if (!context->getStateCache().hasAnyActiveClientAttrib())
    {
        return;
    }

    // Get counts from paramBuffer.
    GLint firstVertex =
        call.params.getParamFlexName("first", "start", ParamType::TGLint, 1).value.GLintVal;
    GLsizei drawCount = call.params.getParam("count", ParamType::TGLsizei, 2).value.GLsizeiVal;
    captureClientArraySnapshot(context, firstVertex + drawCount, instanceCount);
}

void FrameCaptureShared::maybeCaptureDrawElementsClientData(const gl::Context *context,
                                                            CallCapture &call,
                                                            size_t instanceCount)
{
    if (!context->getStateCache().hasAnyActiveClientAttrib())
    {
        return;
    }

    // if the count is zero then the index evaluation is not valid and we wouldn't be drawing
    // anything anyway, so skip capturing
    GLsizei count = call.params.getParam("count", ParamType::TGLsizei, 1).value.GLsizeiVal;
    if (count == 0)
    {
        return;
    }

    gl::DrawElementsType drawElementsType =
        call.params.getParam("typePacked", ParamType::TDrawElementsType, 2)
            .value.DrawElementsTypeVal;
    const void *indices =
        call.params.getParam("indices", ParamType::TvoidConstPointer, 3).value.voidConstPointerVal;

    gl::IndexRange indexRange;

    bool restart = context->getState().isPrimitiveRestartEnabled();

    gl::Buffer *elementArrayBuffer = context->getState().getVertexArray()->getElementArrayBuffer();
    if (elementArrayBuffer)
    {
        size_t offset = reinterpret_cast<size_t>(indices);
        (void)elementArrayBuffer->getIndexRange(context, drawElementsType, offset, count, restart,
                                                &indexRange);
    }
    else
    {
        ASSERT(indices);
        indexRange = gl::ComputeIndexRange(drawElementsType, indices, count, restart);
    }

    // index starts from 0
    captureClientArraySnapshot(context, indexRange.end + 1, instanceCount);
}

void FrameCaptureShared::maybeCapturePreCallUpdates(
    const gl::Context *context,
    CallCapture &call,
    std::vector<CallCapture> *shareGroupSetupCalls,
    ResourceIDToSetupCallsMap *resourceIDToSetupCalls)
{
    switch (call.entryPoint)
    {
        case EntryPoint::GLVertexAttribPointer:
        case EntryPoint::GLVertexPointer:
        case EntryPoint::GLColorPointer:
        case EntryPoint::GLTexCoordPointer:
        case EntryPoint::GLNormalPointer:
        case EntryPoint::GLPointSizePointerOES:
        {
            // Get array location
            GLuint index = 0;
            if (call.entryPoint == EntryPoint::GLVertexAttribPointer)
            {
                index = call.params.getParam("index", ParamType::TGLuint, 0).value.GLuintVal;
            }
            else
            {
                gl::ClientVertexArrayType type;
                switch (call.entryPoint)
                {
                    case EntryPoint::GLVertexPointer:
                        type = gl::ClientVertexArrayType::Vertex;
                        break;
                    case EntryPoint::GLColorPointer:
                        type = gl::ClientVertexArrayType::Color;
                        break;
                    case EntryPoint::GLTexCoordPointer:
                        type = gl::ClientVertexArrayType::TextureCoord;
                        break;
                    case EntryPoint::GLNormalPointer:
                        type = gl::ClientVertexArrayType::Normal;
                        break;
                    case EntryPoint::GLPointSizePointerOES:
                        type = gl::ClientVertexArrayType::PointSize;
                        break;
                    default:
                        UNREACHABLE();
                        type = gl::ClientVertexArrayType::InvalidEnum;
                }
                index = gl::GLES1Renderer::VertexArrayIndex(type, context->getState().gles1());
            }

            if (call.params.hasClientArrayData())
            {
                mClientVertexArrayMap[index] = static_cast<int>(mFrameCalls.size());
            }
            else
            {
                mClientVertexArrayMap[index] = -1;
            }
            break;
        }

        case EntryPoint::GLGenTextures:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::TextureID *textureIDs =
                call.params.getParam("texturesPacked", ParamType::TTextureIDPointer, 1)
                    .value.TextureIDPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                // If we're capturing, track what new textures have been genned
                handleGennedResource(textureIDs[i]);
            }
            break;
        }

        case EntryPoint::GLDeleteBuffers:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::BufferID *bufferIDs =
                call.params.getParam("buffersPacked", ParamType::TBufferIDConstPointer, 1)
                    .value.BufferIDConstPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                // For each buffer being deleted, check our backup of data and remove it
                const auto &bufferDataInfo = mBufferDataMap.find(bufferIDs[i]);
                if (bufferDataInfo != mBufferDataMap.end())
                {
                    mBufferDataMap.erase(bufferDataInfo);
                }
                // If we're capturing, track what buffers have been deleted
                handleDeletedResource(bufferIDs[i]);
            }
            break;
        }

        case EntryPoint::GLGenBuffers:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::BufferID *bufferIDs =
                call.params.getParam("buffersPacked", ParamType::TBufferIDPointer, 1)
                    .value.BufferIDPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                handleGennedResource(bufferIDs[i]);
            }
            break;
        }

        case EntryPoint::GLDeleteProgramPipelines:
        case EntryPoint::GLDeleteProgramPipelinesEXT:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::ProgramPipelineID *pipelineIDs =
                call.params
                    .getParam("pipelinesPacked", ParamType::TProgramPipelineIDConstPointer, 1)
                    .value.ProgramPipelineIDPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                handleDeletedResource(pipelineIDs[i]);
            }
            break;
        }

        case EntryPoint::GLGenProgramPipelines:
        case EntryPoint::GLGenProgramPipelinesEXT:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::ProgramPipelineID *pipelineIDs =
                call.params.getParam("pipelinesPacked", ParamType::TProgramPipelineIDPointer, 1)
                    .value.ProgramPipelineIDPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                handleGennedResource(pipelineIDs[i]);
            }
            break;
        }

        case EntryPoint::GLDeleteSync:
        {
            GLsync sync = call.params.getParam("sync", ParamType::TGLsync, 0).value.GLsyncVal;
            FrameCaptureShared *frameCaptureShared =
                context->getShareGroup()->getFrameCaptureShared();
            // If we're capturing, track which fence sync has been deleted
            if (frameCaptureShared->isCaptureActive())
            {
                mResourceTracker.setDeletedFenceSync(sync);
            }
            break;
        }

        case EntryPoint::GLDrawArrays:
        {
            maybeCaptureDrawArraysClientData(context, call, 1);
            break;
        }

        case EntryPoint::GLDrawArraysInstanced:
        case EntryPoint::GLDrawArraysInstancedANGLE:
        case EntryPoint::GLDrawArraysInstancedEXT:
        {
            GLsizei instancecount =
                call.params.getParamFlexName("instancecount", "primcount", ParamType::TGLsizei, 3)
                    .value.GLsizeiVal;
            maybeCaptureDrawArraysClientData(context, call, instancecount);
            break;
        }

        case EntryPoint::GLDrawElements:
        {
            maybeCaptureDrawElementsClientData(context, call, 1);
            break;
        }

        case EntryPoint::GLDrawElementsInstanced:
        case EntryPoint::GLDrawElementsInstancedANGLE:
        case EntryPoint::GLDrawElementsInstancedEXT:
        {
            GLsizei instancecount =
                call.params.getParamFlexName("instancecount", "primcount", ParamType::TGLsizei, 4)
                    .value.GLsizeiVal;
            maybeCaptureDrawElementsClientData(context, call, instancecount);
            break;
        }

        case EntryPoint::GLCreateShaderProgramv:
        {
            // Refresh the cached shader sources.
            // The command CreateShaderProgramv() creates a stand-alone program from an array of
            // null-terminated source code strings for a single shader type, so we need update the
            // Shader and Program sources, similar to GLCompileShader + GLLinkProgram handling.
            gl::ShaderProgramID programID = {call.params.getReturnValue().value.GLuintVal};
            const ParamCapture &paramCapture =
                call.params.getParam("typePacked", ParamType::TShaderType, 0);
            gl::ShaderType shaderType = paramCapture.value.ShaderTypeVal;
            gl::Program *program      = context->getProgramResolveLink(programID);
            ASSERT(program);
            const gl::Shader *shader = program->getAttachedShader(shaderType);
            ASSERT(shader);
            setShaderSource(shader->getHandle(), shader->getSourceString());
            setProgramSources(programID, GetAttachedProgramSources(program));
            handleGennedResource(programID);
            break;
        }

        case EntryPoint::GLCreateProgram:
        {
            // If we're capturing, track which programs have been created
            gl::ShaderProgramID programID = {call.params.getReturnValue().value.GLuintVal};
            handleGennedResource(programID);
            break;
        }

        case EntryPoint::GLDeleteProgram:
        {
            // If we're capturing, track which programs have been deleted
            const ParamCapture &param =
                call.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
            handleDeletedResource(param.value.ShaderProgramIDVal);
            break;
        }

        case EntryPoint::GLCompileShader:
        {
            // Refresh the cached shader sources.
            gl::ShaderProgramID shaderID =
                call.params.getParam("shaderPacked", ParamType::TShaderProgramID, 0)
                    .value.ShaderProgramIDVal;
            const gl::Shader *shader = context->getShader(shaderID);
            // Shaders compiled for ProgramBinary will not have a shader created
            if (shader)
            {
                setShaderSource(shaderID, shader->getSourceString());
            }
            break;
        }

        case EntryPoint::GLLinkProgram:
        {
            // Refresh the cached program sources.
            gl::ShaderProgramID programID =
                call.params.getParam("programPacked", ParamType::TShaderProgramID, 0)
                    .value.ShaderProgramIDVal;
            const gl::Program *program = context->getProgramResolveLink(programID);
            // Programs linked in support of ProgramBinary will not have attached shaders
            if (program->getState().hasAttachedShader())
            {
                setProgramSources(programID, GetAttachedProgramSources(program));
            }
            break;
        }

        case EntryPoint::GLCompressedTexImage1D:
        case EntryPoint::GLCompressedTexSubImage1D:
        {
            UNIMPLEMENTED();
            break;
        }

        case EntryPoint::GLCompressedTexImage2D:
        case EntryPoint::GLCompressedTexImage3D:
        case EntryPoint::GLCompressedTexSubImage2D:
        case EntryPoint::GLCompressedTexSubImage3D:
        {
            captureCompressedTextureData(context, call);
            break;
        }

        case EntryPoint::GLCopyImageSubData:
        case EntryPoint::GLCopyImageSubDataEXT:
        case EntryPoint::GLCopyImageSubDataOES:
        {
            // glCopyImageSubData supports copying compressed and uncompressed texture formats.
            copyCompressedTextureData(context, call);
            break;
        }

        case EntryPoint::GLDeleteTextures:
        {
            // Free any TextureLevelDataMap entries being tracked for this texture
            // This is to cover the scenario where a texture has been created, its
            // levels cached, then texture deleted and recreated, receiving the same ID

            // Look up how many textures are being deleted
            GLsizei n = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;

            // Look up the pointer to list of textures
            const gl::TextureID *textureIDs =
                call.params.getParam("texturesPacked", ParamType::TTextureIDConstPointer, 1)
                    .value.TextureIDConstPointerVal;

            // For each texture listed for deletion
            for (int32_t i = 0; i < n; ++i)
            {
                // Look it up in the cache, and delete it if found
                deleteCachedTextureLevelData(textureIDs[i]);

                // If we're capturing, track what textures have been deleted
                handleDeletedResource(textureIDs[i]);
            }
            break;
        }

        case EntryPoint::GLMapBuffer:
        case EntryPoint::GLMapBufferOES:
        {
            gl::BufferBinding target =
                call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                    .value.BufferBindingVal;

            GLbitfield access =
                call.params.getParam("access", ParamType::TGLenum, 1).value.GLenumVal;

            gl::Buffer *buffer = context->getState().getTargetBuffer(target);

            GLintptr offset   = 0;
            GLsizeiptr length = static_cast<GLsizeiptr>(buffer->getSize());

            bool writable =
                access == GL_WRITE_ONLY_OES || access == GL_WRITE_ONLY || access == GL_READ_WRITE;

            FrameCaptureShared *frameCaptureShared =
                context->getShareGroup()->getFrameCaptureShared();
            frameCaptureShared->trackBufferMapping(&call, buffer->id(), offset, length, writable);
            break;
        }

        case EntryPoint::GLUnmapNamedBuffer:
        {
            UNIMPLEMENTED();
            break;
        }

        case EntryPoint::GLMapBufferRange:
        case EntryPoint::GLMapBufferRangeEXT:
        {
            GLintptr offset =
                call.params.getParam("offset", ParamType::TGLintptr, 1).value.GLintptrVal;
            GLsizeiptr length =
                call.params.getParam("length", ParamType::TGLsizeiptr, 2).value.GLsizeiptrVal;
            GLbitfield access =
                call.params.getParam("access", ParamType::TGLbitfield, 3).value.GLbitfieldVal;

            gl::BufferBinding target =
                call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                    .value.BufferBindingVal;
            gl::Buffer *buffer = context->getState().getTargetBuffer(target);

            FrameCaptureShared *frameCaptureShared =
                context->getShareGroup()->getFrameCaptureShared();
            frameCaptureShared->trackBufferMapping(&call, buffer->id(), offset, length,
                                                   access & GL_MAP_WRITE_BIT);
            break;
        }

        case EntryPoint::GLUnmapBuffer:
        case EntryPoint::GLUnmapBufferOES:
        {
            // See if we need to capture the buffer contents
            captureMappedBufferSnapshot(context, call);

            // Track that the buffer was unmapped, for use during state reset
            gl::BufferBinding target =
                call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                    .value.BufferBindingVal;
            gl::Buffer *buffer = context->getState().getTargetBuffer(target);
            mResourceTracker.setBufferUnmapped(buffer->id().value);
            break;
        }

        case EntryPoint::GLBufferData:
        case EntryPoint::GLBufferSubData:
        {
            gl::BufferBinding target =
                call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                    .value.BufferBindingVal;

            gl::Buffer *buffer = context->getState().getTargetBuffer(target);

            // Track that this buffer's contents have been modified
            mResourceTracker.getTrackedResource(ResourceIDType::Buffer)
                .setModifiedResource(buffer->id().value);

            // BufferData is equivalent to UnmapBuffer, for what we're tracking.
            // From the ES 3.1 spec in BufferData section:
            //     If any portion of the buffer object is mapped in the current context or any
            //     context current to another thread, it is as though UnmapBuffer (see section
            //     6.3.1) is executed in each such context prior to deleting the existing data
            //     store.
            // Track that the buffer was unmapped, for use during state reset
            mResourceTracker.setBufferUnmapped(buffer->id().value);

            break;
        }
        default:
            break;
    }

    if (IsTextureUpdate(call))
    {
        // If this call modified texture contents, track it for possible reset
        trackTextureUpdate(context, call);
    }

    updateReadBufferSize(call.params.getReadBufferSize());

    gl::ShaderProgramID shaderProgramID;
    if (FindShaderProgramIDInCall(call, &shaderProgramID))
    {
        mResourceTracker.onShaderProgramAccess(shaderProgramID);

        if (isCaptureActive())
        {
            // Track that this call referenced a ShaderProgram, setting it active for Setup
            MarkResourceIDActive(ResourceIDType::ShaderProgram, shaderProgramID.value,
                                 shareGroupSetupCalls, resourceIDToSetupCalls);
        }
    }

    updateResourceCounts(call);
}

void FrameCaptureShared::updateResourceCounts(const CallCapture &call)
{
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        ResourceIDType idType = GetResourceIDTypeFromParamType(param.type);
        if (idType != ResourceIDType::InvalidEnum)
        {
            mHasResourceType.set(idType);

            // Capture resource IDs for non-pointer types.
            if (strcmp(ParamTypeToString(param.type), "GLuint") == 0)
            {
                mMaxAccessedResourceIDs[idType] =
                    std::max(mMaxAccessedResourceIDs[idType], param.value.GLuintVal);
            }
            // Capture resource IDs for pointer types.
            if (strstr(ParamTypeToString(param.type), "GLuint *") != nullptr)
            {
                if (param.data.size() == 1u)
                {
                    const GLuint *dataPtr = reinterpret_cast<const GLuint *>(param.data[0].data());
                    size_t numHandles     = param.data[0].size() / sizeof(GLuint);
                    for (size_t handleIndex = 0; handleIndex < numHandles; ++handleIndex)
                    {
                        mMaxAccessedResourceIDs[idType] =
                            std::max(mMaxAccessedResourceIDs[idType], dataPtr[handleIndex]);
                    }
                }
            }
        }
    }
}

void FrameCaptureShared::captureCall(const gl::Context *context,
                                     CallCapture &&inCall,
                                     bool isCallValid)
{
    if (SkipCall(inCall.entryPoint))
    {
        return;
    }

    if (isCallValid)
    {
        // If the context ID has changed, then we need to inject an eglMakeCurrent() call. Only do
        // this if there is more than 1 context in the share group to avoid unnecessary
        // eglMakeCurrent() calls.
        size_t contextCount = context->getShareGroup()->getShareGroupContextCount();
        if (contextCount > 1 && mLastContextId != context->id())
        {
            // Inject the eglMakeCurrent() call.
            // The EGLDisplay and EGLSurface values can't be known here, since we may not even be
            // running the trace with ANGLE.
            // The EGLContext value is actually the context ID, so we can look it up in
            // 'gContextMap'.
            // Need to go from uint32 -> uint64 -> EGLContext (void*) to handle MSVC compiler
            // warning on 64b systems:
            //   error C4312: 'reinterpret_cast': conversion from 'uint32_t' to 'EGLContext' of
            //   greater size
            uint64_t contextID    = static_cast<uint64_t>(context->id().value);
            EGLContext eglContext = reinterpret_cast<EGLContext>(contextID);
            CallCapture makeCurrentCall =
                CaptureMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);
            mFrameCalls.emplace_back(std::move(makeCurrentCall));
            mLastContextId = context->id();
        }

        std::vector<CallCapture> outCalls;
        maybeOverrideEntryPoint(context, inCall, outCalls);

        // Need to loop on any new calls we added during override
        for (CallCapture &call : outCalls)
        {
            // During capture, consider all frame calls active
            if (isCaptureActive())
            {
                call.isActive = true;
            }

            maybeCapturePreCallUpdates(context, call, &mShareGroupSetupCalls,
                                       &mResourceIDToSetupCalls);
            mFrameCalls.emplace_back(std::move(call));
            maybeCapturePostCallUpdates(context);
        }

        // Evaluate the validation expression to determine if we insert a validation checkpoint.
        // This lets the user pick a subset of calls to check instead of checking every call.
        if (mValidateSerializedState && !mValidationExpression.empty())
        {
            // Example substitution for frame #2, call #110:
            // Before: (call == 2) && (frame >= 100) && (frame <= 120) && ((frame % 10) == 0)
            // After:  (2 == 2) && (110 >= 100) && (110 <= 120) && ((110 % 10) == 0)
            // Evaluates to 1.0.
            std::string expression = mValidationExpression;

            angle::ReplaceAllSubstrings(&expression, "frame", std::to_string(mFrameIndex));
            angle::ReplaceAllSubstrings(&expression, "call", std::to_string(mFrameCalls.size()));

            double result = ceval_result(expression);
            if (result > 0)
            {
                CaptureValidateSerializedState(context, &mFrameCalls);
            }
        }
    }
    else
    {
        INFO() << "FrameCapture: Not capturing invalid call to "
               << GetEntryPointName(inCall.entryPoint);
    }
}

void FrameCaptureShared::maybeCapturePostCallUpdates(const gl::Context *context)
{
    // Process resource ID updates.
    MaybeCaptureUpdateResourceIDs(&mFrameCalls);

    CallCapture &lastCall = mFrameCalls.back();
    switch (lastCall.entryPoint)
    {
        case EntryPoint::GLCreateShaderProgramv:
        {
            gl::ShaderProgramID programId;
            programId.value            = lastCall.params.getReturnValue().value.GLuintVal;
            const gl::Program *program = context->getProgramResolveLink(programId);
            CaptureUpdateUniformLocations(program, &mFrameCalls);
            CaptureUpdateUniformBlockIndexes(program, &mFrameCalls);
            break;
        }
        case EntryPoint::GLLinkProgram:
        {
            const ParamCapture &param =
                lastCall.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
            const gl::Program *program =
                context->getProgramResolveLink(param.value.ShaderProgramIDVal);
            CaptureUpdateUniformLocations(program, &mFrameCalls);
            CaptureUpdateUniformBlockIndexes(program, &mFrameCalls);
            break;
        }
        case EntryPoint::GLUseProgram:
            CaptureUpdateCurrentProgram(lastCall, &mFrameCalls);
            break;
        case EntryPoint::GLDeleteProgram:
        {
            const ParamCapture &param =
                lastCall.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
            CaptureDeleteUniformLocations(param.value.ShaderProgramIDVal, &mFrameCalls);
            break;
        }
        case EntryPoint::GLShaderSource:
        {
            lastCall.params.setValueParamAtIndex("count", ParamType::TGLsizei, 1, 1);

            ParamCapture &paramLength =
                lastCall.params.getParam("length", ParamType::TGLintConstPointer, 3);
            paramLength.data.resize(1);
            // Set the length parameter to {-1} to signal that the actual string length
            // is to be used. Since we store the parameter blob as an array of four uint8_t
            // values, we have to pass the binary equivalent of -1.
            paramLength.data[0] = {0xff, 0xff, 0xff, 0xff};
            break;
        }
        default:
            break;
    }
}

void FrameCaptureShared::captureClientArraySnapshot(const gl::Context *context,
                                                    size_t vertexCount,
                                                    size_t instanceCount)
{
    const gl::VertexArray *vao = context->getState().getVertexArray();

    // Capture client array data.
    for (size_t attribIndex : context->getStateCache().getActiveClientAttribsMask())
    {
        const gl::VertexAttribute &attrib = vao->getVertexAttribute(attribIndex);
        const gl::VertexBinding &binding  = vao->getVertexBinding(attrib.bindingIndex);

        int callIndex = mClientVertexArrayMap[attribIndex];

        if (callIndex != -1)
        {
            size_t count = vertexCount;

            if (binding.getDivisor() > 0)
            {
                count = rx::UnsignedCeilDivide(static_cast<uint32_t>(instanceCount),
                                               binding.getDivisor());
            }

            // The last capture element doesn't take up the full stride.
            size_t bytesToCapture = (count - 1) * binding.getStride() + attrib.format->pixelBytes;

            CallCapture &call   = mFrameCalls[callIndex];
            ParamCapture &param = call.params.getClientArrayPointerParameter();
            ASSERT(param.type == ParamType::TvoidConstPointer);

            ParamBuffer updateParamBuffer;
            updateParamBuffer.addValueParam<GLint>("arrayIndex", ParamType::TGLint,
                                                   static_cast<uint32_t>(attribIndex));

            ParamCapture updateMemory("pointer", ParamType::TvoidConstPointer);
            CaptureMemory(param.value.voidConstPointerVal, bytesToCapture, &updateMemory);
            updateParamBuffer.addParam(std::move(updateMemory));

            updateParamBuffer.addValueParam<GLuint64>("size", ParamType::TGLuint64, bytesToCapture);

            mFrameCalls.emplace_back("UpdateClientArrayPointer", std::move(updateParamBuffer));

            mClientArraySizes[attribIndex] =
                std::max(mClientArraySizes[attribIndex], bytesToCapture);
        }
    }
}

void FrameCaptureShared::captureMappedBufferSnapshot(const gl::Context *context,
                                                     const CallCapture &call)
{
    // If the buffer was mapped writable, we need to restore its data, since we have no
    // visibility into what the client did to the buffer while mapped.
    // This sequence will result in replay calls like this:
    //   ...
    //   gMappedBufferData[gBufferMap[42]] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 65536,
    //                                                        GL_MAP_WRITE_BIT);
    //   ...
    //   UpdateClientBufferData(42, &gBinaryData[164631024], 65536);
    //   glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    //   ...

    // Re-map the buffer, using the info we tracked about the buffer
    gl::BufferBinding target =
        call.params.getParam("targetPacked", ParamType::TBufferBinding, 0).value.BufferBindingVal;

    FrameCaptureShared *frameCaptureShared = context->getShareGroup()->getFrameCaptureShared();
    gl::Buffer *buffer                     = context->getState().getTargetBuffer(target);
    if (!frameCaptureShared->hasBufferData(buffer->id()))
    {
        // This buffer was not marked writable, so we did not back it up
        return;
    }

    std::pair<GLintptr, GLsizeiptr> bufferDataOffsetAndLength =
        frameCaptureShared->getBufferDataOffsetAndLength(buffer->id());
    GLintptr offset   = bufferDataOffsetAndLength.first;
    GLsizeiptr length = bufferDataOffsetAndLength.second;

    // Map the buffer so we can copy its contents out
    ASSERT(!buffer->isMapped());
    angle::Result result = buffer->mapRange(context, offset, length, GL_MAP_READ_BIT);
    if (result != angle::Result::Continue)
    {
        ERR() << "Failed to mapRange of buffer" << std::endl;
    }
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer->getMapPointer());

    // Create the parameters to our helper for use during replay
    ParamBuffer dataParamBuffer;

    // Pass in the target buffer ID
    dataParamBuffer.addValueParam("dest", ParamType::TGLuint, buffer->id().value);

    // Capture the current buffer data with a binary param
    ParamCapture captureData("source", ParamType::TvoidConstPointer);
    CaptureMemory(data, length, &captureData);
    dataParamBuffer.addParam(std::move(captureData));

    // Also track its size for use with memcpy
    dataParamBuffer.addValueParam<GLsizeiptr>("size", ParamType::TGLsizeiptr, length);

    // Call the helper that populates the buffer with captured data
    mFrameCalls.emplace_back("UpdateClientBufferData2", std::move(dataParamBuffer));

    // Unmap the buffer and move on
    GLboolean dontCare;
    (void)buffer->unmap(context, &dontCare);
}

void FrameCaptureShared::checkForCaptureTrigger()
{
    // If the capture trigger has not been set, move on
    if (mCaptureTrigger == 0)
    {
        return;
    }

    // Otherwise, poll the value for a change
    std::string captureTriggerStr = GetCaptureTrigger();
    if (captureTriggerStr.empty())
    {
        return;
    }

    // If the value has changed, use the original value as the frame count
    // TODO (anglebug.com/4949): Improve capture at unknown frame time. It is good to
    // avoid polling if the feature is not enabled, but not entirely intuitive to set
    // a value to zero when you want to trigger it.
    uint32_t captureTrigger = atoi(captureTriggerStr.c_str());
    if (captureTrigger != mCaptureTrigger)
    {
        // Start mid-execution capture for the current frame
        mCaptureStartFrame = mFrameIndex + 1;

        // Use the original trigger value as the frame count
        mCaptureEndFrame = mCaptureStartFrame + mCaptureTrigger - 1;

        INFO() << "Capture triggered after frame " << mFrameIndex << " for " << mCaptureTrigger
               << " frames";

        // Stop polling
        mCaptureTrigger = 0;
    }
}

void FrameCaptureShared::scanSetupCalls(const gl::Context *context,
                                        std::vector<CallCapture> &setupCalls)
{
    // Scan all the instructions in the list for tracking
    for (CallCapture &call : setupCalls)
    {
        updateReadBufferSize(call.params.getReadBufferSize());
        updateResourceCounts(call);
    }
}

void FrameCaptureShared::runMidExecutionCapture(const gl::Context *mainContext)
{
    // Make sure all pending work for every Context in the share group has completed so all data
    // (buffers, textures, etc.) has been updated and no resources are in use.
    egl::ShareGroup *shareGroup = mainContext->getShareGroup();
    shareGroup->finishAllContexts();

    const gl::State &contextState = mainContext->getState();
    gl::State mainContextReplayState(nullptr, nullptr, nullptr, nullptr, nullptr, EGL_OPENGL_ES_API,
                                     contextState.getClientVersion(), false, true, true, true,
                                     false, EGL_CONTEXT_PRIORITY_MEDIUM_IMG,
                                     contextState.hasProtectedContent());
    mainContextReplayState.initializeForCapture(mainContext);

    CaptureShareGroupMidExecutionSetup(mainContext, &mShareGroupSetupCalls, &mResourceTracker,
                                       mainContextReplayState);

    scanSetupCalls(mainContext, mShareGroupSetupCalls);

    for (const gl::Context *shareContext : shareGroup->getContexts())
    {
        FrameCapture *frameCapture = shareContext->getFrameCapture();
        ASSERT(frameCapture->getSetupCalls().empty());

        if (shareContext->id() == mainContext->id())
        {
            CaptureMidExecutionSetup(shareContext, &frameCapture->getSetupCalls(),
                                     &mShareGroupSetupCalls, &mResourceIDToSetupCalls,
                                     &mResourceTracker, mainContextReplayState,
                                     mValidateSerializedState);
            scanSetupCalls(mainContext, frameCapture->getSetupCalls());
        }
        else
        {
            gl::State auxContextReplayState(
                nullptr, nullptr, nullptr, nullptr, nullptr, EGL_OPENGL_ES_API,
                shareContext->getState().getClientVersion(), false, true, true, true, false,
                EGL_CONTEXT_PRIORITY_MEDIUM_IMG, shareContext->getState().hasProtectedContent());
            auxContextReplayState.initializeForCapture(shareContext);

            CaptureMidExecutionSetup(shareContext, &frameCapture->getSetupCalls(),
                                     &mShareGroupSetupCalls, &mResourceIDToSetupCalls,
                                     &mResourceTracker, auxContextReplayState,
                                     mValidateSerializedState);

            scanSetupCalls(mainContext, frameCapture->getSetupCalls());

            WriteAuxiliaryContextCppSetupReplay(mCompression, mOutDirectory, shareContext,
                                                mCaptureLabel, 1, frameCapture->getSetupCalls(),
                                                &mBinaryData, mSerializeStateEnabled, *this);
        }
    }
}

void FrameCaptureShared::onEndFrame(const gl::Context *context)
{
    if (!enabled() || mFrameIndex > mCaptureEndFrame)
    {
        setCaptureInactive();
        return;
    }

    FrameCapture *frameCapture = context->getFrameCapture();

    // Count resource IDs. This is also done on every frame. It could probably be done by
    // checking the GL state instead of the calls.
    for (const CallCapture &call : mFrameCalls)
    {
        for (const ParamCapture &param : call.params.getParamCaptures())
        {
            ResourceIDType idType = GetResourceIDTypeFromParamType(param.type);
            if (idType != ResourceIDType::InvalidEnum)
            {
                mHasResourceType.set(idType);
            }
        }
    }

    // Assume that the context performing the swap is the "main" context.
    ASSERT(mWindowSurfaceContextID.value == 0 || mWindowSurfaceContextID == context->id());
    mWindowSurfaceContextID = context->id();

    // On Android, we can trigger a capture during the run
    checkForCaptureTrigger();

    // Check for MEC. Done after checkForCaptureTrigger(), since that can modify mCaptureStartFrame.
    if (mFrameIndex < mCaptureStartFrame)
    {
        if (mFrameIndex == mCaptureStartFrame - 1)
        {
            runMidExecutionCapture(context);

            // Set the capture active to ensure all GLES commands issued by the next frame are
            // handled correctly by maybeCapturePreCallUpdates() and maybeCapturePostCallUpdates().
            setCaptureActive();
        }
        mFrameIndex++;
        reset();
        return;
    }

    ASSERT(isCaptureActive());

    if (!mFrameCalls.empty())
    {
        mActiveFrameIndices.push_back(getReplayFrameIndex());
    }

    // Make sure all pending work for every Context in the share group has completed so all data
    // (buffers, textures, etc.) has been updated and no resources are in use.
    egl::ShareGroup *shareGroup = context->getShareGroup();
    shareGroup->finishAllContexts();

    // Only validate the first frame for now to save on retracing time.
    if (mValidateSerializedState && mFrameIndex == mCaptureStartFrame)
    {
        CaptureValidateSerializedState(context, &mFrameCalls);
    }

    writeMainContextCppReplay(context, frameCapture->getSetupCalls());

    if (mFrameIndex == mCaptureEndFrame)
    {
        // Write shared MEC after frame sequence so we can eliminate unused assets like programs
        WriteShareGroupCppSetupReplay(mCompression, mOutDirectory, mCaptureLabel, 1, 1,
                                      mShareGroupSetupCalls, &mResourceTracker, &mBinaryData,
                                      mSerializeStateEnabled, *this);

        // Save the index files after the last frame.
        writeCppReplayIndexFiles(context, false);
        SaveBinaryData(mCompression, mOutDirectory, kSharedContextId, mCaptureLabel, mBinaryData);
        mBinaryData.clear();
        mWroteIndexFile = true;
    }

    reset();
    mFrameIndex++;
}

void FrameCaptureShared::onDestroyContext(const gl::Context *context)
{
    if (!mEnabled)
    {
        return;
    }
    if (!mWroteIndexFile && mFrameIndex > mCaptureStartFrame)
    {
        // If context is destroyed before end frame is reached and at least
        // 1 frame has been recorded, then write the index files.
        // It doesn't make sense to write the index files when no frame has been recorded
        mFrameIndex -= 1;
        mCaptureEndFrame = mFrameIndex;
        writeCppReplayIndexFiles(context, true);
        SaveBinaryData(mCompression, mOutDirectory, kSharedContextId, mCaptureLabel, mBinaryData);
        mBinaryData.clear();
        mWroteIndexFile = true;
    }
}

void FrameCaptureShared::onMakeCurrent(const gl::Context *context, const egl::Surface *drawSurface)
{
    if (!drawSurface)
    {
        return;
    }

    // Track the width, height and color space of the draw surface as provided to makeCurrent
    SurfaceParams &params = mDrawSurfaceParams[context->id()];
    params.extents        = gl::Extents(drawSurface->getWidth(), drawSurface->getHeight(), 1);
    params.colorSpace     = egl::FromEGLenum<egl::ColorSpace>(drawSurface->getGLColorspace());
}

DataCounters::DataCounters() = default;

DataCounters::~DataCounters() = default;

int DataCounters::getAndIncrement(EntryPoint entryPoint, const std::string &paramName)
{
    Counter counterKey = {entryPoint, paramName};
    return mData[counterKey]++;
}

DataTracker::DataTracker() = default;

DataTracker::~DataTracker() = default;

StringCounters::StringCounters() = default;

StringCounters::~StringCounters() = default;

int StringCounters::getStringCounter(std::vector<std::string> &strings)
{
    const auto &id = mStringCounterMap.find(strings);
    if (id == mStringCounterMap.end())
    {
        return kStringsNotFound;
    }
    else
    {
        return mStringCounterMap[strings];
    }
}

void StringCounters::setStringCounter(std::vector<std::string> &strings, int &counter)
{
    ASSERT(counter >= 0);
    mStringCounterMap[strings] = counter;
}

TrackedResource::TrackedResource() = default;

TrackedResource::~TrackedResource() = default;

ResourceTracker::ResourceTracker() = default;

ResourceTracker::~ResourceTracker() = default;

void ResourceTracker::setDeletedFenceSync(GLsync sync)
{
    ASSERT(sync != nullptr);
    if (mStartingFenceSyncs.find(sync) == mStartingFenceSyncs.end())
    {
        // This is a fence sync created after MEC was initialized. Ignore it.
        return;
    }

    // In this case, the app is deleting a fence sync we started with, we need to regen on loop.
    mFenceSyncsToRegen.insert(sync);
}

void TrackedResource::setGennedResource(GLuint id)
{
    if (mStartingResources.find(id) == mStartingResources.end())
    {
        // This is a resource created after MEC was initialized, track it
        mNewResources.insert(id);
        return;
    }
}

void TrackedResource::setDeletedResource(GLuint id)
{
    if (id == 0)
    {
        // Ignore ID 0
        return;
    }

    if (mNewResources.find(id) != mNewResources.end())
    {
        // This is a resource created after MEC was initialized, just clear it, since there will be
        // no actions required for it to return to starting state.
        mNewResources.erase(id);
        return;
    }

    if (mStartingResources.find(id) != mStartingResources.end())
    {
        // In this case, the app is deleting a resource we started with, we need to regen on loop
        mResourcesToRegen.insert(id);

        // Also restore its contents
        mResourcesToRestore.insert(id);
    }

    // If none of the above is true, the app is deleting a resource that was never genned.
}

void TrackedResource::setModifiedResource(GLuint id)
{
    // If this was a starting resource, we need to track it for restore
    if (mStartingResources.find(id) != mStartingResources.end())
    {
        mResourcesToRestore.insert(id);
    }
}

void ResourceTracker::setBufferMapped(GLuint id)
{
    // If this was a starting buffer, we may need to restore it to original state during Reset
    TrackedResource &trackedBuffers = getTrackedResource(ResourceIDType::Buffer);
    if (trackedBuffers.getStartingResources().find(id) !=
        trackedBuffers.getStartingResources().end())
    {
        // Track that its current state is mapped (true)
        mStartingBuffersMappedCurrent[id] = true;
    }
}

void ResourceTracker::setBufferUnmapped(GLuint id)
{
    // If this was a starting buffer, we may need to restore it to original state during Reset
    TrackedResource &trackedBuffers = getTrackedResource(ResourceIDType::Buffer);
    if (trackedBuffers.getStartingResources().find(id) !=
        trackedBuffers.getStartingResources().end())
    {
        // Track that its current state is unmapped (false)
        mStartingBuffersMappedCurrent[id] = false;
    }
}

bool ResourceTracker::getStartingBuffersMappedCurrent(GLuint id) const
{
    const auto &foundBool = mStartingBuffersMappedCurrent.find(id);
    ASSERT(foundBool != mStartingBuffersMappedCurrent.end());
    return foundBool->second;
}

bool ResourceTracker::getStartingBuffersMappedInitial(GLuint id) const
{
    const auto &foundBool = mStartingBuffersMappedInitial.find(id);
    ASSERT(foundBool != mStartingBuffersMappedInitial.end());
    return foundBool->second;
}

void ResourceTracker::onShaderProgramAccess(gl::ShaderProgramID shaderProgramID)
{
    mMaxShaderPrograms = std::max(mMaxShaderPrograms, shaderProgramID.value + 1);
}

bool FrameCaptureShared::isCapturing() const
{
    // Currently we will always do a capture up until the last frame. In the future we could improve
    // mid execution capture by only capturing between the start and end frames. The only necessary
    // reason we need to capture before the start is for attached program and shader sources.
    return mEnabled && mFrameIndex <= mCaptureEndFrame;
}

uint32_t FrameCaptureShared::getFrameCount() const
{
    return mCaptureEndFrame - mCaptureStartFrame + 1;
}

uint32_t FrameCaptureShared::getReplayFrameIndex() const
{
    return mFrameIndex - mCaptureStartFrame + 1;
}

void FrameCaptureShared::replay(gl::Context *context)
{
    ReplayContext replayContext(mReadBufferSize, mClientArraySizes);
    for (const CallCapture &call : mFrameCalls)
    {
        INFO() << "frame index: " << mFrameIndex << " " << call.name();

        if (call.entryPoint == EntryPoint::GLInvalid)
        {
            if (call.customFunctionName == "UpdateClientArrayPointer")
            {
                GLint arrayIndex =
                    call.params.getParam("arrayIndex", ParamType::TGLint, 0).value.GLintVal;
                ASSERT(arrayIndex < gl::MAX_VERTEX_ATTRIBS);

                const ParamCapture &pointerParam =
                    call.params.getParam("pointer", ParamType::TvoidConstPointer, 1);
                ASSERT(pointerParam.data.size() == 1);
                const void *pointer = pointerParam.data[0].data();

                size_t size = static_cast<size_t>(
                    call.params.getParam("size", ParamType::TGLuint64, 2).value.GLuint64Val);

                std::vector<uint8_t> &curClientArrayBuffer =
                    replayContext.getClientArraysBuffer()[arrayIndex];
                ASSERT(curClientArrayBuffer.size() >= size);
                memcpy(curClientArrayBuffer.data(), pointer, size);
            }
            continue;
        }

        ReplayCall(context, &replayContext, call);
    }
}

void FrameCaptureShared::writeCppReplayIndexFiles(const gl::Context *context,
                                                  bool writeResetContextCall)
{
    const gl::ContextID contextId           = context->id();
    const egl::Config *config               = context->getConfig();
    const egl::AttributeMap &displayAttribs = context->getDisplay()->getAttributeMap();
    const egl::ShareGroup *shareGroup       = context->getShareGroup();

    unsigned frameCount = getFrameCount();

    const SurfaceParams &surfaceParams = mDrawSurfaceParams.at(contextId);
    const gl::State &glState           = context->getState();

    // Serialize trace metadata into a JSON file. The JSON file will be named "trace_prefix.json".
    //
    // As of writing, it will have the format like so:
    // {
    //     "TraceMetadata":
    //     {
    //         "AreClientArraysEnabled" : 1, "CaptureRevision" : 16631, "ConfigAlphaBits" : 8,
    //             "ConfigBlueBits" : 8, "ConfigDepthBits" : 24, "ConfigGreenBits" : 8,
    // ... etc ...

    JsonSerializer json;
    json.startGroup("TraceMetadata");
    json.addScalar("CaptureRevision", ANGLE_REVISION);
    json.addScalar("ContextClientMajorVersion", context->getClientMajorVersion());
    json.addScalar("ContextClientMinorVersion", context->getClientMinorVersion());
    json.addHexValue("DisplayPlatformType", displayAttribs.getAsInt(EGL_PLATFORM_ANGLE_TYPE_ANGLE));
    json.addHexValue("DisplayDeviceType",
                     displayAttribs.getAsInt(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE));
    json.addScalar("FrameStart", 1);
    json.addScalar("FrameEnd", frameCount);
    json.addScalar("DrawSurfaceWidth", surfaceParams.extents.width);
    json.addScalar("DrawSurfaceHeight", surfaceParams.extents.height);
    json.addHexValue("DrawSurfaceColorSpace", ToEGLenum(surfaceParams.colorSpace));
    if (config)
    {
        json.addScalar("ConfigRedBits", config->redSize);
        json.addScalar("ConfigGreenBits", config->greenSize);
        json.addScalar("ConfigBlueBits", config->blueSize);
        json.addScalar("ConfigAlphaBits", config->alphaSize);
        json.addScalar("ConfigDepthBits", config->depthSize);
        json.addScalar("ConfigStencilBits", config->stencilSize);
    }
    else
    {
        json.addScalar("ConfigRedBits", EGL_DONT_CARE);
        json.addScalar("ConfigGreenBits", EGL_DONT_CARE);
        json.addScalar("ConfigBlueBits", EGL_DONT_CARE);
        json.addScalar("ConfigAlphaBits", EGL_DONT_CARE);
        json.addScalar("ConfigDepthBits", EGL_DONT_CARE);
        json.addScalar("ConfigStencilBits", EGL_DONT_CARE);
    }
    json.addBool("IsBinaryDataCompressed", mCompression);
    json.addBool("AreClientArraysEnabled", glState.areClientArraysEnabled());
    json.addBool("IsBindGeneratesResourcesEnabled", glState.isBindGeneratesResourceEnabled());
    json.addBool("IsWebGLCompatibilityEnabled", glState.isWebGL());
    json.addBool("IsRobustResourceInitEnabled", glState.isRobustResourceInitEnabled());
    json.addBool("IsTrimmingEnabled", mTrimEnabled);
    json.endGroup();

    {
        std::vector<std::string> traceFiles;
        for (uint32_t frameIndex = 1; frameIndex <= frameCount; ++frameIndex)
        {
            traceFiles.push_back(GetCaptureFileName(contextId, mCaptureLabel, frameIndex, ".cpp"));
        }

        for (gl::Context *shareContext : shareGroup->getContexts())
        {
            if (shareContext->id() == contextId)
            {
                // We already listed all of the "main" context's files, so skip it here.
                continue;
            }
            traceFiles.push_back(GetCaptureFileName(shareContext->id(), mCaptureLabel, 1, ".cpp"));
        }

        // Only save the MEC setup if we are using MEC.
        if (mCaptureStartFrame != 1)
        {
            traceFiles.push_back(GetCaptureFileName(kSharedContextId, mCaptureLabel, 1, ".cpp"));
        }

        json.addVectorOfStrings("TraceFiles", traceFiles);
    }

    json.addScalar("WindowSurfaceContextID", contextId.value);

    {
        std::stringstream jsonFileNameStream;
        jsonFileNameStream << mOutDirectory << FmtCapturePrefix(kNoContextId, mCaptureLabel)
                           << ".json";
        std::string jsonFileName = jsonFileNameStream.str();

        SaveFileHelper saveData(jsonFileName);
        saveData.write(reinterpret_cast<const uint8_t *>(json.data()), json.length());
    }

    std::stringstream header;
    std::stringstream source;

    header << "#pragma once\n";
    header << "\n";
    header << "#include <EGL/egl.h>\n";
    header << "#include <cstdint>\n";
    header << "\n";

    if (!mCaptureLabel.empty())
    {
        header << "namespace " << mCaptureLabel << "\n";
        header << "{\n";
    }
    for (uint32_t frameIndex = 1; frameIndex <= frameCount; ++frameIndex)
    {
        header << "void " << FmtReplayFunction(contextId, frameIndex) << ";\n";
    }
    header << "\n";
    if (mSerializeStateEnabled)
    {
        for (uint32_t frameIndex = 1; frameIndex <= frameCount; ++frameIndex)
        {
            header << "const char *" << FmtGetSerializedContextStateFunction(contextId, frameIndex)
                   << ";\n";
        }
        header << "\n";
    }

    header << "void InitReplay();\n";
    if (mCaptureStartFrame != 1)
    {
        header << "void SetupReplayContextShared();\n";
    }

    for (const gl::Context *shareContext : shareGroup->getContexts())
    {
        header << "void " << FmtSetupFunction(kNoPartId, shareContext->id()) << ";\n";
    }

    source << "#include \"" << FmtCapturePrefix(contextId, mCaptureLabel) << ".h\"\n";
    source << "#include \"trace_fixture.h\"\n";
    source << "#include \"angle_trace_gl.h\"\n";
    source << "\n";

    if (!mCaptureLabel.empty())
    {
        source << "using namespace " << mCaptureLabel << ";\n";
        source << "\n";
    }

    source << "void " << mCaptureLabel << "::InitReplay()\n";
    source << "{\n";
    WriteInitReplayCall(mCompression, source, kSharedContextId, mCaptureLabel,
                        MaxClientArraySize(mClientArraySizes), mReadBufferSize,
                        mMaxAccessedResourceIDs);
    source << "}\n";

    source << "extern \"C\" {\n";
    source << "void ReplayFrame(uint32_t frameIndex)\n";
    source << "{\n";
    source << "    switch (frameIndex)\n";
    source << "    {\n";
    for (uint32_t frameIndex : mActiveFrameIndices)
    {
        source << "        case " << frameIndex << ":\n";
        source << "            " << FmtReplayFunction(contextId, frameIndex) << ";\n";
        source << "            break;\n";
    }
    source << "        default:\n";
    source << "            break;\n";
    source << "    }\n";
    source << "}\n";
    source << "\n";

    if (writeResetContextCall)
    {
        source << "void ResetReplay()\n";
        source << "{\n";
        source << "    // Reset context is empty because context is destroyed before end "
                  "frame is reached\n";
        source << "}\n";
        source << "\n";
    }

    if (mSerializeStateEnabled)
    {
        source << "const char *GetSerializedContextState(uint32_t frameIndex)\n";
        source << "{\n";
        source << "    switch (frameIndex)\n";
        source << "    {\n";
        for (uint32_t frameIndex = 1; frameIndex <= frameCount; ++frameIndex)
        {
            source << "        case " << frameIndex << ":\n";
            source << "            return "
                   << FmtGetSerializedContextStateFunction(contextId, frameIndex) << ";\n";
        }
        source << "        default:\n";
        source << "            return nullptr;\n";
        source << "    }\n";
        source << "}\n";
        source << "\n";
    }

    source << "}  // extern \"C\"\n";

    if (!mCaptureLabel.empty())
    {
        header << "} // namespace " << mCaptureLabel << "\n";
    }

    {
        std::string headerContents = header.str();

        std::stringstream headerPathStream;
        headerPathStream << mOutDirectory << FmtCapturePrefix(contextId, mCaptureLabel) << ".h";
        std::string headerPath = headerPathStream.str();

        SaveFileHelper saveHeader(headerPath);
        saveHeader << headerContents;
    }

    {
        std::string sourceContents = source.str();

        std::stringstream sourcePathStream;
        sourcePathStream << mOutDirectory << FmtCapturePrefix(contextId, mCaptureLabel) << ".cpp";
        std::string sourcePath = sourcePathStream.str();

        SaveFileHelper saveSource(sourcePath);
        saveSource << sourceContents;
    }
}

void FrameCaptureShared::writeMainContextCppReplay(const gl::Context *context,
                                                   const std::vector<CallCapture> &setupCalls)
{
    ASSERT(mWindowSurfaceContextID == context->id());

    DataTracker dataTracker;

    std::stringstream out;
    std::stringstream header;

    header << "#include \"" << FmtCapturePrefix(context->id(), mCaptureLabel) << ".h\"\n";
    header << "#include \"angle_trace_gl.h\"\n";
    header << "";
    header << "\n";
    header << "namespace\n";
    header << "{\n";

    uint32_t frameCount = getFrameCount();
    uint32_t frameIndex = getReplayFrameIndex();

    if (frameIndex == 1 || frameIndex == frameCount)
    {
        out << "extern \"C\" {\n";
    }

    if (frameIndex == 1)
    {
        std::stringstream setupCallStream;

        setupCallStream << "void " << FmtSetupFunction(kNoPartId, context->id()) << "\n";
        setupCallStream << "{\n";

        WriteCppReplayFunctionWithParts(context->id(), ReplayFunc::Setup, &dataTracker, frameIndex,
                                        &mBinaryData, setupCalls, header, setupCallStream, out);

        out << setupCallStream.str();
        out << "}\n";
        out << "\n";
        out << "void SetupReplay()\n";
        out << "{\n";
        out << "    EGLContext context = eglGetCurrentContext();\n";
        out << "    gContextMap[" << context->id().value << "] = context;\n";
        out << "\n";

        // Setup all of the shared objects.
        out << "    " << mCaptureLabel << "::InitReplay();\n";
        if (usesMidExecutionCapture())
        {
            out << "    " << mCaptureLabel << "::" << FmtSetupFunction(kNoPartId, kSharedContextId)
                << ";\n";
        }

        // Setup the presentation (this) context first.
        out << "    " << FmtSetupFunction(kNoPartId, context->id()) << ";\n";
        out << "\n";

        // Setup each of the auxiliary contexts.
        egl::ShareGroup *shareGroup            = context->getShareGroup();
        const egl::ContextSet &shareContextSet = shareGroup->getContexts();
        for (gl::Context *shareContext : shareContextSet)
        {
            // Skip the presentation context, since that context was created by the test framework.
            if (shareContext->id() == context->id())
            {
                continue;
            }

            // TODO(http://www.anglebug.com/5878): Support capture/replay of eglCreateContext() so
            // this block can be moved into SetupReplayContextXX() by injecting them into the
            // beginning of the setup call stream.
            out << "    EGLContext context" << shareContext->id()
                << " = eglCreateContext(nullptr, nullptr, context, nullptr);\n";
            out << "    gContextMap[" << shareContext->id().value << "] = context"
                << shareContext->id() << ";\n";
            // The SetupReplayContextXX() calls only exist if this is a mid-execution capture and
            // we can only call them if they exist, so only output the calls if this is a MEC.
            if (usesMidExecutionCapture())
            {
                out << "    " << mCaptureLabel
                    << "::" << FmtSetupFunction(kNoPartId, shareContext->id()) << ";\n";
            }
        }

        // If there are other contexts that were initialized, we need to make the main context
        // current again.
        if (shareContextSet.size() > 1)
        {
            out << "\n";
            out << "    eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, context);\n";
        }

        out << "}\n";
        out << "\n";
    }

    if (frameIndex == frameCount)
    {
        // Emit code to reset back to starting state
        out << "void " << FmtResetFunction() << "\n";
        out << "{\n";

        // TODO(http://anglebug.com/5878): Look at moving this into the shared context file since
        // it's resetting shared objects.
        std::stringstream restoreCallStream;
        for (ResourceIDType resourceType : AllEnums<ResourceIDType>())
        {
            MaybeResetResources(restoreCallStream, resourceType, &dataTracker, header,
                                &mResourceTracker, &mBinaryData);
        }

        // Reset opaque type objects that don't have IDs, so are not ResourceIDTypes.
        MaybeResetOpaqueTypeObjects(restoreCallStream, &dataTracker, header, &mResourceTracker,
                                    &mBinaryData);

        out << restoreCallStream.str();
        out << "}\n";
    }

    if (frameIndex == 1 || frameIndex == frameCount)
    {
        out << "}  // extern \"C\"\n";
        out << "\n";
    }

    if (!mCaptureLabel.empty())
    {
        out << "namespace " << mCaptureLabel << "\n";
        out << "{\n";
    }

    if (!mFrameCalls.empty())
    {
        std::stringstream callStream;

        callStream << "void " << FmtReplayFunction(context->id(), frameIndex) << "\n";
        callStream << "{\n";

        WriteCppReplayFunctionWithParts(context->id(), ReplayFunc::Replay, &dataTracker, frameIndex,
                                        &mBinaryData, mFrameCalls, header, callStream, out);

        out << callStream.str();
        out << "}\n";
    }

    if (mSerializeStateEnabled)
    {
        std::string serializedContextString;
        if (SerializeContextToString(const_cast<gl::Context *>(context),
                                     &serializedContextString) == Result::Continue)
        {
            out << "const char *" << FmtGetSerializedContextStateFunction(context->id(), frameIndex)
                << "\n";
            out << "{\n";
            out << "    return R\"(" << serializedContextString << ")\";\n";
            out << "}\n";
            out << "\n";
        }
    }

    if (!mCaptureLabel.empty())
    {
        out << "} // namespace " << mCaptureLabel << "\n";
    }

    header << "}  // namespace\n";

    {
        std::string outString    = out.str();
        std::string headerString = header.str();

        std::string cppFilePath =
            GetCaptureFilePath(mOutDirectory, context->id(), mCaptureLabel, frameIndex, ".cpp");

        SaveFileHelper saveCpp(cppFilePath);
        saveCpp << headerString << "\n" << outString;
    }
}

void FrameCaptureShared::reset()
{
    mFrameCalls.clear();
    mClientVertexArrayMap.fill(-1);

    // Do not reset replay-specific settings like the maximum read buffer size, client array sizes,
    // or the 'has seen' type map. We could refine this into per-frame and per-capture maximums if
    // necessary.
}

const std::string &FrameCaptureShared::getShaderSource(gl::ShaderProgramID id) const
{
    const auto &foundSources = mCachedShaderSource.find(id);
    ASSERT(foundSources != mCachedShaderSource.end());
    return foundSources->second;
}

void FrameCaptureShared::setShaderSource(gl::ShaderProgramID id, std::string source)
{
    mCachedShaderSource[id] = source;
}

const ProgramSources &FrameCaptureShared::getProgramSources(gl::ShaderProgramID id) const
{
    const auto &foundSources = mCachedProgramSources.find(id);
    ASSERT(foundSources != mCachedProgramSources.end());
    return foundSources->second;
}

void FrameCaptureShared::setProgramSources(gl::ShaderProgramID id, ProgramSources sources)
{
    mCachedProgramSources[id] = sources;
}

const std::vector<uint8_t> &FrameCaptureShared::retrieveCachedTextureLevel(gl::TextureID id,
                                                                           gl::TextureTarget target,
                                                                           GLint level)
{
    // Look up the data for the requested texture
    const auto &foundTextureLevels = mCachedTextureLevelData.find(id);
    if (foundTextureLevels == mCachedTextureLevelData.end())
    {
        ERR() << "Cached texture level not found for id=" << id.value << " target=" << target
              << " level=" << level;
        UNREACHABLE();
    }

    GLint adjustedLevel = GetAdjustedTextureCacheLevel(target, level);

    const auto &foundTextureLevel = foundTextureLevels->second.find(adjustedLevel);
    if (foundTextureLevel == foundTextureLevels->second.end())
    {
        ERR() << "Cached texture level not found for id=" << id.value << " target=" << target
              << " level=" << level << " adjustedLevel=" << adjustedLevel;
        UNREACHABLE();
    }
    const std::vector<uint8_t> &capturedTextureLevel = foundTextureLevel->second;

    return capturedTextureLevel;
}

void FrameCaptureShared::copyCachedTextureLevel(const gl::Context *context,
                                                gl::TextureID srcID,
                                                GLint srcLevel,
                                                gl::TextureID dstID,
                                                GLint dstLevel,
                                                const CallCapture &call)
{
    // TODO(http://anglebug.com/5604): Add support for partial level copies.
    ASSERT(call.params.getParam("srcX", ParamType::TGLint, 3).value.GLintVal == 0);
    ASSERT(call.params.getParam("srcY", ParamType::TGLint, 4).value.GLintVal == 0);
    ASSERT(call.params.getParam("srcZ", ParamType::TGLint, 5).value.GLintVal == 0);
    ASSERT(call.params.getParam("dstX", ParamType::TGLint, 9).value.GLintVal == 0);
    ASSERT(call.params.getParam("dstY", ParamType::TGLint, 10).value.GLintVal == 0);
    ASSERT(call.params.getParam("dstZ", ParamType::TGLint, 11).value.GLintVal == 0);
    GLenum srcTarget  = call.params.getParam("srcTarget", ParamType::TGLenum, 1).value.GLenumVal;
    GLsizei srcWidth  = call.params.getParam("srcWidth", ParamType::TGLsizei, 12).value.GLsizeiVal;
    GLsizei srcHeight = call.params.getParam("srcHeight", ParamType::TGLsizei, 13).value.GLsizeiVal;
    GLsizei srcDepth  = call.params.getParam("srcDepth", ParamType::TGLsizei, 14).value.GLsizeiVal;
    gl::Texture *srcTexture           = context->getTexture({srcID});
    gl::TextureTarget srcTargetPacked = gl::PackParam<gl::TextureTarget>(srcTarget);
    const gl::Extents &srcExtents     = srcTexture->getExtents(srcTargetPacked, srcLevel);
    ASSERT(srcExtents.width == srcWidth && srcExtents.height == srcHeight &&
           srcExtents.depth == srcDepth);

    // Look up the data for the source texture
    const auto &foundSrcTextureLevels = mCachedTextureLevelData.find(srcID);
    ASSERT(foundSrcTextureLevels != mCachedTextureLevelData.end());

    // For that texture, look up the data for the given level
    const auto &foundSrcTextureLevel = foundSrcTextureLevels->second.find(srcLevel);
    ASSERT(foundSrcTextureLevel != foundSrcTextureLevels->second.end());
    const std::vector<uint8_t> &srcTextureLevel = foundSrcTextureLevel->second;

    auto foundDstTextureLevels = mCachedTextureLevelData.find(dstID);
    if (foundDstTextureLevels == mCachedTextureLevelData.end())
    {
        // Initialize the texture ID data.
        auto emplaceResult = mCachedTextureLevelData.emplace(dstID, TextureLevels());
        ASSERT(emplaceResult.second);
        foundDstTextureLevels = emplaceResult.first;
    }

    TextureLevels &foundDstLevels         = foundDstTextureLevels->second;
    TextureLevels::iterator foundDstLevel = foundDstLevels.find(dstLevel);
    if (foundDstLevel != foundDstLevels.end())
    {
        // If we have a cache for this level, remove it since we're recreating it.
        foundDstLevels.erase(dstLevel);
    }

    // Initialize destination texture data and copy the source into it.
    std::vector<uint8_t> dstTextureLevel = srcTextureLevel;
    auto emplaceResult = foundDstLevels.emplace(dstLevel, std::move(dstTextureLevel));
    ASSERT(emplaceResult.second);
}

std::vector<uint8_t> &FrameCaptureShared::getCachedTextureLevelData(gl::Texture *texture,
                                                                    gl::TextureTarget target,
                                                                    GLint textureLevel,
                                                                    EntryPoint entryPoint)
{
    auto foundTextureLevels = mCachedTextureLevelData.find(texture->id());
    if (foundTextureLevels == mCachedTextureLevelData.end())
    {
        // Initialize the texture ID data.
        auto emplaceResult = mCachedTextureLevelData.emplace(texture->id(), TextureLevels());
        ASSERT(emplaceResult.second);
        foundTextureLevels = emplaceResult.first;
    }

    // For this texture, look up the adjusted level, which may not match 1:1 due to cubes
    GLint adjustedLevel = GetAdjustedTextureCacheLevel(target, textureLevel);

    TextureLevels &foundLevels         = foundTextureLevels->second;
    TextureLevels::iterator foundLevel = foundLevels.find(adjustedLevel);
    if (foundLevel != foundLevels.end())
    {
        if (entryPoint == EntryPoint::GLCompressedTexImage2D ||
            entryPoint == EntryPoint::GLCompressedTexImage3D)
        {
            // Delete the cached entry in case the caller is respecifying the level.
            foundLevels.erase(adjustedLevel);
        }
        else
        {
            ASSERT(entryPoint == EntryPoint::GLCompressedTexSubImage2D ||
                   entryPoint == EntryPoint::GLCompressedTexSubImage3D);

            // If we have a cache for this level, return it now
            return foundLevel->second;
        }
    }

    // Otherwise, create an appropriately sized cache for this level

    // Get the format of the texture for use with the compressed block size math.
    const gl::InternalFormat &format = *texture->getFormat(target, textureLevel).info;

    // Divide dimensions according to block size.
    const gl::Extents &levelExtents = texture->getExtents(target, textureLevel);

    // Calculate the size needed to store the compressed level
    GLuint sizeInBytes;
    bool result = format.computeCompressedImageSize(levelExtents, &sizeInBytes);
    ASSERT(result);

    // Initialize texture rectangle data. Default init to zero for stability.
    std::vector<uint8_t> newPixelData(sizeInBytes, 0);
    auto emplaceResult = foundLevels.emplace(adjustedLevel, std::move(newPixelData));
    ASSERT(emplaceResult.second);

    // Using the level entry we just created, return the location (a byte vector) where compressed
    // texture level data should be stored
    return emplaceResult.first->second;
}

void FrameCaptureShared::deleteCachedTextureLevelData(gl::TextureID id)
{
    const auto &foundTextureLevels = mCachedTextureLevelData.find(id);
    if (foundTextureLevels != mCachedTextureLevelData.end())
    {
        // Delete all texture levels at once
        mCachedTextureLevelData.erase(foundTextureLevels);
    }
}

void FrameCaptureShared::markResourceSetupCallsInactive(std::vector<CallCapture> *setupCalls,
                                                        ResourceIDType type,
                                                        GLuint id,
                                                        gl::Range<size_t> range)
{
    if (!mTrimEnabled)
    {
        return;
    }

    ASSERT(mResourceIDToSetupCalls[type].find(id) == mResourceIDToSetupCalls[type].end());

    // Mark all of the calls that were used to initialize this resource as INACTIVE
    for (size_t index : range)
    {
        (*setupCalls)[index].isActive = false;
    }

    mResourceIDToSetupCalls[type][id] = range;
}

void CaptureMemory(const void *source, size_t size, ParamCapture *paramCapture)
{
    std::vector<uint8_t> data(size);
    memcpy(data.data(), source, size);
    paramCapture->data.emplace_back(std::move(data));
}

void CaptureString(const GLchar *str, ParamCapture *paramCapture)
{
    // include the '\0' suffix
    CaptureMemory(str, strlen(str) + 1, paramCapture);
}

void CaptureStringLimit(const GLchar *str, uint32_t limit, ParamCapture *paramCapture)
{
    // Write the incoming string up to limit, including null terminator
    size_t length = strlen(str) + 1;

    if (length > limit)
    {
        // If too many characters, resize the string to fit in the limit
        std::string newStr = str;
        newStr.resize(limit - 1);
        CaptureString(newStr.c_str(), paramCapture);
    }
    else
    {
        CaptureMemory(str, length, paramCapture);
    }
}

void CaptureVertexPointerGLES1(const gl::State &glState,
                               gl::ClientVertexArrayType type,
                               const void *pointer,
                               ParamCapture *paramCapture)
{
    paramCapture->value.voidConstPointerVal = pointer;
    if (!glState.getTargetBuffer(gl::BufferBinding::Array))
    {
        paramCapture->arrayClientPointerIndex =
            gl::GLES1Renderer::VertexArrayIndex(type, glState.gles1());
    }
}

gl::Program *GetProgramForCapture(const gl::State &glState, gl::ShaderProgramID handle)
{
    gl::Program *program = glState.getShaderProgramManagerForCapture().getProgram(handle);
    return program;
}

void CaptureGetActiveUniformBlockivParameters(const gl::State &glState,
                                              gl::ShaderProgramID handle,
                                              gl::UniformBlockIndex uniformBlockIndex,
                                              GLenum pname,
                                              ParamCapture *paramCapture)
{
    int numParams = 1;

    // From the OpenGL ES 3.0 spec:
    // If pname is UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, then a list of the
    // active uniform indices for the uniform block identified by uniformBlockIndex is
    // returned. The number of elements that will be written to params is the value of
    // UNIFORM_BLOCK_ACTIVE_UNIFORMS for uniformBlockIndex
    if (pname == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
    {
        gl::Program *program = GetProgramForCapture(glState, handle);
        if (program)
        {
            gl::QueryActiveUniformBlockiv(program, uniformBlockIndex,
                                          GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numParams);
        }
    }

    paramCapture->readBufferSizeBytes = sizeof(GLint) * numParams;
}

void CaptureGetParameter(const gl::State &glState,
                         GLenum pname,
                         size_t typeSize,
                         ParamCapture *paramCapture)
{
    // kMaxReportedCapabilities is the biggest array we'll need to hold data from glGet calls.
    // This value needs to be updated if any new extensions are introduced that would allow for
    // more compressed texture formats. The current value is taken from:
    // http://opengles.gpuinfo.org/displaycapability.php?name=GL_NUM_COMPRESSED_TEXTURE_FORMATS&esversion=2
    constexpr unsigned int kMaxReportedCapabilities = 69;
    paramCapture->readBufferSizeBytes               = typeSize * kMaxReportedCapabilities;
}

void CaptureGenHandlesImpl(GLsizei n, GLuint *handles, ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLuint) * n;
    CaptureMemory(handles, paramCapture->readBufferSizeBytes, paramCapture);
}

void CaptureShaderStrings(GLsizei count,
                          const GLchar *const *strings,
                          const GLint *length,
                          ParamCapture *paramCapture)
{
    // Concat the array elements of the string into one data vector,
    // append the terminating zero and use this as the captured shader
    // string. The string count and the length array are adjusted
    // accordingly in the capture post-processing

    std::vector<uint8_t> data;
    size_t offset = 0;
    for (GLsizei index = 0; index < count; ++index)
    {
        size_t len = ((length && length[index] >= 0) ? length[index] : strlen(strings[index]));
        data.resize(offset + len);
        std::copy(strings[index], strings[index] + len, data.begin() + offset);
        offset += len;
    }
    data.push_back(0);
    paramCapture->data.emplace_back(std::move(data));
}

template <>
void WriteParamValueReplay<ParamType::TGLboolean>(std::ostream &os,
                                                  const CallCapture &call,
                                                  GLboolean value)
{
    switch (value)
    {
        case GL_TRUE:
            os << "GL_TRUE";
            break;
        case GL_FALSE:
            os << "GL_FALSE";
            break;
        default:
            os << "0x" << std::hex << std::uppercase << GLint(value);
    }
}

template <>
void WriteParamValueReplay<ParamType::TGLbooleanPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         GLboolean *value)
{
    if (value == 0)
    {
        os << "nullptr";
    }
    else
    {
        os << "reinterpret_cast<GLboolean *>("
           << static_cast<int>(reinterpret_cast<uintptr_t>(value)) << ")";
    }
}

template <>
void WriteParamValueReplay<ParamType::TvoidConstPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         const void *value)
{
    if (value == 0)
    {
        os << "nullptr";
    }
    else
    {
        os << "reinterpret_cast<const void *>("
           << static_cast<int>(reinterpret_cast<uintptr_t>(value)) << ")";
    }
}

template <>
void WriteParamValueReplay<ParamType::TGLfloatConstPointer>(std::ostream &os,
                                                            const CallCapture &call,
                                                            const GLfloat *value)
{
    if (value == 0)
    {
        os << "nullptr";
    }
    else
    {
        os << "reinterpret_cast<const GLfloat *>("
           << static_cast<int>(reinterpret_cast<uintptr_t>(value)) << ")";
    }
}

template <>
void WriteParamValueReplay<ParamType::TGLuintConstPointer>(std::ostream &os,
                                                           const CallCapture &call,
                                                           const GLuint *value)
{
    if (value == 0)
    {
        os << "nullptr";
    }
    else
    {
        os << "reinterpret_cast<const GLuint *>("
           << static_cast<int>(reinterpret_cast<uintptr_t>(value)) << ")";
    }
}

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROCKHR>(std::ostream &os,
                                                       const CallCapture &call,
                                                       GLDEBUGPROCKHR value)
{}

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROC>(std::ostream &os,
                                                    const CallCapture &call,
                                                    GLDEBUGPROC value)
{}

template <>
void WriteParamValueReplay<ParamType::TBufferID>(std::ostream &os,
                                                 const CallCapture &call,
                                                 gl::BufferID value)
{
    os << "gBufferMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TFenceNVID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::FenceNVID value)
{
    os << "gFenceNVMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TFramebufferID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::FramebufferID value)
{
    os << "gFramebufferMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TMemoryObjectID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::MemoryObjectID value)
{
    os << "gMemoryObjectMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TProgramPipelineID>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::ProgramPipelineID value)
{
    os << "gProgramPipelineMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TQueryID>(std::ostream &os,
                                                const CallCapture &call,
                                                gl::QueryID value)
{
    os << "gQueryMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TRenderbufferID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::RenderbufferID value)
{
    os << "gRenderbufferMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TSamplerID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::SamplerID value)
{
    os << "gSamplerMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TSemaphoreID>(std::ostream &os,
                                                    const CallCapture &call,
                                                    gl::SemaphoreID value)
{
    os << "gSemaphoreMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TShaderProgramID>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::ShaderProgramID value)
{
    os << "gShaderProgramMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TGLsync>(std::ostream &os,
                                               const CallCapture &call,
                                               GLsync value)
{
    os << "gSyncMap[" << SyncIndexValue(value) << "]";
}

template <>
void WriteParamValueReplay<ParamType::TTextureID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::TextureID value)
{
    os << "gTextureMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TTransformFeedbackID>(std::ostream &os,
                                                            const CallCapture &call,
                                                            gl::TransformFeedbackID value)
{
    os << "gTransformFeedbackMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TVertexArrayID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::VertexArrayID value)
{
    os << "gVertexArrayMap2[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TUniformLocation>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::UniformLocation value)
{
    if (value.value == -1)
    {
        os << "-1";
        return;
    }

    os << "gUniformLocations2[";

    // Find the program from the call parameters.
    gl::ShaderProgramID programID;
    if (FindShaderProgramIDInCall(call, &programID))
    {
        os << "gShaderProgramMap2[" << programID.value << "]";
    }
    else
    {
        os << "gCurrentProgram";
    }

    os << "][" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TUniformBlockIndex>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::UniformBlockIndex value)
{
    // Find the program from the call parameters.
    gl::ShaderProgramID programID;
    bool foundProgram = FindShaderProgramIDInCall(call, &programID);
    ASSERT(foundProgram);

    os << "gUniformBlockIndexes[gShaderProgramMap2[" << programID.value << "]][" << value.value
       << "]";
}

template <>
void WriteParamValueReplay<ParamType::TGLeglImageOES>(std::ostream &os,
                                                      const CallCapture &call,
                                                      GLeglImageOES value)
{
    uint64_t pointerValue = reinterpret_cast<uint64_t>(value);
    os << "reinterpret_cast<EGLImageKHR>(" << pointerValue << "ul)";
}

template <>
void WriteParamValueReplay<ParamType::TGLubyte>(std::ostream &os,
                                                const CallCapture &call,
                                                GLubyte value)
{
    const int v = value;
    os << v;
}

template <>
void WriteParamValueReplay<ParamType::TEGLContext>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLContext value)
{
    os << "gContextMap[" << reinterpret_cast<size_t>(value) << "]";
}

template <>
void WriteParamValueReplay<ParamType::TEGLDisplay>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLDisplay value)
{
    if (value == EGL_NO_DISPLAY)
    {
        os << "EGL_NO_DISPLAY";
        return;
    }

    // We don't support capturing real EGL calls.
    UNREACHABLE();
}

template <>
void WriteParamValueReplay<ParamType::TEGLSurface>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLSurface value)
{
    if (value == EGL_NO_SURFACE)
    {
        os << "EGL_NO_SURFACE";
        return;
    }

    // We don't support capturing real EGL calls.
    UNREACHABLE();
}

template <>
void WriteParamValueReplay<ParamType::TEGLDEBUGPROCKHR>(std::ostream &os,
                                                        const CallCapture &call,
                                                        EGLDEBUGPROCKHR value)
{
    // The value isn't actually useful, but this fixes MSVC compile errors:
    // error: implicit conversion between pointer-to-function and pointer-to-object is a Microsoft
    // extension [-Werror,-Wmicrosoft-cast]
    os << reinterpret_cast<void *>(value);
}

template <>
void WriteParamValueReplay<ParamType::TEGLGetBlobFuncANDROID>(std::ostream &os,
                                                              const CallCapture &call,
                                                              EGLGetBlobFuncANDROID value)
{
    // The value isn't actually useful, but this fixes MSVC compile errors:
    // error: implicit conversion between pointer-to-function and pointer-to-object is a Microsoft
    // extension [-Werror,-Wmicrosoft-cast]
    os << reinterpret_cast<void *>(value);
}

template <>
void WriteParamValueReplay<ParamType::TEGLSetBlobFuncANDROID>(std::ostream &os,
                                                              const CallCapture &call,
                                                              EGLSetBlobFuncANDROID value)
{
    // The value isn't actually useful, but this fixes MSVC compile errors:
    // error: implicit conversion between pointer-to-function and pointer-to-object is a Microsoft
    // extension [-Werror,-Wmicrosoft-cast]
    os << reinterpret_cast<void *>(value);
}

}  // namespace angle
