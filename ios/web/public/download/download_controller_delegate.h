// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_DELEGATE_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/macros.h"

namespace web {

class DownloadController;
class DownloadTask;
class WebState;

// DownloadController delegate. All methods are called on UI thread.
class DownloadControllerDelegate {
 public:
  // Called when renderer-initiated download was created or when client is
  // resuming after the application relaunch by calling
  // DownloadController::CreateDownloadTask().
  //
  // Renderer-initiated download or download created with
  // DownloadController::CreateDownloadTask() call does not start automatically.
  // If the client wants to start the download it should call
  // DownloadTask::Start().
  // Clients may call DownloadTask::GetSuggestedFilename() to get the filename
  // for the download and DownloadTask::GetTotalBytes() to get the estimated
  // size.
  virtual void OnDownloadCreated(DownloadController* download_controller,
                                 const WebState* web_state,
                                 std::unique_ptr<DownloadTask> task) {}

  // Called when DownloadController is about to be destroyed. Delegate should
  // be set to null inside this method. All started DownloadTasks will stop the
  // download. Clients should not call DownloadTask::Start() on the remaining
  // alive tasks.
  virtual void OnDownloadControllerDestroyed(
      DownloadController* download_controller) {}

  DownloadControllerDelegate() = default;
  virtual ~DownloadControllerDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadControllerDelegate);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_DELEGATE_H_
