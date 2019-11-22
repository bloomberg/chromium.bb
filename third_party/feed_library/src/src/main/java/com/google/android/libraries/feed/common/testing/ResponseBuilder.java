// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import com.google.protobuf.ByteString;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation.Operation;
import com.google.search.now.wire.feed.FeatureProto.Feature;
import com.google.search.now.wire.feed.FeatureProto.Feature.RenderableUnit;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponse;
import com.google.search.now.wire.feed.PayloadMetadataProto.PayloadMetadata;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;
import com.google.search.now.wire.feed.TokenProto.Token;

import java.util.ArrayList;
import java.util.List;

/** Builder making creation of wire protocol Response object easier. */
public class ResponseBuilder {
    // this needs to match the root content_id defined in the FeedProtocolAdapter.
    public static final ContentId ROOT_CONTENT_ID = ContentId.newBuilder()
                                                            .setContentDomain("stream_root")
                                                            .setId(0)
                                                            .setTable("feature")
                                                            .build();
    public static final ContentId PIET_SHARED_STATE = ContentId.newBuilder()
                                                              .setContentDomain("piet-shared-state")
                                                              .setId(1)
                                                              .setTable("feature")
                                                              .build();
    private static final int CONTENT_ID_CARD_INCREMENT = 100;
    private static final ContentId TOKEN_CONTENT_ID =
            ContentId.newBuilder().setContentDomain("token").setId(0).setTable("token").build();

    public static ContentId createFeatureContentId(int id) {
        return ContentId.newBuilder()
                .setContentDomain("feature")
                .setId(id)
                .setTable("feature")
                .build();
    }

    private final WireProtocolInfo wireProtocolInfo = new WireProtocolInfo();
    private final FeedResponse.Builder feedResponseBuilder = FeedResponse.newBuilder();
    private final List<ContentId> pietSharedStateContentIds = new ArrayList<>();

    private int tokenId;
    /*@Nullable*/ private ByteString token;

    /** Add a CLEAR_ALL data operation to the response */
    public ResponseBuilder addClearOperation() {
        wireProtocolInfo.hasClearOperation = true;
        feedResponseBuilder.addDataOperation(
                DataOperation.newBuilder().setOperation(Operation.CLEAR_ALL).build());
        return this;
    }

    public ResponseBuilder addStreamToken(int tokenId, ByteString token) {
        this.token = token;
        this.tokenId = tokenId;
        wireProtocolInfo.hasToken = true;
        return this;
    }

    public ResponseBuilder addPietSharedState() {
        return addPietSharedState(PIET_SHARED_STATE);
    }

    public ResponseBuilder addPietSharedState(ContentId pietSharedStateContentId) {
        pietSharedStateContentIds.add(pietSharedStateContentId);
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder().setContentId(pietSharedStateContentId).build();
        feedResponseBuilder.addDataOperation(
                DataOperation.newBuilder()
                        .setOperation(Operation.UPDATE_OR_APPEND)
                        .setPietSharedState(PietSharedState.getDefaultInstance())
                        .setMetadata(payloadMetadata)
                        .build());
        return this;
    }

    /** Add a data operation for a feature that represents the Root of the Stream. */
    public ResponseBuilder addRootFeature() {
        return addRootFeature(ROOT_CONTENT_ID);
    }

    public ResponseBuilder addRootFeature(ContentId contentId) {
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder().setContentId(contentId).build();
        Feature feature = Feature.newBuilder().setRenderableUnit(RenderableUnit.STREAM).build();

        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.UPDATE_OR_APPEND)
                                                     .setFeature(feature)
                                                     .setMetadata(payloadMetadata)
                                                     .build());
        wireProtocolInfo.featuresAdded.add(contentId);
        return this;
    }

    public ResponseBuilder addClusterFeature(ContentId contentId, ContentId parentId) {
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder().setContentId(contentId).build();
        Feature feature = Feature.newBuilder()
                                  .setRenderableUnit(RenderableUnit.CLUSTER)
                                  .setParentId(parentId)
                                  .build();
        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.UPDATE_OR_APPEND)
                                                     .setFeature(feature)
                                                     .setMetadata(payloadMetadata)
                                                     .build());
        wireProtocolInfo.featuresAdded.add(contentId);
        return this;
    }

    public ResponseBuilder addCard(ContentId contentId, ContentId parentId) {
        // Create a Card
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder().setContentId(contentId).build();
        Feature feature = Feature.newBuilder()
                                  .setRenderableUnit(RenderableUnit.CARD)
                                  .setParentId(parentId)
                                  .build();
        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.UPDATE_OR_APPEND)
                                                     .setFeature(feature)
                                                     .setMetadata(payloadMetadata)
                                                     .build());
        wireProtocolInfo.featuresAdded.add(contentId);

        // Create content within the Card
        payloadMetadata =
                PayloadMetadata.newBuilder()
                        .setContentId(createNewContentId(contentId, CONTENT_ID_CARD_INCREMENT))
                        .build();
        feature = Feature.newBuilder()
                          .setRenderableUnit(RenderableUnit.CONTENT)
                          .setParentId(contentId)
                          .setExtension(Content.contentExtension,
                                  Content.newBuilder()
                                          .setType(Content.Type.PIET)
                                          .setExtension(PietContent.pietContentExtension,
                                                  PietContent.newBuilder()
                                                          .addAllPietSharedStates(
                                                                  pietSharedStateContentIds)
                                                          .build())
                                          .build())
                          .build();
        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.UPDATE_OR_APPEND)
                                                     .setFeature(feature)
                                                     .setMetadata(payloadMetadata)
                                                     .build());
        wireProtocolInfo.featuresAdded.add(payloadMetadata.getContentId());
        return this;
    }

    public ResponseBuilder addCardsToRoot(ContentId[] contentIds) {
        for (ContentId contentId : contentIds) {
            addCard(contentId, ROOT_CONTENT_ID);
        }
        return this;
    }

    public ResponseBuilder addCardWithSemanticData(ContentId contentId, ByteString semanticData) {
        feedResponseBuilder.addDataOperation(
                DataOperation.newBuilder()
                        .setOperation(Operation.UPDATE_OR_APPEND)
                        .setFeature(Feature.newBuilder().setRenderableUnit(RenderableUnit.CARD))
                        .setMetadata(
                                PayloadMetadata.newBuilder()
                                        .setContentId(contentId)
                                        .setSemanticProperties(
                                                SemanticProperties.newBuilder()
                                                        .setSemanticPropertiesData(semanticData))));
        wireProtocolInfo.featuresAdded.add(contentId);
        return this;
    }

    public ResponseBuilder removeFeature(ContentId contentId, ContentId parentId) {
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder().setContentId(contentId).build();
        Feature feature = Feature.newBuilder()
                                  .setRenderableUnit(RenderableUnit.CARD)
                                  .setParentId(parentId)
                                  .build();
        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.REMOVE)
                                                     .setFeature(feature)
                                                     .setMetadata(payloadMetadata)
                                                     .build());
        wireProtocolInfo.featuresAdded.add(contentId);
        return this;
    }

    public Response build() {
        if (token != null) {
            addToken(tokenId, token);
        }
        return Response.newBuilder()
                .setExtension(FeedResponse.feedResponse, feedResponseBuilder.build())
                .build();
    }

    private void addToken(int tokenId, ByteString token) {
        PayloadMetadata payloadMetadata =
                PayloadMetadata.newBuilder()
                        .setContentId(createNewContentId(TOKEN_CONTENT_ID, tokenId))
                        .build();
        Feature.Builder featureBuilder = Feature.newBuilder()
                                                 .setRenderableUnit(RenderableUnit.TOKEN)
                                                 .setParentId(ROOT_CONTENT_ID);
        Token wireToken = Token.newBuilder().setNextPageToken(token).build();
        featureBuilder.setExtension(Token.tokenExtension, wireToken);
        feedResponseBuilder.addDataOperation(DataOperation.newBuilder()
                                                     .setOperation(Operation.UPDATE_OR_APPEND)
                                                     .setFeature(featureBuilder.build())
                                                     .setMetadata(payloadMetadata)
                                                     .build());
    }

    public WireProtocolInfo getWireProtocolInfo() {
        return wireProtocolInfo;
    }

    /** Returns a new {@link ResponseBuilder}. */
    public static ResponseBuilder builder() {
        return new ResponseBuilder();
    }

    /** Returns a {@link ResponseBuilder} with a CLEAR_ALL, root, and cards. */
    public static ResponseBuilder forClearAllWithCards(ContentId[] cards) {
        return builder().addClearOperation().addRootFeature().addCardsToRoot(cards);
    }

    /** Captures information about the wire protocol that was created. */
    public static class WireProtocolInfo {
        public final List<ContentId> featuresAdded = new ArrayList<>();
        public boolean hasClearOperation;
        public boolean hasToken;
    }

    private ContentId createNewContentId(ContentId contentId, int idIncrement) {
        ContentId.Builder builder = contentId.toBuilder();
        builder.setId(contentId.getId() + idIncrement);
        return builder.build();
    }
}
