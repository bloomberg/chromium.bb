/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Skottie.h"

#include "SkCanvas.h"
#include "SkData.h"
#include "SkFontMgr.h"
#include "SkMakeUnique.h"
#include "SkOSPath.h"
#include "SkPaint.h"
#include "SkPoint.h"
#include "SkSGColor.h"
#include "SkSGInvalidationController.h"
#include "SkSGOpacityEffect.h"
#include "SkSGPath.h"
#include "SkSGScene.h"
#include "SkSGTransform.h"
#include "SkStream.h"
#include "SkTArray.h"
#include "SkTime.h"
#include "SkTo.h"
#include "SkottieAdapter.h"
#include "SkottieJson.h"
#include "SkottiePriv.h"
#include "SkottieProperty.h"
#include "SkottieValue.h"

#include <cmath>

#include "stdlib.h"

namespace skottie {

namespace internal {

void AnimationBuilder::log(Logger::Level lvl, const skjson::Value* json,
                           const char fmt[], ...) const {
    if (!fLogger) {
        return;
    }

    char buff[1024];
    va_list va;
    va_start(va, fmt);
    const auto len = vsprintf(buff, fmt, va);
    va_end(va);

    if (len < 0 || len >= SkToInt(sizeof(buff))) {
        SkDebugf("!! Could not format log message !!\n");
        return;
    }

    SkString jsonstr = json ? json->toString() : SkString();

    fLogger->log(lvl, buff, jsonstr.c_str());
}

sk_sp<sksg::Matrix> AnimationBuilder::attachMatrix(const skjson::ObjectValue& t,
                                                   AnimatorScope* ascope,
                                                   sk_sp<sksg::Matrix> parentMatrix) const {
    static const VectorValue g_default_vec_0   = {  0,   0},
                             g_default_vec_100 = {100, 100};

    auto matrix = sksg::Matrix::Make(SkMatrix::I(), parentMatrix);
    auto adapter = sk_make_sp<TransformAdapter>(matrix);

    auto bound = this->bindProperty<VectorValue>(t["a"], ascope,
            [adapter](const VectorValue& a) {
                adapter->setAnchorPoint(ValueTraits<VectorValue>::As<SkPoint>(a));
            }, g_default_vec_0);
    bound |= this->bindProperty<VectorValue>(t["p"], ascope,
            [adapter](const VectorValue& p) {
                adapter->setPosition(ValueTraits<VectorValue>::As<SkPoint>(p));
            }, g_default_vec_0);
    bound |= this->bindProperty<VectorValue>(t["s"], ascope,
            [adapter](const VectorValue& s) {
                adapter->setScale(ValueTraits<VectorValue>::As<SkVector>(s));
            }, g_default_vec_100);

    const auto* jrotation = &t["r"];
    if (jrotation->is<skjson::NullValue>()) {
        // 3d rotations have separate rx,ry,rz components.  While we don't fully support them,
        // we can still make use of rz.
        jrotation = &t["rz"];
    }
    bound |= this->bindProperty<ScalarValue>(*jrotation, ascope,
            [adapter](const ScalarValue& r) {
                adapter->setRotation(r);
            }, 0.0f);
    bound |= this->bindProperty<ScalarValue>(t["sk"], ascope,
            [adapter](const ScalarValue& sk) {
                adapter->setSkew(sk);
            }, 0.0f);
    bound |= this->bindProperty<ScalarValue>(t["sa"], ascope,
            [adapter](const ScalarValue& sa) {
                adapter->setSkewAxis(sa);
            }, 0.0f);

    const auto dispatched = this->dispatchTransformProperty(adapter);

    return (bound || dispatched) ? matrix : parentMatrix;
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachOpacity(const skjson::ObjectValue& jtransform,
                                                        AnimatorScope* ascope,
                                                        sk_sp<sksg::RenderNode> childNode) const {
    if (!childNode)
        return nullptr;

    auto opacityNode = sksg::OpacityEffect::Make(childNode);

    const auto bound = this->bindProperty<ScalarValue>(jtransform["o"], ascope,
        [opacityNode](const ScalarValue& o) {
            // BM opacity is [0..100]
            opacityNode->setOpacity(o * 0.01f);
        }, 100.0f);
    const auto dispatched = this->dispatchOpacityProperty(opacityNode);

    // We can ignore constant full opacity.
    return (bound || dispatched) ? std::move(opacityNode) : childNode;
}

sk_sp<sksg::Path> AnimationBuilder::attachPath(const skjson::Value& jpath,
                                               AnimatorScope* ascope) const {
    auto path_node = sksg::Path::Make();
    return this->bindProperty<ShapeValue>(jpath, ascope,
        [path_node](const ShapeValue& p) {
            // FillType is tracked in the SG node, not in keyframes -- make sure we preserve it.
            auto path = ValueTraits<ShapeValue>::As<SkPath>(p);
            path.setFillType(path_node->getFillType());
            path_node->setPath(path);
        })
        ? path_node
        : nullptr;
}

sk_sp<sksg::Color> AnimationBuilder::attachColor(const skjson::ObjectValue& jcolor,
                                                 AnimatorScope* ascope,
                                                 const char prop_name[]) const {
    auto color_node = sksg::Color::Make(SK_ColorBLACK);

    this->bindProperty<VectorValue>(jcolor[prop_name], ascope,
        [color_node](const VectorValue& c) {
            color_node->setColor(ValueTraits<VectorValue>::As<SkColor>(c));
        });
    this->dispatchColorProperty(color_node);

    return color_node;
}

AnimationBuilder::AnimationBuilder(sk_sp<ResourceProvider> rp, sk_sp<SkFontMgr> fontmgr,
                                   sk_sp<PropertyObserver> pobserver, sk_sp<Logger> logger,
                                   sk_sp<AnnotationObserver> aobserver,
                                   Animation::Builder::Stats* stats,
                                   float duration, float framerate)
    : fResourceProvider(std::move(rp))
    , fLazyFontMgr(std::move(fontmgr))
    , fPropertyObserver(std::move(pobserver))
    , fLogger(std::move(logger))
    , fAnnotationObserver(std::move(aobserver))
    , fStats(stats)
    , fDuration(duration)
    , fFrameRate(framerate) {}

std::unique_ptr<sksg::Scene> AnimationBuilder::parse(const skjson::ObjectValue& jroot) {
    this->dispatchAnnotations(jroot["annotations"]);

    this->parseAssets(jroot["assets"]);
    this->parseFonts(jroot["fonts"], jroot["chars"]);

    AnimatorScope animators;
    auto root = this->attachComposition(jroot, &animators);

    fStats->fAnimatorCount = animators.size();

    return sksg::Scene::Make(std::move(root), std::move(animators));
}

void AnimationBuilder::parseAssets(const skjson::ArrayValue* jassets) {
    if (!jassets) {
        return;
    }

    for (const skjson::ObjectValue* asset : *jassets) {
        if (asset) {
            fAssets.set(ParseDefault<SkString>((*asset)["id"], SkString()), { asset, false });
        }
    }
}

void AnimationBuilder::dispatchAnnotations(const skjson::ObjectValue* jannotations) const {
    if (!fAnnotationObserver || !jannotations) {
        return;
    }

    for (const auto& a : *jannotations) {
        if (const skjson::StringValue* value = a.fValue) {
            fAnnotationObserver->onAnnotation(a.fKey.begin(), value->begin());
        } else {
            this->log(Logger::Level::kWarning, &a.fValue, "Ignoring unexpected annotation value.");
        }
    }
}

bool AnimationBuilder::dispatchColorProperty(const sk_sp<sksg::Color>& c) const {
    bool dispatched = false;

    if (fPropertyObserver) {
        fPropertyObserver->onColorProperty(fPropertyObserverContext,
            [&]() {
                dispatched = true;
                return std::unique_ptr<ColorPropertyHandle>(new ColorPropertyHandle(c));
            });
    }

    return dispatched;
}

bool AnimationBuilder::dispatchOpacityProperty(const sk_sp<sksg::OpacityEffect>& o) const {
    bool dispatched = false;

    if (fPropertyObserver) {
        fPropertyObserver->onOpacityProperty(fPropertyObserverContext,
            [&]() {
                dispatched = true;
                return std::unique_ptr<OpacityPropertyHandle>(new OpacityPropertyHandle(o));
            });
    }

    return dispatched;
}

bool AnimationBuilder::dispatchTransformProperty(const sk_sp<TransformAdapter>& t) const {
    bool dispatched = false;

    if (fPropertyObserver) {
        fPropertyObserver->onTransformProperty(fPropertyObserverContext,
            [&]() {
                dispatched = true;
                return std::unique_ptr<TransformPropertyHandle>(new TransformPropertyHandle(t));
            });
    }

    return dispatched;
}

void AnimationBuilder::AutoPropertyTracker::updateContext(PropertyObserver* observer,
                                                          const skjson::ObjectValue& obj) {

    const skjson::StringValue* name = obj["nm"];

    fBuilder->fPropertyObserverContext = name ? name->begin() : nullptr;
}

} // namespace internal

sk_sp<SkData> ResourceProvider::load(const char[], const char[]) const {
    return nullptr;
}

sk_sp<ImageAsset> ResourceProvider::loadImageAsset(const char path[], const char name[]) const {
    return nullptr;
}

sk_sp<SkData> ResourceProvider::loadFont(const char[], const char[]) const {
    return nullptr;
}

void Logger::log(Level, const char[], const char*) {}

Animation::Builder::Builder()  = default;
Animation::Builder::~Builder() = default;

Animation::Builder& Animation::Builder::setResourceProvider(sk_sp<ResourceProvider> rp) {
    fResourceProvider = std::move(rp);
    return *this;
}

Animation::Builder& Animation::Builder::setFontManager(sk_sp<SkFontMgr> fmgr) {
    fFontMgr = std::move(fmgr);
    return *this;
}

Animation::Builder& Animation::Builder::setPropertyObserver(sk_sp<PropertyObserver> pobserver) {
    fPropertyObserver = std::move(pobserver);
    return *this;
}

Animation::Builder& Animation::Builder::setLogger(sk_sp<Logger> logger) {
    fLogger = std::move(logger);
    return *this;
}

Animation::Builder& Animation::Builder::setAnnotationObserver(sk_sp<AnnotationObserver> aobserver) {
    fAnnotationObserver = std::move(aobserver);
    return *this;
}

sk_sp<Animation> Animation::Builder::make(SkStream* stream) {
    if (!stream->hasLength()) {
        // TODO: handle explicit buffering?
        if (fLogger) {
            fLogger->log(Logger::Level::kError, "Cannot parse streaming content.\n");
        }
        return nullptr;
    }

    auto data = SkData::MakeFromStream(stream, stream->getLength());
    if (!data) {
        if (fLogger) {
            fLogger->log(Logger::Level::kError, "Failed to read the input stream.\n");
        }
        return nullptr;
    }

    return this->make(static_cast<const char*>(data->data()), data->size());
}

sk_sp<Animation> Animation::Builder::make(const char* data, size_t data_len) {
    // Sanitize factory args.
    class NullResourceProvider final : public ResourceProvider {
        sk_sp<SkData> load(const char[], const char[]) const override { return nullptr; }
    };
    auto resolvedProvider = fResourceProvider
            ? fResourceProvider : sk_make_sp<NullResourceProvider>();

    memset(&fStats, 0, sizeof(struct Stats));

    fStats.fJsonSize = data_len;
    const auto t0 = SkTime::GetMSecs();

    const skjson::DOM dom(data, data_len);
    if (!dom.root().is<skjson::ObjectValue>()) {
        // TODO: more error info.
        if (fLogger) {
            fLogger->log(Logger::Level::kError, "Failed to parse JSON input.\n");
        }
        return nullptr;
    }
    const auto& json = dom.root().as<skjson::ObjectValue>();

    const auto t1 = SkTime::GetMSecs();
    fStats.fJsonParseTimeMS = t1 - t0;

    const auto version  = ParseDefault<SkString>(json["v"], SkString());
    const auto size     = SkSize::Make(ParseDefault<float>(json["w"], 0.0f),
                                       ParseDefault<float>(json["h"], 0.0f));
    const auto fps      = ParseDefault<float>(json["fr"], -1.0f),
               inPoint  = ParseDefault<float>(json["ip"], 0.0f),
               outPoint = SkTMax(ParseDefault<float>(json["op"], SK_ScalarMax), inPoint),
               duration = sk_ieee_float_divide(outPoint - inPoint, fps);

    if (size.isEmpty() || version.isEmpty() || fps <= 0 ||
        !SkScalarIsFinite(inPoint) || !SkScalarIsFinite(outPoint) || !SkScalarIsFinite(duration)) {
        if (fLogger) {
            const auto msg = SkStringPrintf(
                         "Invalid animation params (version: %s, size: [%f %f], frame rate: %f, "
                         "in-point: %f, out-point: %f)\n",
                         version.c_str(), size.width(), size.height(), fps, inPoint, outPoint);
            fLogger->log(Logger::Level::kError, msg.c_str());
        }
        return nullptr;
    }

    SkASSERT(resolvedProvider);
    internal::AnimationBuilder builder(std::move(resolvedProvider), fFontMgr,
                                       std::move(fPropertyObserver),
                                       std::move(fLogger),
                                       std::move(fAnnotationObserver),
                                       &fStats, duration, fps);
    auto scene = builder.parse(json);

    const auto t2 = SkTime::GetMSecs();
    fStats.fSceneParseTimeMS = t2 - t1;
    fStats.fTotalLoadTimeMS  = t2 - t0;

    if (!scene && fLogger) {
        fLogger->log(Logger::Level::kError, "Could not parse animation.\n");
    }

    return sk_sp<Animation>(
        new Animation(std::move(scene), std::move(version), size, inPoint, outPoint, duration));
}

sk_sp<Animation> Animation::Builder::makeFromFile(const char path[]) {
    class DirectoryResourceProvider final : public ResourceProvider {
    public:
        explicit DirectoryResourceProvider(SkString dir) : fDir(std::move(dir)) {}

        sk_sp<SkData> load(const char resource_path[], const char resource_name[]) const override {
            const auto full_dir  = SkOSPath::Join(fDir.c_str(), resource_path),
                       full_path = SkOSPath::Join(full_dir.c_str(), resource_name);
            return SkData::MakeFromFileName(full_path.c_str());
        }

    private:
        const SkString fDir;
    };

    const auto data = SkData::MakeFromFileName(path);
    if (!data)
        return nullptr;

    const auto useLocalProvider = !fResourceProvider;
    if (useLocalProvider) {
        fResourceProvider = sk_make_sp<DirectoryResourceProvider>(SkOSPath::Dirname(path));
    }

    auto animation = this->make(static_cast<const char*>(data->data()), data->size());

    if (useLocalProvider) {
        fResourceProvider.reset();
    }

    return animation;
}

Animation::Animation(std::unique_ptr<sksg::Scene> scene, SkString version, const SkSize& size,
                     SkScalar inPoint, SkScalar outPoint, SkScalar duration)
    : fScene(std::move(scene))
    , fVersion(std::move(version))
    , fSize(size)
    , fInPoint(inPoint)
    , fOutPoint(outPoint)
    , fDuration(duration) {

    // In case the client calls render before the first tick.
    this->seek(0);
}

Animation::~Animation() = default;

void Animation::setShowInval(bool show) {
    if (fScene) {
        fScene->setShowInval(show);
    }
}

void Animation::render(SkCanvas* canvas, const SkRect* dstR) const {
    if (!fScene)
        return;

    SkAutoCanvasRestore restore(canvas, true);
    const SkRect srcR = SkRect::MakeSize(this->size());
    if (dstR) {
        canvas->concat(SkMatrix::MakeRectToRect(srcR, *dstR, SkMatrix::kCenter_ScaleToFit));
    }
    canvas->clipRect(srcR);
    fScene->render(canvas);
}

void Animation::seek(SkScalar t) {
    if (!fScene)
        return;

    fScene->animate(fInPoint + SkTPin(t, 0.0f, 1.0f) * (fOutPoint - fInPoint));
}

sk_sp<Animation> Animation::Make(const char* data, size_t length) {
    return Builder().make(data, length);
}

sk_sp<Animation> Animation::Make(SkStream* stream) {
    return Builder().make(stream);
}

sk_sp<Animation> Animation::MakeFromFile(const char path[]) {
    return Builder().makeFromFile(path);
}

} // namespace skottie
