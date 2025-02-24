// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontFeatureSettings.h"

#include "core/css/properties/CSSPropertyFontUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* FontFeatureSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyFontUtils::ConsumeFontFeatureSettings(range);
}

}  // namespace CSSLonghand
}  // namespace blink
