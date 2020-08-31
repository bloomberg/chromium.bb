// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

// Client responsible for making requests to the mojom::PaintPreviewRecorder. A
// client coordinates between multiple frames and handles capture and
// aggreagation of data from both the main frame and subframes.
//
// Should be created and accessed from the UI thread as WebContentsUserData
// requires this behavior.
class PaintPreviewClient
    : public content::WebContentsUserData<PaintPreviewClient>,
      public content::WebContentsObserver {
 public:
  using PaintPreviewCallback =
      base::OnceCallback<void(base::UnguessableToken,
                              mojom::PaintPreviewStatus,
                              std::unique_ptr<PaintPreviewProto>)>;

  // Augmented version of mojom::PaintPreviewServiceParams.
  struct PaintPreviewParams {
    PaintPreviewParams();
    ~PaintPreviewParams();

    // The document GUID for this capture.
    base::UnguessableToken document_guid;

    // The root directory in which to store paint_previews. This should be
    // a subdirectory inside the active user profile's directory.
    base::FilePath root_dir;

    // The rect to which to clip the capture to.
    gfx::Rect clip_rect;

    // Whether the capture is for the main frame or an OOP subframe.
    bool is_main_frame;

    // The maximum capture size allowed per SkPicture captured. A size of 0 is
    // unlimited.
    // TODO(crbug/1071446): Ideally, this would cap the total size rather than
    // being a per SkPicture limit. However, that is non-trivial due to the
    // async ordering of captures from different frames making it hard to keep
    // track of available headroom at the time of each capture triggering.
    size_t max_per_capture_size;
  };

  ~PaintPreviewClient() override;

  // IMPORTANT: The Capture* methods must be called on the UI thread!

  // Captures a paint preview corresponding to the content of
  // |render_frame_host|. This will work for capturing entire documents if
  // passed the main frame or for just a specific subframe depending on
  // |render_frame_host|. |callback| is invoked on completion.
  void CapturePaintPreview(const PaintPreviewParams& params,
                           content::RenderFrameHost* render_frame_host,
                           PaintPreviewCallback callback);

  // Captures a paint preview of the subframe corresponding to
  // |render_subframe_host|.
  void CaptureSubframePaintPreview(
      const base::UnguessableToken& guid,
      const gfx::Rect& rect,
      content::RenderFrameHost* render_subframe_host);

  // WebContentsObserver implementation ---------------------------------------

  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  explicit PaintPreviewClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaintPreviewClient>;

  // Internal Storage Classes -------------------------------------------------

  // Representation of data for capturing a paint preview.
  struct PaintPreviewData {
   public:
    PaintPreviewData();
    ~PaintPreviewData();

    // Root directory to store artifacts to.
    base::FilePath root_dir;

    base::UnguessableToken root_frame_token;

    // URL of the root frame.
    GURL root_url;

    // UKM Source ID of the WebContent.
    ukm::SourceId source_id;

    // Main frame capture time.
    base::TimeDelta main_frame_blink_recording_time;

    // Callback that is invoked on completion of data.
    PaintPreviewCallback callback;

    // All the render frames that are still required.
    base::flat_set<base::UnguessableToken> awaiting_subframes;

    // All the render frames that have finished.
    base::flat_set<base::UnguessableToken> finished_subframes;

    // Data proto that is returned via callback.
    std::unique_ptr<PaintPreviewProto> proto;

    bool had_error = false;

    PaintPreviewData& operator=(PaintPreviewData&& other) noexcept;
    PaintPreviewData(PaintPreviewData&& other) noexcept;

   private:
    PaintPreviewData(const PaintPreviewData&) = delete;
    PaintPreviewData& operator=(const PaintPreviewData&) = delete;
  };

  struct CreateResult {
   public:
    CreateResult(base::File file, base::File::Error error);
    ~CreateResult();
    CreateResult(CreateResult&& other);
    CreateResult& operator=(CreateResult&& other);

    base::File file;
    base::File::Error error;

   private:
    CreateResult(const CreateResult&) = delete;
    CreateResult& operator=(const CreateResult&) = delete;
  };

  // Helpers -------------------------------------------------------------------

  static CreateResult CreateFileHandle(const base::FilePath& path);

  mojom::PaintPreviewCaptureParamsPtr CreateMojoParams(
      const PaintPreviewParams& params,
      base::File file);

  // Sets up for a capture of a frame on |render_frame_host| according to
  // |params|.
  void CapturePaintPreviewInternal(const PaintPreviewParams& params,
                                   content::RenderFrameHost* render_frame_host);

  // Initiates capture via the PaintPreviewRecorder associated with
  // |render_frame_host| using |params| to configure the request. |frame_guid|
  // is the GUID associated with the frame. |path| is file path associated with
  // the File stored in |result| (base::File isn't aware of its file path).
  void RequestCaptureOnUIThread(
      const PaintPreviewParams& params,
      const base::UnguessableToken& frame_guid,
      const content::GlobalFrameRoutingId& render_frame_id,
      const base::FilePath& path,
      CreateResult result);

  // Handles recording the frame and updating client state when capture is
  // complete.
  void OnPaintPreviewCapturedCallback(
      const base::UnguessableToken& guid,
      const base::UnguessableToken& frame_guid,
      bool is_main_frame,
      const base::FilePath& filename,
      const content::GlobalFrameRoutingId& render_frame_id,
      mojom::PaintPreviewStatus status,
      mojom::PaintPreviewCaptureResponsePtr response);

  // Marks a frame as having been processed, this should occur regardless of
  // whether the processed frame is valid as there is no retry.
  void MarkFrameAsProcessed(base::UnguessableToken guid,
                            const base::UnguessableToken& frame_guid);

  // Records the data from a processed frame if it was captured successfully.
  mojom::PaintPreviewStatus RecordFrame(
      const base::UnguessableToken& guid,
      const base::UnguessableToken& frame_guid,
      bool is_main_frame,
      const base::FilePath& filename,
      const content::GlobalFrameRoutingId& render_frame_id,
      mojom::PaintPreviewCaptureResponsePtr response);

  // Handles finishing the capture once all frames are received.
  void OnFinished(base::UnguessableToken guid, PaintPreviewData* document_data);

  // Storage ------------------------------------------------------------------

  // Mapping of Process ID || Routing ID to unguessable tokens for the main
  // frame.
  base::flat_map<uint64_t, base::UnguessableToken> main_frame_guids_;

  // Maps a RenderFrameHost and document to a remote interface.
  base::flat_map<base::UnguessableToken,
                 mojo::AssociatedRemote<mojom::PaintPreviewRecorder>>
      interface_ptrs_;

  // Maps render frame's GUID and document cookies that requested the frame.
  base::flat_map<base::UnguessableToken, base::flat_set<base::UnguessableToken>>
      pending_previews_on_subframe_;

  // Maps a document GUID to its data.
  base::flat_map<base::UnguessableToken, PaintPreviewData> all_document_data_;

  base::WeakPtrFactory<PaintPreviewClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  PaintPreviewClient(const PaintPreviewClient&) = delete;
  PaintPreviewClient& operator=(const PaintPreviewClient&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_CLIENT_H_
