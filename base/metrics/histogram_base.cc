// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"

#include <limits.h>

#include <memory>
#include <set>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

namespace base {

std::string HistogramTypeToString(HistogramType type) {
  switch (type) {
    case HISTOGRAM:
      return "HISTOGRAM";
    case LINEAR_HISTOGRAM:
      return "LINEAR_HISTOGRAM";
    case BOOLEAN_HISTOGRAM:
      return "BOOLEAN_HISTOGRAM";
    case CUSTOM_HISTOGRAM:
      return "CUSTOM_HISTOGRAM";
    case SPARSE_HISTOGRAM:
      return "SPARSE_HISTOGRAM";
  }
  NOTREACHED();
  return "UNKNOWN";
}

HistogramBase* DeserializeHistogramInfo(PickleIterator* iter) {
  int type;
  if (!iter->ReadInt(&type))
    return nullptr;

  switch (type) {
    case HISTOGRAM:
      return Histogram::DeserializeInfoImpl(iter);
    case LINEAR_HISTOGRAM:
      return LinearHistogram::DeserializeInfoImpl(iter);
    case BOOLEAN_HISTOGRAM:
      return BooleanHistogram::DeserializeInfoImpl(iter);
    case CUSTOM_HISTOGRAM:
      return CustomHistogram::DeserializeInfoImpl(iter);
    case SPARSE_HISTOGRAM:
      return SparseHistogram::DeserializeInfoImpl(iter);
    default:
      return nullptr;
  }
}

const HistogramBase::Sample HistogramBase::kSampleType_MAX = INT_MAX;

HistogramBase::HistogramBase(const char* name)
    : histogram_name_(name), flags_(kNoFlags) {}

HistogramBase::~HistogramBase() {}

void HistogramBase::CheckName(const StringPiece& name) const {
  DCHECK_EQ(StringPiece(histogram_name()), name);
}

void HistogramBase::SetFlags(int32_t flags) {
  HistogramBase::Count old_flags = subtle::NoBarrier_Load(&flags_);
  subtle::NoBarrier_Store(&flags_, old_flags | flags);
}

void HistogramBase::ClearFlags(int32_t flags) {
  HistogramBase::Count old_flags = subtle::NoBarrier_Load(&flags_);
  subtle::NoBarrier_Store(&flags_, old_flags & ~flags);
}

void HistogramBase::AddTime(const TimeDelta& time) {
  Add(static_cast<Sample>(time.InMilliseconds()));
}

void HistogramBase::AddBoolean(bool value) {
  Add(value ? 1 : 0);
}

void HistogramBase::SerializeInfo(Pickle* pickle) const {
  pickle->WriteInt(GetHistogramType());
  SerializeInfoImpl(pickle);
}

uint32_t HistogramBase::FindCorruption(const HistogramSamples& samples) const {
  // Not supported by default.
  return NO_INCONSISTENCIES;
}

bool HistogramBase::ValidateHistogramContents(bool crash_if_invalid,
                                              int corrupted_count) const {
  return true;
}

void HistogramBase::WriteJSON(std::string* output) const {
  Count count;
  int64_t sum;
  std::unique_ptr<ListValue> buckets(new ListValue());
  GetCountAndBucketData(&count, &sum, buckets.get());
  std::unique_ptr<DictionaryValue> parameters(new DictionaryValue());
  GetParameters(parameters.get());

  JSONStringValueSerializer serializer(output);
  DictionaryValue root;
  root.SetString("name", histogram_name());
  root.SetInteger("count", count);
  root.SetDouble("sum", static_cast<double>(sum));
  root.SetInteger("flags", flags());
  root.Set("params", std::move(parameters));
  root.Set("buckets", std::move(buckets));
  root.SetInteger("pid", GetUniqueIdForProcess());
  serializer.Serialize(root);
}

void HistogramBase::FindAndRunCallback(HistogramBase::Sample sample) const {
  if ((flags() & kCallbackExists) == 0)
    return;

  StatisticsRecorder::OnSampleCallback cb =
      StatisticsRecorder::FindCallback(histogram_name());
  if (!cb.is_null())
    cb.Run(sample);
}

void HistogramBase::WriteAsciiBucketGraph(double current_size,
                                          double max_size,
                                          std::string* output) const {
  const int k_line_length = 72;  // Maximal horizontal width of graph.
  int x_count = static_cast<int>(k_line_length * (current_size / max_size)
                                 + 0.5);
  int x_remainder = k_line_length - x_count;

  while (0 < x_count--)
    output->append("-");
  output->append("O");
  while (0 < x_remainder--)
    output->append(" ");
}

const std::string HistogramBase::GetSimpleAsciiBucketRange(
    Sample sample) const {
  return StringPrintf("%d", sample);
}

void HistogramBase::WriteAsciiBucketValue(Count current,
                                          double scaled_sum,
                                          std::string* output) const {
  StringAppendF(output, " (%d = %3.1f%%)", current, current/scaled_sum);
}

// static
char const* HistogramBase::GetPermanentName(const std::string& name) {
  // A set of histogram names that provides the "permanent" lifetime required
  // by histogram objects for those strings that are not already code constants
  // or held in persistent memory.
  static LazyInstance<std::set<std::string>>::Leaky permanent_names;
  static LazyInstance<Lock>::Leaky permanent_names_lock;

  AutoLock lock(permanent_names_lock.Get());
  auto result = permanent_names.Get().insert(name);
  return result.first->c_str();
}

}  // namespace base
