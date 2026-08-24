var DEG = Math.PI / 180;
var GM = 398600.4418;
var RE = 6378.137;
var J2 = 1.08262668e-3;

function wrap360(x) {
  x = x % 360;
  if (x < 0) {
    x += 360;
  }
  return x;
}

function parseTLE(text) {
  var lines = (text || '').replace(/\r/g, '').split('\n').map(function (s) {
    return s.trim();
  }).filter(Boolean);
  var i;
  var l1;
  var l2;
  for (i = 0; i < lines.length - 1; i++) {
    if (lines[i].charAt(0) === '1' && lines[i + 1].charAt(0) === '2' &&
        (lines[i].indexOf('25544') >= 0 || lines[i + 1].indexOf('25544') >= 0)) {
      l1 = lines[i];
      l2 = lines[i + 1];
      break;
    }
  }
  if (!l1 && lines.length >= 2) {
    if (lines[0].charAt(0) === '1') {
      l1 = lines[0];
      l2 = lines[1];
    } else if (lines.length >= 3) {
      l1 = lines[1];
      l2 = lines[2];
    }
  }
  if (!l1 || !l2 || l1.length < 32 || l2.length < 63) {
    return null;
  }
  var yy = parseInt(l1.substring(18, 20), 10);
  var day = parseFloat(l1.substring(20, 32));
  var year = yy < 57 ? 2000 + yy : 1900 + yy;
  var epochMs = Date.UTC(year, 0, 1) + (day - 1) * 86400000;
  var eStr = l2.substring(26, 33).trim();
  return {
    epochMs: epochMs,
    inc: parseFloat(l2.substring(8, 16)),
    raan: parseFloat(l2.substring(17, 25)),
    ecc: parseFloat('0.' + eStr),
    argp: parseFloat(l2.substring(34, 42)),
    m0: parseFloat(l2.substring(43, 51)),
    n: parseFloat(l2.substring(52, 63))
  };
}

function issRaDec(tle, date) {
  if (!tle) {
    return null;
  }
  var dtDays = (date.getTime() - tle.epochMs) / 86400000;
  var n = tle.n;
  var nRad = n * 2 * Math.PI / 86400;
  var a = Math.pow(GM / (nRad * nRad), 1 / 3);
  var e = tle.ecc;
  var i = tle.inc * DEG;
  var cosi = Math.cos(i);
  var sini = Math.sin(i);
  var ecc2 = 1 - e * e;
  var pFac = Math.pow(RE / a, 2) / (ecc2 * ecc2);
  var raan = wrap360(tle.raan + (-1.5 * n * 360 * J2 * pFac * cosi) * dtDays);
  var argp = wrap360(tle.argp + (0.75 * n * 360 * J2 * pFac * (5 * cosi * cosi - 1)) * dtDays);
  var M = wrap360(tle.m0 + n * 360 * dtDays);
  var Mr = ((M > 180 ? M - 360 : M) * DEG);
  var E = Mr;
  var k;
  for (k = 0; k < 8; k++) {
    E = E - (E - e * Math.sin(E) - Mr) / (1 - e * Math.cos(E));
  }
  var xv = Math.cos(E) - e;
  var yv = Math.sqrt(Math.max(0, ecc2)) * Math.sin(E);
  var v = Math.atan2(yv, xv);
  var r = a * Math.sqrt(xv * xv + yv * yv);
  var u = v + argp * DEG;
  var cosu = Math.cos(u);
  var sinu = Math.sin(u);
  var coso = Math.cos(raan * DEG);
  var sino = Math.sin(raan * DEG);
  var x = r * (coso * cosu - sino * sinu * cosi);
  var y = r * (sino * cosu + coso * sinu * cosi);
  var z = r * (sinu * sini);
  var ra = Math.atan2(y, x) * 180 / Math.PI;
  if (ra < 0) {
    ra += 360;
  }
  var dec = Math.atan2(z, Math.sqrt(x * x + y * y)) * 180 / Math.PI;
  return { ra: ra, dec: dec };
}

function loadCachedTLE() {
  try {
    var raw = localStorage.getItem('starWatchISS');
    if (!raw) {
      return null;
    }
    var obj = JSON.parse(raw);
    if (!obj || !obj.tle || (Date.now() - obj.at) > 12 * 3600 * 1000) {
      return obj && obj.tle ? obj.tle : null;
    }
    return obj.tle;
  } catch (e) {
    return null;
  }
}

function saveTLE(tle) {
  try {
    localStorage.setItem('starWatchISS', JSON.stringify({ tle: tle, at: Date.now() }));
  } catch (e) {}
}

var tleState = loadCachedTLE();
var fetching = false;

function fetchTLE(done) {
  if (fetching) {
    return;
  }
  fetching = true;
  var req = new XMLHttpRequest();
  req.open('GET', 'https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE', true);
  req.timeout = 8000;
  req.onload = function () {
    fetching = false;
    if (req.status >= 200 && req.status < 300) {
      var parsed = parseTLE(req.responseText);
      if (parsed) {
        tleState = parsed;
        saveTLE(parsed);
      }
    }
    if (done) {
      done(tleState);
    }
  };
  req.onerror = function () {
    fetching = false;
    if (done) {
      done(tleState);
    }
  };
  req.ontimeout = req.onerror;
  try {
    req.send();
  } catch (e) {
    fetching = false;
    if (done) {
      done(tleState);
    }
  }
}

function currentISS(date) {
  return issRaDec(tleState, date || new Date());
}

function pushI16(bytes, value) {
  var v = Math.round(value);
  if (v < 0) {
    v += 65536;
  }
  bytes.push(v & 0xff, (v >> 8) & 0xff);
}

function pushI32(bytes, value) {
  var v = Math.round(value) | 0;
  bytes.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
}

function packTLE() {
  if (!tleState) {
    return null;
  }
  var bytes = [];
  pushI32(bytes, tleState.epochMs / 1000);
  pushI16(bytes, tleState.inc * 100);
  pushI16(bytes, wrap360(tleState.raan) * 100);
  pushI32(bytes, tleState.ecc * 1000000);
  pushI16(bytes, wrap360(tleState.argp) * 100);
  pushI16(bytes, wrap360(tleState.m0) * 100);
  pushI32(bytes, tleState.n * 1000000);
  return bytes;
}

module.exports = {
  fetchTLE: fetchTLE,
  currentISS: currentISS,
  packTLE: packTLE,
  hasTLE: function () {
    return !!tleState;
  }
};
