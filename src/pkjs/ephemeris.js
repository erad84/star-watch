var DEG = Math.PI / 180;

function wrap360(x) {
  x = x % 360;
  if (x < 0) {
    x += 360;
  }
  return x;
}

function sind(x) {
  return Math.sin(x * DEG);
}

function cosd(x) {
  return Math.cos(x * DEG);
}

function daysJ2000(date) {
  return date.getTime() / 86400000 - 10957.5;
}

function kepler(Mdeg, e) {
  var M = ((Mdeg % 360) + 360) % 360;
  if (M > 180) {
    M -= 360;
  }
  var E = M * DEG;
  var eRad = e;
  for (var i = 0; i < 8; i++) {
    E = E - (E - eRad * Math.sin(E) - M * DEG) / (1 - eRad * Math.cos(E));
  }
  return E;
}

function eclipticToEquatorial(lon, lat, obl) {
  var sinLon = sind(lon);
  var cosLon = cosd(lon);
  var sinLat = sind(lat);
  var cosLat = cosd(lat);
  var sinObl = sind(obl);
  var cosObl = cosd(obl);
  var ra = Math.atan2(sinLon * cosObl - Math.tan(lat * DEG) * sinObl, cosLon);
  if (ra < 0) {
    ra += 2 * Math.PI;
  }
  var dec = Math.asin(sinLat * cosObl + cosLat * sinObl * sinLon);
  return { ra: ra * 180 / Math.PI, dec: dec * 180 / Math.PI };
}

function sunRaDec(d) {
  var g = wrap360(357.529 + 0.98560028 * d);
  var q = wrap360(280.459 + 0.98564736 * d);
  var L = wrap360(q + 1.915 * sind(g) + 0.020 * sind(2 * g));
  var obl = 23.439 - 0.00000036 * d;
  return eclipticToEquatorial(L, 0, obl);
}

function moonRaDec(d) {
  var L = wrap360(218.316 + 13.176396 * d);
  var M = wrap360(134.963 + 13.064993 * d);
  var F = wrap360(93.272 + 13.229350 * d);
  var lon = wrap360(L + 6.289 * sind(M));
  var lat = 5.128 * sind(F);
  var obl = 23.439 - 0.00000036 * d;
  return eclipticToEquatorial(lon, lat, obl);
}

/* JPL approximate Keplerian elements, T in Julian centuries from J2000. */
var PLANETS = [
  { a: 0.38709927, e: 0.20563593, I: 7.00497902, L: 252.25032350, lp: 77.45779628, Om: 48.33076593,
    da: 0.00000037, de: 0.00001906, dI: -0.00594749, dL: 149472.67411175, dlp: 0.16047689, dOm: -0.12534081 },
  { a: 0.72333566, e: 0.00677672, I: 3.39467605, L: 181.97909950, lp: 131.60246718, Om: 76.67984255,
    da: 0.00000390, de: -0.00004107, dI: -0.00078890, dL: 58517.81538729, dlp: 0.00268329, dOm: -0.27769418 },
  { a: 1.52371034, e: 0.09339410, I: 1.84969142, L: -4.55343205, lp: -23.94362959, Om: 49.55953891,
    da: 0.00001847, de: 0.00007882, dI: -0.00813131, dL: 19140.30268499, dlp: 0.44441088, dOm: -0.29257343 },
  { a: 5.20288700, e: 0.04838624, I: 1.30439695, L: 34.39644051, lp: 14.72847983, Om: 100.47390909,
    da: -0.00011607, de: -0.00013253, dI: -0.00183714, dL: 3034.74612775, dlp: 0.21252668, dOm: 0.20469106 },
  { a: 9.53667594, e: 0.05386179, I: 2.48599187, L: 49.95424423, lp: 92.59887831, Om: 113.66242448,
    da: -0.00125060, de: -0.00050991, dI: 0.00193609, dL: 1222.49362201, dlp: -0.41897216, dOm: -0.28867794 },
  { a: 19.18916464, e: 0.04725744, I: 0.77263783, L: 313.23810451, lp: 170.95427630, Om: 74.01692503,
    da: -0.00196176, de: -0.00004397, dI: -0.00242939, dL: 428.48202785, dlp: 0.40805281, dOm: 0.04240589 },
  { a: 30.06992276, e: 0.00859048, I: 1.77004347, L: -55.12002969, lp: 44.96476227, Om: 131.78422574,
    da: 0.00026291, de: 0.00005105, dI: 0.00035372, dL: 218.45945325, dlp: -0.32241464, dOm: -0.00508664 }
];

function planetRaDec(p, d) {
  var T = d / 36525.0;
  var e = p.e + p.de * T;
  var I = p.I + p.dI * T;
  var L = wrap360(p.L + p.dL * T);
  var lp = wrap360(p.lp + p.dlp * T);
  var Om = wrap360(p.Om + p.dOm * T);
  var w = wrap360(lp - Om);
  var M = wrap360(L - lp);
  var E = kepler(M, e);
  var xv = Math.cos(E) - e;
  var yv = Math.sqrt(Math.max(0, 1 - e * e)) * Math.sin(E);
  var v = Math.atan2(yv, xv) * 180 / Math.PI;
  var r = Math.sqrt(xv * xv + yv * yv) * (p.a + (p.da || 0) * T);
  var lon = wrap360(v + w);
  var xh = r * (cosd(Om) * cosd(lon) - sind(Om) * sind(lon) * cosd(I));
  var yh = r * (sind(Om) * cosd(lon) + cosd(Om) * sind(lon) * cosd(I));
  var zh = r * (sind(lon) * sind(I));

  var g = wrap360(357.529 + 0.98560028 * d);
  var q = wrap360(280.459 + 0.98564736 * d);
  var sunLon = wrap360(q + 1.915 * sind(g) + 0.020 * sind(2 * g));
  var xe = xh + cosd(sunLon);
  var ye = yh + sind(sunLon);
  var ze = zh;
  var lonE = wrap360(Math.atan2(ye, xe) * 180 / Math.PI);
  var latE = Math.atan2(ze, Math.sqrt(xe * xe + ye * ye)) * 180 / Math.PI;
  var obl = 23.439 - 0.00000036 * d;
  return eclipticToEquatorial(lonE, latE, obl);
}

/* a AU, e, I deg, Om deg, w deg, M0 deg, n deg/day, t0 days from J2000. */
function orbitRaDec(b, d) {
  var dt = d - (b.t0 || 0);
  var e = b.e;
  var I = b.I;
  var Om = wrap360(b.Om);
  var w = wrap360(b.w);
  var n = b.n || (0.9856076686 / Math.pow(b.a, 1.5));
  var M = wrap360(b.M0 + n * dt);
  var E = kepler(M, e);
  var xv = Math.cos(E) - e;
  var yv = Math.sqrt(Math.max(0, 1 - e * e)) * Math.sin(E);
  var v = Math.atan2(yv, xv) * 180 / Math.PI;
  var r = Math.sqrt(xv * xv + yv * yv) * b.a;
  var lon = wrap360(v + w);
  var xh = r * (cosd(Om) * cosd(lon) - sind(Om) * sind(lon) * cosd(I));
  var yh = r * (sind(Om) * cosd(lon) + cosd(Om) * sind(lon) * cosd(I));
  var zh = r * (sind(lon) * sind(I));
  var g = wrap360(357.529 + 0.98560028 * d);
  var q = wrap360(280.459 + 0.98564736 * d);
  var sunLon = wrap360(q + 1.915 * sind(g) + 0.020 * sind(2 * g));
  var xe = xh + cosd(sunLon);
  var ye = yh + sind(sunLon);
  var ze = zh;
  var lonE = wrap360(Math.atan2(ye, xe) * 180 / Math.PI);
  var latE = Math.atan2(ze, Math.sqrt(xe * xe + ye * ye)) * 180 / Math.PI;
  var obl = 23.439 - 0.00000036 * d;
  return eclipticToEquatorial(lonE, latE, obl);
}

/* JPL Pluto + typical osculating elements, epoch JD 2451545.0 (J2000). */
var PLUTO = {
  a: 39.48211675, e: 0.24882730, I: 17.14001206, L: 238.92903833,
  lp: 224.06891629, Om: 110.30393684,
  da: 0.00029414, de: 0.00005170, dI: 0.00004818, dL: 145.20780515,
  dlp: -0.04062942, dOm: -0.01183482
};

var DWARF_FALLBACK = [
  { name: 'Ceres', a: 2.7675, e: 0.0789, I: 10.591, Om: 80.305, w: 73.597, M0: 95.989, n: 0.214023, t0: 0 },
  null,
  { name: 'Haumea', a: 43.116, e: 0.1964, I: 28.214, Om: 122.167, w: 239.041, M0: 218.205, n: 0.003485, t0: 7305 },
  { name: 'Makemake', a: 45.430, e: 0.1613, I: 28.984, Om: 79.310, w: 294.834, M0: 165.514, n: 0.003220, t0: 7305 },
  { name: 'Eris', a: 67.864, e: 0.4361, I: 44.040, Om: 35.951, w: 151.639, M0: 205.989, n: 0.001769, t0: 7305 }
];

var ASTEROID_FALLBACK = [
  { name: 'Vesta', a: 2.3615, e: 0.0889, I: 7.142, Om: 103.81, w: 151.20, M0: 20.86, n: 0.271582, t0: 0 },
  { name: 'Pallas', a: 2.7724, e: 0.2306, I: 34.93, Om: 173.08, w: 310.05, M0: 78.23, n: 0.213451, t0: 0 },
  { name: 'Juno', a: 2.669, e: 0.256, I: 12.99, Om: 169.87, w: 248.17, M0: 33.0, n: 0.2258, t0: 0 },
  { name: 'Hygiea', a: 3.139, e: 0.117, I: 3.83, Om: 283.20, w: 312.32, M0: 241.6, n: 0.1773, t0: 0 },
  { name: 'Eros', a: 1.458, e: 0.223, I: 10.83, Om: 304.32, w: 178.82, M0: 320.6, n: 0.559, t0: 0 },
  { name: 'Psyche', a: 2.921, e: 0.140, I: 3.10, Om: 150.18, w: 227.74, M0: 91.3, n: 0.197, t0: 0 },
  { name: 'Bennu', a: 1.1264, e: 0.2037, I: 6.035, Om: 2.061, w: 66.223, M0: 101.4, n: 0.8244, t0: 0 },
  { name: 'Apophis', a: 0.9224, e: 0.1912, I: 3.331, Om: 204.45, w: 126.39, M0: 247.0, n: 1.112, t0: 0 },
  { name: 'Itokawa', a: 1.324, e: 0.280, I: 1.621, Om: 69.08, w: 162.82, M0: 320.0, n: 0.646, t0: 0 },
  { name: 'Ryugu', a: 1.190, e: 0.190, I: 5.884, Om: 251.62, w: 211.43, M0: 3.0, n: 0.758, t0: 0 }
];

function allBodies(date) {
  var d = daysJ2000(date);
  var out = [];
  var sun = sunRaDec(d);
  out.push(sun);
  out.push(moonRaDec(d));
  for (var i = 0; i < PLANETS.length; i++) {
    out.push(planetRaDec(PLANETS[i], d));
  }
  return out;
}

function dwarfBodies(date, override) {
  var d = daysJ2000(date);
  var out = [];
  var i;
  for (i = 0; i < DWARF_FALLBACK.length; i++) {
    if (i === 1 && !(override && override[i])) {
      out.push(planetRaDec(PLUTO, d));
      continue;
    }
    var el = (override && override[i]) ? override[i] : DWARF_FALLBACK[i];
    out.push(orbitRaDec(el, d));
  }
  return out;
}

function asteroidBodies(date, override) {
  var d = daysJ2000(date);
  var out = [];
  var i;
  for (i = 0; i < ASTEROID_FALLBACK.length; i++) {
    var el = (override && override[i]) ? override[i] : ASTEROID_FALLBACK[i];
    out.push(orbitRaDec(el, d));
  }
  return out;
}

function orbitFromSbdb(el) {
  if (!el) {
    return null;
  }
  var epoch = parseFloat(el.epoch);
  var a = parseFloat(el.a);
  var n = parseFloat(el.n);
  if (!isFinite(n) && isFinite(a)) {
    n = 0.9856076686 / Math.pow(a, 1.5);
  }
  return {
    a: a,
    e: parseFloat(el.e),
    I: parseFloat(el.i),
    Om: parseFloat(el.om),
    w: parseFloat(el.w),
    M0: parseFloat(el.ma),
    n: n,
    t0: isFinite(epoch) ? (epoch - 2451545.0) : 0
  };
}

module.exports = {
  allBodies: allBodies,
  dwarfBodies: dwarfBodies,
  asteroidBodies: asteroidBodies,
  orbitFromSbdb: orbitFromSbdb,
  DWARF_QUERY: ['Ceres', '134340', 'Haumea', 'Makemake', 'Eris'],
  ASTEROID_QUERY: ['Vesta', 'Pallas', 'Juno', 'Hygiea', 'Eros', 'Psyche', 'Bennu', 'Apophis', 'Itokawa', 'Ryugu']
};
