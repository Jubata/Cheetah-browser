// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_VOICE_SEARCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_VOICE_SEARCH_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for chrome://voicesearch.
class VoiceSearchUI : public content::WebUIController {
 public:
  explicit VoiceSearchUI(content::WebUI* web_ui);
  ~VoiceSearchUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VoiceSearchUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_VOICE_SEARCH_UI_H_
