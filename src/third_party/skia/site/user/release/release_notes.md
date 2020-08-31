Milestone Release Notes
=======================

This page includes a list of high level updates for each milestone release.

* * *

Milestone 83
------------

  * Remove localmatrix option from SkShaders::[Blend, Lerp]

  * Fill out Direct3D parameters for backend textures and backend rendertargets.

  * SkImage::makeTextureImage() takes an optional SkBudgeted param

  * Made non-GL builds of GPU backend more robust.
    https://review.skia.org/277456

  * MoltenVK support removed. Use Metal backend instead.
    https://review.skia.org/277612

* * *

Milestone 82
------------

  * Removed drawBitmap and related functions from SkDevice; all public drawBitmap functions on
    SkCanvas automatically wrap the bitmap in an SkImage and call the equivalent drawImage function.
    Drawing mutable SkBitmaps will now incur a mandatory copy. Switch to using SkImage directly or
    mark the bitmap as immutable before drawing.

  * Removed "volatile" flag from SkVertices. All SkVertices objects are assumed to be
    volatile (the previous default behavior).

  * Removed exotic legacy bitmap functions from SkCanvas (drawBitmapLattic, drawBitmapNine); the
    exotic SkImage functions still exist.

  * Make it possible to selectively turn on/off individual encoders/decoders,
    using skia_use_(libpng/libjpeg_turbo/libwebp)(decode/encode).

  * Removed GrGpuResource, GrSurface, and GrTexture from public api. These were not
    meant to be public, and we now can move them into src. Also removed getTexture
    function from SkImage.h

  * Removed Bones from SkVertices

  * Added a field to GrContextOptions that controls whether GL errors are checked after
    GL calls that allocate textures, etc. It also controls checking for shader compile
    success, and program linking success.

  * Made SkDeferredDisplayList.h officially part of the public API (i.e., moved it to
    include/core). Also added a ProgramIterator to SkDeferredDisplayList which allows
    clients to pre-compile some of the shaders the DDL requires.

  * Added two new helper methods to SkSurfaceCharacterization: createBackendFormat and
    createFBO0. These make it easier for clients to create new surface characterizations that
    differ only a little from an existing surface characterization.

  * Removed SkTMax and SkTMin.
  * Removed SkTClamp and SkClampMax.
  * Removed SkScalarClampMax and SkScalarPin.
  * Removed SkMax32 and SkMin32.
  * Removed SkMaxScalar and SkMinScalar.

  * SkColorSetA now warns if the result is unused.

  * An SkImageInfo with a null SkColorSpace passed to SkCodec::getPixels() and
    related calls is treated as a request to do no color correction at decode
    time.

  * Add new APIs to add attributes to document structure node when
    creating a tagged PDF.

  * Remove CGFontRef parameter from SkCreateTypefaceFromCTFont.
    Use CTFontManagerCreateFontDescriptorFromData instead of
    CGFontCreateWithDataProvider to create CTFonts to avoid memory use issues.

  * Added SkCodec:: and SkAndroidCodec::getICCProfile for reporting the native
    ICC profile of an encoded image, even if it doesn't map to an SkColorSpace.

  * SkSurface::ReplaceBackendTexture takes ContentChangeMode as a parameter,
    which allow callers to specify whether retain a copy of the current content.

  * Enforce the existing documentation in SkCanvas::saveLayer that it ignores
    any mask filter on the restore SkPaint. The 'coverage' of a layer is
    ill-defined, and masking should be handled by pre-clipping or using the
    auxiliary clip mask image of the SaveLayerRec.

Milestone 81
------------

  * Added support for GL_NV_fence extension.

  * Make SkImageInfo::validRowBytes require rowBytes to be pixel aligned. This
    makes SkBitmap match the behavior of raster SkSurfaces in rejecting
    non-aligned rowBytes.

  * Added an SkImage::MakeRasterFromCompressed entry point. Also updated
    SkImage::MakeFromCompressed to decompress the compressed image data if
    the GPU doesn't support the specified compression type (i.e., macOS Metal
    doesn't support BC1_RGB8_UNORM so such compressed images will always be
    decompressed on that platform).

  * Added support for BC1 RGBA compressed textures

  * Added CachingHint to SkImage::makeRasterImage

  * Added SkAnimatedImage::getCurrentFrame()

  * Add support to create an SkSurface from an MTKView, with delayed acquisition of
    the MTLDrawable.
    Entry point: SkSurface::MakeFromMTKView

  * Removed SkIRect::EmptyIRect(). Use SkIRect::MakeEmpty() instead.
    https://review.skia.org/262382/

  * Moved SkRuntimeEffect to public API. This is the new (experimental) interface to custom SkSL
    shaders and color filters.

  * Added BC1 compressed format support. Metal and Vulkan seem to only support the BC
    formats on desktop machines.

  * Added compressed format support for backend texture creation API.
    This adds the following new entry points:
    GrContext::compressedBackendFormat
    GrContext::createCompressedBackendTexture
    The latter method comes in variants that allow color-initialized and
    compressed texture data initialized.

  * Added SkMatrix::MakeTrans(SkIVector)
    https://review.skia.org/259804


* * *

Milestone 80
------------

  * Removed SkSize& SkSize::operator=(const SkISize&)
    https://review.skia.org/257880

  * SkISize width() and height() now constexpr
    https://review.skia.org/257680

  * Added SkMatrix::MakeTrans(SkVector) and SkRect::makeOffset(SkVector).
    https://review.skia.org/255782

  * Added SkImageInfo::MakeA8(SkISize) and added optional color space parameter to
    SkImageInfo::MakeN32Premul(SkISize).

  * Added dimensions() and getFrameCount() to SkAnimatedImage
    https://review.skia.org/253542

  * Removed SkMatrix44 version of toXYZD50 from SkColorSpace. Switched to skcms types in
    transferFn, invTrasnferFn, and gamutTransformTo functions.
    https://review.skia.org/252596

  * Removed rotation and YUV support from SkColorMatrix
    https://review.skia.org/252188

  * Added kBT2020_SkYUVColorSpace. This is BT.2020's YCbCr conversion (non-constant-luminance).
    https://review.skia.org/252160

  * Remove old async read pixels APIs
    https://review.skia.org/251198

  * Expose SkBlendModeCoeff and SkBlendMode_AsCoeff for Porter-Duff blend modes.
    https://review.skia.org/252600

* * *

Milestone 79
------------

  * SkTextBlob::Iter to discover the glyph indices and typefaces in each run
    https://review.skia.org/246296

  * Added support for PQ and HLG transfer functions to SkColorSpace.
    https://review.skia.org/249000

  * Added new api on GrContext ComputeImageSize. This replaces the hold static helper
    ComputeTextureSize.
    https://review.skia.org/247337

  * New versions of SkSurface async-rescale-and read APIs that allow client to extend
    the lifetime of the result data. Old versions are deprecated.
    https://review.skia.org/245457

  * Add SkColorInfo. It's dimensionless SkImageInfo.
    https://review.skia.org/245261

  * Added SkPixmap-based createBackendTexture method to GrContext. This allows clients to create
    backend resources (initialized with texture data) that Skia/Ganesh doesn't know about/track.
    https://review.skia.org/244676

  * Add explicit src and dst colorspace parameters to SkColorFilter::filterColor4f()
    https://review.skia.org/244882

  * Remove Vulkan/Metal float32 RGBA texture support
    https://review.skia.org/244881

  * Add SkSurface::MakeFromCAMetalLayer
    https://review.skia.org/242563

  * Added kAlpha_F16_SkColorType, kRG_F16_SkColorType and kRGBA_16161616_SkColorType.
    This is intended to help support HDR YUV uses case (e.g., P010 and P016). As such,
    the addition is focused on allowing creation of SkPixmaps and SkImages and not
    SkSurfaces (i.e., who wants to render to render to these?)
    https://review.skia.org/241357

  * Start to move nested SkPath types (e.g. Direction, Verb) up to root level in SkPathTypes.h
    https://review.skia.org/241079

  * Remove isRectContour and ksNestedFillRects from public
    https://review.skia.org/241078

  * Added kRG_88_SkColorType. This is intended to help support YUV uses case (e.g., NV12).
    As such, the addition is focused on allowing creation of SkPixmaps and SkImages and not
    SkSurfaces (i.e., who wants to render to RG?)
    https://review.skia.org/239930
    https://review.skia.org/235797

  * Make the size of program/pipeline caches configurable via
    GrContextOptions::fRuntimeProgramCacheSize
    https://review.skia.org/239756

  * Added kAlpha_16_SkColorType and kRG_1616_SkColorType. This is intended to help support HDR YUV
    uses case (e.g., P010 and P016). As such, the addition is focused on allowing creation of
    SkPixmaps and SkImages and not SkSurfaces (i.e., who wants to render to render to these?)
    https://review.skia.org/239930

  * Add GrContext::precompileShader to allow up-front compilation of previously-cached shaders.
    https://review.skia.org/239438

* * *

Milestone 78
------------
  * Added RELEASE_NOTES.txt file
    https://review.skia.org/229760

  * SkDrawLooper is no longer supported in SkPaint or SkCanvas.
    https://review.skia.org/230579
    https://review.skia.org/231736

  * SkPath::Iter::next() now ignores its consumDegenerates bools. Those will so
    go away entirely
    https://review.skia.org/235104

  * SkImage: new factories: DecodeToRaster, DecodeToTexture
    https://review.skia.org/234476

  * SkImageFilter API refactor started:
    - Provide new factory API in include/effects/SkImageFilters
    - Consolidated enum types to use SkTileMode and SkColorChannel
    - Hide filter implementation classes
    - Hide previously public functions on SkImageFilter that were intended for
      internal use only
    https://review.skia.org/230198
    https://review.skia.org/230876
    https://review.skia.org/231256

  * SkColorFilters::HSLAMatrix - new matrix color filter operating in HSLA
    space.
    https://review.skia.org/231736

  * Modify GrBackendFormat getters to not return internal pointers. Use an enum
    class for GL formats.
    https://review.skia.org/233160

  * Expose GrContext::dump() when SK_ENABLE_DUMP_GPU is defined.
    https://review.skia.org/233557

  * Vulkan backend now supports YCbCr sampler for I420 Vulkan images that are
    not backed by external images.
    https://review.skia.org/233776

  * Add SkCodec::SelectionPolicy for distinguishing between decoding a still
    image or an image sequence for a container format that has both (e.g. HEIF).
    https://review.skia.org/232839

  * SkImage::makeTextureImage and SkImage::MakeCrossContextFromPixmap no longer
    take an SkColorSpace parameter. It was unused.
    https://review.skia.org/234579
    https://review.skia.org/234912

  * SkImage::reinterpretColorSpace - to reinterpret image contents in a new
    color space.
    https://review.skia.org/234328

  * Removed SkImage::MakeCrossContextFromEncoded.
    https://review.skia.org/234912

  * Add Metal support for GrFence, GrSemaphore, and GrBackendSemaphore
    https://review.skia.org/233416

  * SkMallocPixelRef: remove MakeDirect and MakeWithProc from API.
    https://review.skia.org/234660

  * Remove 4-parameter variant of SkRect::join() and intersect(), and
    noemptycheck variants of intersect().
    https://review.skia.org/235832
    https://review.skia.org/237142

  * Remove unused sk_sp comparison operators.
    https://review.skia.org/236942

  * Add SkColor4f variant to experimental_DrawEdgeAAQuad for SkiaRenderer.
    https://review.skia.org/237492

  * Deprecated maxCount resource cache limit for Ganesh.
    This hasn't been relevant for a long time.

  * Changed GrContextOptions' fDisallowGLSLBinaryCaching to fShaderCacheStrategy,
    and allow caching SkSL.
    https://review.skia.org/238856

