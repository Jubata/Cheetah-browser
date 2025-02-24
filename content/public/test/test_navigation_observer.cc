// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class TestNavigationObserver::TestWebContentsObserver
    : public WebContentsObserver {
 public:
  TestWebContentsObserver(TestNavigationObserver* parent,
                          WebContents* web_contents)
      : WebContentsObserver(web_contents),
        parent_(parent) {
  }

 private:
  // WebContentsObserver:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    parent_->OnNavigationEntryCommitted(this, web_contents(), load_details);
  }

  void DidAttachInterstitialPage() override {
    parent_->OnDidAttachInterstitialPage(web_contents());
  }

  void WebContentsDestroyed() override {
    parent_->OnWebContentsDestroyed(this, web_contents());
  }

  void DidStartLoading() override {
    parent_->OnDidStartLoading(web_contents());
  }

  void DidStopLoading() override {
    parent_->OnDidStopLoading(web_contents());
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument())
      return;

    parent_->OnDidStartNavigation();
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    parent_->OnDidFinishNavigation(navigation_handle->IsErrorPage(),
                                   navigation_handle->GetURL(),
                                   navigation_handle->GetNetErrorCode());
  }

  TestNavigationObserver* parent_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    int number_of_navigations,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(web_contents,
                             number_of_navigations,
                             GURL(),
                             quit_mode) {}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(web_contents, 1, quit_mode) {}

TestNavigationObserver::TestNavigationObserver(
    const GURL& target_url,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(nullptr,
                             -1 /* num_of_navigations */,
                             target_url,
                             quit_mode) {}

TestNavigationObserver::~TestNavigationObserver() {
  StopWatchingNewWebContents();
}

void TestNavigationObserver::Wait() {
  message_loop_runner_->Run();
}

void TestNavigationObserver::WaitForNavigationFinished() {
  wait_event_ = WaitEvent::kNavigationFinished;
  message_loop_runner_->Run();
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void TestNavigationObserver::WatchExistingWebContents() {
  for (auto* web_contents : WebContentsImpl::GetAllWebContents())
    RegisterAsObserver(web_contents);
}

void TestNavigationObserver::RegisterAsObserver(WebContents* web_contents) {
  web_contents_observers_.insert(
      std::make_unique<TestWebContentsObserver>(this, web_contents));
}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    int number_of_navigations,
    const GURL& target_url,
    MessageLoopRunner::QuitMode quit_mode)
    : wait_event_(WaitEvent::kLoadStopped),
      navigation_started_(false),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      target_url_(target_url),
      last_navigation_succeeded_(false),
      last_net_error_code_(net::OK),
      message_loop_runner_(new MessageLoopRunner(quit_mode)),
      web_contents_created_callback_(
          base::Bind(&TestNavigationObserver::OnWebContentsCreated,
                     base::Unretained(this))) {
  if (web_contents)
    RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsCreated(WebContents* web_contents) {
  RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsDestroyed(
    TestWebContentsObserver* observer,
    WebContents* web_contents) {
  web_contents_observers_.erase(std::find_if(
      web_contents_observers_.begin(), web_contents_observers_.end(),
      [observer](const std::unique_ptr<TestWebContentsObserver>& ptr) {
        return ptr.get() == observer;
      }));
}

void TestNavigationObserver::OnNavigationEntryCommitted(
    TestWebContentsObserver* observer,
    WebContents* web_contents,
    const LoadCommittedDetails& load_details) {
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidAttachInterstitialPage(
    WebContents* web_contents) {
  // Going to an interstitial page does not trigger NavigationEntryCommitted,
  // but has the same meaning for us here.
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidStartLoading(WebContents* web_contents) {
  navigation_started_ = true;
}

void TestNavigationObserver::OnDidStopLoading(WebContents* web_contents) {
  if (!navigation_started_)
    return;

  if (wait_event_ == WaitEvent::kLoadStopped)
    EventTriggered();
}

void TestNavigationObserver::OnDidStartNavigation() {
  last_navigation_succeeded_ = false;
}

void TestNavigationObserver::OnDidFinishNavigation(bool is_error_page,
                                                   const GURL& url,
                                                   net::Error error_code) {
  last_navigation_url_ = url;
  last_navigation_succeeded_ = !is_error_page;
  last_net_error_code_ = error_code;

  if (wait_event_ == WaitEvent::kNavigationFinished)
    EventTriggered();
}

void TestNavigationObserver::EventTriggered() {
  if (target_url_ == GURL()) {
    DCHECK_GE(navigations_completed_, 0);

    ++navigations_completed_;
    if (navigations_completed_ != number_of_navigations_) {
      return;
    }
  } else if (target_url_ != last_navigation_url_) {
    return;
  }

  navigation_started_ = false;
  message_loop_runner_->Quit();
}

}  // namespace content
