// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "components/variations/variations_associated_data.h"
#include "url/origin.h"

namespace {

// Field trial strings.
const char kSRTCanaryGroup[] = "SRTCanary";
const char kSRTPromptOffGroup[] = "Off";
const char kSRTPromptSeedParam[] = "Seed";

const char kSRTElevationTrial[] = "SRTElevation";
const char kSRTElevationAsNeededGroup[] = "AsNeeded";

const char kSRTReporterTrial[] = "srt_reporter";
const char kSRTReporterOffGroup[] = "Off";

const char kDownloadRootPath[] =
    "https://dl.google.com/dl/softwareremovaltool/win/";

// The download links of the Software Removal Tool.
const char kMainSRTDownloadURL[] =
    "https://dl.google.com/dl"
    "/softwareremovaltool/win/chrome_cleanup_tool.exe?chrome-prompt=1";
const char kCanarySRTDownloadURL[] =
    "https://dl.google.com/dl"
    "/softwareremovaltool/win/c/chrome_cleanup_tool.exe?chrome-prompt=1";

}  // namespace

namespace safe_browsing {

const char kSRTPromptTrial[] = "SRTPromptFieldTrial";

const base::Feature kRebootPromptDialogFeature{
    "RebootPromptDialog", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserInitiatedChromeCleanupsFeature{
    "UserInitiatedChromeCleanups", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCleanerDownloadFeature{"DownloadCleanupToolByBitness",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsInSRTPromptFieldTrialGroups() {
  return !base::StartsWith(base::FieldTrialList::FindFullName(kSRTPromptTrial),
                           kSRTPromptOffGroup, base::CompareCase::SENSITIVE);
}

bool SRTPromptNeedsElevationIcon() {
  return !base::StartsWith(
      base::FieldTrialList::FindFullName(kSRTElevationTrial),
      kSRTElevationAsNeededGroup, base::CompareCase::SENSITIVE);
}

bool UserInitiatedCleanupsEnabled() {
  return base::FeatureList::IsEnabled(kUserInitiatedChromeCleanupsFeature);
}

bool IsSwReporterEnabled() {
  return !base::StartsWith(
      base::FieldTrialList::FindFullName(kSRTReporterTrial),
      kSRTReporterOffGroup, base::CompareCase::SENSITIVE);
}

GURL GetLegacyDownloadURL() {
  if (base::StartsWith(base::FieldTrialList::FindFullName(kSRTPromptTrial),
                       kSRTCanaryGroup, base::CompareCase::SENSITIVE)) {
    return GURL(kCanarySRTDownloadURL);
  }
  return GURL(kMainSRTDownloadURL);
}

GURL GetSRTDownloadURL() {
  constexpr char kDownloadGroupParam[] = "download_group";
  const std::string download_group = base::GetFieldTrialParamValueByFeature(
      kCleanerDownloadFeature, kDownloadGroupParam);
  if (download_group.empty())
    return GetLegacyDownloadURL();

  std::string architecture = base::win::OSInfo::GetInstance()->architecture() ==
                                     base::win::OSInfo::X86_ARCHITECTURE
                                 ? "x86"
                                 : "x64";

  // Construct download URL using the following pattern:
  // https://dl.google.com/.../win/{arch}/{group}/chrome_cleanup_tool.exe
  std::string download_url_str = std::string(kDownloadRootPath) + architecture +
                                 "/" + download_group +
                                 "/chrome_cleanup_tool.exe?chrome-prompt=1";
  GURL download_url(download_url_str);

  // Ensure URL construction didn't change origin.
  const GURL download_root(kDownloadRootPath);
  const url::Origin known_good_origin = url::Origin::Create(download_root);
  url::Origin current_origin = url::Origin::Create(download_url);
  if (!current_origin.IsSameOriginWith(known_good_origin))
    return GetLegacyDownloadURL();

  return download_url;
}

std::string GetIncomingSRTSeed() {
  return variations::GetVariationParamValue(kSRTPromptTrial,
                                            kSRTPromptSeedParam);
}

std::string GetSRTFieldTrialGroupName() {
  return base::FieldTrialList::FindFullName(kSRTPromptTrial);
}

bool IsRebootPromptModal() {
  constexpr char kIsModalParam[] = "modal_reboot_prompt";
  return base::FeatureList::IsEnabled(kRebootPromptDialogFeature) &&
         base::GetFieldTrialParamByFeatureAsBool(kRebootPromptDialogFeature,
                                                 kIsModalParam,
                                                 /*default_value=*/false);
}

void RecordPromptShownWithTypeHistogram(PromptTypeHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.PromptShownWithType", value,
                            PROMPT_TYPE_MAX);
}

void RecordPromptNotShownWithReasonHistogram(
    NoPromptReasonHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.NoPromptReason", value,
                            NO_PROMPT_REASON_MAX);
}

}  // namespace safe_browsing
