var geomagnetism = require('geomagnetism');
var ephemeris = require('./ephemeris');
var sats = require('./sats');

var sendQueue = [];
var sending = false;
var lastLat = null;
var lastLon = null;
var lastHasGps = 0;
var lastGpsAcc = null;
var trackISS = false;
var trackSatIdx = -1;
var skyTimer = null;
var dwarfOrbits = null;
var asteroidOrbits = null;

function loadLastLoc() {
  try {
    return JSON.parse(localStorage.getItem('starWatchLoc') || 'null');
  } catch (e) {
    return null;
  }
}

function saveLastLoc(lat, lon) {
  try {
    localStorage.setItem('starWatchLoc', JSON.stringify({ lat: lat, lon: lon }));
  } catch (e) {}
}

function loadOrbits(key) {
  try {
    return JSON.parse(localStorage.getItem(key) || 'null');
  } catch (e) {
    return null;
  }
}

function saveOrbits(key, val) {
  try {
    localStorage.setItem(key, JSON.stringify(val));
  } catch (e) {}
}

function pumpSend() {
  if (sending || !sendQueue.length) {
    return;
  }
  sending = true;
  var item = sendQueue.shift();
  Pebble.sendAppMessage(item.dict, function () {
    sending = false;
    if (item.success) {
      item.success();
    }
    pumpSend();
  }, function (e) {
    sending = false;
    console.log('send failed ' + JSON.stringify(e));
    if (item.fail) {
      item.fail(e);
    }
    pumpSend();
  });
}

function send(dict, success, fail) {
  sendQueue.push({ dict: dict, success: success, fail: fail });
  pumpSend();
}

function pushI16(bytes, value) {
  var v = Math.round(value);
  if (v < 0) {
    v += 65536;
  }
  bytes.push(v & 0xff);
  bytes.push((v >> 8) & 0xff);
}

function wrap36000(cdeg) {
  var v = Math.round(cdeg);
  v %= 36000;
  if (v < 0) {
    v += 36000;
  }
  return v;
}

function packSatPairs(list) {
  var bytes = [];
  var i;
  var n = sats.SAT_COUNT;
  for (i = 0; i < n; i++) {
    if (!list || !list[i]) {
      pushI16(bytes, 0);
      pushI16(bytes, 32767);
      continue;
    }
    pushI16(bytes, wrap36000(list[i].ra * 100));
    pushI16(bytes, list[i].dec * 100);
  }
  return bytes;
}

function packPairs(list) {
  var bytes = [];
  var i;
  for (i = 0; i < list.length; i++) {
    if (!list[i]) {
      pushI16(bytes, 0);
      pushI16(bytes, 0);
      continue;
    }
    pushI16(bytes, wrap36000(list[i].ra * 100));
    pushI16(bytes, list[i].dec * 100);
  }
  return bytes;
}

function magneticDeclination(lat, lon) {
  try {
    var info = geomagnetism.model(new Date()).point([lat, lon]);
    if (info && typeof info.decl === 'number') {
      return info.decl;
    }
  } catch (e) {
    console.log('declination error ' + e);
  }
  return 0;
}

function sendSky(lat, lon, hasGps, accM) {
  lastLat = lat;
  lastLon = lon;
  lastHasGps = hasGps ? 1 : 0;
  if (accM !== undefined && accM !== null && isFinite(accM)) {
    lastGpsAcc = accM;
  }
  if (!hasGps) {
    lastGpsAcc = null;
  }
  var now = new Date();
  var decl = magneticDeclination(lat, lon);
  var dict = {
    Lat: Math.round(lat * 1000),
    Lon: Math.round(lon * 1000),
    Declination: Math.round(decl * 10),
    HasGps: hasGps ? 1 : 0,
    Planets: packPairs(ephemeris.allBodies(now)),
    Dwarfs: packPairs(ephemeris.dwarfBodies(now, dwarfOrbits)),
    Asteroids: packPairs(ephemeris.asteroidBodies(now, asteroidOrbits)),
    Sats: packSatPairs(sats.currentAll(now))
  };
  if (hasGps && lastGpsAcc !== null && isFinite(lastGpsAcc)) {
    dict.GpsAcc = Math.round(lastGpsAcc);
  }
  if (trackISS && trackSatIdx >= 0) {
    var tleBytes = sats.packTLE(trackSatIdx);
    if (tleBytes) {
      dict.ISS_TLE = tleBytes;
    }
  }
  send(dict);
}

function requestLocation() {
  var cached = loadLastLoc();
  var geo = navigator.geolocation;
  if (!geo) {
    if (cached) {
      sendSky(cached.lat, cached.lon, 0);
    }
    return;
  }
  geo.getCurrentPosition(function (pos) {
    var lat = pos.coords.latitude;
    var lon = pos.coords.longitude;
    saveLastLoc(lat, lon);
    sendSky(lat, lon, 1, pos.coords.accuracy);
  }, function (err) {
    console.log('gps error ' + (err && err.message));
    if (cached) {
      sendSky(cached.lat, cached.lon, 0);
    } else if (lastLat !== null) {
      sendSky(lastLat, lastLon, 0);
    }
  }, {
    enableHighAccuracy: false,
    timeout: 8000,
    maximumAge: 15 * 60 * 1000
  });
}

function tickSky() {
  if (lastLat === null) {
    var cached = loadLastLoc();
    if (!cached) {
      return;
    }
    lastLat = cached.lat;
    lastLon = cached.lon;
  }
  sendSky(lastLat, lastLon, lastHasGps);
}

function restartSkyTimer() {
  if (skyTimer) {
    clearInterval(skyTimer);
  }
  skyTimer = setInterval(tickSky, trackISS ? 5000 : 15000);
}

function fetchSbdb(query, idx, target, done) {
  var req = new XMLHttpRequest();
  req.open('GET', 'https://ssd-api.jpl.nasa.gov/sbdb.api?sstr=' + encodeURIComponent(query), true);
  req.timeout = 8000;
  req.onload = function () {
    if (req.status >= 200 && req.status < 300) {
      try {
        var json = JSON.parse(req.responseText);
        var el = json && json.orbit && json.orbit.elements;
        var map = {};
        var i;
        if (el && el.length) {
          for (i = 0; i < el.length; i++) {
            if (el[i] && el[i].name) {
              map[el[i].name] = el[i].value;
            }
          }
          map.epoch = json.orbit.epoch;
          var orbit = ephemeris.orbitFromSbdb(map);
          if (orbit) {
            target[idx] = orbit;
          }
        }
      } catch (e) {}
    }
    done();
  };
  req.onerror = done;
  req.ontimeout = done;
  try {
    req.send();
  } catch (e) {
    done();
  }
}

function refreshOrbits() {
  var qi = 0;
  var dwarfQ = ephemeris.DWARF_QUERY;
  var astQ = ephemeris.ASTEROID_QUERY;
  var next = function () {
    if (qi < dwarfQ.length) {
      var di = qi;
      qi += 1;
      fetchSbdb(dwarfQ[di], di, dwarfOrbits, next);
      return;
    }
    var ai = qi - dwarfQ.length;
    if (ai < astQ.length) {
      qi += 1;
      fetchSbdb(astQ[ai], ai, asteroidOrbits, next);
      return;
    }
    saveOrbits('starWatchDwarfs', dwarfOrbits);
    saveOrbits('starWatchAsteroids', asteroidOrbits);
    tickSky();
  };
  dwarfOrbits = dwarfOrbits || [];
  asteroidOrbits = asteroidOrbits || [];
  next();
}

dwarfOrbits = loadOrbits('starWatchDwarfs');
asteroidOrbits = loadOrbits('starWatchAsteroids');

Pebble.addEventListener('ready', function () {
  console.log('Star Watch PKJS ready');
  requestLocation();
  sats.refreshAll(function () {
    tickSky();
  });
  refreshOrbits();
  restartSkyTimer();
  setInterval(requestLocation, 60000);
  setInterval(function () {
    sats.refreshAll();
  }, 6 * 3600 * 1000);
});

Pebble.addEventListener('appmessage', function (e) {
  var payload = e && e.payload ? e.payload : {};
  if (payload.TrackISS !== undefined && payload.TrackISS !== null) {
    trackISS = !!payload.TrackISS;
    if (payload.TrackSat !== undefined && payload.TrackSat !== null) {
      trackSatIdx = payload.TrackSat | 0;
    } else {
      trackSatIdx = trackISS ? 0 : -1;
    }
    restartSkyTimer();
    if (trackISS) {
      sats.refreshAll(function () {
        tickSky();
      });
    }
  }
  if (payload.Request) {
    requestLocation();
  }
});
