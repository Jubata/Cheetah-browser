/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/css/CSSGroupingRule.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSGroupingRule::CSSGroupingRule(StyleRuleGroup* group_rule,
                                 CSSStyleSheet* parent)
    : CSSRule(parent),
      group_rule_(group_rule),
      child_rule_cssom_wrappers_(group_rule->ChildRules().size()) {}

CSSGroupingRule::~CSSGroupingRule() {}

unsigned CSSGroupingRule::insertRule(const ExecutionContext* execution_context,
                                     const String& rule_string,
                                     unsigned index,
                                     ExceptionState& exception_state) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());

  if (index > group_rule_->ChildRules().size()) {
    exception_state.ThrowDOMException(
        kIndexSizeError,
        "the index " + String::Number(index) +
            " must be less than or equal to the length of the rule list.");
    return 0;
  }

  CSSStyleSheet* style_sheet = parentStyleSheet();
  CSSParserContext* context = CSSParserContext::CreateWithStyleSheet(
      ParserContext(execution_context->SecureContextMode()), style_sheet);
  StyleRuleBase* new_rule = CSSParser::ParseRule(
      context, style_sheet ? style_sheet->Contents() : nullptr, rule_string);
  if (!new_rule) {
    exception_state.ThrowDOMException(
        kSyntaxError,
        "the rule '" + rule_string + "' is invalid and cannot be parsed.");
    return 0;
  }

  if (new_rule->IsNamespaceRule()) {
    exception_state.ThrowDOMException(
        kHierarchyRequestError,
        "'@namespace' rules cannot be inserted inside a group rule.");
    return 0;
  }

  if (new_rule->IsImportRule()) {
    // FIXME: an HierarchyRequestError should also be thrown for a nested @media
    // rule. They are currently not getting parsed, resulting in a SyntaxError
    // to get raised above.
    exception_state.ThrowDOMException(
        kHierarchyRequestError,
        "'@import' rules cannot be inserted inside a group rule.");
    return 0;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  group_rule_->WrapperInsertRule(index, new_rule);

  child_rule_cssom_wrappers_.insert(index, Member<CSSRule>(nullptr));
  return index;
}

void CSSGroupingRule::deleteRule(unsigned index,
                                 ExceptionState& exception_state) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());

  if (index >= group_rule_->ChildRules().size()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "the index " + String::Number(index) +
                             " is greated than the length of the rule list.");
    return;
  }

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  group_rule_->WrapperRemoveRule(index);

  if (child_rule_cssom_wrappers_[index])
    child_rule_cssom_wrappers_[index]->SetParentRule(nullptr);
  child_rule_cssom_wrappers_.EraseAt(index);
}

void CSSGroupingRule::AppendCSSTextForItems(StringBuilder& result) const {
  unsigned size = length();
  for (unsigned i = 0; i < size; ++i) {
    result.Append("  ");
    result.Append(Item(i)->cssText());
    result.Append('\n');
  }
}

unsigned CSSGroupingRule::length() const {
  return group_rule_->ChildRules().size();
}

CSSRule* CSSGroupingRule::Item(unsigned index) const {
  if (index >= length())
    return nullptr;
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());
  Member<CSSRule>& rule = child_rule_cssom_wrappers_[index];
  if (!rule)
    rule = group_rule_->ChildRules()[index]->CreateCSSOMWrapper(
        const_cast<CSSGroupingRule*>(this));
  return rule.Get();
}

CSSRuleList* CSSGroupingRule::cssRules() const {
  if (!rule_list_cssom_wrapper_)
    rule_list_cssom_wrapper_ = LiveCSSRuleList<CSSGroupingRule>::Create(
        const_cast<CSSGroupingRule*>(this));
  return rule_list_cssom_wrapper_.Get();
}

void CSSGroupingRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  group_rule_ = static_cast<StyleRuleGroup*>(rule);
  for (unsigned i = 0; i < child_rule_cssom_wrappers_.size(); ++i) {
    if (child_rule_cssom_wrappers_[i])
      child_rule_cssom_wrappers_[i]->Reattach(
          group_rule_->ChildRules()[i].Get());
  }
}

void CSSGroupingRule::Trace(blink::Visitor* visitor) {
  CSSRule::Trace(visitor);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(group_rule_);
  visitor->Trace(rule_list_cssom_wrapper_);
}

}  // namespace blink
