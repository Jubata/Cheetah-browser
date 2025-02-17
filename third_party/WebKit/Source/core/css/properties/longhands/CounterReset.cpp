// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CounterReset.h"

#include "core/css/properties/CSSPropertyCounterUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* CounterReset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyCounterUtils::ConsumeCounter(
      range, CSSPropertyCounterUtils::kResetDefaultValue);
}

}  // namespace CSSLonghand
}  // namespace blink
