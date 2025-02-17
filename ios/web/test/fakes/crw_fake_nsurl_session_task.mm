// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWFakeNSURLSessionTask ()
// NSURLSessionTask properties.
@property(nullable, readonly, copy) NSURLRequest* originalRequest;
@property(nullable, readonly, copy) NSURLRequest* currentRequest;
@end

@implementation CRWFakeNSURLSessionTask

@synthesize countOfBytesReceived = _countOfBytesReceived;
@synthesize countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
@synthesize state = _state;
@synthesize originalRequest = _originalRequest;
@synthesize currentRequest = _currentRequest;

- (instancetype)initWithURL:(NSURL*)URL {
  if ((self = [super init])) {
    _state = NSURLSessionTaskStateSuspended;
    _currentRequest = [NSURLRequest requestWithURL:URL];
    _originalRequest = [NSURLRequest requestWithURL:URL];
  }
  return self;
}

- (void)cancel {
  self.state = NSURLSessionTaskStateCanceling;
}
- (void)resume {
  self.state = NSURLSessionTaskStateRunning;
}

// A private method, called by -[NSHTTPCookieStorage storeCookies:forTask:].
// Requires stubbing in order to use NSHTTPCookieStorage API.
- (NSString*)_storagePartitionIdentifier {
  return nil;
}

@end
