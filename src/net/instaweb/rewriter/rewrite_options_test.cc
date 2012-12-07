/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: bmcquade@google.com (Bryan McQuade)

#include "net/instaweb/rewriter/public/rewrite_options.h"

#include <set>

#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteOptionsTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  typedef std::set<RewriteOptions::Filter> FilterSet;

  bool NoneEnabled() {
    FilterSet s;
    return OnlyEnabled(s);
  }

  bool OnlyEnabled(const FilterSet& filters) {
    bool ret = true;
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         ret && (f < RewriteOptions::kEndOfFilters);
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (filters.find(f) != filters.end()) {
        if (!options_.Enabled(f)) {
          ret = false;
        }
      } else {
        if (options_.Enabled(f)) {
          ret = false;
        }
      }
    }
    return ret;
  }

  bool OnlyEnabled(RewriteOptions::Filter filter) {
    FilterSet s;
    s.insert(filter);
    return OnlyEnabled(s);
  }

  void MergeOptions(const RewriteOptions& one, const RewriteOptions& two) {
    options_.Merge(one);
    options_.Merge(two);
  }

  // Tests either SetOptionFromName or SetOptionFromNameAndLog depending
  // on 'test_log_variant'
  void TestNameSet(RewriteOptions::OptionSettingResult expected_result,
                   bool test_log_variant,
                   const StringPiece& name,
                   const StringPiece& value,
                   MessageHandler* handler) {
    if (test_log_variant) {
      bool expected = (expected_result == RewriteOptions::kOptionOk);
      EXPECT_EQ(
          expected,
          options_.SetOptionFromNameAndLog(name, value.as_string(), handler));
    } else {
      GoogleString msg;
      EXPECT_EQ(expected_result,
                options_.SetOptionFromName(name, value.as_string(), &msg));
      // Should produce a message exactly when not OK.
      EXPECT_EQ(expected_result != RewriteOptions::kOptionOk, !msg.empty())
          << msg;
    }
  }

  void TestSetOptionFromName(bool test_log_variant);

  RewriteOptions options_;
  MockHasher hasher_;
};

TEST_F(RewriteOptionsTest, DefaultEnabledFilters) {
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, InstrumentationDisabled) {
  // Make sure the kCoreFilters enables some filters.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Now disable all filters and make sure none are enabled.
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
  }
  ASSERT_TRUE(NoneEnabled());
}

TEST_F(RewriteOptionsTest, DisableTrumpsEnable) {
  // Disable the default filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
    options_.EnableFilter(f);
  }
}

TEST_F(RewriteOptionsTest, ForceEnableFilter) {
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options_.EnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));

  options_.ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, CoreFilters) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    if (options_.Enabled(f)) {
      s.insert(f);
    }
  }

  // Make sure that more than one filter is enabled in the core filter
  // set.
  ASSERT_GT(s.size(), 1);
}

TEST_F(RewriteOptionsTest, Enable) {
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    s.insert(f);
    s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
    options_.EnableFilter(f);
    ASSERT_TRUE(OnlyEnabled(s));
  }
}

TEST_F(RewriteOptionsTest, CommaSeparatedList) {
  FilterSet s;
  s.insert(RewriteOptions::kAddInstrumentation);
  s.insert(RewriteOptions::kLeftTrimUrls);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "add_instrumentation,trim_urls";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, CompoundFlag) {
  FilterSet s;
  // TODO(jmaessen): add kConvertJpegToWebp here when it becomes part of
  // rewrite_images.
  s.insert(RewriteOptions::kConvertGifToPng);
  s.insert(RewriteOptions::kInlineImages);
  s.insert(RewriteOptions::kJpegSubsampling);
  s.insert(RewriteOptions::kRecompressJpeg);
  s.insert(RewriteOptions::kRecompressPng);
  s.insert(RewriteOptions::kRecompressWebp);
  s.insert(RewriteOptions::kResizeImages);
  s.insert(RewriteOptions::kStripImageMetaData);
  s.insert(RewriteOptions::kStripImageColorProfile);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "rewrite_images";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, CompoundFlagRecompressImages) {
  FilterSet s;
  s.insert(RewriteOptions::kConvertGifToPng);
  s.insert(RewriteOptions::kJpegSubsampling);
  s.insert(RewriteOptions::kRecompressJpeg);
  s.insert(RewriteOptions::kRecompressPng);
  s.insert(RewriteOptions::kRecompressWebp);
  s.insert(RewriteOptions::kStripImageMetaData);
  s.insert(RewriteOptions::kStripImageColorProfile);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "recompress_images";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, ParseRewriteLevel) {
  RewriteOptions::RewriteLevel level;
  ASSERT_TRUE(RewriteOptions::ParseRewriteLevel("PassThrough", &level));
  ASSERT_EQ(RewriteOptions::kPassThrough, level);

  ASSERT_TRUE(RewriteOptions::ParseRewriteLevel("CoreFilters", &level));
  ASSERT_EQ(RewriteOptions::kCoreFilters, level);

  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel(NULL, &level));
  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel("", &level));
  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel("Garbage", &level));
}

TEST_F(RewriteOptionsTest, MergeLevelsDefault) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCoreTwoPass) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOnePassTwoCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);  // overrides one
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsBothCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeFilterPassThrough) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOne) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaTwo) {
  RewriteOptions one, two;
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  two.DisableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.DisableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeCoreFilter) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheImages);
  two.DisableFilter(RewriteOptions::kExtendCacheImages);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheScripts);
  two.EnableFilter(RewriteOptions::kExtendCacheScripts);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
}

TEST_F(RewriteOptionsTest, MergeThresholdDefault) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCssInlineMaxBytes,
            options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOne) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  MergeOptions(one, two);
  EXPECT_EQ(5, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdTwo) {
  RewriteOptions one, two;
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOverride) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampDefault) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCacheInvalidationTimestamp,
            options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOne) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  MergeOptions(one, two);
  EXPECT_EQ(11111111, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwo) {
  RewriteOptions one, two;
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOneLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(33333333);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(33333333, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwoLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, Allow) {
  options_.Allow("*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("a*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  options_.Allow("ab*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("abc*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
}

TEST_F(RewriteOptionsTest, MergeAllow) {
  RewriteOptions one, two;
  one.Allow("*.css");
  EXPECT_TRUE(one.IsAllowed("abcd.css"));
  one.Disallow("a*.css");
  EXPECT_FALSE(one.IsAllowed("abcd.css"));

  two.Allow("ab*.css");
  EXPECT_TRUE(two.IsAllowed("abcd.css"));
  two.Disallow("abc*.css");
  EXPECT_FALSE(two.IsAllowed("abcd.css"));

  MergeOptions(one, two);
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  EXPECT_FALSE(options_.IsAllowed("abc.css"));
  EXPECT_TRUE(options_.IsAllowed("ab.css"));
  EXPECT_FALSE(options_.IsAllowed("a.css"));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersNotExplicitlyEnabled) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  two.DisableAllFiltersNotExplicitlyEnabled();  // Should disable AddHead.
  MergeOptions(one, two);

  // Make sure AddHead enabling didn't leak through.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersOverrideFilterLevel) {
  // Disable the default enabled filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);

  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.EnableFilter(RewriteOptions::kAddHead);
  options_.DisableAllFiltersNotExplicitlyEnabled();

  // Check that *only* AddHead is enabled, even though we have CoreFilters
  // level set.
  EXPECT_TRUE(OnlyEnabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, ForbidFilter) {
  // Forbid a core filter: this will disable it.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.ForbidFilter(RewriteOptions::kExtendCacheCss);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));

  // Forbid a filter, then try to merge in an enablement: it won't take.
  // At the same time, merge in a new "forbiddenment": it will take.
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.ForbidFilter(RewriteOptions::kExtendCacheCss);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.ForbidFilter(RewriteOptions::kFlattenCssImports);
  one.Merge(two);
  EXPECT_FALSE(one.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_FALSE(one.Enabled(RewriteOptions::kFlattenCssImports));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kFlattenCssImports)));
}

TEST_F(RewriteOptionsTest, AllDoesNotImplyStripScrips) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kStripScripts));
}

TEST_F(RewriteOptionsTest, RejectedRequestUrl) {
  options_.AddRejectedUrlWildcard("http://www.a.com/b/*");
  EXPECT_TRUE(options_.IsRejectedUrl("http://www.a.com/b/sdsd123"));
  EXPECT_FALSE(options_.IsRejectedUrl("http://www.a.com/"));
  EXPECT_FALSE(options_.IsRejectedUrl("http://www.b.com/b/"));
}

TEST_F(RewriteOptionsTest, RejectedRequest) {
  options_.AddRejectedHeaderWildcard("UserAgent", "*Chrome*");
  EXPECT_FALSE(options_.IsRejectedRequest("Host", "www.a.com"));
  EXPECT_FALSE(options_.IsRejectedRequest("UserAgent", "firefox"));
  EXPECT_TRUE(options_.IsRejectedRequest("UserAgent", "abc Chrome 456"));
}

TEST_F(RewriteOptionsTest, RejectedRequestMerge) {
  RewriteOptions one, two;
  one.AddRejectedUrlWildcard("http://www.a.com/b/*");
  one.AddRejectedHeaderWildcard("UserAgent", "*Chrome*");
  two.AddRejectedUrlWildcard("http://www.b.com/b/*");
  MergeOptions(one, two);

  EXPECT_FALSE(options_.IsRejectedRequest("Host", "www.a.com"));
  EXPECT_FALSE(options_.IsRejectedRequest("UserAgent", "firefox"));
  EXPECT_TRUE(options_.IsRejectedRequest("UserAgent", "abc Chrome 456"));
  EXPECT_TRUE(options_.IsRejectedUrl("http://www.a.com/b/sdsd123"));
  EXPECT_FALSE(options_.IsRejectedUrl("http://www.a.com/"));
  EXPECT_TRUE(options_.IsRejectedUrl("http://www.b.com/b/"));
}

TEST_F(RewriteOptionsTest, ExplicitlyEnabledDangerousFilters) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  options_.EnableFilter(RewriteOptions::kStripScripts);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kStripScripts));
  options_.EnableFilter(RewriteOptions::kDivStructure);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
}

TEST_F(RewriteOptionsTest, CoreAndNotDangerous) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddInstrumentation));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
}

TEST_F(RewriteOptionsTest, CoreByNameNotLevel) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.EnableFiltersByCommaSeparatedList("core", &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Test these for PlusAndMinus validation.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, PlusAndMinus) {
  const char* kList = "core,+div_structure,-inline_css,+extend_cache_css";
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.AdjustFiltersByCommaSeparatedList(kList, &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // These should be opposite from normal.
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, SetDefaultRewriteLevel) {
  NullMessageHandler handler;
  RewriteOptions new_options;
  new_options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  options_.Merge(new_options);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

void RewriteOptionsTest::TestSetOptionFromName(bool test_log_variant) {
  NullMessageHandler handler;

  // TODO(sriharis):  Add tests for all Options here, like in
  // LookupOptionEnumTest.

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "FetcherTimeOutMs",
              "1024",
              &handler);
  // Default for this is 5 * Timer::kSecondMs.
  EXPECT_EQ(1024, options_.blocking_fetch_timeout_ms());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CssInlineMaxBytes",
              "1024",
              &handler);
  // Default for this is 2048.
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "JpegRecompressionQuality",
              "1",
              &handler);
  // Default is -1.
  EXPECT_EQ(1, options_.image_jpeg_recompress_quality());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CombineAcrossPaths",
              "false",
              &handler);
  // Default is true
  EXPECT_FALSE(options_.combine_across_paths());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon2 https://www.example.com/beacon3",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon2", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon3", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "/pagespeed_beacon?",
              &handler);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().http);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().https);

  RewriteOptions::RewriteLevel old_level = options_.level();
  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "RewriteLevel",
              "does_not_work",
              &handler);
  EXPECT_EQ(old_level, options_.level());

  TestNameSet(RewriteOptions::kOptionNameUnknown,
              test_log_variant,
              "InvalidName",
              "example",
              &handler);

  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "JsInlineMaxBytes",
              "NOT_INT",
              &handler);
  EXPECT_EQ(RewriteOptions::kDefaultJsInlineMaxBytes,
            options_.js_inline_max_bytes());  // unchanged from default.
}

TEST_F(RewriteOptionsTest, SetOptionFromName) {
  TestSetOptionFromName(false);
}

TEST_F(RewriteOptionsTest, SetOptionFromNameAndLog) {
  TestSetOptionFromName(true);
}

// All the option names are explicitly enumerated here. Modifications are
// handled by the explicit tests. Additions/deletions are handled by checking
// kEndOfOptions explicitly (and assuming we add/delete an option value when we
// add/delete an option name).
TEST_F(RewriteOptionsTest, LookupOptionEnumTest) {
  EXPECT_EQ(124, RewriteOptions::kEndOfOptions);
  EXPECT_STREQ("AjaxRewritingEnabled",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kAjaxRewritingEnabled));
  EXPECT_STREQ("AlwaysRewriteCss",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kAlwaysRewriteCss));
  EXPECT_STREQ("AnalyticsID",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kAnalyticsID));
  EXPECT_STREQ("AvoidRenamingIntrospectiveJavascript",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kAvoidRenamingIntrospectiveJavascript));
  EXPECT_STREQ("BeaconUrl",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kBeaconUrl));
  EXPECT_STREQ("BlinkMaxHtmlSizeRewritable",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kBlinkMaxHtmlSizeRewritable));
  EXPECT_STREQ("BlinkNonCacheablesForAllFamilies",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kBlinkNonCacheablesForAllFamilies));
  EXPECT_STREQ("CacheSmallImagesUnrewritten",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCacheSmallImagesUnrewritten));
  EXPECT_STREQ("CombineAcrossPaths",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCombineAcrossPaths));
  EXPECT_STREQ("ClientDomainRewrite",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kClientDomainRewrite));
  EXPECT_STREQ("CriticalImagesCacheExpirationTimeMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCriticalImagesCacheExpirationTimeMs));
  EXPECT_STREQ("CriticalLineConfig",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCriticalLineConfig));
  EXPECT_STREQ("CssFlattenMaxBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCssFlattenMaxBytes));
  EXPECT_STREQ("CssImageInlineMaxBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCssImageInlineMaxBytes));
  EXPECT_STREQ("CssInlineMaxBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCssInlineMaxBytes));
  EXPECT_STREQ("CssOutlineMinBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCssOutlineMinBytes));
  EXPECT_STREQ("CssPreserveURLs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCssPreserveURLs));
  EXPECT_STREQ("DefaultCacheHtml",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kDefaultCacheHtml));
  EXPECT_STREQ("DomainRewriteHyperlinks",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kDomainRewriteHyperlinks));
  EXPECT_STREQ("DomainShardCount",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kDomainShardCount));
  EXPECT_STREQ("PropagateBlinkCacheDeletes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kPropagateBlinkCacheDeletes));
  EXPECT_STREQ("EnableAggressiveRewritersForMobile",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableAggressiveRewritersForMobile));
  EXPECT_STREQ("EnableBlinkCriticalLine",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableBlinkCriticalLine));
  EXPECT_STREQ("EnableBlinkHtmlChangeDetection",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableBlinkHtmlChangeDetection));
  EXPECT_STREQ("EnableBlinkHtmlChangeDetectionLogging",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableBlinkHtmlChangeDetectionLogging));
  EXPECT_STREQ("EnableDeferJsExperimental",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableDeferJsExperimental));
  EXPECT_STREQ("EnableFlushSubresourcesExperimental",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableFlushSubresourcesExperimental));
  EXPECT_STREQ("EnableLazyloadInBlink",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnableLazyloadInBlink));
  EXPECT_STREQ("EnableRewriting",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kEnabled));
  EXPECT_STREQ("FlushBufferLimitBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFlushBufferLimitBytes));
  EXPECT_STREQ("FlushHtml",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFlushHtml));
  EXPECT_STREQ("FlushMoreResourcesEarlyIfTimePermits",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFlushMoreResourcesEarlyIfTimePermits));
  EXPECT_STREQ("ForbidAllDisabledFilters",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kForbidAllDisabledFilters));
  EXPECT_STREQ("IdleFlushTimeMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kIdleFlushTimeMs));
  EXPECT_STREQ("ImageInlineMaxBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageInlineMaxBytes));
  EXPECT_STREQ("ImageJpegNumProgressiveScans",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageJpegNumProgressiveScans));
  EXPECT_STREQ("ImageLimitOptimizedPercent",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageLimitOptimizedPercent));
  EXPECT_STREQ("ImageLimitResizeAreaPercent",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageLimitResizeAreaPercent));
  EXPECT_STREQ("ImageMaxRewritesAtOnce",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageMaxRewritesAtOnce));
  EXPECT_STREQ("ImageResolutionLimitBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageResolutionLimitBytes));
  EXPECT_STREQ("ImageRetainColorProfile",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageRetainColorProfile));
  EXPECT_STREQ("ImageRetainColorSampling",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageRetainColorSampling));
  EXPECT_STREQ("ImageRetainExifData",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageRetainExifData));
  EXPECT_STREQ("ImageRecompressionQuality",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageRecompressionQuality));
  EXPECT_STREQ("ImagePreserveURLs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImagePreserveURLs));
  EXPECT_STREQ("ImageWebpRecompressionQuality",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageWebpRecompressionQuality));
  EXPECT_STREQ("ImplicitCacheTtlMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImplicitCacheTtlMs));
  EXPECT_STREQ("InPlaceWaitForOptimized",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kInPlaceWaitForOptimized));
  EXPECT_STREQ("JpegRecompressionQuality",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kImageJpegRecompressionQuality));
  EXPECT_STREQ("JsInlineMaxBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kJsInlineMaxBytes));
  EXPECT_STREQ("JsOutlineMinBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kJsOutlineMinBytes));
  EXPECT_STREQ("LazyloadImagesBlankUrl",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLazyloadImagesBlankUrl));
  EXPECT_STREQ("JsPreserveURLs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kJsPreserveURLs));
  EXPECT_STREQ("LazyloadImagesAfterOnload",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLazyloadImagesAfterOnload));
  EXPECT_STREQ("InlineOnlyCriticalImages",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kInlineOnlyCriticalImages));
  EXPECT_STREQ("LogRewriteTiming",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLogRewriteTiming));
  EXPECT_STREQ("LowercaseHtmlNames",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLowercaseHtmlNames));
  EXPECT_STREQ("MaxCacheableContentLength",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxCacheableResponseContentLength));
  EXPECT_STREQ("MaxHtmlCacheTimeMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxHtmlCacheTimeMs));
  EXPECT_STREQ("MaxImageBytesForWebpInCss",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxImageBytesForWebpInCss));
  EXPECT_STREQ("MaxImageSizeLowResolutionBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxImageSizeLowResolutionBytes));
  EXPECT_STREQ("MaxInlinedPreviewImagesIndex",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxInlinedPreviewImagesIndex));
  EXPECT_STREQ("MaxSegmentLength",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxUrlSegmentSize));
  EXPECT_STREQ("MaxUrlSize",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMaxUrlSize));
  EXPECT_STREQ("MinImageSizeLowResolutionBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMinImageSizeLowResolutionBytes));
  EXPECT_STREQ("MinResourceCacheTimeToRewriteMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMinResourceCacheTimeToRewriteMs));
  EXPECT_STREQ("ModifyCachingHeaders",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kModifyCachingHeaders));
  EXPECT_STREQ("OverrideCachingTtlMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kOverrideCachingTtlMs));
  EXPECT_STREQ("OverrideIeDocumentMode",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kOverrideIeDocumentMode));
  EXPECT_STREQ("ProgressiveJpegMinBytes",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kProgressiveJpegMinBytes));
  EXPECT_STREQ("RejectBlacklisted",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRejectBlacklisted));
  EXPECT_STREQ("RejectBlacklistedStatusCode",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRejectBlacklistedStatusCode));
  EXPECT_STREQ("RespectVary",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRespectVary));
  EXPECT_STREQ("RespectXForwardedProto",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRespectXForwardedProto));
  EXPECT_STREQ("RewriteDeadlinePerFlushMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRewriteDeadlineMs));
  EXPECT_STREQ("RewriteLevel",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRewriteLevel));
  EXPECT_STREQ("RunExperiment",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRunningFurious));
  EXPECT_STREQ("ServeStaleIfFetchError",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kServeStaleIfFetchError));
  EXPECT_STREQ("SupportNoScriptEnabled",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kSupportNoScriptEnabled));
  EXPECT_STREQ("UseFixedUserAgentForBlinkCacheMisses",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kUseFixedUserAgentForBlinkCacheMisses));
  EXPECT_STREQ("XHeaderValue",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kXModPagespeedHeaderValue));

  // Apache-specific options.
  EXPECT_STREQ("CacheFlushFilename",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCacheFlushFilename));
  EXPECT_STREQ("CacheFlushPollIntervalSec",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCacheFlushPollIntervalSec));
  EXPECT_STREQ("CollectRefererStatistics",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kCollectRefererStatistics));
  EXPECT_STREQ("ExperimentalFetchFromModSpdy",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kExperimentalFetchFromModSpdy));
  EXPECT_STREQ("FetchProxy",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFetcherProxy));
  EXPECT_STREQ("FetcherTimeOutMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFetcherTimeOutMs));
  EXPECT_STREQ("FileCacheCleanIntervalMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFileCacheCleanIntervalMs));
  EXPECT_STREQ("FileCachePath",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFileCachePath));
  EXPECT_STREQ("FileCacheSizeKb",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFileCacheCleanSizeKb));
  EXPECT_STREQ("FileCacheInodeLimit",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kFileCacheCleanInodeLimit));
  EXPECT_STREQ("HashRefererStatistics",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kHashRefererStatistics));
  EXPECT_STREQ("LRUCacheByteLimit",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLruCacheByteLimit));
  EXPECT_STREQ("LRUCacheKbPerProcess",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kLruCacheKbPerProcess));
  EXPECT_STREQ("MemcachedServers",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMemcachedServers));
  EXPECT_STREQ("MemcachedThreads",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMemcachedThreads));
  EXPECT_STREQ("MemcachedTimeoutUs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kMemcachedTimeoutUs));
  EXPECT_STREQ("RateLimitBackgroundFetches",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRateLimitBackgroundFetches));
  EXPECT_STREQ("RefererStatisticsOutputLevel",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kRefererStatisticsOutputLevel));
  EXPECT_STREQ("SharedMemoryLocks",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kUseSharedMemLocking));
  EXPECT_STREQ("SlurpDirectory",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kSlurpDirectory));
  EXPECT_STREQ("SlurpFlushLimit",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kSlurpFlushLimit));
  EXPECT_STREQ("SlurpReadOnly",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kSlurpReadOnly));
  EXPECT_STREQ("Statistics",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsEnabled));
  EXPECT_STREQ("StatisticsLogging",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsLoggingEnabled));
  EXPECT_STREQ("StatisticsLoggingFile",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsLoggingFile));
  EXPECT_STREQ("StatisticsLoggingIntervalMs",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsLoggingIntervalMs));
  EXPECT_STREQ("StatisticsLoggingChartsCSS",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsLoggingChartsCSS));
  EXPECT_STREQ("StatisticsLoggingChartsJS",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kStatisticsLoggingChartsJS));
  EXPECT_STREQ("TestProxy",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kTestProxy));
  EXPECT_STREQ("UseSmartDiffInBlink",
               RewriteOptions::LookupOptionEnum(
                   RewriteOptions::kUseSmartDiffInBlink));
}

TEST_F(RewriteOptionsTest, PrioritizeVisibleContentFamily) {
  GoogleUrl gurl_one("http://www.test.org/one.html");
  GoogleUrl gurl_two("http://www.test.org/two.html");

  EXPECT_FALSE(options_.IsInBlinkCacheableFamily(gurl_one));
  options_.set_apply_blink_if_no_families(true);
  EXPECT_TRUE(options_.IsInBlinkCacheableFamily(gurl_one));
  EXPECT_EQ(RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs,
            options_.GetBlinkCacheTimeFor(gurl_one));
  EXPECT_EQ(RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs,
            options_.GetBlinkCacheTimeFor(gurl_two));
  EXPECT_EQ("", options_.GetBlinkNonCacheableElementsFor(gurl_one));
  EXPECT_EQ("", options_.GetBlinkNonCacheableElementsFor(gurl_two));

  options_.AddBlinkCacheableFamily("http://www.test.org/one*", 10, "something");
  EXPECT_TRUE(options_.IsInBlinkCacheableFamily(gurl_one));
  EXPECT_FALSE(options_.IsInBlinkCacheableFamily(gurl_two));
  EXPECT_EQ(10, options_.GetBlinkCacheTimeFor(gurl_one));
  EXPECT_EQ("something", options_.GetBlinkNonCacheableElementsFor(gurl_one));

  options_.set_blink_non_cacheables_for_all_families("all1");
  EXPECT_EQ("something,all1",
            options_.GetBlinkNonCacheableElementsFor(gurl_one));
  EXPECT_EQ("all1", options_.GetBlinkNonCacheableElementsFor(gurl_two));

  RewriteOptions options1;
  options1.AddBlinkCacheableFamily("http://www.test.org/two*", 20, "something");
  options1.set_blink_non_cacheables_for_all_families("all2");
  options_.Merge(options1);
  EXPECT_FALSE(options_.IsInBlinkCacheableFamily(gurl_one));
  EXPECT_TRUE(options_.IsInBlinkCacheableFamily(gurl_two));
  EXPECT_EQ(RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs,
            options_.GetBlinkCacheTimeFor(gurl_one));
  EXPECT_EQ(20, options_.GetBlinkCacheTimeFor(gurl_two));
  EXPECT_EQ("all2", options_.GetBlinkNonCacheableElementsFor(gurl_one));
  EXPECT_EQ("something,all2",
            options_.GetBlinkNonCacheableElementsFor(gurl_two));

  EXPECT_EQ(RewriteOptions::kDefaultOverrideBlinkCacheTimeMs,
            options1.override_blink_cache_time_ms());
  options1.set_override_blink_cache_time_ms(120000);
  EXPECT_EQ(120000, options1.GetBlinkCacheTimeFor(gurl_one));
  EXPECT_EQ(120000, options1.GetBlinkCacheTimeFor(gurl_two));

  options_.set_blink_non_cacheables_for_all_families("all3");
  RewriteOptions options2;
  options2.AddBlinkCacheableFamily("http://www.test.org/two*", 40, "");
  options_.Merge(options2);
  EXPECT_EQ(40, options_.GetBlinkCacheTimeFor(gurl_two));
  EXPECT_EQ("all3", options_.GetBlinkNonCacheableElementsFor(gurl_one));
  EXPECT_EQ("all3", options_.GetBlinkNonCacheableElementsFor(gurl_two));
}

TEST_F(RewriteOptionsTest, FuriousSpecTest) {
  // Test that we handle furious specs properly, and that when
  // we set the options to one experiment or another, it works.
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  // Set the default slot to 4.
  options_.set_furious_ga_slot(4);
  EXPECT_FALSE(options_.AddFuriousSpec("id=0", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=7;percent=10;level=CoreFilters;enabled=sprite_images;"
      "disabled=inline_css;inline_js=600000", &handler));

  // Extra spaces to test whitespace handling.
  EXPECT_TRUE(options_.AddFuriousSpec("id=2;    percent=15;ga=UA-2222-1;"
                                      "disabled=insert_ga ;slot=3;",
                                      &handler));

  // Invalid slot - make sure the spec still gets added, and the slot defaults
  // to the global slot (4).
  EXPECT_TRUE(options_.AddFuriousSpec("id=17;percent=3;slot=8", &handler));

  options_.SetFuriousState(7);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  // This experiment didn't have a ga_id, so make sure we still have the
  // global ga_id.
  EXPECT_EQ("UA-111111-1", options_.ga_id());
  EXPECT_EQ(4, options_.furious_ga_slot());

  // insert_ga can not be disabled in any furious experiment because
  // that filter injects the instrumentation we use to collect the data.
  options_.SetFuriousState(2);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInsertGA));
  EXPECT_EQ(3, options_.furious_ga_slot());
  // This experiment specified a ga_id, so make sure that we set it.
  EXPECT_EQ("UA-2222-1", options_.ga_id());

  options_.SetFuriousState(17);
  EXPECT_EQ(4, options_.furious_ga_slot());

  options_.SetFuriousState(7);
  EXPECT_EQ("a", options_.GetFuriousStateStr());
  options_.SetFuriousState(2);
  EXPECT_EQ("b", options_.GetFuriousStateStr());
  options_.SetFuriousState(17);
  EXPECT_EQ("c", options_.GetFuriousStateStr());
  options_.SetFuriousState(furious::kFuriousNotSet);
  EXPECT_EQ("", options_.GetFuriousStateStr());
  options_.SetFuriousState(furious::kFuriousNoExperiment);
  EXPECT_EQ("", options_.GetFuriousStateStr());

  options_.SetFuriousStateStr("a");
  EXPECT_EQ("a", options_.GetFuriousStateStr());
  options_.SetFuriousStateStr("b");
  EXPECT_EQ("b", options_.GetFuriousStateStr());
  options_.SetFuriousStateStr("c");
  EXPECT_EQ("c", options_.GetFuriousStateStr());

  // Invalid state index 'd'; we only added three specs above.
  options_.SetFuriousStateStr("d");
  // No effect on the furious state; stay with 'c' from before.
  EXPECT_EQ("c", options_.GetFuriousStateStr());

  // Check a state index that will be out of bounds in the other direction.
  options_.SetFuriousStateStr("`");
  // Still no effect on the furious state.
  EXPECT_EQ("c", options_.GetFuriousStateStr());

  // Check that we have a maximum size of 26 concurrent experiment specs.
  // Get us up to 26.
  for (int i = options_.num_furious_experiments(); i < 26 ; ++i) {
    int tmp_id = i+100;  // Don't want conflict with experiments added above.
    EXPECT_TRUE(options_.AddFuriousSpec(
        StrCat("id=", IntegerToString(tmp_id),
               ";percent=1;default"), &handler));
  }
  EXPECT_EQ(26, options_.num_furious_experiments());
  // Object to adding a 27th.
  EXPECT_FALSE(options_.AddFuriousSpec("id=200;percent=1;default", &handler));
}

TEST_F(RewriteOptionsTest, PreserveURLDefaults) {
  // This test serves as a warning. If you enable preserve URLs by default then
  // many unit tests will fail due to filters being omitted from the HTML path.
  // Further, preserve_urls is not explicitly tested for the 'false' case, it is
  // assumed to be tested by the normal unit tests since the default value is
  // false.
  EXPECT_FALSE(options_.image_preserve_urls());
  EXPECT_FALSE(options_.css_preserve_urls());
  EXPECT_FALSE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, RewriteDeadlineTest) {
  EXPECT_EQ(RewriteOptions::kDefaultRewriteDeadlineMs,
            options_.rewrite_deadline_ms());
  options_.set_rewrite_deadline_ms(40);
  EXPECT_EQ(40, options_.rewrite_deadline_ms());
}

TEST_F(RewriteOptionsTest, FuriousPrintTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  options_.set_running_furious_experiment(true);
  EXPECT_FALSE(options_.AddFuriousSpec("id=2;enabled=rewrite_css;", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=1;percent=15;default", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=7;percent=15;level=AllFilters;",
                                      &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=2;percent=15;enabled=rewrite_css;"
                                      "inline_css=4096;ga_id=122333-4",
                                      &handler));
  options_.SetFuriousState(-7);
  // This should be the core filters.
  EXPECT_EQ("ah,cc,gp,jp,mc,pj,ec,ei,es,fc,if,hw,ci,ii,il,ji,js,rj,rp,rw,"
            "ri,cf,jm,cu,cp,md,css:2048,im:2048,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("", options_.ToExperimentString());
  options_.SetFuriousState(1);
  EXPECT_EQ("Experiment: 1; ah,ai,cc,gp,jp,mc,pj,ec,ei,es,fc,if,hw,ci,ii,"
            "il,ji,ig,js,rj,rp,rw,ri,cf,jm,cu,cp,md,css:2048,im:2048,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 1", options_.ToExperimentString());
  options_.SetFuriousState(7);
  // This should be all non-dangerous filters.
  EXPECT_EQ("Experiment: 7; ab,ah,ai,cw,fe,cc,ch,jc,gp,jp,jw,mc,pj,db,di,"
            "ea,ec,ei,ep,es,fc,if,fs,hn,hw,ci,ii,il,ji,idp,ig,id,js,tu,ls,"
            "ga,cj,cm,co,jo,pc,pv,rj,rp,rw,rc,rq,ri,rm,cf,rd,jm,cs,cu,is,sq,cp,"
            "md,css:2048,im:2048,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 7", options_.ToExperimentString());
  options_.SetFuriousState(2);
  // This should be the filters we need to run an experiment (add_head,
  // add_instrumentation, html_writer, insert_ga) plus rewrite_css.
  // The image inline threshold is 0 because ImageInlineMaxBytes()
  // only returns the threshold if inline_images is enabled.
  EXPECT_EQ("Experiment: 2; ah,ai,hw,ig,cf,css:4096,im:0,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 2", options_.ToExperimentString());

  // Make sure we set the ga_id to the one specified by spec 2.
  EXPECT_EQ("122333-4", options_.ga_id());
}

TEST_F(RewriteOptionsTest, FuriousUndoOptionsTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_running_furious_experiment(true);

  // Default for this is 2048.
  EXPECT_EQ(2048L, options_.ImageInlineMaxBytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=1;percent=15;enable=inline_images;"
      "inline_images=1024", &handler));
  options_.SetFuriousState(1);
  EXPECT_EQ(1024L, options_.ImageInlineMaxBytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=2;percent=15;enable=inline_images", &handler));
  options_.SetFuriousState(2);
  EXPECT_EQ(2048L, options_.ImageInlineMaxBytes());
}

TEST_F(RewriteOptionsTest, FuriousOptionsTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_running_furious_experiment(true);

  // Default for this is 2048.
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=1;percent=15;enable=defer_javascript;"
      "options=CssInlineMaxBytes=1024", &handler));
  options_.SetFuriousState(1);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=2;percent=15;enable=resize_images;options=BogusOption=35", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=3;percent=15;enable=defer_javascript", &handler));
  options_.SetFuriousState(3);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=4;percent=15;enable=defer_javascript;"
      "options=CssInlineMaxBytes=Cabbage", &handler));
  options_.SetFuriousState(4);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=5;percent=15;enable=defer_javascript;"
      "options=Potato=Carrot,5=10,6==9,CssInlineMaxBytes=1024", &handler));
  options_.SetFuriousState(5);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=6;percent=15;enable=defer_javascript;"
      "options=JsOutlineMinBytes=4096,JpegRecompresssionQuality=50,"
      "CssInlineMaxBytes=100,JsInlineMaxBytes=123", &handler));
  options_.SetFuriousState(6);
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, FuriousMergeTest) {
  NullMessageHandler handler;
  RewriteOptions::FuriousSpec *spec = new
      RewriteOptions::FuriousSpec("id=1;percentage=15;"
                                  "enable=defer_javascript;"
                                  "options=CssInlineMaxBytes=100",
                                  &options_, &handler);

  RewriteOptions::FuriousSpec *spec2 = new
      RewriteOptions::FuriousSpec("id=2;percentage=25;enable=resize_images;"
                                  "options=CssInlineMaxBytes=125", &options_,
                                  &handler);
  options_.InsertFuriousSpecInVector(spec);
  options_.InsertFuriousSpecInVector(spec2);
  options_.SetFuriousState(1);
  EXPECT_EQ(15, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
  spec->Merge(*spec2);
  options_.SetFuriousState(1);
  EXPECT_EQ(25, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(125L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, SetOptionsFromName) {
  RewriteOptions::OptionSet option_set;
  option_set.insert(RewriteOptions::OptionStringPair(
      "CssInlineMaxBytes", "1024"));
  EXPECT_TRUE(options_.SetOptionsFromName(option_set));
  option_set.insert(RewriteOptions::OptionStringPair(
      "Not an Option", "nothing"));
  EXPECT_FALSE(options_.SetOptionsFromName(option_set));
}

// TODO(sriharis):  Add thorough ComputeSignature tests

TEST_F(RewriteOptionsTest, ComputeSignatureWildcardGroup) {
  // hasher_ is a MockHasher and always returns 0.  This is fine for this test
  // (and all tests that do not depend on Option<GoogleString>'s signature
  // changing with change in value).  But if hasher is used more widely in
  // ComputeSignature we need to revisit the usage of MockHasher here.
  options_.ComputeSignature(&hasher_);
  GoogleString signature1 = options_.signature();
  // Tweak allow_resources_ and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.Disallow("http://www.example.com/*");
  options_.ComputeSignature(&hasher_);
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);
  // Tweak retain_comments and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.RetainComment("TEST");
  options_.ComputeSignature(&hasher_);
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature1, signature3);
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ComputeSignatureOptionEffect) {
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(2048);
  options_.set_ajax_rewriting_enabled(false);
  options_.ComputeSignature(&hasher_);
  GoogleString signature1 = options_.signature();

  // Changing an Option used in signature computation will change the signature.
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(1024);
  options_.ComputeSignature(&hasher_);
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);

  // Changing an Option not used in signature computation will not change the
  // signature.
  options_.ClearSignatureForTesting();
  options_.set_ajax_rewriting_enabled(true);
  options_.ComputeSignature(&hasher_);
  GoogleString signature3 = options_.signature();

  // See the comment in RewriteOptions::RewriteOptions -- we need to leave
  // signatures sensitive to ajax_rewriting.
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ImageOptimizableCheck) {
  options_.ClearFilters();
  options_.EnableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationTest) {
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.AddUrlCacheInvalidationEntry("two*", 25L, false);
  RewriteOptions options1;
  options1.AddUrlCacheInvalidationEntry("one*", 20L, true);
  options1.AddUrlCacheInvalidationEntry("three*", 23L, false);
  options1.AddUrlCacheInvalidationEntry("three*", 30L, true);
  options_.Merge(options1);
  EXPECT_TRUE(options_.IsUrlCacheInvalidationEntriesSorted());
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 9L));
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 19L));
  EXPECT_TRUE(options_.IsUrlCacheValid("one1", 21L));
  EXPECT_FALSE(options_.IsUrlCacheValid("two2", 21L));
  EXPECT_TRUE(options_.IsUrlCacheValid("two2", 26L));
  EXPECT_TRUE(options_.IsUrlCacheValid("three3", 31L));
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationSignatureTest) {
  options_.ComputeSignature(&hasher_);
  GoogleString signature1 = options_.signature();
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.ComputeSignature(&hasher_);
  GoogleString signature2 = options_.signature();
  EXPECT_EQ(signature1, signature2);
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("two*", 10L, false);
  options_.ComputeSignature(&hasher_);
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, EnabledFiltersRequiringJavaScriptTest) {
  RewriteOptions foo;
  foo.ClearFilters();
  foo.EnableFilter(RewriteOptions::kDeferJavascript);
  foo.EnableFilter(RewriteOptions::kResizeImages);
  FilterSet foo_fs;
  foo.GetEnabledFiltersRequiringScriptExecution(&foo_fs);
  EXPECT_FALSE(foo_fs.empty());
  EXPECT_EQ(1, foo_fs.size());

  RewriteOptions bar;
  bar.ClearFilters();
  bar.EnableFilter(RewriteOptions::kResizeImages);
  bar.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  FilterSet bar_fs;
  bar.GetEnabledFiltersRequiringScriptExecution(&bar_fs);
  EXPECT_TRUE(bar_fs.empty());
}

TEST_F(RewriteOptionsTest, FilterLookupMethods) {
  EXPECT_STREQ("Add Head",
               RewriteOptions::FilterName(RewriteOptions::kAddHead));
  EXPECT_STREQ("Remove Comments",
               RewriteOptions::FilterName(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("End of Filters",
  //              RewriteOptions::FilterName(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("Unknown Filter",
  //              RewriteOptions::FilterName(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_STREQ("ah",
               RewriteOptions::FilterId(RewriteOptions::kAddHead));
  EXPECT_STREQ("rc",
               RewriteOptions::FilterId(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("  "));
  EXPECT_EQ(RewriteOptions::kAddHead,
            RewriteOptions::LookupFilterById("ah"));
  EXPECT_EQ(RewriteOptions::kRemoveComments,
            RewriteOptions::LookupFilterById("rc"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("zz"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("UF"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("junk"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(""));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(NULL));
}

}  // namespace net_instaweb
