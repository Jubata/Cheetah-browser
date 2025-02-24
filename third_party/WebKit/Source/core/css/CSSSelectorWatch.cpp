/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSSelectorWatch.h"

#include "core/css/CSSPropertyValueSet.h"
#include "core/css/StyleEngine.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "public/platform/TaskType.h"

namespace blink {

// The address of this string is important; its value is just documentation.
static const char kSupplementNameWatch[] = "CSSSelectorWatch";

CSSSelectorWatch::CSSSelectorWatch(Document& document)
    : Supplement<Document>(document),
      callback_selector_change_timer_(
          document.GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &CSSSelectorWatch::CallbackSelectorChangeTimerFired),
      timer_expirations_(0) {}

CSSSelectorWatch& CSSSelectorWatch::From(Document& document) {
  CSSSelectorWatch* watch = FromIfExists(document);
  if (!watch) {
    watch = new CSSSelectorWatch(document);
    Supplement<Document>::ProvideTo(document, kSupplementNameWatch, watch);
  }
  return *watch;
}

CSSSelectorWatch* CSSSelectorWatch::FromIfExists(Document& document) {
  return static_cast<CSSSelectorWatch*>(
      Supplement<Document>::From(document, kSupplementNameWatch));
}

void CSSSelectorWatch::CallbackSelectorChangeTimerFired(TimerBase*) {
  // Should be ensured by updateSelectorMatches():
  DCHECK(!added_selectors_.IsEmpty() || !removed_selectors_.IsEmpty());

  if (timer_expirations_ < 1) {
    timer_expirations_++;
    callback_selector_change_timer_.StartOneShot(0, BLINK_FROM_HERE);
    return;
  }
  if (GetSupplementable()->GetFrame()) {
    Vector<String> added_selectors;
    Vector<String> removed_selectors;
    CopyToVector(added_selectors_, added_selectors);
    CopyToVector(removed_selectors_, removed_selectors);
    GetSupplementable()->GetFrame()->Client()->SelectorMatchChanged(
        added_selectors, removed_selectors);
  }
  added_selectors_.clear();
  removed_selectors_.clear();
  timer_expirations_ = 0;
}

void CSSSelectorWatch::UpdateSelectorMatches(
    const Vector<String>& removed_selectors,
    const Vector<String>& added_selectors) {
  bool should_update_timer = false;

  for (const auto& selector : removed_selectors) {
    if (!matching_callback_selectors_.erase(selector))
      continue;

    // Count reached 0.
    should_update_timer = true;
    auto it = added_selectors_.find(selector);
    if (it != added_selectors_.end())
      added_selectors_.erase(it);
    else
      removed_selectors_.insert(selector);
  }

  for (const auto& selector : added_selectors) {
    HashCountedSet<String>::AddResult result =
        matching_callback_selectors_.insert(selector);
    if (!result.is_new_entry)
      continue;

    should_update_timer = true;
    auto it = removed_selectors_.find(selector);
    if (it != removed_selectors_.end())
      removed_selectors_.erase(it);
    else
      added_selectors_.insert(selector);
  }

  if (!should_update_timer)
    return;

  if (removed_selectors_.IsEmpty() && added_selectors_.IsEmpty()) {
    if (callback_selector_change_timer_.IsActive()) {
      timer_expirations_ = 0;
      callback_selector_change_timer_.Stop();
    }
  } else {
    timer_expirations_ = 0;
    if (!callback_selector_change_timer_.IsActive())
      callback_selector_change_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }
}

static bool AllCompound(const CSSSelectorList& selector_list) {
  for (const CSSSelector* selector = selector_list.First(); selector;
       selector = selector_list.Next(*selector)) {
    if (!selector->IsCompound())
      return false;
  }
  return true;
}

void CSSSelectorWatch::WatchCSSSelectors(const Vector<String>& selectors) {
  watched_callback_selectors_.clear();

  CSSPropertyValueSet* callback_property_set =
      ImmutableCSSPropertyValueSet::Create(nullptr, 0, kUASheetMode);

  // UA stylesheets always parse in the insecure context mode.
  CSSParserContext* context = CSSParserContext::Create(
      kUASheetMode, SecureContextMode::kInsecureContext);
  for (const auto& selector : selectors) {
    CSSSelectorList selector_list =
        CSSParser::ParseSelector(context, nullptr, selector);
    if (!selector_list.IsValid())
      continue;

    // Only accept Compound Selectors, since they're cheaper to match.
    if (!AllCompound(selector_list))
      continue;

    watched_callback_selectors_.push_back(
        StyleRule::Create(std::move(selector_list), callback_property_set));
  }
  GetSupplementable()->GetStyleEngine().WatchedSelectorsChanged();
}

void CSSSelectorWatch::Trace(blink::Visitor* visitor) {
  visitor->Trace(watched_callback_selectors_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
