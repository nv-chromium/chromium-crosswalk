if (self.importScripts) {
    importScripts('helpers.js');

    if (get_current_scope() == 'ServiceWorker')
        importScripts('../../serviceworker/resources/worker-testharness.js');
    else
        importScripts('../../resources/testharness.js');
}

async_test(function(test) {
    // Querying a random permission name should fail.
    navigator.permissions.query({name:'foobar'}).then(function(result) {
        assert_unreached('querying a random permission should fail');
    }, function(error) {
        assert_equals(error.name, 'TypeError');

        // Querying a permission without a name should fail.
        return navigator.permissions.query({});
    }).then(function(result) {
        assert_unreached('querying a permission without a name should fail');
    }, function(error) {
        assert_equals(error.name, 'TypeError');
        test.done();
    });
}, 'Test PermissionDescription WebIDL rules in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    navigator.permissions.query({name:'geolocation'}).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');
        test.done();
    }).catch(function() {
        assert_unreached('querying geolocation permission should not fail.')
    });
}, 'Test geolocation permission in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    navigator.permissions.query({name:'midi'}).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');

        // Test for sysex=false.
        return navigator.permissions.query({name:'midi', sysex: false});
    }).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');

        // Test for sysex=true.
        return navigator.permissions.query({name:'midi', sysex: true});
    }).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');
        test.done();
    }).catch(function() {
        assert_unreached('querying midi permission should not fail.')
    });
}, 'Test midi permission in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    navigator.permissions.query({name:'notifications'}).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');
        test.done();
    }).catch(function() {
        assert_unreached('querying notifications permission should not fail.')
    });
}, 'Test notifications permission in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    navigator.permissions.query({name:'push'}).catch(function(e) {
        // By default, the permission query is rejected if "userVisibleOnly" option
        // isn't set or set to true.
        assert_equals(e.name, "NotSupportedError");

        // Test for userVisibleOnly=false.
        return navigator.permissions.query({name:'push', userVisibleOnly: false});
    }).catch(function(e) {
        // By default, the permission query is rejected if "userVisibleOnly" option
        // isn't set or set to true.
        assert_equals(e.name, "NotSupportedError");

        // Test for userVisibleOnly=true.
        return navigator.permissions.query({name:'push', userVisibleOnly: true});
    }).then(function(result) {
        assert_true(result instanceof PermissionStatus);
        assert_equals(result.status, 'denied');
        test.done();
    }).catch(function() {
        assert_unreached('querying push permission should not fail.')
    });
}, 'Test push permission in ' + get_current_scope() + ' scope.');

done();
