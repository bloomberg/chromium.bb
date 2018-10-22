//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.cpp: Implements the egl::Image class representing the EGLimage object.

#include "libANGLE/Image.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Texture.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/renderer/ImageImpl.h"

namespace egl
{

namespace
{
gl::ImageIndex GetImageIndex(EGLenum eglTarget, const egl::AttributeMap &attribs)
{
    if (eglTarget == EGL_GL_RENDERBUFFER)
    {
        return gl::ImageIndex();
    }

    gl::TextureTarget target = egl_gl::EGLImageTargetToTextureTarget(eglTarget);
    GLint mip                = static_cast<GLint>(attribs.get(EGL_GL_TEXTURE_LEVEL_KHR, 0));
    GLint layer              = static_cast<GLint>(attribs.get(EGL_GL_TEXTURE_ZOFFSET_KHR, 0));

    if (target == gl::TextureTarget::_3D)
    {
        return gl::ImageIndex::Make3D(mip, layer);
    }
    else
    {
        ASSERT(layer == 0);
        return gl::ImageIndex::MakeFromTarget(target, mip);
    }
}

const Display *DisplayFromContext(const gl::Context *context)
{
    return (context ? context->getCurrentDisplay() : nullptr);
}

}  // anonymous namespace

ImageSibling::ImageSibling() : FramebufferAttachmentObject(), mSourcesOf(), mTargetOf()
{
}

ImageSibling::~ImageSibling()
{
    // EGL images should hold a ref to their targets and siblings, a Texture should not be deletable
    // while it is attached to an EGL image.
    // Child class should orphan images before destruction.
    ASSERT(mSourcesOf.empty());
    ASSERT(mTargetOf.get() == nullptr);
}

void ImageSibling::setTargetImage(const gl::Context *context, egl::Image *imageTarget)
{
    ASSERT(imageTarget != nullptr);
    mTargetOf.set(DisplayFromContext(context), imageTarget);
    imageTarget->addTargetSibling(this);
}

gl::Error ImageSibling::orphanImages(const gl::Context *context)
{
    if (mTargetOf.get() != nullptr)
    {
        // Can't be a target and have sources.
        ASSERT(mSourcesOf.empty());

        ANGLE_TRY(mTargetOf->orphanSibling(context, this));
        mTargetOf.set(DisplayFromContext(context), nullptr);
    }
    else
    {
        for (Image *sourceImage : mSourcesOf)
        {
            ANGLE_TRY(sourceImage->orphanSibling(context, this));
        }
        mSourcesOf.clear();
    }

    return gl::NoError();
}

void ImageSibling::addImageSource(egl::Image *imageSource)
{
    ASSERT(imageSource != nullptr);
    mSourcesOf.insert(imageSource);
}

void ImageSibling::removeImageSource(egl::Image *imageSource)
{
    ASSERT(mSourcesOf.find(imageSource) != mSourcesOf.end());
    mSourcesOf.erase(imageSource);
}

bool ImageSibling::isEGLImageTarget() const
{
    return (mTargetOf.get() != nullptr);
}

gl::InitState ImageSibling::sourceEGLImageInitState() const
{
    ASSERT(isEGLImageTarget());
    return mTargetOf->sourceInitState();
}

void ImageSibling::setSourceEGLImageInitState(gl::InitState initState) const
{
    ASSERT(isEGLImageTarget());
    mTargetOf->setInitState(initState);
}

ImageState::ImageState(EGLenum target, ImageSibling *buffer, const AttributeMap &attribs)
    : label(nullptr),
      imageIndex(GetImageIndex(target, attribs)),
      source(buffer),
      targets(),
      format(buffer->getAttachmentFormat(GL_NONE, imageIndex)),
      size(buffer->getAttachmentSize(imageIndex)),
      samples(buffer->getAttachmentSamples(imageIndex))
{
}

ImageState::~ImageState()
{
}

Image::Image(rx::EGLImplFactory *factory,
             const gl::Context *context,
             EGLenum target,
             ImageSibling *buffer,
             const AttributeMap &attribs)
    : mState(target, buffer, attribs),
      mImplementation(factory->createImage(mState, context, target, attribs)),
      mOrphanedAndNeedsInit(false)
{
    ASSERT(mImplementation != nullptr);
    ASSERT(buffer != nullptr);

    mState.source->addImageSource(this);
}

Error Image::onDestroy(const Display *display)
{
    // All targets should hold a ref to the egl image and it should not be deleted until there are
    // no siblings left.
    ASSERT(mState.targets.empty());

    // Tell the source that it is no longer used by this image
    if (mState.source != nullptr)
    {
        mState.source->removeImageSource(this);
        mState.source = nullptr;
    }

    return NoError();
}

Image::~Image()
{
    SafeDelete(mImplementation);
}

void Image::setLabel(EGLLabelKHR label)
{
    mState.label = label;
}

EGLLabelKHR Image::getLabel() const
{
    return mState.label;
}

void Image::addTargetSibling(ImageSibling *sibling)
{
    mState.targets.insert(sibling);
}

gl::Error Image::orphanSibling(const gl::Context *context, ImageSibling *sibling)
{
    ASSERT(sibling != nullptr);

    // notify impl
    ANGLE_TRY(mImplementation->orphan(context, sibling));

    if (mState.source == sibling)
    {
        // If the sibling is the source, it cannot be a target.
        ASSERT(mState.targets.find(sibling) == mState.targets.end());
        mState.source = nullptr;
        mOrphanedAndNeedsInit =
            (sibling->initState(mState.imageIndex) == gl::InitState::MayNeedInit);
    }
    else
    {
        mState.targets.erase(sibling);
    }

    return gl::NoError();
}

const gl::Format &Image::getFormat() const
{
    return mState.format;
}

size_t Image::getWidth() const
{
    return mState.size.width;
}

size_t Image::getHeight() const
{
    return mState.size.height;
}

size_t Image::getSamples() const
{
    return mState.samples;
}

rx::ImageImpl *Image::getImplementation() const
{
    return mImplementation;
}

Error Image::initialize(const Display *display)
{
    return mImplementation->initialize(display);
}

bool Image::orphaned() const
{
    return (mState.source == nullptr);
}

gl::InitState Image::sourceInitState() const
{
    if (orphaned())
    {
        return mOrphanedAndNeedsInit ? gl::InitState::MayNeedInit : gl::InitState::Initialized;
    }

    return mState.source->initState(mState.imageIndex);
}

void Image::setInitState(gl::InitState initState)
{
    if (orphaned())
    {
        mOrphanedAndNeedsInit = false;
    }

    return mState.source->setInitState(mState.imageIndex, initState);
}

}  // namespace egl
