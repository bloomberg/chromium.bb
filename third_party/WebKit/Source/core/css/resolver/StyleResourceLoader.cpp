/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/css/resolver/StyleResourceLoader.h"

#include "CSSPropertyNames.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShaderValue.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/loader/cache/ResourceFetcher.h"
#include "core/platform/graphics/filters/custom/CustomFilterOperation.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/FillLayer.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleCustomFilterProgram.h"
#include "core/rendering/style/StyleCustomFilterProgramCache.h"
#include "core/rendering/style/StyleFetchedImage.h"
#include "core/rendering/style/StyleFetchedImageSet.h"
#include "core/rendering/style/StyleFetchedShader.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/rendering/style/StylePendingImage.h"
#include "core/rendering/style/StylePendingShader.h"

namespace WebCore {

StyleResourceLoader::StyleResourceLoader(ResourceFetcher* fetcher)
    : m_fetcher(fetcher)
    , m_customFilterProgramCache(StyleCustomFilterProgramCache::create())
{
}

void StyleResourceLoader::loadPendingSVGDocuments(RenderStyle* renderStyle, const ElementStyleResources& elementStyleResources)
{
    if (!renderStyle->hasFilter() || elementStyleResources.pendingSVGDocuments().isEmpty())
        return;

    Vector<RefPtr<FilterOperation> >& filterOperations = renderStyle->mutableFilter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filterOperations.at(i);
        if (filterOperation->getOperationType() == FilterOperation::REFERENCE) {
            ReferenceFilterOperation* referenceFilter = static_cast<ReferenceFilterOperation*>(filterOperation.get());

            CSSSVGDocumentValue* value = elementStyleResources.pendingSVGDocuments().get(referenceFilter);
            if (!value)
                continue;
            DocumentResource* resource = value->load(m_fetcher);
            if (!resource)
                continue;

            // Stash the DocumentResource on the reference filter.
            referenceFilter->setDocumentResourceReference(adoptPtr(new DocumentResourceReference(resource)));
        }
    }
}

PassRefPtr<StyleImage> StyleResourceLoader::loadPendingImage(StylePendingImage* pendingImage, float deviceScaleFactor)
{
    if (pendingImage->cssImageValue()) {
        CSSImageValue* imageValue = pendingImage->cssImageValue();
        return imageValue->cachedImage(m_fetcher);
    }

    if (pendingImage->cssImageGeneratorValue()) {
        CSSImageGeneratorValue* imageGeneratorValue = pendingImage->cssImageGeneratorValue();
        imageGeneratorValue->loadSubimages(m_fetcher);
        return StyleGeneratedImage::create(imageGeneratorValue);
    }

    if (pendingImage->cssCursorImageValue()) {
        CSSCursorImageValue* cursorImageValue = pendingImage->cssCursorImageValue();
        return cursorImageValue->cachedImage(m_fetcher, deviceScaleFactor);
    }

    if (pendingImage->cssImageSetValue()) {
        CSSImageSetValue* imageSetValue = pendingImage->cssImageSetValue();
        return imageSetValue->cachedImageSet(m_fetcher, deviceScaleFactor);
    }

    return 0;
}

void StyleResourceLoader::loadPendingShapeImage(RenderStyle* renderStyle, ShapeValue* shapeValue)
{
    if (!shapeValue)
        return;

    StyleImage* image = shapeValue->image();
    if (!image || !image->isPendingImage())
        return;

    StylePendingImage* pendingImage = static_cast<StylePendingImage*>(image);
    CSSImageValue* cssImageValue =  pendingImage->cssImageValue();

    ResourceLoaderOptions options = ResourceFetcher::defaultResourceOptions();
    options.requestOriginPolicy = RestrictToSameOrigin;

    shapeValue->setImage(cssImageValue->cachedImage(m_fetcher, options));
}

void StyleResourceLoader::loadPendingImages(RenderStyle* style, const ElementStyleResources& elementStyleResources)
{
    if (elementStyleResources.pendingImageProperties().isEmpty())
        return;

    PendingImagePropertyMap::const_iterator::Keys end = elementStyleResources.pendingImageProperties().end().keys();
    for (PendingImagePropertyMap::const_iterator::Keys it = elementStyleResources.pendingImageProperties().begin().keys(); it != end; ++it) {
        CSSPropertyID currentProperty = *it;

        switch (currentProperty) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = style->accessBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                if (backgroundLayer->image() && backgroundLayer->image()->isPendingImage())
                    backgroundLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(backgroundLayer->image()), elementStyleResources.deviceScaleFactor()));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(style->contentData()); contentData; contentData = contentData->next()) {
                if (contentData->isImage()) {
                    StyleImage* image = static_cast<ImageContentData*>(contentData)->image();
                    if (image->isPendingImage()) {
                        RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(image), elementStyleResources.deviceScaleFactor());
                        if (loadedImage)
                            static_cast<ImageContentData*>(contentData)->setImage(loadedImage.release());
                    }
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = style->cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    if (StyleImage* image = currentCursor.image()) {
                        if (image->isPendingImage())
                            currentCursor.setImage(loadPendingImage(static_cast<StylePendingImage*>(image), elementStyleResources.deviceScaleFactor()));
                    }
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            if (style->listStyleImage() && style->listStyleImage()->isPendingImage())
                style->setListStyleImage(loadPendingImage(static_cast<StylePendingImage*>(style->listStyleImage()), elementStyleResources.deviceScaleFactor()));
            break;
        }
        case CSSPropertyBorderImageSource: {
            if (style->borderImageSource() && style->borderImageSource()->isPendingImage())
                style->setBorderImageSource(loadPendingImage(static_cast<StylePendingImage*>(style->borderImageSource()), elementStyleResources.deviceScaleFactor()));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = style->boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                if (maskImage.image() && maskImage.image()->isPendingImage()) {
                    RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(maskImage.image()), elementStyleResources.deviceScaleFactor());
                    reflection->setMask(NinePieceImage(loadedImage.release(), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            if (style->maskBoxImageSource() && style->maskBoxImageSource()->isPendingImage())
                style->setMaskBoxImageSource(loadPendingImage(static_cast<StylePendingImage*>(style->maskBoxImageSource()), elementStyleResources.deviceScaleFactor()));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = style->accessMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                if (maskLayer->image() && maskLayer->image()->isPendingImage())
                    maskLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(maskLayer->image()), elementStyleResources.deviceScaleFactor()));
            }
            break;
        }
        case CSSPropertyWebkitShapeInside:
            loadPendingShapeImage(style, style->shapeInside());
            break;
        case CSSPropertyWebkitShapeOutside:
            loadPendingShapeImage(style, style->shapeOutside());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void StyleResourceLoader::loadPendingShaders(RenderStyle* style, const ElementStyleResources& elementStyleResources)
{
    if (!style->hasFilter() || !elementStyleResources.hasPendingShaders())
        return;

    Vector<RefPtr<FilterOperation> >& filterOperations = style->mutableFilter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filterOperations.at(i);
        if (filterOperation->getOperationType() == FilterOperation::CUSTOM) {
            CustomFilterOperation* customFilter = static_cast<CustomFilterOperation*>(filterOperation.get());
            ASSERT(customFilter->program());
            StyleCustomFilterProgram* program = static_cast<StyleCustomFilterProgram*>(customFilter->program());
            // Note that the StylePendingShaders could be already resolved to StyleFetchedShaders. That's because the rule was matched before.
            // However, the StyleCustomFilterProgram that was initially created could have been removed from the cache in the meanwhile,
            // meaning that we get a new StyleCustomFilterProgram here that is not yet in the cache, but already has loaded StyleShaders.
            if (!program->hasPendingShaders() && program->inCache())
                continue;
            RefPtr<StyleCustomFilterProgram> styleProgram = m_customFilterProgramCache->lookup(program);
            if (styleProgram.get()) {
                customFilter->setProgram(styleProgram.release());
            } else {
                if (program->vertexShader() && program->vertexShader()->isPendingShader()) {
                    CSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->vertexShader())->cssShaderValue();
                    program->setVertexShader(shaderValue->resource(m_fetcher));
                }
                if (program->fragmentShader() && program->fragmentShader()->isPendingShader()) {
                    CSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->fragmentShader())->cssShaderValue();
                    program->setFragmentShader(shaderValue->resource(m_fetcher));
                }
                m_customFilterProgramCache->add(program);
            }
        }
    }
}

void StyleResourceLoader::loadPendingResources(RenderStyle* renderStyle, ElementStyleResources& elementStyleResources)
{
    // Start loading images referenced by this style.
    loadPendingImages(renderStyle, elementStyleResources);

    // Start loading the shaders referenced by this style.
    loadPendingShaders(renderStyle, elementStyleResources);

    // Start loading the SVG Documents referenced by this style.
    loadPendingSVGDocuments(renderStyle, elementStyleResources);

    // FIXME: Investigate if this clearing is necessary.
    elementStyleResources.clear();
}

}
