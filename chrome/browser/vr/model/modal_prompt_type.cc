// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/modal_prompt_type.h"

#include "base/logging.h"

namespace vr {

UiUnsupportedMode GetReasonForPrompt(ModalPromptType prompt) {
  switch (prompt) {
    case kModalPromptTypeExitVRForSiteInfo:
      return UiUnsupportedMode::kUnhandledPageInfo;
    case kModalPromptTypeExitVRForAudioPermission:
      return UiUnsupportedMode::kAndroidPermissionNeeded;
    case kModalPromptTypeNone:
      return UiUnsupportedMode::kCount;
  }
  NOTREACHED();
  return UiUnsupportedMode::kCount;
}

}  // namespace vr
