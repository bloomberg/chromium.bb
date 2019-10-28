#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom/aom_codec.h"
#include "aom/internal/aom_image_internal.h"
#include "aom_scale/yv12config.h"

TEST(MetadataMemoryHandlingTest, MetadataAllocation) {
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  aom_metadata_t *metadata =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(aom_img_metadata_free(metadata), 0);
}

TEST(MetadataMemoryHandlingTest, MetadataArrayAllocation) {
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(2);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);
  metadata_array->metadata_array[1] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);

  EXPECT_EQ(aom_img_metadata_array_free(metadata_array), 2u);
}

TEST(MetadataMemoryHandlingTest, AddMetadataToImage) {
  aom_image_t image;
  image.metadata = NULL;

  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35, data, 10),
            0);
  EXPECT_EQ(aom_img_metadata_array_free(image.metadata), 1u);
  EXPECT_EQ(aom_img_add_metadata(NULL, OBU_METADATA_TYPE_ITUT_T35, data, 10),
            -1);
}

TEST(MetadataMemoryHandlingTest, RemoveMetadataFromImage) {
  aom_image_t image;
  image.metadata = NULL;

  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  ASSERT_EQ(aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35, data, 10),
            0);
  EXPECT_EQ(aom_img_remove_metadata(&image), 1u);
  EXPECT_EQ(aom_img_remove_metadata(NULL), 0u);
}

TEST(MetadataMemoryHandlingTest, CopyMetadataToFrameBUffer) {
  YV12_BUFFER_CONFIG yvBuf;
  yvBuf.metadata = NULL;
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  aom_metadata_array_t *metadata_array = aom_img_metadata_array_alloc(1);
  ASSERT_NE(metadata_array, nullptr);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);

  // Metadata_array
  int status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array);
  EXPECT_EQ(status, 0);
  status = aom_copy_metadata_to_frame_buffer(NULL, metadata_array);
  EXPECT_EQ(status, -1);
  EXPECT_EQ(aom_img_metadata_array_free(metadata_array), 1u);

  // Metadata_array_2
  aom_metadata_array_t *metadata_array_2 = aom_img_metadata_array_alloc(0);
  ASSERT_NE(metadata_array_2, nullptr);
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array_2);
  EXPECT_EQ(status, -1);
  EXPECT_EQ(aom_img_metadata_array_free(metadata_array_2), 0u);

  // YV12_BUFFER_CONFIG
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, NULL);
  EXPECT_EQ(status, -1);
  EXPECT_EQ(aom_remove_metadata_from_frame_buffer(NULL), 0u);
  EXPECT_EQ(aom_remove_metadata_from_frame_buffer(&yvBuf), 1u);
}
