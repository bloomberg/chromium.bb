// Copyright 2018 The Feed Authors.
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

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link NoContentViewHolder}. */
@RunWith(RobolectricTestRunner.class)
public class NoContentViewHolderTest {

  private NoContentViewHolder noContentViewHolder;
  private FrameLayout frameLayout;

  ColorDrawable RED_BACKGROUND = new ColorDrawable(Color.RED);

  @Mock private CardConfiguration cardConfiguration;

  @Before
  public void setup() {
    initMocks(this);
    Context context = Robolectric.buildActivity(Activity.class).get();
    context.setTheme(R.style.Light);
    frameLayout = new FrameLayout(context);
    frameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
    noContentViewHolder = new NoContentViewHolder(cardConfiguration, context, frameLayout);
    when(cardConfiguration.getCardBackground()).thenReturn(RED_BACKGROUND);
  }

  @Test
  public void testNoContentViewInflated() {
    View view = frameLayout.findViewById(R.id.no_content);
    assertThat(view).isNotNull();
    assertThat(view.getVisibility()).isEqualTo(View.VISIBLE);
  }

  @Test
  public void testBind_setsBackground() {
    noContentViewHolder.bind();

    assertThat(frameLayout.getBackground()).isEqualTo(RED_BACKGROUND);
  }

  @Test
  public void testBind_setsMargins() {
    noContentViewHolder.bind();

    MarginLayoutParams marginLayoutParams =
        (MarginLayoutParams) noContentViewHolder.itemView.getLayoutParams();
    assertThat(marginLayoutParams.bottomMargin).isEqualTo(cardConfiguration.getCardBottomMargin());
    assertThat(marginLayoutParams.leftMargin).isEqualTo(cardConfiguration.getCardStartMargin());
    assertThat(marginLayoutParams.rightMargin).isEqualTo(cardConfiguration.getCardEndMargin());
  }
}
