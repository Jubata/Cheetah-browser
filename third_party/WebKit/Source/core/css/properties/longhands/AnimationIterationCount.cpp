// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationIterationCount.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAnimationIterationCountUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationIterationCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyAnimationIterationCountUtils::ConsumeAnimationIterationCount,
      range);
}

}  // namespace CSSLonghand
}  // namespace blink
