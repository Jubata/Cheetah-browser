// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/WebkitColumnBreakBefore.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyLegacyBreakUtils.h"

namespace blink {
namespace CSSShorthand {

bool WebkitColumnBreakBefore::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValueID value;
  if (!CSSPropertyLegacyBreakUtils::ConsumeFromColumnBreakBetween(range,
                                                                  value)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBreakBefore, CSSPropertyWebkitColumnBreakBefore,
      *CSSIdentifierValue::Create(value), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
