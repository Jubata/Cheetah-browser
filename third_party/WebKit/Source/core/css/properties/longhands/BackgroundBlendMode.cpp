// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BackgroundBlendMode.h"

#include "core/css/properties/CSSPropertyBackgroundUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BackgroundBlendMode::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyBackgroundUtils::ConsumeBackgroundBlendMode, range);
}

}  // namespace CSSLonghand
}  // namespace blink
