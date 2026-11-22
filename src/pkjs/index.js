var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Handle Clay config manually so we can reset the one-shot Quick Add fields
// after sending them to the watch, without losing the user's saved preset names.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }

  var dict = clay.getSettings(e.response);

  // Send settings dictionary directly to the watch
  Pebble.sendAppMessage(dict, function() {
    console.log('Named Timer settings sent to watch');
  }, function(err) {
    console.log('Failed to send settings to watch: ' + JSON.stringify(err));
  });

  // Reset the one-shot Quick Add controls in localStorage so saving preset names
  // again later does not accidentally add a duplicate timer.
  try {
    var stored = JSON.parse(localStorage.getItem('clay-settings') || '{}');
    stored.QUICK_ADD_NAME = '';
    stored.QUICK_ADD_MINUTES = 0;
    stored.QUICK_ADD_SECONDS = 0;
    localStorage.setItem('clay-settings', JSON.stringify(stored));
  } catch (err) {
    console.log('Could not reset quick-add fields: ' + err);
  }
});

Pebble.addEventListener('ready', function() {
  console.log('Named Timer PKJS ready');
});
