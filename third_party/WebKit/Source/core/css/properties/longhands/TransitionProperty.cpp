// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TransitionProperty.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TransitionProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyTransitionPropertyUtils::ConsumeTransitionProperty, range);
  if (!list || !CSSPropertyTransitionPropertyUtils::IsValidPropertyList(*list))
    return nullptr;
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
