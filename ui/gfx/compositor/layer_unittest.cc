// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/compositor/compositor_observer.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/test/test_compositor.h"
#include "ui/gfx/compositor/test/test_compositor_host.h"
#include "ui/gfx/gfx_paths.h"

namespace ui {

namespace {

// Encodes a bitmap into a PNG and write to disk. Returns true on success. The
// parent directory does not have to exist.
bool WritePNGFile(const SkBitmap& bitmap, const FilePath& file_path) {
  std::vector<unsigned char> png_data;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data) &&
      file_util::CreateDirectory(file_path.DirName())) {
    char* data = reinterpret_cast<char*>(&png_data[0]);
    int size = static_cast<int>(png_data.size());
    return file_util::WriteFile(file_path, data, size) == size;
  }
  return false;
}

// Reads and decodes a PNG image to a bitmap. Returns true on success. The PNG
// should have been encoded using |gfx::PNGCodec::Encode|.
bool ReadPNGFile(const FilePath& file_path, SkBitmap* bitmap) {
  DCHECK(bitmap);
  std::string png_data;
  return file_util::ReadFileToString(file_path, &png_data) &&
         gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&png_data[0]),
                               png_data.length(),
                               bitmap);
}

// Compares with a PNG file on disk, and returns true if it is the same as
// the given image. |ref_img_path| is absolute.
bool IsSameAsPNGFile(const SkBitmap& gen_bmp, FilePath ref_img_path) {
  SkBitmap ref_bmp;
  if (!ReadPNGFile(ref_img_path, &ref_bmp)) {
    LOG(ERROR) << "Cannot read reference image: " << ref_img_path.value();
    return false;
  }

  if (ref_bmp.width() != gen_bmp.width() ||
      ref_bmp.height() != gen_bmp.height()) {
    LOG(ERROR)
        << "Dimensions do not match (Expected) vs (Actual):"
        << "(" << ref_bmp.width() << "x" << ref_bmp.height()
            << ") vs. "
        << "(" << gen_bmp.width() << "x" << gen_bmp.height() << ")";
    return false;
  }

  // Compare pixels and create a simple diff image.
  int diff_pixels_count = 0;
  SkAutoLockPixels lock_bmp(gen_bmp);
  SkAutoLockPixels lock_ref_bmp(ref_bmp);
  // The reference images were saved with no alpha channel. Use the mask to
  // set alpha to 0.
  uint32_t kAlphaMask = 0x00FFFFFF;
  for (int x = 0; x < gen_bmp.width(); ++x) {
    for (int y = 0; y < gen_bmp.height(); ++y) {
      if ((*gen_bmp.getAddr32(x, y) & kAlphaMask) !=
          (*ref_bmp.getAddr32(x, y) & kAlphaMask)) {
        ++diff_pixels_count;
      }
    }
  }

  if (diff_pixels_count != 0) {
    LOG(ERROR) << "Images differ by pixel count: " << diff_pixels_count;
    return false;
  }

  return true;
}

// Returns a comma-separated list of the names of |layer|'s children in
// bottom-to-top stacking order.
std::string GetLayerChildrenNames(const Layer& layer) {
  std::string names;
  for (std::vector<Layer*>::const_iterator it = layer.children().begin();
       it != layer.children().end(); ++it) {
    if (!names.empty())
      names += ",";
    names += (*it)->name();
  }
  return names;
}


// There are three test classes in here that configure the Compositor and
// Layer's slightly differently:
// - LayerWithNullDelegateTest uses TestCompositor and NullLayerDelegate as the
//   LayerDelegate. This is typically the base class you want to use.
// - LayerWithDelegateTest uses TestCompositor and does not set a LayerDelegate
//   on the delegates.
// - LayerWithRealCompositorTest when a real compositor is required for testing.
//    - Slow because they bring up a window and run the real compositor. This
//      is typically not what you want.

class ColoredLayer : public Layer, public LayerDelegate {
 public:
  explicit ColoredLayer(SkColor color)
      : Layer(Layer::LAYER_HAS_TEXTURE),
        color_(color) {
    set_delegate(this);
  }

  virtual ~ColoredLayer() { }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    canvas->GetSkCanvas()->drawColor(color_);
  }

 private:
  SkColor color_;
};

class LayerWithRealCompositorTest : public testing::Test {
 public:
  LayerWithRealCompositorTest() {
    if (PathService::Get(gfx::DIR_TEST_DATA, &test_data_directory_)) {
      test_data_directory_ = test_data_directory_.AppendASCII("compositor");
    } else {
      LOG(ERROR) << "Could not open test data directory.";
    }
  }
  virtual ~LayerWithRealCompositorTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    const gfx::Rect host_bounds(10, 10, 500, 500);
    window_.reset(TestCompositorHost::Create(host_bounds));
    window_->Show();
  }

  virtual void TearDown() OVERRIDE {
  }

  Compositor* GetCompositor() {
    return window_->GetCompositor();
  }

  Layer* CreateLayer(Layer::LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  gfx::Canvas* CreateCanvasForLayer(const Layer* layer) {
    return gfx::Canvas::CreateCanvas(layer->bounds().size(), false);
  }

  void DrawTree(Layer* root) {
    GetCompositor()->SetRootLayer(root);
    GetCompositor()->Draw(false);
  }

  bool ReadPixels(SkBitmap* bitmap) {
    return GetCompositor()->ReadPixels(bitmap,
                                       gfx::Rect(GetCompositor()->size()));
  }

  void RunPendingMessages() {
    MessageLoopForUI::current()->RunAllPending();
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  const FilePath& test_data_directory() const { return test_data_directory_; }

 private:
  scoped_ptr<TestCompositorHost> window_;

  // The root directory for test files.
  FilePath test_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithRealCompositorTest);
};

// LayerDelegate that paints colors to the layer.
class TestLayerDelegate : public LayerDelegate {
 public:
  explicit TestLayerDelegate() : color_index_(0) {}
  virtual ~TestLayerDelegate() {}

  void AddColor(SkColor color) {
    colors_.push_back(color);
  }

  gfx::Size paint_size() const { return paint_size_; }
  int color_index() const { return color_index_; }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    SkBitmap contents = canvas->AsCanvasSkia()->ExtractBitmap();
    paint_size_ = gfx::Size(contents.width(), contents.height());
    canvas->FillRect(colors_[color_index_],
                     gfx::Rect(gfx::Point(), paint_size_));
    color_index_ = (color_index_ + 1) % static_cast<int>(colors_.size());
  }

 private:
  std::vector<SkColor> colors_;
  int color_index_;
  gfx::Size paint_size_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

// LayerDelegate that verifies that a layer was asked to update its canvas.
class DrawTreeLayerDelegate : public LayerDelegate {
 public:
  DrawTreeLayerDelegate() : painted_(false) {}
  virtual ~DrawTreeLayerDelegate() {}

  void Reset() {
    painted_ = false;
  }

  bool painted() const { return painted_; }

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    painted_ = true;
  }

  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(DrawTreeLayerDelegate);
};

// The simplest possible layer delegate. Does nothing.
class NullLayerDelegate : public LayerDelegate {
 public:
  NullLayerDelegate() {}
  virtual ~NullLayerDelegate() {}

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(NullLayerDelegate);
};

// Remembers if it has been notified.
class TestCompositorObserver : public CompositorObserver {
 public:
  TestCompositorObserver() : notified_(false) {}

  bool notified() const { return notified_; }

  void Reset() { notified_ = false; }

 private:
  virtual void OnCompositingEnded(Compositor* compositor) OVERRIDE {
    notified_ = true;
  }

  bool notified_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorObserver);
};

}

#if defined(OS_WIN)
// These are disabled on windows as they don't run correctly on the buildbot.
// Reenable once we move to the real compositor.
#define MAYBE_Delegate DISABLED_Delegate
#define MAYBE_Draw DISABLED_Draw
#define MAYBE_DrawTree DISABLED_DrawTree
#define MAYBE_Hierarchy DISABLED_Hierarchy
#define MAYBE_HierarchyNoTexture DISABLED_HierarchyNoTexture
#define MAYBE_DrawPixels DISABLED_DrawPixels
#define MAYBE_SetRootLayer DISABLED_SetRootLayer
#define MAYBE_CompositorObservers DISABLED_CompositorObservers
#define MAYBE_ModifyHierarchy DISABLED_ModifyHierarchy
#define MAYBE_Opacity DISABLED_Opacity
#else
#define MAYBE_Delegate Delegate
#define MAYBE_Draw Draw
#define MAYBE_DrawTree DrawTree
#define MAYBE_Hierarchy Hierarchy
#define MAYBE_HierarchyNoTexture HierarchyNoTexture
#define MAYBE_DrawPixels DrawPixels
#define MAYBE_SetRootLayer SetRootLayer
#define MAYBE_CompositorObservers CompositorObservers
#define MAYBE_ModifyHierarchy ModifyHierarchy
#define MAYBE_Opacity Opacity
#endif

TEST_F(LayerWithRealCompositorTest, MAYBE_Draw) {
  scoped_ptr<Layer> layer(CreateColorLayer(SK_ColorRED,
                                           gfx::Rect(20, 20, 50, 50)));
  DrawTree(layer.get());
}

// Create this hierarchy:
// L1 - red
// +-- L2 - blue
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, MAYBE_Hierarchy) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  DrawTree(l1.get());
}

class LayerWithDelegateTest : public testing::Test, public CompositorDelegate {
 public:
  LayerWithDelegateTest() : schedule_draw_invoked_(false) {}
  virtual ~LayerWithDelegateTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    compositor_ = new TestCompositor(this);
  }

  virtual void TearDown() OVERRIDE {
  }

  Compositor* compositor() { return compositor_.get(); }

  virtual Layer* CreateLayer(Layer::LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  virtual Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  gfx::Canvas* CreateCanvasForLayer(const Layer* layer) {
    return gfx::Canvas::CreateCanvas(layer->bounds().size(), false);
  }

  void DrawTree(Layer* root) {
    compositor()->SetRootLayer(root);
    compositor()->Draw(false);
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  // Invokes DrawTree on the compositor.
  void Draw() {
    compositor_->Draw(false);
  }

  // CompositorDelegate overrides.
  virtual void ScheduleDraw() OVERRIDE {
    schedule_draw_invoked_ = true;
  }

 protected:
  // Set to true when ScheduleDraw (CompositorDelegate override) is invoked.
  bool schedule_draw_invoked_;

 private:
  scoped_refptr<TestCompositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithDelegateTest);
};

// L1
//  +-- L2
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Simple) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l2_coords(5, 5);
  Layer::ConvertPointToLayer(l2.get(), l1.get(), &point1_in_l2_coords);
  gfx::Point point1_in_l1_coords(15, 15);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l2_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l2.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l2_coords(-5, -5);
  EXPECT_EQ(point2_in_l2_coords, point2_in_l1_coords);
}

// L1
//  +-- L2
//       +-- L3
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Medium) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l3_coords(5, 5);
  Layer::ConvertPointToLayer(l3.get(), l1.get(), &point1_in_l3_coords);
  gfx::Point point1_in_l1_coords(25, 25);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l3_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l3.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l3_coords(-15, -15);
  EXPECT_EQ(point2_in_l3_coords, point2_in_l1_coords);
}

TEST_F(LayerWithRealCompositorTest, MAYBE_Delegate) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorBLACK,
                                        gfx::Rect(20, 20, 400, 400)));
  GetCompositor()->SetRootLayer(l1.get());
  RunPendingMessages();

  TestLayerDelegate delegate;
  l1->set_delegate(&delegate);
  delegate.AddColor(SK_ColorWHITE);
  delegate.AddColor(SK_ColorYELLOW);
  delegate.AddColor(SK_ColorGREEN);

  l1->SchedulePaint(gfx::Rect(0, 0, 400, 400));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 1);
  EXPECT_EQ(delegate.paint_size(), l1->bounds().size());

  l1->SchedulePaint(gfx::Rect(10, 10, 200, 200));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 2);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(200, 200));

  l1->SchedulePaint(gfx::Rect(5, 5, 50, 50));
  RunPendingMessages();
  EXPECT_EQ(delegate.color_index(), 0);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(50, 50));
}

TEST_F(LayerWithRealCompositorTest, MAYBE_DrawTree) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  RunPendingMessages();

  DrawTreeLayerDelegate d1;
  l1->set_delegate(&d1);
  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  RunPendingMessages();
  EXPECT_FALSE(d1.painted());
  EXPECT_TRUE(d2.painted());
  EXPECT_FALSE(d3.painted());
}

// Tests no-texture Layers.
// Create this hierarchy:
// L1 - red
// +-- L2 - NO TEXTURE
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, MAYBE_HierarchyNoTexture) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateNoTextureLayer(gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  RunPendingMessages();

  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  l3->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  RunPendingMessages();

  // |d2| should not have received a paint notification since it has no texture.
  EXPECT_FALSE(d2.painted());
  // |d3| should have received a paint notification.
  EXPECT_TRUE(d3.painted());
}

class LayerWithNullDelegateTest : public LayerWithDelegateTest {
 public:
  LayerWithNullDelegateTest() {}
  virtual ~LayerWithNullDelegateTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    LayerWithDelegateTest::SetUp();
    default_layer_delegate_.reset(new NullLayerDelegate());
  }

  virtual void TearDown() OVERRIDE {
  }

  Layer* CreateLayer(Layer::LayerType type) OVERRIDE {
    Layer* layer = new Layer(type);
    layer->set_delegate(default_layer_delegate_.get());
    return layer;
  }

  Layer* CreateTextureRootLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateTextureLayer(bounds);
    compositor()->SetRootLayer(layer);
    return layer;
  }

  Layer* CreateTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) OVERRIDE {
    Layer* layer = CreateLayer(Layer::LAYER_HAS_NO_TEXTURE);
    layer->SetBounds(bounds);
    return layer;
  }

  void RunPendingMessages() {
    MessageLoopForUI::current()->RunAllPending();
  }

 private:
  scoped_ptr<NullLayerDelegate> default_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithNullDelegateTest);
};

// With the webkit compositor, we don't explicitly textures for layers, making
// tests that check that we do fail.
#if defined(USE_WEBKIT_COMPOSITOR)
#define NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(X) DISABLED_ ## X
#else
#define NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(X) X
#endif

// Verifies that a layer which is set never to have a texture does not
// get a texture when SetFillsBoundsOpaquely is called.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(
           LayerNoTextureSetFillsBoundsOpaquely)) {
  scoped_ptr<Layer> parent(CreateNoTextureLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateNoTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  compositor()->SetRootLayer(parent.get());
  parent->SetFillsBoundsOpaquely(true);
  child->SetFillsBoundsOpaquely(true);
  Draw();
  RunPendingMessages();
  EXPECT_TRUE(child->texture() == NULL);
  EXPECT_TRUE(parent->texture() == NULL);

  parent->SetFillsBoundsOpaquely(false);
  child->SetFillsBoundsOpaquely(false);
  Draw();
  RunPendingMessages();
  EXPECT_TRUE(child->texture() == NULL);
  EXPECT_TRUE(parent->texture() == NULL);
}

// Verifies that a layer does not have a texture when the hole is the size
// of the parent layer.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(LayerNoTextureHoleSizeOfLayer)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  Draw();
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), parent->hole_rect());
  EXPECT_TRUE(parent->texture() != NULL);

  child->SetBounds(gfx::Rect(0, 0, 400, 400));
  Draw();
  EXPECT_TRUE(parent->texture() == NULL);
}

// Verifies that a layer which has opacity == 0 does not have a texture.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(LayerNoTextureTransparent)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  parent->SetOpacity(0.0f);
  child->SetOpacity(0.0f);
  Draw();
  EXPECT_TRUE(parent->texture() == NULL);
  EXPECT_TRUE(child->texture() == NULL);

  parent->SetOpacity(1.0f);
  Draw();
  EXPECT_TRUE(parent->texture() != NULL);
  EXPECT_TRUE(child->texture() == NULL);

  child->SetOpacity(1.0f);
  Draw();
  EXPECT_TRUE(parent->texture() != NULL);
  EXPECT_TRUE(child->texture() != NULL);
}

// Verifies that no texture is created for a layer with empty bounds.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(LayerTextureNonEmptySchedulePaint)) {
  scoped_ptr<Layer> layer(CreateTextureRootLayer(gfx::Rect(0, 0, 0, 0)));
  Draw();
  EXPECT_TRUE(layer->texture() == NULL);

  layer->SetBounds(gfx::Rect(0, 0, 400, 400));
  Draw();
  EXPECT_TRUE(layer->texture() != NULL);
}

// Verifies that when there are many potential holes, the largest one is picked.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(LargestHole)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> child1(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child1.get());

  scoped_ptr<Layer> child2(CreateTextureLayer(gfx::Rect(75, 75, 200, 200)));
  parent->Add(child2.get());

  Draw();

  EXPECT_EQ(gfx::Rect(75, 75, 200, 200), parent->hole_rect());
}

// Verifies that the largest hole in the draw order is picked
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(HoleGeneratedFromLeaf)) {
  // Layer tree looks like:
  // node 1
  // |_ node 11
  //    |_ node 111
  // |_ node 12
  //    |_ node 121

  scoped_ptr<Layer> node1(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> node11(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  node1->Add(node11.get());

  scoped_ptr<Layer> node12(CreateTextureLayer(gfx::Rect(75, 75, 200, 200)));
  node1->Add(node12.get());

  scoped_ptr<Layer> node111(CreateTextureLayer(gfx::Rect(10, 10, 20, 20)));
  node11->Add(node111.get());

  scoped_ptr<Layer> node121(CreateTextureLayer(gfx::Rect(10, 10, 190, 190)));
  node12->Add(node121.get());

  Draw();

  EXPECT_EQ(gfx::Rect(75, 75, 200, 200), node1->hole_rect());
  EXPECT_EQ(gfx::Rect(25, 25, 75, 75), node11->hole_rect());
  EXPECT_EQ(gfx::Rect(10, 10, 190, 190), node12->hole_rect());
}

// Verifies that a hole can only punched into a layer with opacity = 1.0f.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(NoHoleWhenPartialOpacity)) {
  // Layer tree looks like:
  // node 1
  // |_ node 11
  //    |_ node 111

  scoped_ptr<Layer> node1(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> node11(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  node1->Add(node11.get());

  scoped_ptr<Layer> node111(CreateTextureLayer(gfx::Rect(10, 10, 20, 20)));
  node11->Add(node111.get());

  Draw();
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), node1->hole_rect());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), node11->hole_rect());


  node11->SetOpacity(0.5f);
  Draw();
  EXPECT_TRUE(node1->hole_rect().IsEmpty());
  EXPECT_TRUE(node11->hole_rect().IsEmpty());
}

// Verifies that a non visible layer or any of its children is not a hole.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(NonVisibleLayerCannotBeHole)) {
  // Layer tree looks like:
  // node 1
  // |_ node 11
  //    |_ node 111

  scoped_ptr<Layer> node1(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> node11(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  node1->Add(node11.get());

  scoped_ptr<Layer> node111(CreateTextureLayer(gfx::Rect(10, 10, 20, 20)));
  node11->Add(node111.get());

  Draw();
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), node1->hole_rect());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), node11->hole_rect());


  node11->SetVisible(false);
  Draw();
  EXPECT_TRUE(node1->hole_rect().IsEmpty());
  EXPECT_TRUE(node11->hole_rect().IsEmpty());
}

// Verifies that a layer which doesn't fill its bounds opaquely cannot punch a
// hole. However its children should still be able to punch a hole.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(
           LayerNotFillingBoundsOpaquelyCannotBeHole)) {
  // Layer tree looks like:
  // node 1
  // |_ node 11
  //    |_ node 111

  scoped_ptr<Layer> node1(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> node11(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  node1->Add(node11.get());

  scoped_ptr<Layer> node111(CreateTextureLayer(gfx::Rect(10, 10, 20, 20)));
  node11->Add(node111.get());

  Draw();
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), node1->hole_rect());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), node11->hole_rect());


  node11->SetFillsBoundsOpaquely(false);
  Draw();
  EXPECT_EQ(gfx::Rect(60, 60, 20, 20), node1->hole_rect());
  EXPECT_TRUE(node11->hole_rect().IsEmpty());
}

// Verifies that the hole is with respect to the local bounds of its parent.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(HoleLocalBounds)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(
      gfx::Rect(100, 100, 150, 150)));

  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  Draw();

  EXPECT_EQ(gfx::Rect(50, 50, 100, 100), parent->hole_rect());
}

// Verifies that there is no hole present when one of the child layers has a
// transform.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(NoHoleWithTransform)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 100, 100)));
  parent->Add(child.get());

  Draw();

  EXPECT_TRUE(!parent->hole_rect().IsEmpty());

  ui::Transform t;
  t.SetTranslate(-50, -50);
  t.ConcatRotate(45.0f);
  t.ConcatTranslate(50, 50);
  child->SetTransform(t);

  Draw();

  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), parent->hole_rect());
}

// Verifies that if the child layer is rotated by a multiple of ninety degrees
// we punch a hole
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(HoleWithNinetyDegreeTransforms)) {
  scoped_ptr<Layer> parent(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));
  scoped_ptr<Layer> child(CreateTextureLayer(gfx::Rect(50, 50, 50, 50)));
  parent->Add(child.get());

  Draw();

  EXPECT_TRUE(!parent->hole_rect().IsEmpty());

  for (int i = -4; i <= 4; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);

    ui::Transform t;
    // Need to rotate in local coordinates.
    t.SetTranslate(-25, -25);
    t.ConcatRotate(90.0f * i);
    t.ConcatTranslate(25, 25);
    child->SetTransform(t);

    gfx::Rect target_rect(child->bounds().size());
    t.ConcatTranslate(child->bounds().x(), child->bounds().y());
    t.TransformRect(&target_rect);

    Draw();

    EXPECT_EQ(target_rect, parent->hole_rect());
  }
}

// Verifies that a layer which doesn't have a texture cannot punch a
// hole. However its children should still be able to punch a hole.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(
           HoleWithRelativeNinetyDegreeTransforms)) {
  // Layer tree looks like:
  // node 1
  // |_ node 11
  // |_ node 12

  scoped_ptr<Layer> node1(CreateTextureRootLayer(gfx::Rect(0, 0, 400, 400)));

  scoped_ptr<Layer> node11(CreateTextureLayer(gfx::Rect(50, 50, 50, 50)));
  node1->Add(node11.get());

  scoped_ptr<Layer> node12(CreateTextureLayer(gfx::Rect(50, 50, 50, 50)));
  node1->Add(node12.get());

  Draw();

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), node11->hole_rect());
  EXPECT_TRUE(node12->hole_rect().IsEmpty());

  ui::Transform t1;
  // Need to rotate in local coordinates.
  t1.SetTranslate(-25, -25);
  t1.ConcatRotate(45.0f);
  t1.ConcatTranslate(25, 25);
  node11->SetTransform(t1);

  Draw();

  EXPECT_TRUE(node12->hole_rect().IsEmpty());
  EXPECT_TRUE(node11->hole_rect().IsEmpty());

  ui::Transform t2;
  // Need to rotate in local coordinates.
  t2.SetTranslate(-25, -25);
  t2.ConcatRotate(-135.0f);
  t2.ConcatTranslate(25, 25);
  node12->SetTransform(t2);

  // Do translation of target rect in order to account for inprecision of
  // using floating point matrices vs integer rects.
  ui::Transform t3;
  gfx::Rect target_rect = gfx::Rect(node11->bounds().size());
  t3.ConcatTransform(t2);
  t1.GetInverse(&t1);
  t3.ConcatTransform(t1);
  t3.TransformRect(&target_rect);

  Draw();

  EXPECT_TRUE(node12->hole_rect().IsEmpty());
  EXPECT_EQ(target_rect, node11->hole_rect());
}

// Create this hierarchy:
// L1 (no texture)
//  +- L11 (texture)
//  +- L12 (no texture) (added after L1 is already set as root-layer)
//    +- L121 (texture)
//    +- L122 (texture)
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(NoCompositor)) {
  scoped_ptr<Layer> l1(CreateLayer(Layer::LAYER_HAS_NO_TEXTURE));
  scoped_ptr<Layer> l11(CreateLayer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l12(CreateLayer(Layer::LAYER_HAS_NO_TEXTURE));
  scoped_ptr<Layer> l121(CreateLayer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l122(CreateLayer(Layer::LAYER_HAS_TEXTURE));

  EXPECT_EQ(NULL, l1->texture());
  EXPECT_EQ(NULL, l11->texture());
  EXPECT_EQ(NULL, l12->texture());
  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());

  l1->Add(l11.get());
  l1->SetBounds(gfx::Rect(0, 0, 500, 500));
  l11->SetBounds(gfx::Rect(5, 5, 490, 490));

  EXPECT_EQ(NULL, l11->texture());

  compositor()->SetRootLayer(l1.get());

  EXPECT_EQ(NULL, l1->texture());

  // Despite having type LAYER_HAS_TEXTURE, l11 will not have one set yet
  // because it has never been asked to draw.
  EXPECT_EQ(NULL, l11->texture());

  l12->Add(l121.get());
  l12->Add(l122.get());
  l12->SetBounds(gfx::Rect(5, 5, 480, 480));
  l121->SetBounds(gfx::Rect(5, 5, 100, 100));
  l122->SetBounds(gfx::Rect(110, 110, 100, 100));

  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());

  l1->Add(l12.get());

  // By asking l121 and l122 to paint, we cause them to generate a texture.
  SchedulePaintForLayer(l121.get());
  SchedulePaintForLayer(l122.get());
  Draw();

  EXPECT_EQ(NULL, l12->texture());
  EXPECT_TRUE(NULL != l121->texture());
  EXPECT_TRUE(NULL != l122->texture());

  // Toggling l2's visibility should drop all sub-layer textures.
  l12->SetVisible(false);
  EXPECT_EQ(NULL, l12->texture());
  EXPECT_EQ(NULL, l121->texture());
  EXPECT_EQ(NULL, l122->texture());
}

// Various visibile/drawn assertions.
TEST_F(LayerWithNullDelegateTest, Visibility) {
  scoped_ptr<Layer> l1(new Layer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l2(new Layer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l3(new Layer(Layer::LAYER_HAS_TEXTURE));
  l1->Add(l2.get());
  l2->Add(l3.get());

  NullLayerDelegate delegate;
  l1->set_delegate(&delegate);
  l2->set_delegate(&delegate);
  l3->set_delegate(&delegate);

  // Layers should initially be drawn.
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_TRUE(l3->IsDrawn());
#if defined(USE_WEBKIT_COMPOSITOR)
  EXPECT_EQ(1.f, l1->web_layer().opacity());
  EXPECT_EQ(1.f, l2->web_layer().opacity());
  EXPECT_EQ(1.f, l3->web_layer().opacity());
#endif

  compositor()->SetRootLayer(l1.get());

  Draw();

  l1->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
#if defined(USE_WEBKIT_COMPOSITOR)
  EXPECT_EQ(0.f, l1->web_layer().opacity());
#endif

  l3->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
#if defined(USE_WEBKIT_COMPOSITOR)
  EXPECT_EQ(0.f, l3->web_layer().opacity());
#endif

  l1->SetVisible(true);
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
#if defined(USE_WEBKIT_COMPOSITOR)
  EXPECT_EQ(1.f, l1->web_layer().opacity());
#endif
}

// Checks that stacking-related methods behave as advertised.
TEST_F(LayerWithNullDelegateTest, Stacking) {
  scoped_ptr<Layer> root(new Layer(Layer::LAYER_HAS_NO_TEXTURE));
  scoped_ptr<Layer> l1(new Layer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l2(new Layer(Layer::LAYER_HAS_TEXTURE));
  scoped_ptr<Layer> l3(new Layer(Layer::LAYER_HAS_TEXTURE));
  l1->set_name("1");
  l2->set_name("2");
  l3->set_name("3");
  root->Add(l3.get());
  root->Add(l2.get());
  root->Add(l1.get());

  // Layers' children are stored in bottom-to-top order.
  EXPECT_EQ("3,2,1", GetLayerChildrenNames(*root.get()));

  root->StackAtTop(l3.get());
  EXPECT_EQ("2,1,3", GetLayerChildrenNames(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2,3,1", GetLayerChildrenNames(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2,3,1", GetLayerChildrenNames(*root.get()));

  root->StackAbove(l2.get(), l3.get());
  EXPECT_EQ("3,2,1", GetLayerChildrenNames(*root.get()));

  root->StackAbove(l1.get(), l3.get());
  EXPECT_EQ("3,1,2", GetLayerChildrenNames(*root.get()));

  root->StackAbove(l2.get(), l1.get());
  EXPECT_EQ("3,1,2", GetLayerChildrenNames(*root.get()));
}

// Checks that the invalid rect assumes correct values when setting bounds.
// TODO(vollick): for USE_WEBKIT_COMPOSITOR, use WebKit's dirty rect.
TEST_F(LayerWithNullDelegateTest,
       NOT_APPLICABLE_TO_WEBKIT_COMPOSITOR(SetBoundsInvalidRect)) {
  scoped_ptr<Layer> l1(CreateTextureLayer(gfx::Rect(0, 0, 200, 200)));
  compositor()->SetRootLayer(l1.get());

  // After a draw the invalid rect should be empty.
  Draw();
  EXPECT_TRUE(l1->invalid_rect().IsEmpty());

  // After a move the invalid rect should be empty.
  l1->SetBounds(gfx::Rect(5, 5, 200, 200));
  EXPECT_TRUE(l1->invalid_rect().IsEmpty());

  // Bounds change should trigger both the invalid rect to update as well as
  // CompositorDelegate being told to draw.
  l1->SetBounds(gfx::Rect(5, 5, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            l1->invalid_rect().ToString());
}

// Verifies SetBounds triggers the appropriate painting/drawing.
TEST_F(LayerWithNullDelegateTest, SetBoundsSchedulesPaint) {
  scoped_ptr<Layer> l1(CreateTextureLayer(gfx::Rect(0, 0, 200, 200)));
  compositor()->SetRootLayer(l1.get());

  Draw();

  schedule_draw_invoked_ = false;
  l1->SetBounds(gfx::Rect(5, 5, 200, 200));

  // The CompositorDelegate (us) should have been told to draw for a move.
  EXPECT_TRUE(schedule_draw_invoked_);

  schedule_draw_invoked_ = false;
  l1->SetBounds(gfx::Rect(5, 5, 100, 100));

  // The CompositorDelegate (us) should have been told to draw for a resize.
  EXPECT_TRUE(schedule_draw_invoked_);
}

// Checks that pixels are actually drawn to the screen with a read back.
TEST_F(LayerWithRealCompositorTest, MAYBE_DrawPixels) {
  scoped_ptr<Layer> layer(CreateColorLayer(SK_ColorRED,
                                           gfx::Rect(0, 0, 500, 500)));
  scoped_ptr<Layer> layer2(CreateColorLayer(SK_ColorBLUE,
                                            gfx::Rect(0, 0, 500, 10)));

  layer->Add(layer2.get());

  DrawTree(layer.get());

  SkBitmap bitmap;
  gfx::Size size = GetCompositor()->size();
  ASSERT_TRUE(GetCompositor()->ReadPixels(&bitmap,
                                          gfx::Rect(0, 10,
                                          size.width(), size.height() - 10)));
  ASSERT_FALSE(bitmap.empty());

  SkAutoLockPixels lock(bitmap);
  bool is_all_red = true;
  for (int x = 0; is_all_red && x < 500; x++)
    for (int y = 0; is_all_red && y < 490; y++)
      is_all_red = is_all_red && (bitmap.getColor(x, y) == SK_ColorRED);

  EXPECT_TRUE(is_all_red);
}

// Checks the logic around Compositor::SetRootLayer and Layer::SetCompositor.
TEST_F(LayerWithRealCompositorTest, MAYBE_SetRootLayer) {
  Compositor* compositor = GetCompositor();
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));

  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());

  compositor->SetRootLayer(l1.get());
  EXPECT_EQ(compositor, l1->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  l1->Remove(l2.get());
  EXPECT_EQ(NULL, l2->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  compositor->SetRootLayer(NULL);
  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());
}

// Checks that compositor observers are notified when:
// - DrawTree is called,
// - After ScheduleDraw is called, or
// - Whenever SetBounds, SetOpacity or SetTransform are called.
// TODO(vollick): could be reorganized into compositor_unittest.cc
TEST_F(LayerWithRealCompositorTest, MAYBE_CompositorObservers) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  TestCompositorObserver observer;
  GetCompositor()->AddObserver(&observer);

  // Explicitly called DrawTree should cause the observers to be notified.
  // NOTE: this call to DrawTree sets l1 to be the compositor's root layer.
  DrawTree(l1.get());
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // As should scheduling a draw and waiting.
  observer.Reset();
  l1->ScheduleDraw();
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // Moving, but not resizing, a layer should alert the observers.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 350, 350));
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // So should resizing a layer.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 400, 400));
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // Opacity changes should alert the observers.
  observer.Reset();
  l2->SetOpacity(0.5f);
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // So should setting the opacity back.
  observer.Reset();
  l2->SetOpacity(1.0f);
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  // Setting the transform of a layer should alert the observers.
  observer.Reset();
  Transform transform;
  transform.ConcatTranslate(-200, -200);
  transform.ConcatRotate(90.0f);
  transform.ConcatTranslate(200, 200);
  l2->SetTransform(transform);
  RunPendingMessages();
  EXPECT_TRUE(observer.notified());

  GetCompositor()->RemoveObserver(&observer);

  // Opacity changes should no longer alert the removed observer.
  observer.Reset();
  l2->SetOpacity(0.5f);
  RunPendingMessages();
  EXPECT_FALSE(observer.notified());
}

// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, MAYBE_ModifyHierarchy) {
  GetCompositor()->WidgetSizeChanged(gfx::Size(50, 50));

  // l0
  //  +-l11
  //  | +-l21
  //  +-l12
  scoped_ptr<Layer> l0(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<Layer> l11(CreateColorLayer(SK_ColorGREEN,
                                         gfx::Rect(0, 0, 25, 25)));
  scoped_ptr<Layer> l21(CreateColorLayer(SK_ColorMAGENTA,
                                         gfx::Rect(0, 0, 15, 15)));
  scoped_ptr<Layer> l12(CreateColorLayer(SK_ColorBLUE,
                                         gfx::Rect(10, 10, 25, 25)));

  FilePath ref_img1 = test_data_directory().AppendASCII("ModifyHierarchy1.png");
  FilePath ref_img2 = test_data_directory().AppendASCII("ModifyHierarchy2.png");
  SkBitmap bitmap;

  l0->Add(l11.get());
  l11->Add(l21.get());
  l0->Add(l12.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img1);
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img1));

  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img2);
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img2));

  // l11 is already at the front, should have no effect.
  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img2));

  // l11 is already at the front, should have no effect.
  l0->StackAbove(l11.get(), l12.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img2));

  // should restore to original configuration
  l0->StackAbove(l12.get(), l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img1));
}

// Opacity is rendered correctly.
// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, MAYBE_Opacity) {
  GetCompositor()->WidgetSizeChanged(gfx::Size(50, 50));

  // l0
  //  +-l11
  scoped_ptr<Layer> l0(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<Layer> l11(CreateColorLayer(SK_ColorGREEN,
                                         gfx::Rect(0, 0, 25, 25)));

  FilePath ref_img = test_data_directory().AppendASCII("Opacity.png");

  l11->SetOpacity(0.75);
  l0->Add(l11.get());
  DrawTree(l0.get());
  SkBitmap bitmap;
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img);
  EXPECT_TRUE(IsSameAsPNGFile(bitmap, ref_img));
}

} // namespace ui
