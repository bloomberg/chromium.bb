// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runner.manipulation.Filter;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 *  Unit tests for GtestFilter.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class GtestFilterTest {

    @Test
    public void testDescription() {
        Filter filterUnderTest = new GtestFilter("TestClass.*");
        Assert.assertEquals("gtest-filter: TestClass.*", filterUnderTest.describe());
    }

    @Test
    public void testNoFilter() {
        Filter filterUnderTest = new GtestFilter("");
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testPositiveFilterExplicit() {
        Filter filterUnderTest = new GtestFilter("TestClass.testMethod");
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testPositiveFilterClassRegex() {
        Filter filterUnderTest = new GtestFilter("TestClass.*");
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testNegativeFilterExplicit() {
        Filter filterUnderTest = new GtestFilter("-TestClass.testMethod");
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testNegativeFilterClassRegex() {
        Filter filterUnderTest = new GtestFilter("-TestClass.*");
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testPositiveAndNegativeFilter() {
        Filter filterUnderTest = new GtestFilter("TestClass.*-TestClass.testMethod");
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

    @Test
    public void testMultiplePositiveFilters() {
        Filter filterUnderTest = new GtestFilter(
                "TestClass.otherTestMethod:OtherTestClass.otherTestMethod");
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "otherTestMethod")));
    }

    @Test
    public void testMultipleFiltersPositiveAndNegative() {
        Filter filterUnderTest = new GtestFilter("TestClass.*:-TestClass.testMethod");
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "testMethod")));
        Assert.assertTrue(filterUnderTest.shouldRun(
                Description.createTestDescription("TestClass", "otherTestMethod")));
        Assert.assertFalse(filterUnderTest.shouldRun(
                Description.createTestDescription("OtherTestClass", "testMethod")));
    }

}

