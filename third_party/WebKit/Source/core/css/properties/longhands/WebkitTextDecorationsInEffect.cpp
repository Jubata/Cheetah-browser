// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitTextDecorationsInEffect.h"

#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitTextDecorationsInEffect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(range);
}

}  // namespace CSSLonghand
}  // namespace blink
