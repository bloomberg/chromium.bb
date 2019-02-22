/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkMatrix.h"
#include "Skottie.h"
#include "SkottieProperty.h"
#include "SkStream.h"

#include "Test.h"

#include <tuple>
#include <vector>

using namespace skottie;

DEF_TEST(Skottie_OssFuzz8956, reporter) {
    static constexpr char json[] =
        "{\"v\":\" \",\"fr\":3,\"w\":4,\"h\":3,\"layers\":[{\"ty\": 1, \"sw\": 10, \"sh\": 10,"
            " \"sc\":\"#ffffff\", \"ks\":{\"o\":{\"a\": true, \"k\":"
            " [{\"t\": 0, \"s\": 0, \"e\": 1, \"i\": {\"x\":[]}}]}}}]}";

    SkMemoryStream stream(json, strlen(json));

    // Passes if parsing doesn't crash.
    auto animation = Animation::Make(&stream);
}

DEF_TEST(Skottie_Properties, reporter) {
    static constexpr char json[] = R"({
                                     "v": "5.2.1",
                                     "w": 100,
                                     "h": 100,
                                     "fr": 1,
                                     "ip": 0,
                                     "op": 1,
                                     "layers": [
                                       {
                                         "ty": 4,
                                         "nm": "layer_0",
                                         "ind": 0,
                                         "ip": 0,
                                         "op": 1,
                                         "ks": {
                                           "o": { "a": 0, "k": 50 }
                                         },
                                         "shapes": [
                                           {
                                             "ty": "el",
                                             "nm": "geometry_0",
                                             "p": { "a": 0, "k": [ 50, 50 ] },
                                             "s": { "a": 0, "k": [ 50, 50 ] }
                                           },
                                           {
                                             "ty": "fl",
                                             "nm": "fill_0",
                                             "c": { "a": 0, "k": [ 1, 0, 0] }
                                           },
                                           {
                                             "ty": "tr",
                                             "nm": "shape_transform_0",
                                             "o": { "a": 0, "k": 100 },
                                             "s": { "a": 0, "k": [ 50, 50 ] }
                                           }
                                         ]
                                       }
                                     ]
                                   })";

    class TestPropertyObserver final : public PropertyObserver {
    public:
        struct ColorInfo {
            SkString node_name;
            SkColor  color;
        };

        struct OpacityInfo {
            SkString node_name;
            float    opacity;
        };

        struct TransformInfo {
            SkString node_name;
            SkMatrix matrix;
        };

        void onColorProperty(const char node_name[],
                const PropertyObserver::LazyHandle<ColorPropertyHandle>& lh) override {
            fColors.push_back({SkString(node_name), lh()->getColor()});
        }

        void onOpacityProperty(const char node_name[],
                const PropertyObserver::LazyHandle<OpacityPropertyHandle>& lh) override {
            fOpacities.push_back({SkString(node_name), lh()->getOpacity()});
        }

        void onTransformProperty(const char node_name[],
                const PropertyObserver::LazyHandle<TransformPropertyHandle>& lh) override {
            fTransforms.push_back({SkString(node_name), lh()->getTotalMatrix()});
        }

        const std::vector<ColorInfo>& colors() const { return fColors; }
        const std::vector<OpacityInfo>& opacities() const { return fOpacities; }
        const std::vector<TransformInfo>& transforms() const { return fTransforms; }

    private:
        std::vector<ColorInfo>     fColors;
        std::vector<OpacityInfo>   fOpacities;
        std::vector<TransformInfo> fTransforms;
    };

    SkMemoryStream stream(json, strlen(json));
    auto observer = sk_make_sp<TestPropertyObserver>();

    auto animation = skottie::Animation::Builder()
            .setPropertyObserver(observer)
            .make(&stream);

    REPORTER_ASSERT(reporter, animation);

    const auto& colors = observer->colors();
    REPORTER_ASSERT(reporter, colors.size() == 1);
    REPORTER_ASSERT(reporter, colors[0].node_name.equals("fill_0"));
    REPORTER_ASSERT(reporter, colors[0].color == 0xffff0000);

    const auto& opacities = observer->opacities();
    REPORTER_ASSERT(reporter, opacities.size() == 2);
    REPORTER_ASSERT(reporter, opacities[0].node_name.equals("shape_transform_0"));
    REPORTER_ASSERT(reporter, SkScalarNearlyEqual(opacities[0].opacity, 100));
    REPORTER_ASSERT(reporter, opacities[1].node_name.equals("layer_0"));
    REPORTER_ASSERT(reporter, SkScalarNearlyEqual(opacities[1].opacity, 50));

    const auto& transforms = observer->transforms();
    REPORTER_ASSERT(reporter, transforms.size() == 2);
    REPORTER_ASSERT(reporter, transforms[0].node_name.equals("shape_transform_0"));
    REPORTER_ASSERT(reporter, transforms[0].matrix == SkMatrix::MakeScale(0.5, 0.5));
    REPORTER_ASSERT(reporter, transforms[1].node_name.equals("layer_0"));
    REPORTER_ASSERT(reporter, transforms[1].matrix == SkMatrix::I());
}

DEF_TEST(Skottie_Annotations, reporter) {
    static constexpr char json[] = R"({
                                     "v": "5.2.1",
                                     "w": 100,
                                     "h": 100,
                                     "fr": 1,
                                     "ip": 0,
                                     "op": 1,
                                     "layers": [
                                       {
                                         "ty": 1,
                                         "ind": 0,
                                         "ip": 0,
                                         "op": 1,
                                         "ks": {
                                           "o": { "a": 0, "k": 50 }
                                         },
                                         "sw": 100,
                                         "sh": 100,
                                         "sc": "#ffffff"
                                       }
                                     ],
                                     "annotations": {
                                       "key1": "foo",
                                       "key2": "bar",
                                       "key3": "baz"
                                     }
                                   })";

    class TestAnnotationObserver final : public AnnotationObserver {
    public:
        void onAnnotation(const char key[], const char value[]) override {
            fAnnotations.push_back(std::make_tuple(key, value));
        }

        std::vector<std::tuple<std::string, std::string>> fAnnotations;
    };

    SkMemoryStream stream(json, strlen(json));
    auto observer = sk_make_sp<TestAnnotationObserver>();

    auto animation = skottie::Animation::Builder()
            .setAnnotationObserver(observer)
            .make(&stream);

    REPORTER_ASSERT(reporter, animation);

    REPORTER_ASSERT(reporter, observer->fAnnotations.size() == 3ul);
    REPORTER_ASSERT(reporter, std::get<0>(observer->fAnnotations[0]) == "key1");
    REPORTER_ASSERT(reporter, std::get<1>(observer->fAnnotations[0]) == "foo");
    REPORTER_ASSERT(reporter, std::get<0>(observer->fAnnotations[1]) == "key2");
    REPORTER_ASSERT(reporter, std::get<1>(observer->fAnnotations[1]) == "bar");
    REPORTER_ASSERT(reporter, std::get<0>(observer->fAnnotations[2]) == "key3");
    REPORTER_ASSERT(reporter, std::get<1>(observer->fAnnotations[2]) == "baz");
}
