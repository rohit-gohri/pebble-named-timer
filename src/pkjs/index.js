'use strict';

var keys = require('message_keys');
var settingsPage = require('./settings_page');

var STORAGE_KEY = 'named-timer-state-v1';
var MAX_TIMERS = 8;
var MAX_NAME_LEN = 23;
var DEFAULT_PRESETS = ['Coffee', 'Laundry', 'Tea', 'Cooking', 'Workout', 'Focus', 'Eggs', 'Nap'];

var configWaitTimer = null;
var awaitingConfigSync = false;
var pushInFlight = false;

function sanitizeName(name) {
  return String(name || '').replace(/[|\r\n]/g, ' ').trim().slice(0, MAX_NAME_LEN);
}

function getPayloadValue(payload, keyName) {
  if (!payload) {
    return undefined;
  }
  if (Object.prototype.hasOwnProperty.call(payload, keyName)) {
    return payload[keyName];
  }
  var numericKey = keys[keyName];
  if (typeof numericKey !== 'undefined' && Object.prototype.hasOwnProperty.call(payload, numericKey)) {
    return payload[numericKey];
  }
  return undefined;
}

function loadState() {
  var state = null;
  try {
    var raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      state = JSON.parse(raw);
    }
  } catch (err) {
    console.log('Could not parse saved state: ' + err);
  }

  if (!state || typeof state !== 'object') {
    state = {
      presets: DEFAULT_PRESETS.slice(),
      timers: [],
      pendingPush: false,
      syncedAt: 0
    };
    // Migrate presets from legacy Clay storage if present
    try {
      var legacy = JSON.parse(localStorage.getItem('clay-settings') || '{}');
      for (var i = 0; i < 8; i++) {
        if (typeof legacy['PRESET_' + i] === 'string') {
          state.presets[i] = sanitizeName(legacy['PRESET_' + i]);
        }
      }
    } catch (err2) {
      // ignore legacy parse error
    }
  }

  if (!Array.isArray(state.presets) || state.presets.length !== 8) {
    var fixedPresets = DEFAULT_PRESETS.slice();
    if (Array.isArray(state.presets)) {
      for (var j = 0; j < 8 && j < state.presets.length; j++) {
        fixedPresets[j] = sanitizeName(state.presets[j]);
      }
    }
    state.presets = fixedPresets;
  }
  if (!Array.isArray(state.timers)) {
    state.timers = [];
  }
  return state;
}

function saveState(state) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
  } catch (err) {
    console.log('Could not persist state: ' + err);
  }
}

function parseTimersData(str) {
  if (typeof str !== 'string' || str.length === 0) {
    return [];
  }
  var lines = str.split('\n');
  var result = [];
  for (var i = 0; i < lines.length && result.length < MAX_TIMERS; i++) {
    var line = lines[i];
    if (!line) {
      continue;
    }
    var p1 = line.indexOf('|');
    var p2 = p1 >= 0 ? line.indexOf('|', p1 + 1) : -1;
    var p3 = p2 >= 0 ? line.indexOf('|', p2 + 1) : -1;
    if (p1 < 0 || p2 < 0 || p3 < 0) {
      continue;
    }
    var id = parseInt(line.slice(0, p1), 10);
    var durationSec = parseInt(line.slice(p1 + 1, p2), 10);
    var flags = parseInt(line.slice(p2 + 1, p3), 10);
    var name = line.slice(p3 + 1);
    if (isNaN(id) || isNaN(durationSec) || durationSec < 5) {
      continue;
    }
    result.push({
      id: id,
      durationSec: durationSec,
      running: (flags & 1) !== 0,
      name: sanitizeName(name)
    });
  }
  return result;
}

function serializeTimersData(timers) {
  var lines = [];
  for (var i = 0; i < timers.length && i < MAX_TIMERS; i++) {
    var t = timers[i];
    var id = typeof t.id === 'number' && !isNaN(t.id) ? Math.floor(t.id) : 0;
    var dur = Math.min(359999, Math.max(5, Math.floor(t.durationSec || 0)));
    var flags = (id === 0 && t.startImmediately) ? 1 : 0;
    var name = sanitizeName(t.name);
    lines.push(id + '|' + dur + '|' + flags + '|' + name);
  }
  return lines.join('\n');
}

function buildAppMessageDict(state) {
  var dict = {};
  for (var i = 0; i < 8; i++) {
    dict[keys['PRESET_' + i]] = sanitizeName(state.presets[i]);
  }
  dict[keys.TIMERS_DATA] = serializeTimersData(state.timers);
  return dict;
}

function pushStateToWatch(state) {
  pushInFlight = true;
  Pebble.sendAppMessage(buildAppMessageDict(state), function() {
    pushInFlight = false;
    console.log('Named Timers settings synced to watch');
    var latest = loadState();
    latest.pendingPush = false;
    latest.syncedAt = Date.now();
    saveState(latest);
  }, function(err) {
    pushInFlight = false;
    console.log('Watch did not accept settings immediately (will sync on next open): ' + JSON.stringify(err));
  });
}

function openSettingsWebview(state, live) {
  var html = settingsPage.buildSettingsHtml({
    live: !!live,
    pendingPush: !!state.pendingPush,
    timers: state.timers,
    presets: state.presets
  });
  var url = 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
  Pebble.openURL(url);
}

Pebble.addEventListener('appmessage', function(e) {
  var payload = e && e.payload;
  var isSync = getPayloadValue(payload, 'STATE_SYNC');
  if (!isSync) {
    return;
  }

  var state = loadState();

  // If the user saved phone edits while the watchapp was closed, push those
  // edits to the watch instead of overwriting them with older watch state.
  if (state.pendingPush && !pushInFlight && !awaitingConfigSync) {
    pushStateToWatch(state);
    return;
  }

  var timersRaw = getPayloadValue(payload, 'TIMERS_DATA');
  if (typeof timersRaw === 'string') {
    state.timers = parseTimersData(timersRaw);
  }
  for (var i = 0; i < 8; i++) {
    var presetVal = getPayloadValue(payload, 'PRESET_' + i);
    if (typeof presetVal === 'string') {
      state.presets[i] = sanitizeName(presetVal);
    }
  }
  state.pendingPush = false;
  state.syncedAt = Date.now();
  saveState(state);

  if (awaitingConfigSync) {
    awaitingConfigSync = false;
    if (configWaitTimer !== null) {
      clearTimeout(configWaitTimer);
      configWaitTimer = null;
    }
    openSettingsWebview(state, true);
  }
});

Pebble.addEventListener('showConfiguration', function() {
  var state = loadState();

  if (configWaitTimer !== null) {
    clearTimeout(configWaitTimer);
    configWaitTimer = null;
  }
  awaitingConfigSync = false;

  if (state.pendingPush) {
    openSettingsWebview(state, false);
    return;
  }

  awaitingConfigSync = true;
  function openCached() {
    if (!awaitingConfigSync) {
      return;
    }
    awaitingConfigSync = false;
    if (configWaitTimer !== null) {
      clearTimeout(configWaitTimer);
      configWaitTimer = null;
    }
    openSettingsWebview(loadState(), false);
  }

  configWaitTimer = setTimeout(openCached, 900);

  var req = {};
  req[keys.SYNC_REQUEST] = 1;
  Pebble.sendAppMessage(req, function() {
    // Watch received SYNC_REQUEST; wait for STATE_SYNC reply (or timeout fallback)
  }, function() {
    // Watch app not open or unreachable — immediately open cached settings
    openCached();
  });
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }

  var parsed;
  try {
    var raw = e.response.charAt(0) === '{' ? e.response : decodeURIComponent(e.response);
    parsed = JSON.parse(raw);
  } catch (err) {
    console.log('Could not parse settings response: ' + err);
    return;
  }

  var state = loadState();
  if (Array.isArray(parsed.timers)) {
    state.timers = parsed.timers.slice(0, MAX_TIMERS).map(function(t) {
      return {
        id: typeof t.id === 'number' ? Math.floor(t.id) : 0,
        name: sanitizeName(t.name),
        durationSec: Math.min(359999, Math.max(5, Math.floor(t.durationSec || 5))),
        running: false,
        startImmediately: !!t.startImmediately
      };
    });
  }
  if (Array.isArray(parsed.presets)) {
    for (var i = 0; i < 8; i++) {
      state.presets[i] = sanitizeName(parsed.presets[i]);
    }
  }
  state.pendingPush = true;
  saveState(state);

  pushStateToWatch(state);
});

Pebble.addEventListener('ready', function() {
  console.log('Named Timers PKJS ready');
  var state = loadState();
  if (state.pendingPush) {
    pushStateToWatch(state);
  } else {
    var req = {};
    req[keys.SYNC_REQUEST] = 1;
    Pebble.sendAppMessage(req, function() {}, function() {});
  }
});
