/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/editing/LayoutSelection.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/forms/TextControlElement.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

SelectionPaintRange::SelectionPaintRange(LayoutObject* start_layout_object,
                                         WTF::Optional<int> start_offset,
                                         LayoutObject* end_layout_object,
                                         WTF::Optional<int> end_offset)
    : start_layout_object_(start_layout_object),
      start_offset_(start_offset),
      end_layout_object_(end_layout_object),
      end_offset_(end_offset) {}

bool SelectionPaintRange::operator==(const SelectionPaintRange& other) const {
  return start_layout_object_ == other.start_layout_object_ &&
         start_offset_ == other.start_offset_ &&
         end_layout_object_ == other.end_layout_object_ &&
         end_offset_ == other.end_offset_;
}

LayoutObject* SelectionPaintRange::StartLayoutObject() const {
  DCHECK(!IsNull());
  return start_layout_object_;
}

WTF::Optional<int> SelectionPaintRange::StartOffset() const {
  DCHECK(!IsNull());
  return start_offset_;
}

LayoutObject* SelectionPaintRange::EndLayoutObject() const {
  DCHECK(!IsNull());
  return end_layout_object_;
}

WTF::Optional<int> SelectionPaintRange::EndOffset() const {
  DCHECK(!IsNull());
  return end_offset_;
}

SelectionPaintRange::Iterator::Iterator(const SelectionPaintRange* range) {
  if (!range) {
    current_ = nullptr;
    return;
  }
  current_ = range->StartLayoutObject();
  stop_ = range->EndLayoutObject()->NextInPreOrder();
}

LayoutObject* SelectionPaintRange::Iterator::operator*() const {
  DCHECK(current_);
  return current_;
}

SelectionPaintRange::Iterator& SelectionPaintRange::Iterator::operator++() {
  DCHECK(current_);
  current_ = current_->NextInPreOrder();
  if (current_ && current_ != stop_)
    return *this;

  current_ = nullptr;
  return *this;
}

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      paint_range_(SelectionPaintRange()) {}

enum class SelectionMode {
  kNone,
  kRange,
  kBlockCursor,
};

static SelectionMode ComputeSelectionMode(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsRange())
    return SelectionMode::kRange;
  DCHECK(selection_in_dom.IsCaret());
  if (!frame_selection.ShouldShowBlockCursor())
    return SelectionMode::kNone;
  if (IsLogicalEndOfLine(CreateVisiblePosition(selection_in_dom.Base())))
    return SelectionMode::kNone;
  return SelectionMode::kBlockCursor;
}

static EphemeralRangeInFlatTree CalcSelectionInFlatTree(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  switch (ComputeSelectionMode(frame_selection)) {
    case SelectionMode::kNone:
      return {};
    case SelectionMode::kRange: {
      const PositionInFlatTree& base =
          ToPositionInFlatTree(selection_in_dom.Base());
      const PositionInFlatTree& extent =
          ToPositionInFlatTree(selection_in_dom.Extent());
      if (base.IsNull() || extent.IsNull() || base == extent ||
          !base.IsValidFor(frame_selection.GetDocument()) ||
          !extent.IsValidFor(frame_selection.GetDocument()))
        return {};
      return base <= extent ? EphemeralRangeInFlatTree(base, extent)
                            : EphemeralRangeInFlatTree(extent, base);
    }
    case SelectionMode::kBlockCursor: {
      const PositionInFlatTree& base =
          CreateVisiblePosition(ToPositionInFlatTree(selection_in_dom.Base()))
              .DeepEquivalent();
      if (base.IsNull())
        return {};
      const PositionInFlatTree end_position =
          NextPositionOf(base, PositionMoveType::kGraphemeCluster);
      if (end_position.IsNull())
        return {};
      return base <= end_position
                 ? EphemeralRangeInFlatTree(base, end_position)
                 : EphemeralRangeInFlatTree(end_position, base);
    }
  }
  NOTREACHED();
  return {};
}

// LayoutObjects each has SelectionState of kStart, kEnd, kStartAndEnd, or
// kInside
using SelectedLayoutObjects = HashSet<LayoutObject*>;

#ifndef NDEBUG
void PrintPaintInvalidationSet(const SelectedLayoutObjects& selected_objects) {
  std::stringstream stream;
  stream << std::endl << "layout_objects:" << std::endl;
  for (LayoutObject* layout_object : selected_objects) {
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << std::endl;
  }
  LOG(INFO) << stream.str();
}
#endif

static SelectedLayoutObjects CollectInvalidationSet(
    const SelectionPaintRange& range) {
  if (range.IsNull())
    return SelectedLayoutObjects();

  SelectedLayoutObjects selected_objects;
  for (LayoutObject* runner : range)
    selected_objects.insert(runner);
  return selected_objects;
}

// This class represents a selection range in layout tree and each LayoutObject
// is SelectionState-marked.
class NewPaintRangeAndSelectedLayoutObjects {
  STACK_ALLOCATED();

 public:
  NewPaintRangeAndSelectedLayoutObjects() = default;
  NewPaintRangeAndSelectedLayoutObjects(SelectionPaintRange paint_range,
                                        SelectedLayoutObjects selected_objects)
      : paint_range_(paint_range),
        selected_objects_(std::move(selected_objects)) {}
  NewPaintRangeAndSelectedLayoutObjects(
      NewPaintRangeAndSelectedLayoutObjects&& other) {
    paint_range_ = other.paint_range_;
    selected_objects_ = std::move(other.selected_objects_);
  }

  SelectionPaintRange PaintRange() const { return paint_range_; }

  const SelectedLayoutObjects& LayoutObjects() const {
    return selected_objects_;
  }

 private:
  SelectionPaintRange paint_range_;
  SelectedLayoutObjects selected_objects_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewPaintRangeAndSelectedLayoutObjects);
};

static void SetShouldInvalidateIfNeeds(LayoutObject* layout_object) {
  if (layout_object->ShouldInvalidateSelection())
    return;
  layout_object->SetShouldInvalidateSelection();

  // We should invalidate if ancestor of |layout_object| is LayoutSVGText
  // because SVGRootInlineBoxPainter::Paint() paints selection for
  // |layout_object| in/ LayoutSVGText and it is invoked when parent
  // LayoutSVGText is invalidated.
  // That is different from InlineTextBoxPainter::Paint() which paints
  // LayoutText selection when LayoutText is invalidated.
  if (!layout_object->IsSVG())
    return;
  for (LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    if (parent->IsSVGRoot())
      return;
    if (parent->IsSVGText()) {
      if (!parent->ShouldInvalidateSelection())
        parent->SetShouldInvalidateSelection();
      return;
    }
  }
}

static void SetSelectionStateIfNeeded(LayoutObject* layout_object,
                                      SelectionState state) {
  DCHECK_NE(state, SelectionState::kContain) << layout_object;
  if (layout_object->GetSelectionState() == state)
    return;
  // TODO(yoichio): Once we make LayoutObject::SetSelectionState() tribial, use
  // it directly.
  layout_object->LayoutObject::SetSelectionState(state);

  // Set parent SelectionState kContain for CSS ::selection style.
  // See LayoutObject::InvalidatePaintForSelection().
  // TODO(yoichio): We should not propagation kNone state.
  // if (state == SelectionState::kNone)
  //   return;
  const SelectionState propagate_state = state == SelectionState::kNone
                                             ? SelectionState::kNone
                                             : SelectionState::kContain;
  for (LayoutObject* containing_block = layout_object->ContainingBlock();
       containing_block;
       containing_block = containing_block->ContainingBlock()) {
    if (containing_block->GetSelectionState() == propagate_state)
      return;
    containing_block->LayoutObject::SetSelectionState(propagate_state);
  }
}

// Set ShouldInvalidateSelection flag of LayoutObjects
// comparing them in |new_range| and |old_range|.
static void SetShouldInvalidateSelection(
    const NewPaintRangeAndSelectedLayoutObjects& new_range,
    const SelectionPaintRange& old_range) {
  const SelectedLayoutObjects& new_selected_objects = new_range.LayoutObjects();
  SelectedLayoutObjects old_selected_objects =
      CollectInvalidationSet(old_range);

  // We invalidate each LayoutObject which is
  // - included in new selection range and has valid SelectionState(!= kNone).
  // - included in old selection range
  // Invalidate new selected LayoutObjects.
  for (LayoutObject* layout_object : new_selected_objects) {
    if (layout_object->GetSelectionState() != SelectionState::kNone) {
      SetShouldInvalidateIfNeeds(layout_object);
      old_selected_objects.erase(layout_object);
      continue;
    }
  }

  // Invalidate previous selected LayoutObjects except already invalidated
  // above.
  for (LayoutObject* layout_object : old_selected_objects) {
    const SelectionState old_state = layout_object->GetSelectionState();
    SetSelectionStateIfNeeded(layout_object, SelectionState::kNone);
    if (layout_object->GetSelectionState() == old_state)
      continue;
    SetShouldInvalidateIfNeeds(layout_object);
  }
}

WTF::Optional<int> LayoutSelection::SelectionStart() const {
  DCHECK(!HasPendingSelection());
  if (paint_range_.IsNull())
    return WTF::nullopt;
  return paint_range_.StartOffset();
}

WTF::Optional<int> LayoutSelection::SelectionEnd() const {
  DCHECK(!HasPendingSelection());
  if (paint_range_.IsNull())
    return WTF::nullopt;
  return paint_range_.EndOffset();
}

void LayoutSelection::ClearSelection() {
  // For querying Layer::compositingState()
  // This is correct, since destroying layout objects needs to cause eager paint
  // invalidations.
  DisableCompositingQueryAsserts disabler;

  // Just return if the selection is already empty.
  if (paint_range_.IsNull())
    return;

  for (auto layout_object : paint_range_) {
    if (layout_object->GetSelectionState() == SelectionState::kNone)
      continue;
    layout_object->LayoutObject::SetSelectionState(SelectionState::kNone);
    layout_object->SetShouldInvalidateSelection();
    for (LayoutObject* runner = layout_object->ContainingBlock(); runner;
         runner = runner->ContainingBlock()) {
      if (runner->GetSelectionState() == SelectionState::kNone)
        break;
      runner->LayoutObject::SetSelectionState(SelectionState::kNone);
    }
  }

  // Reset selection.
  paint_range_ = SelectionPaintRange();
}

static WTF::Optional<int> ComputeStartOffset(
    const LayoutObject& layout_object,
    const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return WTF::nullopt;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return 0;
}

static WTF::Optional<int> ComputeEndOffset(const LayoutObject& layout_object,
                                           const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return WTF::nullopt;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return ToText(layout_node)->length();
}

static LayoutTextFragment* FirstLetterPartFor(LayoutObject* layout_object) {
  if (!layout_object->IsText())
    return nullptr;
  if (!ToLayoutText(layout_object)->IsTextFragment())
    return nullptr;
  return ToLayoutTextFragment(const_cast<LayoutObject*>(
      AssociatedLayoutObjectOf(*layout_object->GetNode(), 0)));
}

static void MarkSelected(SelectedLayoutObjects* selected_objects,
                         LayoutObject* layout_object,
                         SelectionState state) {
  DCHECK(layout_object->CanBeSelectionLeaf());
  SetSelectionStateIfNeeded(layout_object, state);
  selected_objects->insert(layout_object);
}

static void MarkSelectedInside(SelectedLayoutObjects* selected_objects,
                               LayoutObject* layout_object) {
  MarkSelected(selected_objects, layout_object, SelectionState::kInside);
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part)
    return;
  MarkSelected(selected_objects, first_letter_part, SelectionState::kInside);
}

static NewPaintRangeAndSelectedLayoutObjects MarkStartAndEndInOneNode(
    SelectedLayoutObjects selected_objects,
    LayoutObject* layout_object,
    WTF::Optional<int> start_offset,
    WTF::Optional<int> end_offset) {
  if (!layout_object->GetNode()->IsTextNode()) {
    DCHECK(!start_offset.has_value());
    DCHECK(!end_offset.has_value());
    MarkSelected(&selected_objects, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, WTF::nullopt, layout_object, WTF::nullopt},
            std::move(selected_objects)};
  }

  DCHECK(start_offset.has_value());
  DCHECK(end_offset.has_value());
  DCHECK_GE(start_offset.value(), 0);
  DCHECK_GE(end_offset.value(), start_offset.value());
  if (start_offset.value() == end_offset.value())
    return {};
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part) {
    MarkSelected(&selected_objects, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, start_offset, layout_object, end_offset},
            std::move(selected_objects)};
  }
  const unsigned unsigned_start = static_cast<unsigned>(start_offset.value());
  const unsigned unsigned_end = static_cast<unsigned>(end_offset.value());
  LayoutTextFragment* const remaining_part =
      ToLayoutTextFragment(layout_object);
  if (unsigned_start >= remaining_part->Start()) {
    // Case 1: The selection starts and ends in remaining part.
    DCHECK_GT(unsigned_end, remaining_part->Start());
    MarkSelected(&selected_objects, remaining_part,
                 SelectionState::kStartAndEnd);
    return {{remaining_part,
             static_cast<int>(unsigned_start - remaining_part->Start()),
             remaining_part,
             static_cast<int>(unsigned_end - remaining_part->Start())},
            std::move(selected_objects)};
  }
  if (unsigned_end <= remaining_part->Start()) {
    // Case 2: The selection starts and ends in first letter part.
    MarkSelected(&selected_objects, first_letter_part,
                 SelectionState::kStartAndEnd);
    return {{first_letter_part, start_offset, first_letter_part, end_offset},
            std::move(selected_objects)};
  }
  // Case 3: The selection starts in first-letter part and ends in remaining
  // part.
  DCHECK_GT(unsigned_end, remaining_part->Start());
  MarkSelected(&selected_objects, first_letter_part, SelectionState::kStart);
  MarkSelected(&selected_objects, remaining_part, SelectionState::kEnd);
  return {{first_letter_part, start_offset, remaining_part,
           static_cast<int>(unsigned_end - remaining_part->Start())},
          std::move(selected_objects)};
}

// LayoutObjectAndOffset represents start or end of SelectionPaintRange.
struct LayoutObjectAndOffset {
  STACK_ALLOCATED();
  LayoutObject* layout_object;
  WTF::Optional<int> offset;

  explicit LayoutObjectAndOffset(LayoutObject* passed_layout_object)
      : layout_object(passed_layout_object), offset(WTF::nullopt) {
    DCHECK(passed_layout_object);
    DCHECK(!passed_layout_object->GetNode()->IsTextNode());
  }
  LayoutObjectAndOffset(LayoutText* layout_text, int passed_offset)
      : layout_object(layout_text), offset(passed_offset) {
    DCHECK(layout_object);
    DCHECK_GE(passed_offset, 0);
  }
};

LayoutObjectAndOffset MarkStart(SelectedLayoutObjects* selected_objects,
                                LayoutObject* start_layout_object,
                                WTF::Optional<int> start_offset) {
  if (!start_layout_object->GetNode()->IsTextNode()) {
    DCHECK(!start_offset.has_value());
    MarkSelected(selected_objects, start_layout_object, SelectionState::kStart);
    return LayoutObjectAndOffset(start_layout_object);
  }

  DCHECK(start_offset.has_value());
  DCHECK_GE(start_offset.value(), 0);
  const unsigned unsigned_offset = static_cast<unsigned>(start_offset.value());
  LayoutText* const start_layout_text = ToLayoutText(start_layout_object);
  if (unsigned_offset >= start_layout_text->TextStartOffset()) {
    // |start_offset| is within |start_layout_object| whether it has first
    // letter part or not.
    MarkSelected(selected_objects, start_layout_object, SelectionState::kStart);
    return {start_layout_text,
            static_cast<int>(unsigned_offset -
                             start_layout_text->TextStartOffset())};
  }

  // |start_layout_object| has first letter part and |start_offset| is within
  // the part.
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(start_layout_object);
  DCHECK(first_letter_part);
  MarkSelected(selected_objects, first_letter_part, SelectionState::kStart);
  MarkSelected(selected_objects, start_layout_text, SelectionState::kInside);
  return {first_letter_part, start_offset.value()};
}

LayoutObjectAndOffset MarkEnd(SelectedLayoutObjects* selected_objects,
                              LayoutObject* end_layout_object,
                              WTF::Optional<int> end_offset) {
  if (!end_layout_object->GetNode()->IsTextNode()) {
    DCHECK(!end_offset.has_value());
    MarkSelected(selected_objects, end_layout_object, SelectionState::kEnd);
    return LayoutObjectAndOffset(end_layout_object);
  }

  DCHECK(end_offset.has_value());
  DCHECK_GE(end_offset.value(), 0);
  const unsigned unsigned_offset = static_cast<unsigned>(end_offset.value());
  LayoutText* const end_layout_text = ToLayoutText(end_layout_object);
  if (unsigned_offset >= end_layout_text->TextStartOffset()) {
    // |end_offset| is within |end_layout_object| whether it has first
    // letter part or not.
    MarkSelected(selected_objects, end_layout_object, SelectionState::kEnd);
    if (LayoutTextFragment* const first_letter_part =
            FirstLetterPartFor(end_layout_object)) {
      MarkSelected(selected_objects, first_letter_part,
                   SelectionState::kInside);
    }
    return {
        end_layout_text,
        static_cast<int>(unsigned_offset - end_layout_text->TextStartOffset())};
  }

  // |end_layout_object| has first letter part and |end_offset| is within
  // the part.
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(end_layout_object);
  DCHECK(first_letter_part);
  MarkSelected(selected_objects, first_letter_part, SelectionState::kEnd);
  return {first_letter_part, end_offset.value()};
}

static NewPaintRangeAndSelectedLayoutObjects MarkStartAndEndInTwoNodes(
    SelectedLayoutObjects selected_objects,
    LayoutObject* start_layout_object,
    WTF::Optional<int> start_offset,
    LayoutObject* end_layout_object,
    WTF::Optional<int> end_offset) {
  const LayoutObjectAndOffset& start =
      MarkStart(&selected_objects, start_layout_object, start_offset);
  const LayoutObjectAndOffset& end =
      MarkEnd(&selected_objects, end_layout_object, end_offset);
  return {{start.layout_object, start.offset, end.layout_object, end.offset},
          std::move(selected_objects)};
}

static NewPaintRangeAndSelectedLayoutObjects
CalcSelectionRangeAndSetSelectionState(const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsNone())
    return {};

  const EphemeralRangeInFlatTree& selection =
      CalcSelectionInFlatTree(frame_selection);
  if (selection.IsCollapsed() || frame_selection.IsHidden())
    return {};

  // Find first/last visible LayoutObject while
  // marking SelectionState and collecting invalidation candidate LayoutObjects.
  LayoutObject* start_layout_object = nullptr;
  LayoutObject* end_layout_object = nullptr;
  SelectedLayoutObjects selected_objects;
  for (const Node& node : selection.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf())
      continue;

    if (!start_layout_object) {
      DCHECK(!end_layout_object);
      start_layout_object = end_layout_object = layout_object;
      continue;
    }

    // In this loop, |end_layout_object| is pointing current last candidate
    // LayoutObject and if it is not start and we find next, we mark the
    // current one as kInside.
    if (end_layout_object != start_layout_object)
      MarkSelectedInside(&selected_objects, end_layout_object);
    end_layout_object = layout_object;
  }

  // No valid LayOutObject found.
  if (!start_layout_object) {
    DCHECK(!end_layout_object);
    return {};
  }

  // Compute offset. It has value iff start/end is text.
  const WTF::Optional<int> start_offset = ComputeStartOffset(
      *start_layout_object, selection.StartPosition().ToOffsetInAnchor());
  const WTF::Optional<int> end_offset = ComputeEndOffset(
      *end_layout_object, selection.EndPosition().ToOffsetInAnchor());

  if (start_layout_object == end_layout_object) {
    return MarkStartAndEndInOneNode(std::move(selected_objects),
                                    start_layout_object, start_offset,
                                    end_offset);
  }
  return MarkStartAndEndInTwoNodes(std::move(selected_objects),
                                   start_layout_object, start_offset,
                                   end_layout_object, end_offset);
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  DCHECK_GE(frame_selection_->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_selection_->GetDocument().Lifecycle());
  const NewPaintRangeAndSelectedLayoutObjects& new_range =
      CalcSelectionRangeAndSetSelectionState(*frame_selection_);
  const SelectionPaintRange& new_paint_range = new_range.PaintRange();
  if (new_paint_range.IsNull()) {
    ClearSelection();
    return;
  }
  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  SetShouldInvalidateSelection(new_range, paint_range_);
  paint_range_ = new_paint_range;
  // TODO(yoichio): Remove this if state.
  // This SelectionState reassignment is ad-hoc patch for
  // prohibiting use-after-free(crbug.com/752715).
  // LayoutText::setSelectionState(state) propergates |state| to ancestor
  // LayoutObjects, which can accidentally change start/end LayoutObject state
  // then LayoutObject::IsSelectionBorder() returns false although we should
  // clear selection at LayoutObject::WillBeRemoved().
  // We should make LayoutObject::setSelectionState() trivial and remove
  // such propagation or at least do it in LayoutSelection.
  if ((paint_range_.StartLayoutObject()->GetSelectionState() !=
           SelectionState::kStart &&
       paint_range_.StartLayoutObject()->GetSelectionState() !=
           SelectionState::kStartAndEnd) ||
      (paint_range_.EndLayoutObject()->GetSelectionState() !=
           SelectionState::kEnd &&
       paint_range_.EndLayoutObject()->GetSelectionState() !=
           SelectionState::kStartAndEnd)) {
    if (paint_range_.StartLayoutObject() == paint_range_.EndLayoutObject()) {
      paint_range_.StartLayoutObject()->LayoutObject::SetSelectionState(
          SelectionState::kStartAndEnd);
    } else {
      paint_range_.StartLayoutObject()->LayoutObject::SetSelectionState(
          SelectionState::kStart);
      paint_range_.EndLayoutObject()->LayoutObject::SetSelectionState(
          SelectionState::kEnd);
    }
  }
  // TODO(yoichio): If start == end, they should be kStartAndEnd.
  // If not, start.SelectionState == kStart and vice versa.
  DCHECK(paint_range_.StartLayoutObject()->GetSelectionState() ==
             SelectionState::kStart ||
         paint_range_.StartLayoutObject()->GetSelectionState() ==
             SelectionState::kStartAndEnd);
  DCHECK(paint_range_.EndLayoutObject()->GetSelectionState() ==
             SelectionState::kEnd ||
         paint_range_.EndLayoutObject()->GetSelectionState() ==
             SelectionState::kStartAndEnd);
}

void LayoutSelection::OnDocumentShutdown() {
  has_pending_selection_ = false;
  paint_range_ = SelectionPaintRange();
}

static LayoutRect SelectionRectForLayoutObject(const LayoutObject* object) {
  if (!object->IsRooted())
    return LayoutRect();

  if (!object->CanUpdateSelectionOnRootLineBoxes())
    return LayoutRect();

  return object->SelectionRectInViewCoordinates();
}

IntRect LayoutSelection::SelectionBounds() {
  Commit();
  if (paint_range_.IsNull())
    return IntRect();

  // Create a single bounding box rect that encloses the whole selection.
  LayoutRect selected_rect;
  const SelectedLayoutObjects& current_map =
      CollectInvalidationSet(paint_range_);
  for (auto layout_object : current_map)
    selected_rect.Unite(SelectionRectForLayoutObject(layout_object));

  return PixelSnappedIntRect(selected_rect);
}

void LayoutSelection::InvalidatePaintForSelection() {
  if (paint_range_.IsNull())
    return;

  for (LayoutObject* runner : paint_range_) {
    if (runner->GetSelectionState() == SelectionState::kNone)
      continue;

    runner->SetShouldInvalidateSelection();
  }
}

void LayoutSelection::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_selection_);
}

void PrintLayoutObjectForSelection(std::ostream& ostream,
                                   LayoutObject* layout_object) {
  if (!layout_object) {
    ostream << "<null>";
    return;
  }
  ostream << (void*)layout_object << ' ' << layout_object->GetNode()
          << ", state:" << layout_object->GetSelectionState()
          << (layout_object->ShouldInvalidateSelection() ? ", ShouldInvalidate"
                                                         : ", NotInvalidate");
}
#ifndef NDEBUG
void ShowLayoutObjectForSelection(LayoutObject* layout_object) {
  std::stringstream stream;
  PrintLayoutObjectForSelection(stream, layout_object);
  LOG(INFO) << '\n' << stream.str();
}
#endif

}  // namespace blink
