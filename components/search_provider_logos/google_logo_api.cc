// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include <stdint.h>

#include <algorithm>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/search_provider_logos/switches.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace search_provider_logos {

namespace {
// Appends the provided |value| to the "async" query param, according to the
// format used by the Google doodle servers: "async=param:value,other:foo"
// Derived from net::AppendOrReplaceQueryParameter, that can't be used because
// it escapes ":" to "%3A", but the server requires the colon not to be escaped.
// See: http://crbug.com/413845
GURL AppendToAsyncQueryparam(const GURL& url, const std::string& value) {
  const std::string param_name = "async";
  bool replaced = false;
  const std::string input = url.query();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  while (url::ExtractQueryKeyValue(input.data(), &cursor, &key_range,
                                   &value_range)) {
    const base::StringPiece key(input.data() + key_range.begin, key_range.len);
    std::string key_value_pair(input, key_range.begin,
                               value_range.end() - key_range.begin);
    if (!replaced && key == param_name) {
      // Check |replaced| as only the first match should be replaced.
      replaced = true;
      key_value_pair += "," + value;
    }
    if (!output.empty()) {
      output += "&";
    }

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty()) {
      output += "&";
    }

    output += (param_name + "=" + value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

}  // namespace

GURL GetGoogleDoodleURL(const GURL& google_base_url) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGoogleDoodleUrl)) {
    return GURL(command_line->GetSwitchValueASCII(switches::kGoogleDoodleUrl));
  }

  GURL::Replacements replacements;
  replacements.SetPathStr("async/ddljson");
  // Make sure we use https rather than http (except for .cn).
  if (google_base_url.SchemeIs(url::kHttpScheme) &&
      !base::EndsWith(google_base_url.host_piece(), ".cn",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    replacements.SetSchemeStr(url::kHttpsScheme);
  }
  return google_base_url.ReplaceComponents(replacements);
}

GURL AppendFingerprintParamToDoodleURL(const GURL& logo_url,
                                       const std::string& fingerprint) {
  if (fingerprint.empty()) {
    return logo_url;
  }

  return AppendToAsyncQueryparam(logo_url, "es_dfp:" + fingerprint);
}

GURL AppendPreliminaryParamsToDoodleURL(bool gray_background,
                                        const GURL& logo_url) {
  std::string api_params = "ntp:1";
  if (gray_background) {
    api_params += ",graybg:1";
  }

  return AppendToAsyncQueryparam(logo_url, api_params);
}

namespace {
const char kResponsePreamble[] = ")]}'";

GURL ParseUrl(const base::DictionaryValue& parent_dict,
              const std::string& key,
              const GURL& base_url) {
  std::string url_str;
  if (!parent_dict.GetString(key, &url_str) || url_str.empty()) {
    return GURL();
  }
  return base_url.Resolve(url_str);
}

}  // namespace

std::unique_ptr<EncodedLogo> ParseDoodleLogoResponse(
    const GURL& base_url,
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed) {
  // The response may start with )]}'. Ignore this.
  base::StringPiece response_sp(*response);
  if (response_sp.starts_with(kResponsePreamble))
    response_sp.remove_prefix(strlen(kResponsePreamble));

  // Default parsing failure to be true.
  *parsing_failed = true;

  int error_code;
  std::string error_string;
  int error_line;
  int error_col;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      response_sp, 0, &error_code, &error_string, &error_line, &error_col);
  if (!value) {
    LOG(WARNING) << error_string << " at " << error_line << ":" << error_col;
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> config =
      base::DictionaryValue::From(std::move(value));
  if (!config)
    return nullptr;

  const base::DictionaryValue* ddljson = nullptr;
  if (!config->GetDictionary("ddljson", &ddljson))
    return nullptr;

  // If there is no logo today, the "ddljson" dictionary will be empty.
  if (ddljson->empty()) {
    *parsing_failed = false;
    return nullptr;
  }

  auto logo = base::MakeUnique<EncodedLogo>();

  std::string doodle_type;
  logo->metadata.type = LogoType::SIMPLE;
  if (ddljson->GetString("doodle_type", &doodle_type)) {
    if (doodle_type == "ANIMATED") {
      logo->metadata.type = LogoType::ANIMATED;
    } else if (doodle_type == "INTERACTIVE") {
      logo->metadata.type = LogoType::INTERACTIVE;
    } else if (doodle_type == "VIDEO") {
      logo->metadata.type = LogoType::INTERACTIVE;
    }
  }

  const bool is_animated = (logo->metadata.type == LogoType::ANIMATED);

  // Check if the main image is animated.
  if (is_animated) {
    // If animated, get the URL for the animated image.
    const base::DictionaryValue* image = nullptr;
    if (!ddljson->GetDictionary("large_image", &image))
      return nullptr;
    logo->metadata.animated_url = ParseUrl(*image, "url", base_url);
    if (!logo->metadata.animated_url.is_valid())
      return nullptr;
  }

  logo->metadata.full_page_url =
      ParseUrl(*ddljson, "fullpage_interactive_url", base_url);

  // Data is optional, since we may be revalidating a cached logo.
  // If there is a CTA image, get that; otherwise use the regular image.
  std::string encoded_image_data;
  if (ddljson->GetString("cta_data_uri", &encoded_image_data) ||
      ddljson->GetString("data_uri", &encoded_image_data)) {
    GURL encoded_image_uri(encoded_image_data);
    if (!encoded_image_uri.is_valid() ||
        !encoded_image_uri.SchemeIs(url::kDataScheme)) {
      return nullptr;
    }
    std::string content = encoded_image_uri.GetContent();
    // The content should look like this: "image/png;base64,aaa..." (where
    // "aaa..." is the base64-encoded image data).
    size_t mime_type_end = content.find_first_of(';');
    if (mime_type_end == std::string::npos)
      return nullptr;
    logo->metadata.mime_type = content.substr(0, mime_type_end);

    size_t base64_begin = mime_type_end + 1;
    size_t base64_end = content.find_first_of(',', base64_begin);
    if (base64_end == std::string::npos)
      return nullptr;
    base::StringPiece base64(content.begin() + base64_begin,
                             content.begin() + base64_end);
    if (base64 != "base64")
      return nullptr;

    size_t data_begin = base64_end + 1;
    base::StringPiece data(content.begin() + data_begin, content.end());
    logo->encoded_image = base::MakeRefCounted<base::RefCountedString>();
    if (!base::Base64Decode(data, &logo->encoded_image->data()))
      return nullptr;
  }

  logo->metadata.on_click_url = ParseUrl(*ddljson, "target_url", base_url);
  ddljson->GetString("alt_text", &logo->metadata.alt_text);

  ddljson->GetString("fingerprint", &logo->metadata.fingerprint);

  base::TimeDelta time_to_live;
  // The JSON doesn't guarantee the number to fit into an int.
  double ttl_ms = 0;  // Expires immediately if the parameter is missing.
  if (ddljson->GetDouble("time_to_live_ms", &ttl_ms)) {
    time_to_live = base::TimeDelta::FromMillisecondsD(ttl_ms);
    logo->metadata.can_show_after_expiration = false;
  } else {
    time_to_live = base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS);
    logo->metadata.can_show_after_expiration = true;
  }
  logo->metadata.expiration_time = response_time + time_to_live;

  *parsing_failed = false;
  return logo;
}

}  // namespace search_provider_logos
