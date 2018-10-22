/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkottiePriv.h"

#include "SkData.h"
#include "SkFontMgr.h"
#include "SkImage.h"
#include "SkJSON.h"
#include "SkMakeUnique.h"
#include "SkottieJson.h"
#include "SkottieValue.h"
#include "SkParse.h"
#include "SkSGClipEffect.h"
#include "SkSGColor.h"
#include "SkSGDraw.h"
#include "SkSGGroup.h"
#include "SkSGImage.h"
#include "SkSGMaskEffect.h"
#include "SkSGMerge.h"
#include "SkSGOpacityEffect.h"
#include "SkSGPath.h"
#include "SkSGRect.h"
#include "SkSGTransform.h"

#include <vector>

namespace skottie {
namespace internal {

namespace {

struct MaskInfo {
    SkBlendMode       fBlendMode;      // used when masking with layers/blending
    sksg::Merge::Mode fMergeMode;      // used when clipping
    bool              fInvertGeometry;
};

const MaskInfo* GetMaskInfo(char mode) {
    static constexpr MaskInfo k_add_info =
        { SkBlendMode::kSrcOver   , sksg::Merge::Mode::kUnion     , false };
    static constexpr MaskInfo k_int_info =
        { SkBlendMode::kSrcIn     , sksg::Merge::Mode::kIntersect , false };
    // AE 'subtract' is the same as 'intersect' + inverted geometry
    // (draws the opacity-adjusted paint *outside* the shape).
    static constexpr MaskInfo k_sub_info =
        { SkBlendMode::kSrcIn     , sksg::Merge::Mode::kIntersect , true  };
    static constexpr MaskInfo k_dif_info =
        { SkBlendMode::kDifference, sksg::Merge::Mode::kDifference, false };

    switch (mode) {
    case 'a': return &k_add_info;
    case 'f': return &k_dif_info;
    case 'i': return &k_int_info;
    case 's': return &k_sub_info;
    default: break;
    }

    return nullptr;
}

sk_sp<sksg::RenderNode> AttachMask(const skjson::ArrayValue* jmask,
                                   const AnimationBuilder* abuilder,
                                   AnimatorScope* ascope,
                                   sk_sp<sksg::RenderNode> childNode) {
    if (!jmask) return childNode;

    struct MaskRecord {
        sk_sp<sksg::Path>  mask_path;  // for clipping and masking
        sk_sp<sksg::Color> mask_paint; // for masking
        sksg::Merge::Mode  merge_mode; // for clipping
    };

    SkSTArray<4, MaskRecord, true> mask_stack;

    bool has_opacity = false;

    for (const skjson::ObjectValue* m : *jmask) {
        if (!m) continue;

        const skjson::StringValue* jmode = (*m)["mode"];
        if (!jmode || jmode->size() != 1) {
            LogJSON((*m)["mode"], "!! Invalid mask mode");
            continue;
        }

        const auto mode = *jmode->begin();
        if (mode == 'n') {
            // "None" masks have no effect.
            continue;
        }

        const auto* mask_info = GetMaskInfo(mode);
        if (!mask_info) {
            LOG("?? Unsupported mask mode: '%c'\n", mode);
            continue;
        }

        auto mask_path = abuilder->attachPath((*m)["pt"], ascope);
        if (!mask_path) {
            LogJSON(*m, "!! Could not parse mask path");
            continue;
        }

        // "inv" is cumulative with mask info fInvertGeometry
        const auto inverted =
            (mask_info->fInvertGeometry != ParseDefault<bool>((*m)["inv"], false));
        mask_path->setFillType(inverted ? SkPath::kInverseWinding_FillType
                                        : SkPath::kWinding_FillType);

        auto mask_paint = sksg::Color::Make(SK_ColorBLACK);
        mask_paint->setAntiAlias(true);
        // First mask in the stack initializes the mask buffer.
        mask_paint->setBlendMode(mask_stack.empty() ? SkBlendMode::kSrc
                                                    : mask_info->fBlendMode);

        has_opacity |= abuilder->bindProperty<ScalarValue>((*m)["o"], ascope,
            [mask_paint](const ScalarValue& o) {
                mask_paint->setOpacity(o * 0.01f);
        }, 100.0f);

        mask_stack.push_back({mask_path, mask_paint, mask_info->fMergeMode});
    }

    if (mask_stack.empty())
        return childNode;

    // If the masks are fully opaque, we can clip.
    if (!has_opacity) {
        sk_sp<sksg::GeometryNode> clip_node;

        if (mask_stack.count() == 1) {
            // Single path -> just clip.
            clip_node = std::move(mask_stack.front().mask_path);
        } else {
            // Multiple clip paths -> merge.
            std::vector<sksg::Merge::Rec> merge_recs;
            merge_recs.reserve(SkToSizeT(mask_stack.count()));

            for (const auto& mask : mask_stack) {
                const auto mode = merge_recs.empty() ? sksg::Merge::Mode::kMerge : mask.merge_mode;
                merge_recs.push_back({std::move(mask.mask_path), mode});
            }
            clip_node = sksg::Merge::Make(std::move(merge_recs));
        }

        return sksg::ClipEffect::Make(std::move(childNode), std::move(clip_node), true);
    }

    auto mask_group = sksg::Group::Make();
    for (const auto& rec : mask_stack) {
        mask_group->addChild(sksg::Draw::Make(std::move(rec.mask_path),
                                              std::move(rec.mask_paint)));

    }

    return sksg::MaskEffect::Make(std::move(childNode), std::move(mask_group));
}

} // namespace

sk_sp<sksg::RenderNode> AnimationBuilder::attachNestedAnimation(const char* name,
                                                                AnimatorScope* ascope) const {
    class SkottieSGAdapter final : public sksg::RenderNode {
    public:
        explicit SkottieSGAdapter(sk_sp<Animation> animation)
            : fAnimation(std::move(animation)) {
            SkASSERT(fAnimation);
        }

    protected:
        SkRect onRevalidate(sksg::InvalidationController*, const SkMatrix&) override {
            return SkRect::MakeSize(fAnimation->size());
        }

        void onRender(SkCanvas* canvas, const RenderContext* ctx) const override {
            const auto local_scope =
                ScopedRenderContext(canvas, ctx).setIsolation(this->bounds(), true);
            fAnimation->render(canvas);
        }

    private:
        const sk_sp<Animation> fAnimation;
    };

    class SkottieAnimatorAdapter final : public sksg::Animator {
    public:
        SkottieAnimatorAdapter(sk_sp<Animation> animation, float time_scale)
            : fAnimation(std::move(animation))
            , fTimeScale(time_scale) {
            SkASSERT(fAnimation);
        }

    protected:
        void onTick(float t) {
            // TODO: we prolly need more sophisticated timeline mapping for nested animations.
            fAnimation->seek(t * fTimeScale);
        }

    private:
        const sk_sp<Animation> fAnimation;
        const float            fTimeScale;
    };

    const auto data = fResourceProvider->load("", name);
    if (!data) {
        LOG("!! Could not load: %s\n", name);
        return nullptr;
    }

    auto animation = Animation::Builder()
            .setResourceProvider(fResourceProvider)
            .setFontManager(fFontMgr)
            .make(static_cast<const char*>(data->data()), data->size());
    if (!animation) {
        LOG("!! Could not parse nested animation: %s\n", name);
        return nullptr;
    }

    ascope->push_back(
        skstd::make_unique<SkottieAnimatorAdapter>(animation, animation->duration() / fDuration));

    return sk_make_sp<SkottieSGAdapter>(std::move(animation));
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachAssetRef(
    const skjson::ObjectValue& jlayer, AnimatorScope* ascope,
    sk_sp<sksg::RenderNode>(AnimationBuilder::* attach_proc)(const skjson::ObjectValue& comp,
                                                             AnimatorScope* ascope) const) const {

    const auto refId = ParseDefault<SkString>(jlayer["refId"], SkString());
    if (refId.isEmpty()) {
        LOG("!! Layer missing refId\n");
        return nullptr;
    }

    if (refId.startsWith("$")) {
        return this->attachNestedAnimation(refId.c_str() + 1, ascope);
    }

    const auto* asset_info = fAssets.find(refId);
    if (!asset_info) {
        LOG("!! Asset not found: '%s'\n", refId.c_str());
        return nullptr;
    }

    if (asset_info->fIsAttaching) {
        LOG("!! Asset cycle detected for: '%s'\n", refId.c_str());
        return nullptr;
    }

    asset_info->fIsAttaching = true;
    auto asset = (this->*attach_proc)(*asset_info->fAsset, ascope);
    asset_info->fIsAttaching = false;

    return asset;
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachSolidLayer(const skjson::ObjectValue& jlayer,
                                                           AnimatorScope*) const {
    const auto size = SkSize::Make(ParseDefault<float>(jlayer["sw"], 0.0f),
                                   ParseDefault<float>(jlayer["sh"], 0.0f));
    const skjson::StringValue* hex_str = jlayer["sc"];
    uint32_t c;
    if (size.isEmpty() ||
        !hex_str ||
        *hex_str->begin() != '#' ||
        !SkParse::FindHex(hex_str->begin() + 1, &c)) {
        LogJSON(jlayer, "!! Could not parse solid layer");
        return nullptr;
    }

    const SkColor color = 0xff000000 | c;

    return sksg::Draw::Make(sksg::Rect::Make(SkRect::MakeSize(size)),
                            sksg::Color::Make(color));
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachImageAsset(const skjson::ObjectValue& jimage,
                                                           AnimatorScope*) const {
    const skjson::StringValue* name = jimage["p"];
    const skjson::StringValue* path = jimage["u"];
    if (!name) {
        return nullptr;
    }

    const auto name_cstr = name->begin(),
               path_cstr = path ? path->begin() : "";
    const auto res_id = SkStringPrintf("%s|%s", path_cstr, name_cstr);
    if (auto* attached_image = fAssetCache.find(res_id)) {
        return *attached_image;
    }

    const auto data = fResourceProvider->load(path_cstr, name_cstr);
    if (!data) {
        LOG("!! Could not load image resource: %s/%s\n", path_cstr, name_cstr);
        return nullptr;
    }

    // TODO: non-intrisic image sizing
    return *fAssetCache.set(res_id, sksg::Image::Make(SkImage::MakeFromEncoded(data)));
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachImageLayer(const skjson::ObjectValue& jlayer,
                                                           AnimatorScope* ascope) const {
    return this->attachAssetRef(jlayer, ascope, &AnimationBuilder::attachImageAsset);
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachNullLayer(const skjson::ObjectValue& layer,
                                                          AnimatorScope*) const {
    // Null layers are used solely to drive dependent transforms,
    // but we use free-floating sksg::Matrices for that purpose.
    return nullptr;
}

struct AnimationBuilder::AttachLayerContext {
    AttachLayerContext(const skjson::ArrayValue& jlayers, AnimatorScope* scope)
        : fLayerList(jlayers), fScope(scope) {}

    const skjson::ArrayValue&            fLayerList;
    AnimatorScope*                       fScope;
    SkTHashMap<int, sk_sp<sksg::Matrix>> fLayerMatrixMap;
    sk_sp<sksg::RenderNode>              fCurrentMatte;

    sk_sp<sksg::Matrix> AttachLayerMatrix(const skjson::ObjectValue& jlayer,
                                          const AnimationBuilder* abuilder) {
        const auto layer_index = ParseDefault<int>(jlayer["ind"], -1);
        if (layer_index < 0)
            return nullptr;

        if (auto* m = fLayerMatrixMap.find(layer_index))
            return *m;

        return this->AttachLayerMatrixImpl(jlayer, abuilder, layer_index);
    }

private:
    sk_sp<sksg::Matrix> AttachParentLayerMatrix(const skjson::ObjectValue& jlayer,
                                                const AnimationBuilder* abuilder,
                                                int layer_index) {
        const auto parent_index = ParseDefault<int>(jlayer["parent"], -1);
        if (parent_index < 0 || parent_index == layer_index)
            return nullptr;

        if (auto* m = fLayerMatrixMap.find(parent_index))
            return *m;

        for (const skjson::ObjectValue* l : fLayerList) {
            if (!l) continue;

            if (ParseDefault<int>((*l)["ind"], -1) == parent_index) {
                return this->AttachLayerMatrixImpl(*l, abuilder, parent_index);
            }
        }

        return nullptr;
    }

    sk_sp<sksg::Matrix> AttachLayerMatrixImpl(const skjson::ObjectValue& jlayer,
                                              const AnimationBuilder* abuilder,
                                              int layer_index) {
        SkASSERT(!fLayerMatrixMap.find(layer_index));

        // Add a stub entry to break recursion cycles.
        fLayerMatrixMap.set(layer_index, nullptr);

        auto parent_matrix = this->AttachParentLayerMatrix(jlayer, abuilder, layer_index);

        if (const skjson::ObjectValue* jtransform = jlayer["ks"]) {
            return *fLayerMatrixMap.set(layer_index,
                                        abuilder->attachMatrix(*jtransform, fScope,
                                                               std::move(parent_matrix)));

        }
        return nullptr;
    }
};

sk_sp<sksg::RenderNode> AnimationBuilder::attachLayer(const skjson::ObjectValue* jlayer,
                                                     AttachLayerContext* layerCtx) const {
    if (!jlayer) return nullptr;

    using LayerAttacher = sk_sp<sksg::RenderNode> (AnimationBuilder::*)(const skjson::ObjectValue&,
                                                                        AnimatorScope*) const;
    static constexpr LayerAttacher gLayerAttachers[] = {
        &AnimationBuilder::attachPrecompLayer,  // 'ty': 0
        &AnimationBuilder::attachSolidLayer,    // 'ty': 1
        &AnimationBuilder::attachImageLayer,    // 'ty': 2
        &AnimationBuilder::attachNullLayer,     // 'ty': 3
        &AnimationBuilder::attachShapeLayer,    // 'ty': 4
        &AnimationBuilder::attachTextLayer,     // 'ty': 5
    };

    int type = ParseDefault<int>((*jlayer)["ty"], -1);
    if (type < 0 || type >= SkTo<int>(SK_ARRAY_COUNT(gLayerAttachers))) {
        return nullptr;
    }

    AnimatorScope layer_animators;

    // Layer content.
    auto layer = (this->*(gLayerAttachers[type]))(*jlayer, &layer_animators);

    // Clip layers with explicit dimensions.
    float w = 0, h = 0;
    if (Parse<float>((*jlayer)["w"], &w) && Parse<float>((*jlayer)["h"], &h)) {
        layer = sksg::ClipEffect::Make(std::move(layer),
                                       sksg::Rect::Make(SkRect::MakeWH(w, h)),
                                       true);
    }

    // Optional layer mask.
    layer = AttachMask((*jlayer)["masksProperties"], this, &layer_animators, std::move(layer));

    // Optional layer transform.
    if (auto layerMatrix = layerCtx->AttachLayerMatrix(*jlayer, this)) {
        layer = sksg::Transform::Make(std::move(layer), std::move(layerMatrix));
    }

    // Optional layer opacity.
    // TODO: de-dupe this "ks" lookup with matrix above.
    if (const skjson::ObjectValue* jtransform = (*jlayer)["ks"]) {
        layer = this->attachOpacity(*jtransform, &layer_animators, std::move(layer));
    }

    // Optional layer effects.
    if (const skjson::ArrayValue* jeffects = (*jlayer)["ef"]) {
        layer = this->attachLayerEffects(*jeffects, &layer_animators, std::move(layer));
    }

    class LayerController final : public sksg::GroupAnimator {
    public:
        LayerController(sksg::AnimatorList&& layer_animators,
                        sk_sp<sksg::OpacityEffect> controlNode,
                        float in, float out)
            : INHERITED(std::move(layer_animators))
            , fControlNode(std::move(controlNode))
            , fIn(in)
            , fOut(out) {}

        void onTick(float t) override {
            const auto active = (t >= fIn && t <= fOut);

            // Keep the layer fully transparent except for its [in..out] lifespan.
            // (note: opacity == 0 disables rendering, while opacity == 1 is a noop)
            fControlNode->setOpacity(active ? 1 : 0);

            // Dispatch ticks only while active.
            if (active) this->INHERITED::onTick(t);
        }

    private:
        const sk_sp<sksg::OpacityEffect> fControlNode;
        const float                      fIn,
                                         fOut;

        using INHERITED = sksg::GroupAnimator;
    };

    auto controller_node = sksg::OpacityEffect::Make(std::move(layer));
    const auto        in = ParseDefault<float>((*jlayer)["ip"], 0.0f),
                     out = ParseDefault<float>((*jlayer)["op"], in);

    if (in >= out || !controller_node)
        return nullptr;

    layerCtx->fScope->push_back(
        skstd::make_unique<LayerController>(std::move(layer_animators), controller_node, in, out));

    if (ParseDefault<bool>((*jlayer)["td"], false)) {
        // This layer is a matte.  We apply it as a mask to the next layer.
        layerCtx->fCurrentMatte = std::move(controller_node);
        return nullptr;
    }

    if (layerCtx->fCurrentMatte) {
        // There is a pending matte. Apply and reset.
        static constexpr sksg::MaskEffect::Mode gMaskModes[] = {
            sksg::MaskEffect::Mode::kNormal, // tt: 1
            sksg::MaskEffect::Mode::kInvert, // tt: 2
        };
        const auto matteType = ParseDefault<size_t>((*jlayer)["tt"], 1) - 1;

        if (matteType < SK_ARRAY_COUNT(gMaskModes)) {
            return sksg::MaskEffect::Make(std::move(controller_node),
                                          std::move(layerCtx->fCurrentMatte),
                                          gMaskModes[matteType]);
        }
        layerCtx->fCurrentMatte.reset();
    }

    return std::move(controller_node);
}

sk_sp<sksg::RenderNode> AnimationBuilder::attachComposition(const skjson::ObjectValue& comp,
                                                            AnimatorScope* scope) const {
    const skjson::ArrayValue* jlayers = comp["layers"];
    if (!jlayers) return nullptr;

    SkSTArray<16, sk_sp<sksg::RenderNode>, true> layers;
    AttachLayerContext                           layerCtx(*jlayers, scope);

    for (const auto& l : *jlayers) {
        if (auto layer_fragment = this->attachLayer(l, &layerCtx)) {
            layers.push_back(std::move(layer_fragment));
        }
    }

    if (layers.empty()) {
        return nullptr;
    }

    // Layers are painted in bottom->top order.
    auto comp_group = sksg::Group::Make();
    for (int i = layers.count() - 1; i >= 0; --i) {
        comp_group->addChild(std::move(layers[i]));
    }

    return std::move(comp_group);
}

} // namespace internal
} // namespace skottie
