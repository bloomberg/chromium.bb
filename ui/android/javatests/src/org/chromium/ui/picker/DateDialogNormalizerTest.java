// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.picker;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.DatePicker;

import java.util.Calendar;
import java.util.TimeZone;

/**
 * Tests methods in DateDialogNormalizer.
 */
public class DateDialogNormalizerTest extends InstrumentationTestCase {
    @SmallTest
    public void testTrimToDate() {
        Calendar cal = DateDialogNormalizer.trimToDate(1501542000000L);
        assertEquals(2017, cal.get(Calendar.YEAR));
        assertEquals(6, cal.get(Calendar.MONTH)); // getMonth returns a 0 based range
        assertEquals(31, cal.get(Calendar.DAY_OF_MONTH));
        assertEquals(0, cal.get(Calendar.HOUR));
        assertEquals(0, cal.get(Calendar.MINUTE));
        assertEquals(0, cal.get(Calendar.SECOND));

        cal = DateDialogNormalizer.trimToDate(1501542002365L);
        assertEquals(2017, cal.get(Calendar.YEAR));
        assertEquals(6, cal.get(Calendar.MONTH));
        assertEquals(31, cal.get(Calendar.DAY_OF_MONTH));
        assertEquals(0, cal.get(Calendar.HOUR));
        assertEquals(0, cal.get(Calendar.MINUTE));
        assertEquals(0, cal.get(Calendar.SECOND));
    }

    @SmallTest
    public void testSetLimits() {
        DatePicker picker = new DatePicker(getInstrumentation().getContext());
        picker.updateDate(2015, 5, 25);

        // if min && max are the same does nothing
        DateDialogNormalizer.setLimits(picker, 5, 5);
        assertEquals(2015, picker.getYear());
        assertEquals(5, picker.getMonth());
        assertEquals(25, picker.getDayOfMonth());

        // min > max does nothing
        DateDialogNormalizer.setLimits(picker, 6, 5);
        assertEquals(2015, picker.getYear());
        assertEquals(5, picker.getMonth());
        assertEquals(25, picker.getDayOfMonth());

        // If the picker date is not within range it is not respected
        // July 31 2017 to July 31 2018
        DateDialogNormalizer.setLimits(picker, 1501542000000L, 1533078000000L);
        assertEquals(2017, picker.getYear());
        assertEquals(6, picker.getMonth()); // getMonth returns a 0 based range
        assertEquals(31, picker.getDayOfMonth());

        picker.updateDate(2015, 5, 25);

        TimeZone defaultTimeZone = TimeZone.getDefault();
        // "Japan" timezone: earlier than UTC by 9 hours, no DST.
        TimeZone.setDefault(TimeZone.getTimeZone("Japan"));

        // Same thing if it's not respected because it's in the future of the range
        // July 31 2012 to July 31 2014
        DateDialogNormalizer.setLimits(picker, 1343775600000L, 1406847600000L);
        assertEquals(2012, picker.getYear());
        assertEquals(6, picker.getMonth());
        assertEquals(31, picker.getDayOfMonth());

        // MinDate and MaxDate should be translated to the defalut time zone.
        final long millisPerHour = 60 * 60 * 1000;
        // 1343692800000: July 31 2012 00:00 UTC
        assertEquals(1343692800000L - 9 * millisPerHour, picker.getMinDate());
        // 1406764800000: July 31 2014 00:00 UTC
        assertEquals(1406764800000L - 9 * millisPerHour, picker.getMaxDate());
        TimeZone.setDefault(defaultTimeZone);
    }
}
