// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_

#include <Foundation/Foundation.h>

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

class GURL;

namespace net {
class URLFetcherResponseWriter;
}  // namespace net

namespace web {

class DownloadTaskObserver;

// Provides API for a single browser download task. This is the model class that
// stores all the state for a download. Must be used on the UI thread.
class DownloadTask {
 public:
  // Starts the download. |writer| allows clients to perform in-memory or
  // in-file downloads and must not be null. Start() can only be called once.
  virtual void Start(std::unique_ptr<net::URLFetcherResponseWriter> writer) = 0;

  // Response writer, which was passed to Start(). Can be used to obtain the
  // download data.
  virtual net::URLFetcherResponseWriter* GetResponseWriter() const = 0;

  // Unique indentifier for this task. Also can be used to resume unfinished
  // downloads after the application relaunch (see example in DownloadController
  // class comments).
  virtual NSString* GetIndentifier() const = 0;

  // The URL that the download request originally attempted to fetch. This may
  // differ from the final download URL if there were redirects.
  virtual const GURL& GetOriginalUrl() const = 0;

  // Returns true if the download is in a terminal state. This includes
  // completed downloads, cancelled downloads, and interrupted downloads that
  // can't be resumed.
  virtual bool IsDone() const = 0;

  // Error code for this download task. 0 if the download is still in progress
  // or the download has sucessfully completed. See net_errors.h for the
  // possible error codes.
  virtual int GetErrorCode() const = 0;

  // Total number of expected bytes (a best-guess upper-bound). Returns -1 if
  // the total size is unknown.
  virtual int64_t GetTotalBytes() const = 0;

  // Rough percent complete. Returns -1 if progress is unknown. 100 if the
  // download is already complete.
  virtual int GetPercentComplete() const = 0;

  // Content-Disposition header value from HTTP response.
  virtual std::string GetContentDisposition() const = 0;

  // Effective MIME type of downloaded content.
  virtual std::string GetMimeType() const = 0;

  // Suggested name for the downloaded file.
  virtual base::string16 GetSuggestedFilename() const = 0;

  // Adds and Removes DownloadTaskObserver. Clients must remove self from
  // observers before the task is destroyed.
  virtual void AddObserver(DownloadTaskObserver* observer) = 0;
  virtual void RemoveObserver(DownloadTaskObserver* observer) = 0;

  DownloadTask() = default;
  virtual ~DownloadTask() = default;

  DISALLOW_COPY_AND_ASSIGN(DownloadTask);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_
