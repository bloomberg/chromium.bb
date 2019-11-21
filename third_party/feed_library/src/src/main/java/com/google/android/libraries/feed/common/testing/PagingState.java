// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import static com.google.android.libraries.feed.common.testing.ResponseBuilder.ROOT_CONTENT_ID;
import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.common.testing.ResponseBuilder.WireProtocolInfo;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.ResponseProto.Response;
import java.nio.charset.Charset;

/**
 * Creates a paging based responses. Create two {@link Response} object, and initial response and a
 * page response. In addition, create a {@link StreamToken} which is used to trigger the paged
 * response.
 */
public class PagingState {
  public final Response initialResponse;
  public final Response pageResponse;
  public final StreamToken streamToken;

  public PagingState(
      ContentId[] initialCards,
      ContentId[] pageCards,
      int tokenId,
      ContentIdGenerators idGenerator) {
    ByteString token = ByteString.copyFrom(Integer.toString(tokenId), Charset.defaultCharset());
    initialResponse = getInitialResponse(initialCards, tokenId, token);
    streamToken = getSessionId(token, tokenId, idGenerator);
    pageResponse = getPageResponse(pageCards);
  }

  private static Response getInitialResponse(ContentId[] cards, int tokenId, ByteString token) {
    ResponseBuilder responseBuilder =
        ResponseBuilder.forClearAllWithCards(cards).addStreamToken(tokenId, token);
    WireProtocolInfo info = responseBuilder.getWireProtocolInfo();
    assertThat(info.hasToken).isTrue();
    return responseBuilder.build();
  }

  private static Response getPageResponse(ContentId[] pagedCards) {
    return ResponseBuilder.builder().addCardsToRoot(pagedCards).build();
  }

  private static StreamToken getSessionId(
      ByteString token, int id, ContentIdGenerators idGenerator) {
    ContentId tokenId =
        ContentId.newBuilder().setContentDomain("token").setId(id).setTable("feature").build();

    StreamToken.Builder tokenBuilder = StreamToken.newBuilder();
    tokenBuilder.setParentId(idGenerator.createContentId(ROOT_CONTENT_ID));
    tokenBuilder.setContentId(idGenerator.createContentId(tokenId));
    tokenBuilder.setNextPageToken(token);
    return tokenBuilder.build();
  }
}
