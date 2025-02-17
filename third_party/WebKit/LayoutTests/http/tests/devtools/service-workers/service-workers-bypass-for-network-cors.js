// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox works with CORS requests. crbug.com/771742\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
      <p>Tests &quot;Bypass for network&quot; checkbox works with CORS requests. crbug.com/771742</p>
    `);
  await TestRunner.evaluateInPagePromise(`
      function takeInterceptedRequests(scope) {
        return new Promise((resolve) => {
            let channel = new MessageChannel();
            channel.port1.onmessage = msg => { resolve(JSON.stringify(msg.data)); };
            registrations[scope].active.postMessage({port: channel.port2}, [channel.port2]);
          });
      }

      function fetchInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow
            .fetch(url, {mode: 'cors'}).then((r) => r.text());
      }
      function xhrInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow.xhr(url);
      }
      function corsImageInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow.load_cors_image(url);
      }
  `);

  const scriptURL =
      'http://127.0.0.1:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-worker.js';
  const scope =
      'http://127.0.0.1:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-iframe.html';
  const target =
      'http://localhost:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-target.php';
  const frameId = 'frame_id';

  function dumpInterceptedRequests() {
    return TestRunner.callFunctionInPageAsync('takeInterceptedRequests', [scope]).then((data) => {
      TestRunner.addResult('Intercepted requests:');
      JSON.parse(data.value).forEach((request) => {
        TestRunner.addResult(' url: ' + request.url);
        TestRunner.addResult(' mode: ' + request.mode);
      });
    });
  }

  function testCorsRequests(index) {
    TestRunner.addResult('CORS fetch(): ' + index);
    return TestRunner.callFunctionInPageAsync('fetchInIframe', [target + '?type=txt&fetch' + index, frameId])
        .then((data) => {
          if (data.value !== 'hello') {
            TestRunner.addResult('fetch response miss match: ' + data.value);
          }
          TestRunner.addResult('CORS XHR: ' + index);
          return TestRunner.callFunctionInPageAsync('xhrInIframe', [target + '?type=txt&xhr' + index, frameId]);
        })
        .then((data) => {
          if (data.value !== 'hello') {
            TestRunner.addResult('XHR response miss match: ' + data.value);
          }
          TestRunner.addResult('CORS image: ' + index);
          return TestRunner.callFunctionInPageAsync('corsImageInIframe', [target + '?type=img&img' + index, frameId]);
        });
  }

  ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
      .then(_ => ApplicationTestRunner.waitForActivated(scope))
      .then(() => {
        TestRunner.addResult('Loading an iframe.');
        return TestRunner.addIframe(scope, {id: frameId});
      })
      .then(() => {
        TestRunner.addResult('The iframe loaded.');
        return testCorsRequests('1');
      })
      .then(() => {
        TestRunner.addResult('Enable bypassServiceWorker');
        Common.settings.settingForTest('bypassServiceWorker').set(true);
        return testCorsRequests('2');
      })
      .then(() => {
        TestRunner.addResult('Disable bypassServiceWorker');
        Common.settings.settingForTest('bypassServiceWorker').set(false);
        return testCorsRequests('3');
      })
      .then(() => {
        return dumpInterceptedRequests();
      })
      .then(() => {
        TestRunner.completeTest();
      });
})();
