// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/text_elider.h"
#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(JavaScriptDialogTabHelper);

namespace {

app_modal::JavaScriptDialogManager* AppModalDialogManager() {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

bool IsWebContentsForemost(content::WebContents* web_contents) {
#if defined(OS_ANDROID)
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && tab->IsUserInteractable();
#else
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
#endif
}

}  // namespace

enum class JavaScriptDialogTabHelper::DismissalCause {
  // This is used for a UMA histogram. Please never alter existing values, only
  // append new ones.
  TAB_HELPER_DESTROYED = 0,
  SUBSEQUENT_DIALOG_SHOWN = 1,
  HANDLE_DIALOG_CALLED = 2,
  CANCEL_DIALOGS_CALLED = 3,
  TAB_HIDDEN = 4,
  BROWSER_SWITCHED = 5,
  DIALOG_BUTTON_CLICKED = 6,
  TAB_NAVIGATED = 7,
  TAB_SWITCHED_OUT = 8,
  MAX,
};

JavaScriptDialogTabHelper::JavaScriptDialogTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

JavaScriptDialogTabHelper::~JavaScriptDialogTabHelper() {
  CloseDialog(DismissalCause::TAB_HELPER_DESTROYED, false, base::string16());
}

void JavaScriptDialogTabHelper::SetDialogShownCallbackForTesting(
    base::OnceClosure callback) {
  dialog_shown_ = std::move(callback);
}

bool JavaScriptDialogTabHelper::IsShowingDialogForTesting() const {
  return !!dialog_;
}

void JavaScriptDialogTabHelper::RunJavaScriptDialog(
    content::WebContents* alerting_web_contents,
    const GURL& alerting_frame_url,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  content::WebContents* parent_web_contents =
      WebContentsObserver::web_contents();
  bool foremost = IsWebContentsForemost(parent_web_contents);
  navigation_metrics::Scheme scheme =
      navigation_metrics::GetScheme(alerting_frame_url);
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Alert", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Alert", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Confirm", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Confirm", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_BOOLEAN("JSDialogs.IsForemost.Prompt", foremost);
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.Scheme.Prompt", scheme,
                                navigation_metrics::Scheme::COUNT);
      break;
  }

  // Close any dialog already showing.
  CloseDialog(DismissalCause::SUBSEQUENT_DIALOG_SHOWN, false, base::string16());

  bool make_pending = false;
  if (!IsWebContentsForemost(parent_web_contents)) {
    switch (dialog_type) {
      case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
        // When an alert fires in the background, make the callback so that the
        // render process can continue.
        std::move(callback).Run(true, base::string16());
        callback.Reset();

        SetTabNeedsAttention(true);

        make_pending = true;
        break;
      }
      case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
        // TODO(avi): Remove confirm() dialogs from non-foremost tabs, just like
        // has been done for prompt() dialogs.
        break;
      }
      case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
        *did_suppress_message = true;
        alerting_web_contents->GetMainFrame()->AddMessageToConsole(
            content::CONSOLE_MESSAGE_LEVEL_WARNING,
            "A window.prompt() dialog generated by this page was suppressed "
            "because this page is not the active tab of the front window. "
            "Please make sure your dialogs are triggered by user interactions "
            "to avoid this situation. "
            "https://www.chromestatus.com/feature/5637107137642496");
        return;
      }
    }
  }

  // Enforce sane sizes. ElideRectangleString breaks horizontally, which isn't
  // strictly needed, but it restricts the vertical size, which is crucial.
  // This gives about 2000 characters, which is about the same as the
  // AppModalDialogManager provides, but allows no more than 24 lines.
  const int kMessageTextMaxRows = 24;
  const int kMessageTextMaxCols = 80;
  const size_t kDefaultPromptMaxSize = 2000;
  base::string16 truncated_message_text;
  gfx::ElideRectangleString(message_text, kMessageTextMaxRows,
                            kMessageTextMaxCols, false,
                            &truncated_message_text);
  base::string16 truncated_default_prompt_text;
  gfx::ElideString(default_prompt_text, kDefaultPromptMaxSize,
                   &truncated_default_prompt_text);

  base::string16 title = AppModalDialogManager()->GetTitle(
      alerting_web_contents, alerting_frame_url);
  dialog_callback_ = std::move(callback);
  dialog_type_ = dialog_type;
  if (make_pending) {
    DCHECK(!dialog_);
    pending_dialog_ =
        base::BindOnce(&JavaScriptDialog::Create, parent_web_contents,
                       alerting_web_contents, title, dialog_type,
                       truncated_message_text, truncated_default_prompt_text,
                       base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                                      base::Unretained(this),
                                      DismissalCause::DIALOG_BUTTON_CLICKED));
  } else {
    DCHECK(!pending_dialog_);
    dialog_ = JavaScriptDialog::Create(
        parent_web_contents, alerting_web_contents, title, dialog_type,
        truncated_message_text, truncated_default_prompt_text,
        base::BindOnce(&JavaScriptDialogTabHelper::CloseDialog,
                       base::Unretained(this),
                       DismissalCause::DIALOG_BUTTON_CLICKED));
  }

#if !defined(OS_ANDROID)
  BrowserList::AddObserver(this);
#endif

  // Message suppression is something that we don't give the user a checkbox
  // for any more. It was useful back in the day when dialogs were app-modal
  // and clicking the checkbox was the only way to escape a loop that the page
  // was doing, but now the user can just close the page.
  *did_suppress_message = false;

  if (!dialog_shown_.is_null())
    std::move(dialog_shown_).Run();

  if (did_suppress_message) {
    UMA_HISTOGRAM_COUNTS("JSDialogs.CharacterCountUserSuppressed",
                         message_text.length());
  }
}

void JavaScriptDialogTabHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    bool is_reload,
    DialogClosedCallback callback) {
  // onbeforeunload dialogs are always handled with an app-modal dialog, because
  // - they are critical to the user not losing data
  // - they can be requested for tabs that are not foremost
  // - they can be requested for many tabs at the same time
  // and therefore auto-dismissal is inappropriate for them.

  return AppModalDialogManager()->RunBeforeUnloadDialog(web_contents, is_reload,
                                                        std::move(callback));
}

bool JavaScriptDialogTabHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  if (dialog_ || pending_dialog_) {
    CloseDialog(DismissalCause::HANDLE_DIALOG_CALLED, accept,
                prompt_override ? *prompt_override : dialog_->GetUserInput());
    return true;
  }

  // Handle any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->HandleJavaScriptDialog(web_contents, accept,
                                                         prompt_override);
}

void JavaScriptDialogTabHelper::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  CloseDialog(DismissalCause::CANCEL_DIALOGS_CALLED, false, base::string16());

  // Cancel any app-modal dialogs being run by the app-modal dialog system.
  return AppModalDialogManager()->CancelDialogs(web_contents, reset_state);
}

void JavaScriptDialogTabHelper::WasShown() {
  if (pending_dialog_) {
    dialog_ = std::move(pending_dialog_).Run();
    pending_dialog_.Reset();

    SetTabNeedsAttention(false);
  }
}

void JavaScriptDialogTabHelper::WasHidden() {
  HandleTabSwitchAway(DismissalCause::TAB_HIDDEN);
}

// This function handles the case where browser-side navigation (PlzNavigate) is
// enabled. DidStartNavigationToPendingEntry, below, handles the case where
// PlzNavigate is not enabled. TODO(avi): When the non-PlzNavigate code is
// removed, remove DidStartNavigationToPendingEntry.
void JavaScriptDialogTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Close the dialog if the user started a new navigation. This allows reloads
  // and history navigations to proceed.
  CloseDialog(DismissalCause::TAB_NAVIGATED, false, base::string16());
}

// This function handles the case where browser-side navigation (PlzNavigate) is
// not enabled. DidStartNavigation, above, handles the case where PlzNavigate is
// enabled. TODO(avi): When the non-PlzNavigate code is removed, remove
// DidStartNavigationToPendingEntry.
void JavaScriptDialogTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::ReloadType reload_type) {
  // Close the dialog if the user started a new navigation. This allows reloads
  // and history navigations to proceed.
  CloseDialog(DismissalCause::TAB_NAVIGATED, false, base::string16());
}

#if !defined(OS_ANDROID)
void JavaScriptDialogTabHelper::OnBrowserSetLastActive(Browser* browser) {
  if (IsWebContentsForemost(web_contents())) {
    WasShown();
  } else {
    HandleTabSwitchAway(DismissalCause::BROWSER_SWITCHED);
  }
}

void JavaScriptDialogTabHelper::TabReplacedAt(
    TabStripModel* tab_strip_model,
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index) {
  if (old_contents == WebContentsObserver::web_contents()) {
    // At this point, this WebContents is no longer in the tabstrip. The usual
    // teardown will not be able to turn off the attention indicator, so that
    // must be done here.
    SetTabNeedsAttentionImpl(false, tab_strip_model, index);

    CloseDialog(DismissalCause::TAB_SWITCHED_OUT, false, base::string16());
  }
}
#endif

void JavaScriptDialogTabHelper::LogDialogDismissalCause(
    JavaScriptDialogTabHelper::DismissalCause cause) {
  switch (dialog_type_) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Alert",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Confirm",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      UMA_HISTOGRAM_ENUMERATION("JSDialogs.DismissalCause.Prompt",
                                static_cast<int>(cause),
                                static_cast<int>(DismissalCause::MAX));
      break;
  }
}

void JavaScriptDialogTabHelper::HandleTabSwitchAway(DismissalCause cause) {
  if (!dialog_)
    return;

  if (dialog_type_ == content::JAVASCRIPT_DIALOG_TYPE_ALERT) {
    // When the user switches tabs, make the callback so that the render process
    // can continue.
    if (dialog_callback_) {
      std::move(dialog_callback_).Run(true, base::string16());
      dialog_callback_.Reset();
    }
  } else {
    CloseDialog(cause, false, base::string16());
  }
}

void JavaScriptDialogTabHelper::CloseDialog(DismissalCause cause,
                                            bool success,
                                            const base::string16& user_input) {
  if (!dialog_ && !pending_dialog_)
    return;

  LogDialogDismissalCause(cause);

  // CloseDialog() can be called two ways. It can be called from within
  // JavaScriptDialogTabHelper, in which case the dialog needs to be closed.
  // However, it can also be called, bound, from the JavaScriptDialog. In that
  // case, the dialog is already closing, so the JavaScriptDialog doesn't need
  // to be told to close.
  //
  // Using the |cause| to distinguish a call from JavaScriptDialog vs from
  // within JavaScriptDialogTabHelper is a bit hacky, but is the simplest way.
  if (dialog_ && cause != DismissalCause::DIALOG_BUTTON_CLICKED)
    dialog_->CloseDialogWithoutCallback();

  // If there is a callback, call it. There might not be one, if a tab-modal
  // alert() dialog is showing.
  if (dialog_callback_)
    std::move(dialog_callback_).Run(success, user_input);

  // If there's a pending dialog, then the tab is still in the "needs attention"
  // state; clear it out. However, if the tab was switched out, the turning off
  // of the "needs attention" state was done in TabReplacedAt() because
  // SetTabNeedsAttention won't work, so don't call it.
  if (pending_dialog_ && cause != DismissalCause::TAB_SWITCHED_OUT)
    SetTabNeedsAttention(false);

  dialog_.reset();
  pending_dialog_.Reset();
  dialog_callback_.Reset();

#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#endif
}

void JavaScriptDialogTabHelper::SetTabNeedsAttention(bool attention) {
#if !defined(OS_ANDROID)
  content::WebContents* web_contents = WebContentsObserver::web_contents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  SetTabNeedsAttentionImpl(
      attention, tab_strip_model,
      tab_strip_model->GetIndexOfWebContents(web_contents));
#endif
}

#if !defined(OS_ANDROID)
void JavaScriptDialogTabHelper::SetTabNeedsAttentionImpl(
    bool attention,
    TabStripModel* tab_strip_model,
    int index) {
  tab_strip_model->SetTabNeedsAttentionAt(index, attention);
  if (attention)
    tab_strip_model->AddObserver(this);
  else
    tab_strip_model->RemoveObserver(this);
}
#endif
