// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that Debugger.getFunctionDetails command returns correct location. Bug 71808\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
        <p>Tests that Debugger.getFunctionDetails command returns correct location.
        <a href="https://bugs.webkit.org/show_bug.cgi?id=71808">Bug 71808</a>
        </p>
      `);
  await TestRunner.evaluateInPagePromise(`
        function firstLineFunction()

        {
        }

        function notFirstLineFunction()

        {
        }

        var obj = {
            m: function() {}
        }

        function functionWithDisplayName() {}
        functionWithDisplayName.displayName = "user-friendly name";

        function functionWithDisplayNameGetter() {}
        functionWithDisplayNameGetter.__defineGetter__("displayName", function() { return "FAIL_should_not_execute"; });

        var smallClosure = (function(p) { return function() { return p; }; })("Capybara");

        var bigClosure = (function(p) {
            var o = {
               e: 7,
               f: 5,
               get u() { return 3; },
               set v(value) { }
            };
            with (o) {
                try {
                    throw Error("Test");
                } catch (ex) {
                    return function() {
                        return String(p) + String(ex) + u + e;
                    };
                }
            }
        })({});

        function* gen() { yield [1,2,3] }
    `);

  function dumpFunctionDetails(properties) {
    var location = properties.get('[[FunctionLocation]]').value.value;
    TestRunner.addResult('Function details: ');
    TestRunner.addResult('lineNumber: ' + location.lineNumber);
    TestRunner.addResult('columnNumber: ' + location.columnNumber);
    TestRunner.addResult('scriptId is valid: ' + !!location.scriptId);
    TestRunner.addResult('functionName: ' + properties.get('name').value.value);
    if (properties.has('[[IsGenerator]]'))
      TestRunner.addResult('isGenerator: ' + properties.get('[[IsGenerator]]').value.value);
  }

  function dumpFunctionNoScopes() {
    TestRunner.addResult('scopeChain: n/a');
  }

  function dumpFunctionScope(pos, type, propertyDescriptors) {
    var variables;
    if (type == 'Global') {
      variables = '<global object properties omitted>';
    } else {
      var varArray = [];
      for (var i = 0; i < propertyDescriptors.length; i++) {
        var d = propertyDescriptors[i];
        var valueStr;
        if (d.value) {
          if (d.value.value)
            valueStr = JSON.stringify(d.value.value);
          else
            valueStr = '<no string representation>';
        } else {
          valueStr = '<no value>';
        }
        varArray.push(d.name + ': ' + valueStr);
      }
      varArray.sort();
      variables = varArray.join();
    }
    TestRunner.addResult('scopeChain #' + pos + ': ' + type + '; ' + variables);
  }

  async function loadAndDumpScopeObjects(scopeChain, end) {
    if (!scopeChain) {
      dumpFunctionNoScopes();
      end();
      return;
    }

    var properties = await TestRunner.RuntimeAgent.getProperties(scopeChain.value.objectId, true);
    var scopes = [];
    for (var prop of properties) {
      if (String(prop.name >>> 0) === prop.name)
        scopes.push(prop.value);
    }

    for (var pos = 0; pos < scopes.length; ++pos) {
      var propertyDescriptors = await TestRunner.RuntimeAgent.getProperties(scopes[pos].objectId, true);
      dumpFunctionScope(pos, scopes[pos].description, propertyDescriptors);
    }

    end();
  }

  function performStandardTestCase(pageExpression, next) {
    TestRunner.evaluateInPage(pageExpression, didEvaluate);

    async function didEvaluate(remote) {
      TestRunner.addResult(pageExpression + ' type = ' + remote.type);
      var response =
          await TestRunner.RuntimeAgent.invoke_getProperties({objectId: remote.objectId, isOwnProperty: false});

      var propertiesMap = new Map();
      for (var prop of response.internalProperties)
        propertiesMap.set(prop.name, prop);
      for (var prop of response.result) {
        if (prop.name === 'name' && prop.value && prop.value.type === 'string')
          propertiesMap.set('name', prop);
        if (prop.name === 'displayName' && prop.value && prop.value.type === 'string') {
          propertiesMap.set('name', prop);
          break;
        }
      }
      dumpFunctionDetails(propertiesMap);
      loadAndDumpScopeObjects(propertiesMap.get('[[Scopes]]'), next);
    }
  }

  SourcesTestRunner.runDebuggerTestSuite([
    function testGetFirstLineFunctionDetails(next) {
      performStandardTestCase('firstLineFunction', next);
    },
    function testGetNonFirstLineFunctionDetails(next) {
      performStandardTestCase('notFirstLineFunction', next);
    },
    function testGetDetailsOfFunctionWithInferredName(next) {
      performStandardTestCase('obj.m', next);
    },
    function testGetDetailsOfFunctionWithDisplayName(next) {
      performStandardTestCase('functionWithDisplayName', next);
    },
    function testGetDetailsOfFunctionWithDisplayNameGetter(next) {
      performStandardTestCase('functionWithDisplayNameGetter', next);
    },
    function testSmallClosure(next) {
      performStandardTestCase('smallClosure', next);
    },
    function testBigClosure(next) {
      performStandardTestCase('bigClosure', next);
    },
    function testGenFunction(next) {
      performStandardTestCase('gen', next);
    }
  ]);
})();
