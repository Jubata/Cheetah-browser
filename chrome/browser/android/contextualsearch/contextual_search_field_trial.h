// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_FIELD_TRIAL_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_FIELD_TRIAL_H_

#include <stddef.h>

#include "base/values.h"

// This class manages the Contextual Search native field trials.
class ContextualSearchFieldTrial {
 public:
  ContextualSearchFieldTrial();
  virtual ~ContextualSearchFieldTrial();

  // Returns a partial URL to use for a Contextual Search Resolve request, or
  // an empty string if no override is required.  The returned value is a prefix
  // of a URL -- from the beginning up through the GWS end-point in the path,
  // which consists of the authority and the beginning of the path.
  std::string GetResolverURLPrefix();

  // Gets the size of the surrounding text to return for normal Resolve requests
  // when a Contextual Search is being performed.
  int GetResolveSurroundingSize();

  // Gets the size of the surrounding text to return as a sample to Java.
  int GetSampleSurroundingSize();

  // Gets whether sending the URL of the base page is disabled.
  bool IsSendBasePageURLDisabled();

  // Gets whether decoding the mentions fields in the Resolve is disabled.
  bool IsDecodeMentionsDisabled();

  // Gets an explicit version to use for Contextual Cards integration, or 0 if
  // not set.
  int GetContextualCardsVersion();

  // Gets whether either the CS Ranker integration param, or the ML Tap
  // Suppression feature flag is enabled.
  bool IsRankerIntegrationOrMlTapSuppressionEnabled();

  // Disables the cache.
  void DisableCache();

  // Constant used in tests.
  static const int kContextualSearchDefaultSampleSurroundingSize;

 protected:
  // Checks if command-line switch of the given name exists.
  virtual bool HasSwitch(const std::string& name);

  // Gets a command-line switch of the given name, returns the empty string
  // if the switch does not exist.
  virtual std::string GetSwitch(const std::string& name);

  // Gets a Variation parameter by the given name.
  virtual std::string GetParam(const std::string& name);

 private:
  // Gets a boolean param value of the given name from the Contextual Search
  // field trial.  Default is false if no param is present.
  bool GetBooleanParam(const std::string& param_name,
                       bool* is_value_cached,
                       bool* cached_value);

  // Gets an int param value of the given name from the Contextual Search
  // field trial.  Returns the |default_value| if no switch or param is present.
  // Caches the result pointed to by the given |cached_value|, using the given
  // |is_value_cached| to remember if the value has been cached or not.
  int GetIntParamValueOrDefault(const std::string& param_name,
                                const int default_value,
                                bool* is_value_cached,
                                int* cached_value);

  // Cached values.
  bool is_resolver_url_prefix_cached_;
  std::string resolver_url_prefix_;

  bool is_surrounding_size_cached_;
  int surrounding_size_;

  bool is_sample_surrounding_size_cached_;
  int sample_surrounding_size_;

  bool is_send_base_page_url_disabled_cached_;
  bool is_send_base_page_url_disabled_;

  bool is_decode_mentions_disabled_cached_;
  bool is_decode_mentions_disabled_;

  bool is_contextual_cards_version_cached_;
  int contextual_cards_version_;

  bool is_ranker_integration_enabled_cached_;
  bool is_ranker_integration_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchFieldTrial);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_FIELD_TRIAL_H_
