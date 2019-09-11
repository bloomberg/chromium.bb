#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom/aom_codec.h"
#include "aom/internal/aom_image_internal.h"
#include "aom_scale/yv12config.h"

TEST(MetadataMemoryHandlingTest, MetadataAllocation) {
  aom_metadata_t *metadata;
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  metadata = aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);
  ASSERT_TRUE(metadata != NULL);

  int status = aom_img_metadata_free(metadata);
  EXPECT_EQ(status, 0);
}

TEST(MetadataMemoryHandlingTest, MetadataArrayAllocation) {
  aom_metadata_array_t *metadata_array;
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  metadata_array = aom_img_metadata_array_alloc(2);
  ASSERT_TRUE(metadata_array != NULL);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);
  metadata_array->metadata_array[1] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);

  int status = aom_img_metadata_array_free(metadata_array);
  EXPECT_EQ(status, 2);
}

TEST(MetadataMemoryHandlingTest, AddMetadataToImage) {
  aom_image_t image;
  image.metadata = NULL;

  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  int status =
      aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35, data, 10);
  ASSERT_EQ(status, 0);

  status = aom_img_metadata_array_free(image.metadata);
  EXPECT_EQ(status, 1);

  status = aom_img_add_metadata(NULL, OBU_METADATA_TYPE_ITUT_T35, data, 10);
  EXPECT_EQ(status, -1);
}

TEST(MetadataMemoryHandlingTest, RemoveMetadataFromImage) {
  aom_image_t image;
  image.metadata = NULL;

  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  int status =
      aom_img_add_metadata(&image, OBU_METADATA_TYPE_ITUT_T35, data, 10);
  ASSERT_EQ(status, 0);

  status = aom_img_remove_metadata(&image);
  EXPECT_EQ(status, 1);

  status = aom_img_remove_metadata(NULL);
  EXPECT_EQ(status, 0);
}

TEST(MetadataMemoryHandlingTest, CopyMetadataToFrameBUffer) {
  YV12_BUFFER_CONFIG yvBuf;
  yvBuf.metadata = NULL;
  aom_metadata_array_t *metadata_array;
  aom_metadata_array_t *metadata_array_2;
  uint8_t data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  metadata_array = aom_img_metadata_array_alloc(1);
  ASSERT_TRUE(metadata_array != NULL);

  metadata_array->metadata_array[0] =
      aom_img_metadata_alloc(OBU_METADATA_TYPE_ITUT_T35, data, 10);

  // Metadata_array
  int status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array);
  EXPECT_EQ(status, 0);

  status = aom_copy_metadata_to_frame_buffer(NULL, metadata_array);
  EXPECT_EQ(status, -1);

  status = aom_img_metadata_array_free(metadata_array);
  EXPECT_EQ(status, 1);

  // Metadata_array_2
  metadata_array_2 = aom_img_metadata_array_alloc(0);
  ASSERT_TRUE(metadata_array_2 != NULL);

  status = aom_copy_metadata_to_frame_buffer(&yvBuf, metadata_array_2);
  EXPECT_EQ(status, -1);

  status = aom_img_metadata_array_free(metadata_array_2);
  EXPECT_EQ(status, 0);

  // YV12_BUFFER_CONFIG
  status = aom_copy_metadata_to_frame_buffer(&yvBuf, NULL);
  EXPECT_EQ(status, -1);

  status = aom_remove_metadata_from_frame_buffer(NULL);
  EXPECT_EQ(status, 0);

  status = aom_remove_metadata_from_frame_buffer(&yvBuf);
  EXPECT_EQ(status, 1);
}
