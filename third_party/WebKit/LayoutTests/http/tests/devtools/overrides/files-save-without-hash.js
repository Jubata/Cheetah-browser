// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Ensures iframes are overridable if overrides are setup.\n`);
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.loadModule('sources');

  var fileSystemPath = 'file:///tmp/';

  var fileSystem = await createFileSystem();
  var project = Workspace.workspace.project(fileSystem.fileSystemPath);
  // Using data url because about:blank does not trigger onload.
  await TestRunner.addIframe('data:,', { id: 'test-iframe' });

  await testFileName('resources/bar.js');
  await testFileName('resources/a space/bar.js');
  await testFileName('resources/bar.js?#hello');
  await testFileName('resources/bar.js?params');
  await testFileName('resources/bar.js?params&and=more&pa&ra?ms');
  await testFileName('resources/bar2.js?params&and=more&pa&ra?ms#hello?with&params');
  await testFileName('resources/no-extension');
  await testFileName('resources/foo&with%20some*bad%5EC!h%7Ba...r,acter%%s/file&with?s%20t^rang@e~character%27S');
  await testFileName('resources/'); // Should be index.html

  TestRunner.completeTest();


  async function testFileName(url) {
    TestRunner.addResult('Creating UISourcecode for url: ' + url);
    TestRunner.evaluateInPagePromise(`document.getElementById('test-iframe').src = '${url}'`)
    var networkUISourceCode = await TestRunner.waitForEvent(
        Workspace.Workspace.Events.UISourceCodeAdded, Workspace.workspace,
        uiSourceCode => uiSourceCode.url().startsWith('http'));
    if (!networkUISourceCode) {
      testRunner.addResult('ERROR: No uiSourceCode');
      testRunner.completeTest();
      return;
    }
    TestRunner.addResult('Found network UISourceCode: ' + networkUISourceCode.url());

    TestRunner.addResult('Saving network UISourceCode');
    Persistence.networkPersistenceManager.saveUISourceCodeForOverrides(networkUISourceCode);
    var newFile = await waitForNextCreatedFile();
    TestRunner.addResult('Created File: ' + newFile);
    TestRunner.addResult('');
  }

  async function waitForNextCreatedFile() {
    return new Promise(result => {
      TestRunner.addSniffer(
          Persistence.networkPersistenceManager, '_fileCreatedForTest',
          (path, name) => result(path + '/' + name), false);
    });
  }

  async function createFileSystem() {
    var project = await BindingsTestRunner.createOverrideProject(fileSystemPath);
    BindingsTestRunner.setOverridesEnabled(true);
    Persistence.networkPersistenceManager.addFileSystemOverridesProject(
        Persistence.NetworkPersistenceManager.inspectedPageDomain(), project);

    var fileSystem = InspectorFrontendHost.isolatedFileSystem(fileSystemPath);
    if (!fileSystem) {
      testRunner.addResult('ERROR: Expected filesystem with path: ' + fileSystemPath);
      testRunner.completeTest();
      return;
    }
    return fileSystem;
  }
})();
