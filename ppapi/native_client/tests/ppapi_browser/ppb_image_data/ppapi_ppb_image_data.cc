// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for PPB_ImageData functions.
//
// TODO(polina): use PPB_Testing for additional automatic validation.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"

namespace {

const int kPixelSizeInBytes = 4;
const PP_Size kOnePixelImageSize = PP_MakeSize(1, 1);
const PP_Size kSmallImageSize = PP_MakeSize(30, 30);
const PP_Size kLargeImageSize = PP_MakeSize(2048, 2048);
const PP_Size kValidImageSize[] = {
    kOnePixelImageSize,
    kSmallImageSize,
    kLargeImageSize
};
const int kNumValidImages = sizeof(kValidImageSize) / sizeof(PP_Size);
const PP_Size kInvalidImageSize[] = {
    PP_MakeSize(100000000, 100000000),
    PP_MakeSize(-100000000, 100000000),
    PP_MakeSize(-100, 100000000),
    PP_MakeSize(100, -100000000),
    PP_MakeSize(-100, 100),
    PP_MakeSize(-100, -100),
    PP_MakeSize(0, 0),
    PP_MakeSize(0, 100),
    PP_MakeSize(100, 0)
};
const int kNumInvalidImages = sizeof(kInvalidImageSize) / sizeof(PP_Size);
const int kManyResources = 100;
const int kManyLargeResources = 500;

union BogusFormat {
  int bogus;
  PP_ImageDataFormat format;
};

// Collection of small images.
PP_Resource image_data[kManyResources];

bool IsEqualSize(PP_Size size1, PP_Size size2) {
  return (size1.width == size2.width && size1.height == size2.height);
}

////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

// Tests PPB_ImageData::GetNativeImageDataFormat().
void TestGetNativeImageDataFormat() {
  bool test_passed = false;
  PP_ImageDataFormat format;
  // TODO(nfullagar): If new formats are added after this test is written,
  // then the switch statement below will need to be updated, and additional
  // testing will need to be done in the other test cases below.
  // Also, new formats could introduce new pixel sizeis (in bytes), which
  // could affect some of the stride checks.
  format = PPBImageData()->GetNativeImageDataFormat();
  switch (format) {
    case PP_IMAGEDATAFORMAT_BGRA_PREMUL:
    case PP_IMAGEDATAFORMAT_RGBA_PREMUL:
      test_passed = true;
  }
  EXPECT(test_passed);
  TEST_PASSED;
}

// Tests PPB_ImageData::IsImageDataFormatSupported().
void TestIsImageDataFormatSupported() {
  PP_ImageDataFormat format;
  BogusFormat bogus;
  format = PPBImageData()->GetNativeImageDataFormat();
  // EXPECT the format recommended by the browser is supported.
  EXPECT(PP_TRUE == PPBImageData()->IsImageDataFormatSupported(format));
  // Invoke with known formats, should not crash.
  PPBImageData()->IsImageDataFormatSupported(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  PPBImageData()->IsImageDataFormatSupported(PP_IMAGEDATAFORMAT_RGBA_PREMUL);
  // Make up invalid enums and verify they are not supported.
  bogus.bogus = -1;
  EXPECT(PP_FALSE == PPBImageData()->
      IsImageDataFormatSupported(bogus.format));
  bogus.bogus = 123;
  EXPECT(PP_FALSE == PPBImageData()->
      IsImageDataFormatSupported(bogus.format));
  TEST_PASSED;
}

// Tests PPB_ImageData::Create().
void TestCreate() {
  PP_Resource image_data = kInvalidResource;
  BogusFormat bogus;
  // Invalid instance and valid size -> expect an invalid resource.
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_BGRA_PREMUL, &kSmallImageSize, PP_FALSE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL, &kSmallImageSize, PP_FALSE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_BGRA_PREMUL, &kSmallImageSize, PP_TRUE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL, &kSmallImageSize, PP_TRUE);
  EXPECT(image_data == kInvalidResource);
  // NULL size -> Internal error in rpc method, expect an invalid resource.
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_BGRA_PREMUL, NULL, PP_FALSE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL, NULL, PP_FALSE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_BGRA_PREMUL, NULL, PP_TRUE);
  EXPECT(image_data == kInvalidResource);
  image_data = PPBImageData()->Create(kInvalidInstance,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL, NULL, PP_TRUE);
  EXPECT(image_data == kInvalidResource);
  // bogus image format -> expect an invalid resource.
  bogus.bogus = -1;
  image_data = PPBImageData()->Create(pp_instance(), bogus.format,
      &kSmallImageSize, PP_FALSE);
  EXPECT(image_data == kInvalidResource);
  bogus.bogus = 123;
  image_data = PPBImageData()->Create(pp_instance(), bogus.format,
      &kSmallImageSize, PP_TRUE);
  EXPECT(image_data == kInvalidResource);

  PP_Bool kPixelFill[] = {PP_FALSE, PP_TRUE};
  int kNumPixelFill = sizeof(kPixelFill) / sizeof(PP_Bool);
  PP_ImageDataFormat native_format = PPBImageData()->GetNativeImageDataFormat();
  for (int j = 0; j < kNumPixelFill; ++j) {
    // Valid instance and valid image size should generate a valid resource.
    for (int i = 0; i < kNumValidImages; ++i) {
      image_data = PPBImageData()->Create(pp_instance(), native_format,
          &kValidImageSize[i], kPixelFill[j]);
      EXPECT(image_data != kInvalidResource);
      PPBCore()->ReleaseResource(image_data);
    }
    // Valid instance and invalid size should generate an invalid resource.
    for (int i = 0; i < kNumInvalidImages; ++i) {
      image_data = PPBImageData()->Create(pp_instance(), native_format,
          &kInvalidImageSize[i], kPixelFill[j]);
      EXPECT(image_data == kInvalidResource);
    }
  }
  TEST_PASSED;
}

// Tests PPB_ImageData::IsImageData().
void TestIsImageData() {
  // Invalid / non-existent / non-ImageData resource -> false.
  EXPECT(PP_FALSE == PPBImageData()->IsImageData(kInvalidResource));
  EXPECT(PP_FALSE == PPBImageData()->IsImageData(kNotAResource));
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  CHECK(url_loader != kInvalidResource);
  EXPECT(PP_FALSE == PPBImageData()->IsImageData(url_loader));
  PPBCore()->ReleaseResource(url_loader);

  PP_ImageDataFormat format = PPBImageData()->GetNativeImageDataFormat();

  // Created ImageData resource -> true.
  PP_Resource image_data = PPBImageData()->Create(
      pp_instance(), format, &kSmallImageSize, PP_TRUE);
  EXPECT(PP_TRUE == PPBImageData()->IsImageData(image_data));

  // Released ImageData resource -> false.
  PPBCore()->ReleaseResource(image_data);
  EXPECT(PP_FALSE == PPBImageData()->IsImageData(image_data));
  TEST_PASSED;
}

// Tests PPB_ImageData::Describe().
void TestDescribe() {
  PP_Resource image_data = kInvalidResource;
  PP_ImageDataFormat format = PPBImageData()->GetNativeImageDataFormat();
  PP_ImageDataDesc description;

  // Valid resource -> output = configuration, true.
  image_data = PPBImageData()->Create(pp_instance(), format, &kSmallImageSize,
      PP_FALSE);
  memset(&description, 0, sizeof(description));
  EXPECT(PP_TRUE == PPBImageData()->Describe(image_data, &description));
  EXPECT(description.format == format);
  EXPECT(IsEqualSize(description.size, kSmallImageSize));
  EXPECT(description.stride >= description.size.width *
      kPixelSizeInBytes);
  PPBCore()->ReleaseResource(image_data);

  image_data = PPBImageData()->Create(pp_instance(), format, &kSmallImageSize,
      PP_TRUE);
  memset(&description, 0, sizeof(description));
  EXPECT(PP_TRUE == PPBImageData()->Describe(image_data, &description));
  EXPECT(description.format == format);
  EXPECT(IsEqualSize(description.size, kSmallImageSize));
  EXPECT(description.stride >= description.size.width * kPixelSizeInBytes);
  PPBCore()->ReleaseResource(image_data);

  // NULL outputs -> output = unchanged, false.
  EXPECT(PP_FALSE == PPBImageData()->Describe(image_data, NULL));

  // Invalid / non-existent resource -> output = 0, false.
  memset(&description, 0, sizeof(description));
  EXPECT(PP_FALSE == PPBImageData()->Describe(kInvalidResource, &description));
  EXPECT(IsEqualSize(description.size, PP_MakeSize(0, 0)));
  EXPECT(PP_FALSE == PPBImageData()->Describe(kNotAResource, &description));
  EXPECT(IsEqualSize(description.size, PP_MakeSize(0, 0)));
  TEST_PASSED;
}

// Tests PPB_ImageData::Map() & PPB_ImageData::Unmap().
void TestMapUnmap() {
  PP_Resource image_data = kInvalidResource;
  PP_ImageDataFormat format = PPBImageData()->GetNativeImageDataFormat();
  PP_ImageDataDesc description;
  uint8_t *pixel_byte_ptr;
  uint8_t *pixel_byte_ptr2;
  uint8_t pixel_bytes_or_together;
  image_data = PPBImageData()->Create(pp_instance(), format, &kSmallImageSize,
      PP_TRUE);
  EXPECT(PP_TRUE == PPBImageData()->Describe(image_data, &description));
  int image_size_in_bytes = description.stride * description.size.height;
  pixel_byte_ptr = static_cast<uint8_t *>(PPBImageData()->Map(image_data));
  EXPECT(NULL != pixel_byte_ptr);
  pixel_bytes_or_together = 0;
  // The image was created with a request to zero fill, so verify that
  // memory is zero filled.
  for (int i = 0; i < image_size_in_bytes; ++i) {
    pixel_bytes_or_together = pixel_bytes_or_together | pixel_byte_ptr[i];
  }
  EXPECT(0 == pixel_bytes_or_together);
  PPBImageData()->Unmap(image_data);
  PPBCore()->ReleaseResource(image_data);
  // The following shouldn't crash -- it attempts to map an image resource more
  // than once, then it attempts to release the resource, then calls unmap with
  // the released resource. It then attempts to re-map the resource, which
  // should return a NULL pointer.
  image_data = PPBImageData()->Create(pp_instance(), format, &kSmallImageSize,
      PP_TRUE);
  pixel_byte_ptr = static_cast<uint8_t *>(PPBImageData()->Map(image_data));
  EXPECT(NULL != pixel_byte_ptr);
  pixel_byte_ptr2 = static_cast<uint8_t *>(PPBImageData()->Map(image_data));
  EXPECT(pixel_byte_ptr == pixel_byte_ptr2);
  PPBCore()->ReleaseResource(image_data);
  PPBImageData()->Unmap(image_data);
  pixel_byte_ptr = static_cast<uint8_t *>(PPBImageData()->Map(image_data));
  EXPECT(NULL == pixel_byte_ptr);
  // Attempt to call map/unmap with a few other non-image data resources.
  pixel_byte_ptr = static_cast<uint8_t *>(PPBImageData()->
      Map(kInvalidResource));
  EXPECT(NULL == pixel_byte_ptr);
  PPBImageData()->Unmap(kInvalidResource);
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  CHECK(url_loader != kInvalidResource);
  pixel_byte_ptr = static_cast<uint8_t *>(PPBImageData()->Map(url_loader));
  EXPECT(NULL == pixel_byte_ptr);
  PPBCore()->ReleaseResource(url_loader);
  TEST_PASSED;
}

// Stress testing creation of a large number of small images.
void TestStressPartA() {
  PP_ImageDataFormat format = PPBImageData()->GetNativeImageDataFormat();
  for (int i = 0; i < kManyResources; ++i) {
    image_data[i] = kInvalidResource;
  }
  // Create a large number of co-existing memory mapped small images.
  for (int i = 0; i < kManyResources; ++i) {
    image_data[i] = PPBImageData()->Create(pp_instance(), format,
        &kSmallImageSize, PP_TRUE);
    EXPECT(image_data[i] != kInvalidResource);
    // Check to see if resource id is duplicate.
    // TODO(nfullagar): This could be done faster if the test needs to increase
    // the number of resources to create.
    for (int j = 0; j < i; ++j) {
      EXPECT(image_data[j] != image_data[i]);
    }
    EXPECT(PP_TRUE == PPBImageData()->IsImageData(image_data[i]));
    uint32_t* pixel_ptr = NULL;
    pixel_ptr = static_cast<uint32_t*>(PPBImageData()->Map(image_data[i]));
    EXPECT(NULL != pixel_ptr);
    // Attempt to write first pixel.
    pixel_ptr[0] = 0;
  }
  TEST_PASSED;
}

// Stress testing many create / release of large images.
void TestStressPartB() {
  PP_Resource large_image_data;
  PP_ImageDataFormat format = PPBImageData()->GetNativeImageDataFormat();
  // A large number of create-map-unmap-release large images.
  // Only one large image exists at a time; make sure memory space isn't
  // exhausted.  See issue:
  // http://code.google.com/p/chromium/issues/detail?id=120728
  for (int i = 0; i < kManyLargeResources; i++) {
    large_image_data = PPBImageData()->Create(pp_instance(), format,
        &kLargeImageSize, PP_TRUE);
    EXPECT(large_image_data != kInvalidResource);
    EXPECT(PP_TRUE == PPBImageData()->IsImageData(large_image_data));
    uint32_t* pixel_ptr = NULL;
    pixel_ptr = static_cast<uint32_t*>(PPBImageData()->Map(large_image_data));
    EXPECT(NULL != pixel_ptr);
    // Scatter write across many pixels in the large image.
    const int scatter_delta = 16;
    for (int y = 0; y < kLargeImageSize.height; y += scatter_delta)
      for (int x = 0; x < kLargeImageSize.width; x += scatter_delta)
        pixel_ptr[y * kLargeImageSize.width + x] = 0;
    PPBImageData()->Unmap(large_image_data);
    PPBCore()->ReleaseResource(large_image_data);
    EXPECT(PP_FALSE == PPBImageData()->IsImageData(large_image_data));
  }
  TEST_PASSED;
}

// Release the collection of small images created in part A.
void TestStressPartC() {
  // Go back and unmap the smaller images.
  for (int i = 0; i < kManyResources; i++) {
    if (image_data[i] != kInvalidResource) {
      PPBImageData()->Unmap(image_data[i]);
      EXPECT(PP_TRUE == PPBImageData()->IsImageData(image_data[i]));
      PPBCore()->ReleaseResource(image_data[i]);
      EXPECT(PP_FALSE == PPBImageData()->IsImageData(image_data[i]));
    }
  }
  TEST_PASSED;
}
}  // namespace

void SetupTests() {
  RegisterTest("TestGetNativeImageDataFormat", TestGetNativeImageDataFormat);
  RegisterTest("TestIsImageDataFormatSupported",
      TestIsImageDataFormatSupported);
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsImageData", TestIsImageData);
  RegisterTest("TestDescribe", TestDescribe);
  RegisterTest("TestMapUnmap", TestMapUnmap);
  RegisterTest("TestStressPartA", TestStressPartA);
  RegisterTest("TestStressPartB", TestStressPartB);
  RegisterTest("TestStressPartC", TestStressPartC);
}

void SetupPluginInterfaces() {
  // none
}
