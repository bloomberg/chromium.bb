// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TOOLS_COMPARISON_VIEWER_CODEC_COMPARISON_WINDOW_H_
#define TOOLS_COMPARISON_VIEWER_CODEC_COMPARISON_WINDOW_H_

#include <QDir>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QString>

#include "lib/jxl/base/padded_bytes.h"
#include "tools/comparison_viewer/ui_codec_comparison_window.h"

namespace jxl {

class CodecComparisonWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit CodecComparisonWindow(QWidget* parent = nullptr);
  explicit CodecComparisonWindow(const QString& directory,
                                 QWidget* parent = nullptr);
  ~CodecComparisonWindow() override = default;

 private slots:
  void handleImageSetSelection(const QString& imageSetName);
  void handleImageSelection(const QString& imageName);

 private:
  struct ComparableImage {
    // Absolute path to the decoded PNG (or an image that Qt can read).
    QString decodedImagePath;
    // Size of the encoded image (*not* the PNG).
    qint64 byteSize = 0;
  };
  // Keys are compression levels.
  using Codec = QMap<QString, ComparableImage>;
  // Keys are codec names.
  using Codecs = QMap<QString, Codec>;
  // Keys are image names (relative to the image set directory).
  using ImageSet = QMap<QString, Codecs>;
  // Keys are paths to image sets (relative to the base directory chosen by the
  // user).
  using ImageSets = QMap<QString, ImageSet>;

  enum class Side { LEFT, RIGHT };

  QString pathToOriginalImage(const QString& imageSet,
                              const QString& imageName) const;
  ComparableImage currentlySelectedImage(Side side) const;

  void handleCodecChange(Side side);
  void updateSideImage(Side side);
  void matchSize(Side side);

  void loadDirectory(const QString& directory);
  // Recursive, called by loadDirectory.
  void browseDirectory(const QDir& directory, int depth = 0);

  Ui::CodecComparisonWindow ui_;

  QDir baseDirectory_;
  ImageSets imageSets_;
  QSet<QString> visited_;

  const QByteArray monitorIccProfile_;
};

}  // namespace jxl

#endif  // TOOLS_COMPARISON_VIEWER_CODEC_COMPARISON_WINDOW_H_
