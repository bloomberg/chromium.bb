/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DDLTileHelper_DEFINED
#define DDLTileHelper_DEFINED

#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurfaceCharacterization.h"

class DDLPromiseImageHelper;
class PromiseImageCallbackContext;
class SkCanvas;
class SkData;
class SkDeferredDisplayList;
class SkDeferredDisplayListRecorder;
class SkPicture;
class SkSurface;
class SkSurfaceCharacterization;

class DDLTileHelper {
public:
    // The TileData class encapsulates the information and behavior of a single tile when
    // rendering with DDLs.
    class TileData {
    public:
        TileData() {}
        ~TileData();

        void init(int id,
                  GrContext* context,
                  const SkSurfaceCharacterization& dstChar,
                  const SkIRect& clip);

        // Convert the compressedPictureData into an SkPicture replacing each image-index
        // with a promise image.
        void createTileSpecificSKP(SkData* compressedPictureData,
                                   const DDLPromiseImageHelper& helper);

        // Create the DDL for this tile (i.e., fill in 'fDisplayList').
        void createDDL();

        // Precompile all the programs required to draw this tile's DDL
        void precompile(GrContext*);

        // Just draw the re-inflated per-tile SKP directly into this tile w/o going through a DDL
        // first. This is used for determining the overhead of using DDLs (i.e., it replaces
        // a 'createDDL' and 'draw' pair.
        void drawSKPDirectly(GrContext*);

        // Replay the recorded DDL into the tile surface - filling in 'fBackendTexture'.
        void draw(GrContext*);

        void reset();

        int id() const { return fID; }
        SkIRect clipRect() const { return fClip; }

        SkDeferredDisplayList* ddl() { return fDisplayList.get(); }

        sk_sp<SkImage> makePromiseImage(SkDeferredDisplayListRecorder*);
        void dropCallbackContext() { fCallbackContext.reset(); }

        static void CreateBackendTexture(GrContext*, TileData*);
        static void DeleteBackendTexture(GrContext*, TileData*);

    private:
        sk_sp<SkSurface> makeWrappedTileDest(GrContext* context);

        sk_sp<PromiseImageCallbackContext> refCallbackContext() { return fCallbackContext; }

        int                       fID = -1;
        SkIRect                   fClip;             // in the device space of the final SkSurface
        SkSurfaceCharacterization fCharacterization; // characterization for the tile's surface

        // The callback context holds (via its SkPromiseImageTexture) the backend texture
        // that is both wrapped in 'fTileSurface' and backs this tile's promise image
        // (i.e., the one returned by 'makePromiseImage').
        sk_sp<PromiseImageCallbackContext> fCallbackContext;
        // 'fTileSurface' wraps the backend texture in 'fCallbackContext' and must exist until
        // after 'fDisplayList' has been flushed (bc it owns the proxy the DDL's destination
        // trampoline points at).
        // TODO: fix the ref-order so we don't need 'fTileSurface' here
        sk_sp<SkSurface>          fTileSurface;

        sk_sp<SkPicture>          fReconstitutedPicture;
        SkTArray<sk_sp<SkImage>>  fPromiseImages;    // All the promise images in the
                                                     // reconstituted picture
        std::unique_ptr<SkDeferredDisplayList> fDisplayList;
    };

    DDLTileHelper(GrContext* context,
                  const SkSurfaceCharacterization& dstChar,
                  const SkIRect& viewport,
                  int numDivisions);

    void createSKPPerTile(SkData* compressedPictureData, const DDLPromiseImageHelper& helper);

    void kickOffThreadedWork(SkTaskGroup* recordingTaskGroup,
                             SkTaskGroup* gpuTaskGroup,
                             GrContext* gpuThreadContext);

    void createDDLsInParallel();

    // Create the DDL that will compose all the tile images into a final result.
    void createComposeDDL();
    SkDeferredDisplayList* composeDDL() const { return fComposeDDL.get(); }

    void precompileAndDrawAllTiles(GrContext*);

    // For each tile, create its DDL and then draw it - all on a single thread. This is to allow
    // comparison w/ just drawing the SKP directly (i.e., drawAllTilesDirectly). The
    // DDL creations and draws are interleaved to prevent starvation of the GPU.
    // Note: this is somewhat of a misuse/pessimistic-use of DDLs since they are supposed to
    // be created on a separate thread.
    void interleaveDDLCreationAndDraw(GrContext*);

    // This draws all the per-tile SKPs directly into all of the tiles w/o converting them to
    // DDLs first - all on a single thread.
    void drawAllTilesDirectly(GrContext*);

    void dropCallbackContexts();
    void resetAllTiles();

    int numTiles() const { return fNumDivisions * fNumDivisions; }

    void createBackendTextures(SkTaskGroup*, GrContext*);
    void deleteBackendTextures(SkTaskGroup*, GrContext*);

private:
    int                                    fNumDivisions; // number of tiles along a side
    SkAutoTArray<TileData>                 fTiles;        // 'fNumDivisions' x 'fNumDivisions'

    std::unique_ptr<SkDeferredDisplayList> fComposeDDL;

    const SkSurfaceCharacterization        fDstCharacterization;
};

#endif
