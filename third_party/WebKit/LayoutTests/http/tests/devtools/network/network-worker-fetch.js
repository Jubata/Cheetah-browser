// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests fetch in worker.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  NetworkTestRunner.makeFetchInWorker('resource.php', {}, fetchCallback);

  function fetchCallback(result) {
    TestRunner.addResult('Fetch in worker result: ' + result.value);

    var requests = NetworkTestRunner.networkRequests();
    requests.forEach((request) => {
      TestRunner.addResult(request.url());
      TestRunner.addResult('resource.type: ' + request.resourceType());
      TestRunner.addResult('request.failed: ' + !!request.failed);
    });

    TestRunner.completeTest();
  }
})();
