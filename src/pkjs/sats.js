var DEG = Math.PI / 180;
var GM = 398600.4418;
var RE = 6378.137;
var J2 = 1.08262668e-3;
var SAT_NAMED = 18;
var SAT_GPS = 32;
var SAT_COUNT = SAT_NAMED + SAT_GPS;
var SAT_GPS_0 = SAT_NAMED;
var CACHE_MS = 6 * 3600 * 1000;

/* LEO: Earth-centered Kepler/TLE is usable. */
var LEO = [
  { id: 0, norad: '25544' },
  { id: 1, norad: '48274' },
  { id: 3, norad: '20580' },
  { id: 8, norad: '33053' },
  { id: 9, norad: '28485' }
];

/* GEO/HEO/L1/L2/deep space: Horizons only. SDO NORAD is 36395 (not 35315). */
var HORIZONS = [
  { id: 2, cmd: '-136395' },
  { id: 4, cmd: '-151' },
  { id: 5, cmd: '-125989' },
  { id: 6, cmd: '-139479' },
  { id: 7, cmd: '-95' },
  { id: 10, cmd: '-170' },
  { id: 11, cmd: '-680' },
  { id: 12, cmd: '-78' },
  { id: 13, cmd: '-98' },
  { id: 14, cmd: '-31' },
  { id: 15, cmd: '-32' },
  { id: 16, cmd: '-23' },
  { id: 17, cmd: '-24' }
];

var TLE_GROUPS = ['stations', 'science', 'geo'];
var SDO_NORAD = '36395';

function wrap360(x) {
  x = x % 360;
  if (x < 0) {
    x += 360;
  }
  return x;
}

function pad2(n) {
  return n < 10 ? '0' + n : '' + n;
}

function ymdUTC(d) {
  return d.getUTCFullYear() + '-' + pad2(d.getUTCMonth() + 1) + '-' + pad2(d.getUTCDate());
}

function parseTLEPair(l1, l2, name) {
  if (!l1 || !l2 || l1.charAt(0) !== '1' || l2.charAt(0) !== '2') {
    return null;
  }
  if (l1.length < 32 || l2.length < 63) {
    return null;
  }
  var yy = parseInt(l1.substring(18, 20), 10);
  var day = parseFloat(l1.substring(20, 32));
  var year = yy < 57 ? 2000 + yy : 1900 + yy;
  var epochMs = Date.UTC(year, 0, 1) + (day - 1) * 86400000;
  var eStr = l2.substring(26, 33).trim();
  return {
    name: name || '',
    norad: l1.substring(2, 7).trim(),
    epochMs: epochMs,
    inc: parseFloat(l2.substring(8, 16)),
    raan: parseFloat(l2.substring(17, 25)),
    ecc: parseFloat('0.' + eStr),
    argp: parseFloat(l2.substring(34, 42)),
    m0: parseFloat(l2.substring(43, 51)),
    n: parseFloat(l2.substring(52, 63))
  };
}

function parseTLECatalog(text) {
  var lines = (text || '').replace(/\r/g, '').split('\n').map(function (s) {
    return s.trim();
  }).filter(Boolean);
  var out = [];
  var i = 0;
  while (i < lines.length) {
    var name = '';
    var l1;
    var l2;
    if (lines[i].charAt(0) !== '1' && i + 2 < lines.length &&
        lines[i + 1].charAt(0) === '1') {
      name = lines[i];
      l1 = lines[i + 1];
      l2 = lines[i + 2];
      i += 3;
    } else if (i + 1 < lines.length) {
      l1 = lines[i];
      l2 = lines[i + 1];
      i += 2;
    } else {
      break;
    }
    var tle = parseTLEPair(l1, l2, name);
    if (tle) {
      out.push(tle);
    }
  }
  return out;
}

function prnFromName(name) {
  var m = (name || '').match(/PRN\s*(\d+)/i);
  if (!m) {
    return 0;
  }
  return parseInt(m[1], 10);
}

function satRaDec(tle, date) {
  if (!tle) {
    return null;
  }
  var dtDays = (date.getTime() - tle.epochMs) / 86400000;
  var n = tle.n;
  if (!(n > 10 && n < 17) && !(n > 1.8 && n < 2.4) && !(n > 0.9 && n < 1.1)) {
    return null;
  }
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

function loadCache() {
  try {
    var raw = localStorage.getItem('starWatchSats2');
    if (!raw) {
      return { earth: {}, gps: {}, deep: {}, at: 0 };
    }
    var obj = JSON.parse(raw);
    if (!obj) {
      return { earth: {}, gps: {}, deep: {}, at: 0 };
    }
    obj.earth = obj.earth || {};
    obj.gps = obj.gps || {};
    obj.deep = obj.deep || {};
    obj.at = obj.at || 0;
    return obj;
  } catch (e) {
    return { earth: {}, gps: {}, deep: {}, at: 0 };
  }
}

function saveCache() {
  try {
    localStorage.setItem('starWatchSats2', JSON.stringify(cache));
  } catch (e) {}
}

var cache = loadCache();
var fetching = false;

function tleByIndex(index) {
  var i;
  if (index >= SAT_GPS_0 && index < SAT_COUNT) {
    return cache.gps['' + (index - SAT_GPS_0 + 1)] || null;
  }
  for (i = 0; i < LEO.length; i++) {
    if (LEO[i].id === index) {
      return cache.earth[LEO[i].norad] || null;
    }
  }
  if (index === 2) {
    return cache.earth[SDO_NORAD] || null;
  }
  return null;
}

function parseHorizons(json) {
  var text = json && json.result;
  var i;
  var j;
  var block;
  var lines;
  var k;
  var m;
  if (!text) {
    return null;
  }
  i = text.indexOf('$$SOE');
  j = text.indexOf('$$EOE');
  if (i < 0 || j < 0) {
    return null;
  }
  block = text.substring(i + 5, j);
  lines = block.split('\n');
  for (k = 0; k < lines.length; k++) {
    m = lines[k].trim().match(/(-?\d+\.\d+)\s+(-?\d+\.\d+)\s*$/);
    if (m) {
      return { ra: parseFloat(m[1]), dec: parseFloat(m[2]), at: Date.now() };
    }
  }
  return null;
}

function httpGet(url, timeout, done) {
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.timeout = timeout || 8000;
  req.onload = function () {
    done(req.status >= 200 && req.status < 300 ? req.responseText : null);
  };
  req.onerror = function () {
    done(null);
  };
  req.ontimeout = req.onerror;
  try {
    req.send();
  } catch (e) {
    done(null);
  }
}

function ingestEarth(list) {
  var i;
  if (!list) {
    return;
  }
  for (i = 0; i < list.length; i++) {
    if (list[i] && list[i].norad) {
      cache.earth[list[i].norad] = list[i];
    }
  }
}

function fetchGroup(group, done) {
  httpGet('https://celestrak.org/NORAD/elements/gp.php?GROUP=' + group + '&FORMAT=TLE',
          12000, function (text) {
    ingestEarth(parseTLECatalog(text));
    done();
  });
}

function fetchEarth(item, done) {
  var url = 'https://celestrak.org/NORAD/elements/gp.php?CATNR=' +
            item.norad + '&FORMAT=TLE';
  httpGet(url, 8000, function (text) {
    ingestEarth(parseTLECatalog(text));
    done();
  });
}

function fetchGps(done) {
  httpGet('https://celestrak.org/NORAD/elements/gp.php?GROUP=gps-ops&FORMAT=TLE',
          12000, function (text) {
    var list = parseTLECatalog(text);
    var next = {};
    var i;
    var prn;
    if (list && list.length) {
      for (i = 0; i < list.length; i++) {
        prn = prnFromName(list[i].name);
        if (prn >= 1 && prn <= SAT_GPS) {
          next['' + prn] = list[i];
        }
      }
      cache.gps = next;
    }
    done();
  });
}

function fetchDeep(item, done) {
  var now = new Date();
  var start = ymdUTC(now);
  var stop = ymdUTC(new Date(now.getTime() + 86400000));
  var q = [
    'format=json',
    'COMMAND=' + encodeURIComponent("'" + item.cmd + "'"),
    'OBJ_DATA=NO',
    'MAKE_EPHEM=YES',
    'EPHEM_TYPE=OBSERVER',
    'CENTER=' + encodeURIComponent("'500@399'"),
    'START_TIME=' + encodeURIComponent("'" + start + "'"),
    'STOP_TIME=' + encodeURIComponent("'" + stop + "'"),
    'STEP_SIZE=' + encodeURIComponent("'1d'"),
    'QUANTITIES=' + encodeURIComponent("'1'"),
    'ANG_FORMAT=DEG'
  ].join('&');
  httpGet('https://ssd.jpl.nasa.gov/api/horizons.api?' + q, 15000, function (text) {
    var pos = null;
    if (text) {
      try {
        pos = parseHorizons(JSON.parse(text));
      } catch (e) {
        pos = null;
      }
    }
    if (pos) {
      cache.deep['' + item.id] = pos;
    }
    done();
  });
}

function cacheFresh() {
  return cache.at && (Date.now() - cache.at) < CACHE_MS;
}

function fetchMissingLeo(done) {
  var k = 0;
  var step = function () {
    if (k >= LEO.length) {
      done();
      return;
    }
    var item = LEO[k];
    k += 1;
    if (cache.earth[item.norad]) {
      step();
      return;
    }
    fetchEarth(item, step);
  };
  step();
}

function refreshAll(done) {
  var qi = 0;
  var next;
  if (fetching) {
    return;
  }
  fetching = true;
  next = function () {
    if (qi < TLE_GROUPS.length) {
      var gi = qi;
      qi += 1;
      fetchGroup(TLE_GROUPS[gi], next);
      return;
    }
    if (qi === TLE_GROUPS.length) {
      qi += 1;
      fetchGps(next);
      return;
    }
    if (qi === TLE_GROUPS.length + 1) {
      qi += 1;
      fetchMissingLeo(next);
      return;
    }
    var di = qi - TLE_GROUPS.length - 2;
    if (di < HORIZONS.length) {
      qi += 1;
      fetchDeep(HORIZONS[di], next);
      return;
    }
    cache.at = Date.now();
    saveCache();
    fetching = false;
    if (done) {
      done();
    }
  };
  next();
}

function currentAll(date) {
  var now = date || new Date();
  var out = new Array(SAT_COUNT);
  var i;
  var tle;
  var pos;
  for (i = 0; i < SAT_COUNT; i++) {
    out[i] = null;
  }
  for (i = 0; i < LEO.length; i++) {
    tle = cache.earth[LEO[i].norad];
    out[LEO[i].id] = satRaDec(tle, now);
  }
  for (i = 0; i < HORIZONS.length; i++) {
    pos = cache.deep['' + HORIZONS[i].id];
    if (pos) {
      out[HORIZONS[i].id] = { ra: pos.ra, dec: pos.dec };
    }
  }
  if (!out[2]) {
    out[2] = satRaDec(cache.earth[SDO_NORAD], now);
  }
  for (i = 1; i <= SAT_GPS; i++) {
    tle = cache.gps['' + i];
    out[SAT_GPS_0 + i - 1] = satRaDec(tle, now);
  }
  return out;
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

function packTLE(index) {
  var tle = tleByIndex(index);
  var bytes;
  if (!tle || !((tle.n > 10 && tle.n < 17) || (tle.n > 1.8 && tle.n < 2.4))) {
    return null;
  }
  bytes = [];
  pushI16(bytes, index);
  pushI32(bytes, tle.epochMs / 1000);
  pushI16(bytes, tle.inc * 100);
  pushI16(bytes, wrap360(tle.raan) * 100);
  pushI32(bytes, tle.ecc * 1000000);
  pushI16(bytes, wrap360(tle.argp) * 100);
  pushI16(bytes, wrap360(tle.m0) * 100);
  pushI32(bytes, tle.n * 1000000);
  return bytes;
}

module.exports = {
  SAT_COUNT: SAT_COUNT,
  refreshAll: refreshAll,
  currentAll: currentAll,
  packTLE: packTLE,
  cacheFresh: cacheFresh
};
