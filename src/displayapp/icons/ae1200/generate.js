#!/usr/bin/env node
// Generates the static images used by WatchFaceCasioAE1200:
//   worldmap.c : 2 bpp indexed equirectangular world map with a 30 degree grid, plus a copy where only the
//                land is opaque, drawn in darker colours to highlight the local time zone
//   dial.c     : 4 bpp indexed analog dial background (bezel ring, ticks, LCD disc, inner circle, crosshair)
//   lcd.c      : 4 bpp indexed main LCD background: raised date block joined to the time area by a concave
//                curve, with its frame, rim, date divider and underline
// Usage: node generate.js   (writes the .c files next to this script)
// The colours must match the ones defined in WatchFaceCasioAE1200.h.
// labels.c is not generated here: it holds the printed texts traced from the reference design.

const fs = require("fs");
const path = require("path");

const colors = {
  tile: "#050505",
  black: "#000000",
  white: "#ffffff",
  tick: "#6a6a6a",
  lcd: "#6e7c82",
  seg: "#0a1216",
  land: "#4a565e",
  gridSea: "#63717a",
  gridLand: "#56636a",
  highlightLand: "#222a30",
  highlightGrid: "#2c363c",
  frame: "#161818",
  frameRim: "#34393a",
};

// Main LCD outline in screen coordinates. The LCD area is the union of the date block and the time area,
// plus a concave fillet of radius filletRadius where they meet. The frame and rim are the same shape grown by
// frameWidth and frameWidth + 1: growing rounded rectangles adds to their radius, and the fillet keeps its
// centre while its radius shrinks by the same amount.
const lcdGeometry = {
  x: 3, y: 134, width: 234, height: 94, // image position and size
  dateBlock: [107, 138, 233, 224], timeArea: [7, 164, 233, 224], radius: 7, filletRadius: 14, frameWidth: 3,
  divider: {x: 158, y0: 140, y1: 163}, underline: {y: 165, x0: 96, x1: 233},
};

function insideRoundRect(x, y, [x0, y0, x1, y1], r) {
  if (x < x0 || x > x1 || y < y0 || y > y1) return false;
  const cx = Math.min(Math.max(x, x0 + r), x1 - r), cy = Math.min(Math.max(y, y0 + r), y1 - r);
  return Math.hypot(x - cx, y - cy) <= r;
}

function insideLcdShape(x, y, grow, g) {
  const growRect = ([x0, y0, x1, y1]) => [x0 - grow, y0 - grow, x1 + grow, y1 + grow];
  if (insideRoundRect(x, y, growRect(g.dateBlock), g.radius + grow) || insideRoundRect(x, y, growRect(g.timeArea), g.radius + grow)) {
    return true;
  }
  // concave fillet in the corner between the left edge of the date block and the top of the time area
  const cornerX = g.dateBlock[0] - grow, cornerY = g.timeArea[1] - grow;
  const centerX = g.dateBlock[0] - g.filletRadius, centerY = g.timeArea[1] - g.filletRadius;
  return x >= centerX && x <= cornerX && y >= centerY && y <= cornerY && Math.hypot(x - centerX, y - centerY) >= g.filletRadius - grow;
}

function renderLcd(C, g) {
  const black = hex(C.black), rim = hex(C.frameRim), frame = hex(C.frame), lcd = hex(C.lcd), seg = hex(C.seg);
  const pal = [black, rim, frame, lcd, seg, mix(black, rim, 0.5), mix(rim, frame, 0.5), mix(frame, lcd, 0.25), mix(frame, lcd, 0.5),
    mix(frame, lcd, 0.75), mix(lcd, seg, 0.25), mix(lcd, seg, 0.5), mix(lcd, seg, 0.75), mix(black, frame, 0.5), mix(rim, lcd, 0.5),
    mix(black, rim, 0.25)];
  const ss = 4;
  function sample(x, y) {
    if (insideLcdShape(x, y, 0, g)) {
      const onDivider = Math.abs(x - (g.divider.x + 0.5)) <= 0.5 && y >= g.divider.y0 && y <= g.divider.y1 + 1;
      const onUnderline = Math.abs(y - (g.underline.y + 0.5)) <= 0.5 && x >= g.underline.x0 && x <= g.underline.x1;
      return onDivider || onUnderline ? seg : lcd;
    }
    if (insideLcdShape(x, y, g.frameWidth, g)) return frame;
    if (insideLcdShape(x, y, g.frameWidth + 1, g)) return rim;
    return black;
  }
  const rows = [];
  for (let py = 0; py < g.height; py++) {
    const row = [];
    for (let px = 0; px < g.width; px++) {
      let acc = [0, 0, 0];
      for (let sy = 0; sy < ss; sy++) for (let sx = 0; sx < ss; sx++) {
        const s = sample(g.x + px + (sx + 0.5) / ss, g.y + py + (sy + 0.5) / ss);
        acc = acc.map((v, i) => v + s[i]);
      }
      acc = acc.map((v) => v / (ss * ss));
      let best = 0, bd = Infinity;
      pal.forEach((q, i) => {
        const d = (q[0] - acc[0]) ** 2 + (q[1] - acc[1]) ** 2 + (q[2] - acc[2]) ** 2;
        if (d < bd) { bd = d; best = i; }
      });
      row.push(best);
    }
    rows.push(row);
  }
  return { rows, pal };
}

// Simplified coastlines as [lon, lat] polygons, rasterized into a small equirectangular map.
const LAND = {
  northAmerica: [[-168,66],[-165,69],[-156,71.3],[-145,70],[-135,69],[-125,70],[-115,68.5],[-105,68],[-95,68],[-90,69],[-85,68],[-82,66],[-86,64],[-90,63.5],[-94,61],[-93,58.5],[-88,56.5],[-82,55],[-82,52],[-79,51.5],[-78,55],[-77,58],[-78,60.5],[-76,62.5],[-72,62],[-68,60],[-65,60],[-64,58],[-61,56],[-56,52.5],[-60,50],[-64,48.5],[-65,46],[-61,45.5],[-66,44.5],[-70,43.5],[-70,41.7],[-74,40.5],[-76,38],[-76,35],[-78,34],[-81,31.5],[-80.5,28],[-80,25.3],[-82,26.5],[-83,29.5],[-85,30],[-89,30.3],[-90,29.2],[-94,29.7],[-97,27.8],[-97.5,25],[-97.5,22],[-96,19],[-94.5,18.2],[-91,18.8],[-90.5,21],[-87,21.5],[-88,18],[-88.5,16],[-84,15.8],[-83.5,13],[-83.7,11],[-81.5,9],[-79,9.5],[-77.5,8.5],[-80,7.5],[-82,8.2],[-85.5,10],[-87.5,13],[-91.5,14],[-94.5,16],[-97,16],[-101,17.5],[-105.5,20],[-105.7,22.5],[-109,25.6],[-112,29],[-114.7,31.5],[-112.5,28],[-110,24],[-109.5,23],[-112,24.8],[-114,27.5],[-115.8,30],[-117,32.5],[-118.5,34],[-120.6,34.6],[-122.5,37.5],[-124,40.5],[-124.5,43],[-124,46],[-124.7,48.4],[-123,49],[-127,50.5],[-130,54],[-133,57],[-137,58.5],[-140,60],[-146,61],[-150,59.5],[-154,57.5],[-158,56.5],[-162,55],[-164,55],[-158,58],[-162,59],[-165,61],[-165,63],[-161,64.5],[-166,65.3]],
  greenland: [[-73,78],[-60,82],[-40,83.5],[-22,82.5],[-18,79],[-20,75],[-22,71],[-25,69],[-32,68],[-40,65],[-43,60],[-48,61],[-52,65],[-54,68],[-55,70.5],[-58,75],[-66,76]],
  baffin: [[-80,73.5],[-73,71.5],[-68,70],[-63,66.5],[-65,63],[-72,64.5],[-78,64.5],[-80,68],[-85,70],[-90,72],[-85,73.5]],
  ellesmere: [[-90,80],[-75,83],[-62,82],[-70,79.5],[-78,77],[-88,76.5]],
  devon: [[-96,77],[-80,76.5],[-80,74.5],[-92,74.5]],
  victoria: [[-125,72],[-122,74.2],[-117,74.5],[-105,73.5],[-101,70],[-108,68.8],[-117,69.5]],
  cuba: [[-85,22],[-80,23.2],[-75,20],[-77.5,19.8],[-82,21.8]],
  hispaniola: [[-74.5,19.8],[-68.5,18.6],[-71,17.8],[-74.4,18.4]],
  southAmerica: [[-77.5,8.5],[-76,9.5],[-72,12],[-68,11],[-63,10.7],[-61,10],[-58,6.8],[-54,5.8],[-51.5,4.3],[-50,1.8],[-48.5,-1],[-44.5,-2.5],[-41,-3],[-37,-4.8],[-35,-6],[-35,-9],[-37,-12],[-39,-14.5],[-39,-17.5],[-40.5,-21],[-42,-23],[-45,-23.8],[-48.5,-26],[-48.7,-28.5],[-51,-31],[-53,-33.8],[-55,-35],[-57.5,-36],[-57,-38.2],[-62,-39],[-62.3,-41],[-65,-42],[-64.5,-45],[-67.5,-46.5],[-66,-48],[-68.5,-50.5],[-68.3,-52.4],[-70,-53.5],[-74,-53],[-75.5,-48],[-74.5,-44],[-73.5,-40],[-73.5,-36.5],[-71.5,-32],[-71.5,-28],[-70.5,-23],[-70.2,-18.4],[-75,-15.5],[-77,-12],[-79.5,-7.5],[-81,-5],[-80.5,-2],[-80,0.5],[-79,1.5],[-78,3],[-77.5,6.5]],
  eurasia: [[-8.8,37],[-9.5,39],[-8.8,42],[-9.3,43.2],[-8,43.7],[-1.8,43.4],[-1.2,46],[-2.5,47.3],[-4.7,48],[-1.5,48.7],[1.5,50.2],[3,51.3],[4.5,53],[7,53.5],[8.5,55],[8.2,57],[10.5,57.7],[10.8,56],[12.5,54.5],[14,54],[18,54.8],[21,55],[21.5,57.5],[24,57.5],[24,59.2],[28,59.7],[22,60.3],[21.5,63],[25,65],[22,65.8],[17.5,62.5],[18.8,60],[16.5,57],[14.5,56],[12.8,55.6],[11.5,58],[8,58],[5.5,59],[5,62],[8,63.5],[12,66],[15,68.5],[18,69.8],[23,70.6],[28,71],[31,70],[33,69.3],[40,67.5],[44,68.5],[53,68.5],[60,69],[66,69.5],[69,73],[73,71],[80,72.5],[87,74],[97,76],[104,77.7],[113,73.7],[120,73],[128,72.5],[131,71],[140,72.5],[150,71.5],[160,70],[170,70],[178,69.5],[180,68.5],[180,65],[178,64.5],[177,62.5],[170,60],[163,60],[164,58],[162,56],[156,51],[156,57],[160,61.5],[155,59.5],[150,59.5],[143,59.3],[138,56],[141,53],[140,48],[135,43.5],[131,42.6],[129.5,41],[129.5,36],[127,34.8],[126.3,37.5],[125,39.5],[121.5,39],[122,40.8],[118,39.2],[119,37.2],[122.5,37],[119.5,34.5],[121.8,31.5],[122,30],[120,26],[116.5,23],[113.5,22.2],[110,21],[108,21.5],[106,20],[105.5,18.5],[108.8,15],[109,11.5],[105,8.6],[103,10.5],[100.5,13.5],[100,12],[99,10],[100.3,8],[101.5,6.8],[103.4,4.2],[104.2,1.4],[101,2.8],[98.5,8],[98.3,10],[98.5,13],[97.5,16.5],[94.3,16],[94.2,18.8],[92.3,21],[90,22],[87,21.5],[86,20],[84,18],[82,16.5],[80.2,15],[80.2,13],[79.8,10.3],[77.5,8],[76.5,8.8],[74,14.5],[72.8,19],[72.5,21.5],[70.5,21],[68.8,23],[66.5,25.4],[61.5,25.2],[57.3,25.8],[56.4,27],[54,26.7],[50.8,29.5],[48,30],[48.5,28.5],[50.2,26.6],[51.5,24.5],[54,24.2],[56,26],[57,23.8],[59.7,22.5],[58.5,20.5],[55,17.3],[52,15.7],[48,13.9],[44,12.6],[43,14],[42.7,16.8],[40.8,19.8],[39,22],[37,25.8],[35,28.1],[34.9,29.5],[32.6,29.9],[34.5,31.5],[35,33],[36,35.7],[36.2,36.6],[33,36.1],[30,36.2],[27.5,36.7],[26.2,39.2],[26,40.8],[23.5,40.2],[24,38],[22,36.5],[21,38.5],[19.5,40.5],[19.3,42],[16,43.5],[13.7,45.5],[12.4,45.5],[12.3,44.3],[14,42],[16,41],[18.5,40.2],[16.5,38.8],[16,38],[15.7,40],[12.5,41.8],[10.5,43],[8.7,44.4],[7,43.6],[4,43.5],[3.2,42],[0.9,41],[0,39.5],[-0.7,37.6],[-2.2,36.7],[-5.3,36.1],[-6.3,36.8],[-7.5,37.1]],
  chukotka: [[-180,68.5],[-174,67],[-170,66],[-172,64.5],[-180,65]],
  britain: [[-5.7,50],[1.5,51.2],[1.7,52.7],[0,53.5],[-1.5,55.5],[-2,57],[-3,58.6],[-5,58.6],[-6,57.2],[-5.5,55.5],[-4.8,54.8],[-3,54],[-3,53.3],[-4.5,52.8],[-5.2,51.7],[-3,51.4]],
  ireland: [[-6,52],[-6.2,54],[-8,55.2],[-10,54.2],[-10.2,51.6]],
  iceland: [[-24,65.5],[-18,66.5],[-13.5,65.2],[-18,63.4],[-22.5,63.8]],
  svalbard: [[11,78.5],[17,80.5],[27,80],[22,77.5],[16,76.5]],
  novayaZemlya: [[52,71],[57,70.6],[60,75],[68,76.9],[62,76.3],[55,73.7]],
  severnaya: [[95,79],[105,78.5],[102,81],[95,80.5]],
  sakhalin: [[142,46],[143.5,49],[143,54],[142.5,54.3],[141.8,51]],
  honshu: [[130,31],[131.5,33.5],[135,33.5],[140,35],[141,38],[141.5,41.3],[140,40.5],[139.8,38],[137,37],[133,35.5],[130.8,34]],
  hokkaido: [[140,41.5],[145.5,43.3],[144,44],[141.7,45.4],[140,43]],
  taiwan: [[120.2,22.5],[121,25],[122,25],[121,22]],
  sriLanka: [[79.8,6],[80,9.6],[81.8,7.5],[81,6]],
  philippines: [[120,18.5],[122.3,18.5],[124,12.5],[126,7],[125.5,6],[122,7],[124,10],[121,13.5],[120,15.5]],
  sumatra: [[95.3,5.6],[98,4],[104,-1],[106,-5.8],[104.5,-5.9],[101,-2.5],[98.5,1]],
  java: [[105.2,-6.8],[114.5,-7.7],[114.5,-8.7],[106.5,-7.4]],
  borneo: [[109,1.8],[110,-3],[114,-4],[116.3,-3.5],[118,1],[119,5],[117,7],[115,5],[111,2.5]],
  sulawesi: [[119.5,-5.5],[120.5,0.5],[125,1.5],[121,-1],[122,-4.5]],
  newGuinea: [[131,-1],[135,-3.3],[138,-1.5],[141,-2.6],[145.5,-4.5],[147.5,-6],[150.5,-10.5],[146,-8],[143,-9.2],[141,-9],[138,-8.3],[137.8,-5.3],[133,-4],[131.5,-2.5]],
  australia: [[113.5,-22],[114,-26.5],[115,-34],[118,-35],[123,-33.9],[126,-32.3],[131,-31.5],[134,-32.8],[135.7,-34.8],[138,-34.3],[138.5,-35.5],[140,-37.8],[144,-38.3],[146.5,-39],[150,-37.5],[151.3,-33.7],[153.5,-28.5],[153.2,-25],[150.8,-22.5],[149,-20.5],[146.3,-18.8],[145.3,-15],[143.5,-14],[142.5,-10.7],[141.6,-12.8],[141.6,-16.5],[140.8,-17.4],[139,-17],[136,-15],[136.8,-12.2],[132.5,-11.4],[130.2,-13],[129.4,-14.9],[127.8,-14.2],[125.2,-15],[122.3,-17.1],[121.5,-19.5],[118.8,-20.3],[116.5,-20.8]],
  tasmania: [[144.7,-40.7],[148.3,-41],[147.3,-43.3],[145.5,-42.5]],
  nzNorth: [[172.7,-34.5],[175.5,-37],[178.5,-37.7],[177,-39.5],[175,-41.5],[173.8,-39.2],[174.6,-36.8]],
  nzSouth: [[172.8,-40.5],[174.3,-41.7],[172.8,-43.7],[171,-45],[168.3,-46.6],[166.5,-45.8],[168.3,-44],[170.8,-42.7]],
  madagascar: [[49.3,-12],[50.5,-15.5],[47.2,-24.8],[45,-25.5],[43.5,-22],[44.3,-16.2],[47,-15.5]],
  africa: [[-5.9,35.8],[-2,35.1],[1,36.5],[3,36.9],[8,36.9],[10.2,37.2],[11.1,36.8],[10.3,33.8],[11.2,33.2],[15.2,32.3],[19,30.3],[20,32],[23,32.6],[25,31.6],[29,30.9],[32.3,31.3],[32.6,29.9],[33.6,28],[35.3,24],[37.4,18.8],[38.6,18],[39.7,15.5],[41.7,13.4],[43.3,12.5],[43.2,11.5],[45,10.4],[51.2,11.8],[50.8,10],[48.5,4.5],[46,2],[43,-0.8],[40.5,-2.5],[39.2,-4.7],[39.3,-8],[40.5,-10.5],[40.5,-15],[37,-17.5],[35.3,-22],[35.5,-24],[32.8,-25.8],[32.6,-28.5],[31,-30],[28,-33],[25.5,-34],[22.5,-34],[20,-34.8],[18.4,-34],[18.2,-32],[17,-29.5],[15.2,-27],[14.5,-22.5],[11.8,-17.2],[13.5,-12],[12.3,-6],[11.8,-3.5],[9.5,-0.7],[9.5,3.5],[8.5,4.5],[5.8,4.3],[4.5,6.3],[1.5,6.1],[-2,4.8],[-4.5,5.2],[-7.5,4.4],[-9.5,5.5],[-11.5,6.9],[-13.3,8.8],[-15,10.9],[-16.8,13.5],[-17.4,14.7],[-16.5,16.2],[-16.1,19.5],[-17,21],[-15,24],[-13.5,26.7],[-11.5,28],[-9.7,30],[-9.8,32],[-8,33.5],[-6.8,34.1]],
};
const WATER = {
  blackSea: [[28,41.5],[28.5,43.5],[30,45.5],[33,46],[35,45.5],[37,47],[39.5,47],[38,44.5],[41.5,42],[36,41.2],[32,41.7]],
  caspian: [[47,44.5],[50,46.7],[53.5,46.5],[53,42],[54,38],[51.3,36.6],[49,38],[49.5,40.5],[47,43]],
};

function inside(poly, x, y) {
  let c = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, yi] = poly[i], [xj, yj] = poly[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) c = !c;
  }
  return c;
}

// Returns rows of palette indices: 0 sea, 1 grid over sea, 2 land, 3 grid over land.
// ss = supersampling per axis; a pixel is land when at least `cover` of its samples are.
function rasterize(w, h, latTop, latBottom, opts = {}) {
  const ss = opts.ss || 4, cover = opts.cover ?? 0.35, gridLon = opts.gridLon || 30, gridLat = opts.gridLat || 30;
  const lonAt = (px) => -180 + (px / w) * 360;
  const latAt = (py) => latTop - (py / h) * (latTop - latBottom);
  const polys = Object.values(LAND), holes = Object.values(WATER);
  const rows = [];
  const gx = new Set(), gy = new Set();
  for (let lon = -180 + gridLon; lon < 180; lon += gridLon) gx.add(Math.round(((lon + 180) / 360) * w - 0.5));
  for (let lat = 90 - gridLat; lat > latBottom; lat -= gridLat) if (lat < latTop) gy.add(Math.round(((latTop - lat) / (latTop - latBottom)) * h - 0.5));
  for (let py = 0; py < h; py++) {
    const row = [];
    for (let px = 0; px < w; px++) {
      let n = 0;
      for (let sy = 0; sy < ss; sy++) for (let sx = 0; sx < ss; sx++) {
        const lon = lonAt(px + (sx + 0.5) / ss), lat = latAt(py + (sy + 0.5) / ss);
        if (polys.some((p) => inside(p, lon, lat)) && !holes.some((p) => inside(p, lon, lat))) n++;
      }
      const land = n / (ss * ss) >= cover;
      const grid = gx.has(px) || gy.has(py);
      row.push((land ? 2 : 0) + (grid ? 1 : 0));
    }
    rows.push(row);
  }
  return rows;
}


// Renders the static part of the analog dial (bezel ring, ticks, LCD disc, crosshair) into a palette-indexed image.
const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
const mix = (a, b, t) => a.map((v, i) => Math.round(v + (b[i] - v) * t));

function dialPalette(C) {
  const tile = hex(C.tile), black = hex(C.black), white = hex(C.white), grey = hex(C.tick), lcd = hex(C.lcd), seg = hex(C.seg);
  return [tile, black, white, grey, lcd, seg,
    mix(black, white, 0.25), mix(black, white, 0.5), mix(black, white, 0.75),
    mix(black, grey, 0.5), mix(lcd, seg, 0.25), mix(lcd, seg, 0.5), mix(lcd, seg, 0.75), mix(tile, black, 0.5),
    mix(grey, white, 0.5), mix(black, lcd, 0.5)];
}

// size: image width/height; geometry radii in pixels relative to the image centre.
function renderDial(size, C, g) {
  const pal = dialPalette(C);
  const col = { tile: hex(C.tile), black: hex(C.black), white: hex(C.white), grey: hex(C.tick), lcd: hex(C.lcd), seg: hex(C.seg) };
  const c = size / 2, ss = 4;
  // distance from point to a radial segment at angle a (deg, clockwise from 12 o'clock) between radii r0..r1
  function radial(x, y, a, r0, r1, halfW) {
    const t = ((a - 90) * Math.PI) / 180, ux = Math.cos(t), uy = Math.sin(t);
    const along = x * ux + y * uy, across = Math.abs(-x * uy + y * ux);
    return along >= r0 && along <= r1 && across <= halfW;
  }
  function sample(x, y) {
    const r = Math.hypot(x, y);
    let p = col.tile;
    if (r <= g.ring) p = col.black;
    for (let i = 0; i < 60; i++) {
      if (i % 5 !== 0 && radial(x, y, i * 6, g.minorIn, g.ring - 1, 0.5)) p = col.grey;
    }
    for (let i = 0; i < 12; i++) {
      if (radial(x, y, i * 30, g.majorIn, g.ring - 1, 1)) p = col.white;
    }
    if (r <= g.disc) {
      p = col.lcd;
      if (Math.abs(r - g.circle) <= g.circleWidth / 2) p = col.seg;
      if (Math.abs(x) <= g.crossWidth / 2 || Math.abs(y) <= g.crossWidth / 2) p = col.seg;
    }
    return p;
  }
  const rows = [];
  for (let py = 0; py < size; py++) {
    const row = [];
    for (let px = 0; px < size; px++) {
      let acc = [0, 0, 0];
      for (let sy = 0; sy < ss; sy++) for (let sx = 0; sx < ss; sx++) {
        const s = sample(px + (sx + 0.5) / ss - c, py + (sy + 0.5) / ss - c);
        acc = acc.map((v, i) => v + s[i]);
      }
      acc = acc.map((v) => v / (ss * ss));
      let best = 0, bd = Infinity;
      pal.forEach((q, i) => {
        const d = (q[0] - acc[0]) ** 2 + (q[1] - acc[1]) ** 2 + (q[2] - acc[2]) ** 2;
        if (d < bd) { bd = d; best = i; }
      });
      row.push(best);
    }
    rows.push(row);
  }
  return { rows, pal };
}


// palette entries are [r, g, b] or [r, g, b, alpha]
function toCArray(name, rows, palette, bpp) {
  const w = rows[0].length, h = rows.length;
  const bytes = [];
  for (const [r, g, b, a = 0xff] of palette) bytes.push(b, g, r, a);
  const perByte = 8 / bpp;
  for (const row of rows) {
    for (let x = 0; x < w; x += perByte) {
      let v = 0;
      for (let k = 0; k < perByte; k++) v |= (row[x + k] ?? 0) << (8 - bpp * (k + 1));
      bytes.push(v);
    }
  }
  const lines = [];
  for (let i = 0; i < bytes.length; i += 16) {
    lines.push("  " + bytes.slice(i, i + 16).map((b) => "0x" + b.toString(16).padStart(2, "0")).join(", ") + ",");
  }
  return `const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t ${name}_map[] = {
${lines.join("\n")}
};

const lv_img_dsc_t ${name} = {
  {
    LV_IMG_CF_INDEXED_${bpp}BIT,
    0,
    0,
    ${w},
    ${h},
  },
  ${bytes.length},
  ${name}_map,
};
`;
}

const fileHeader = `// Generated by generate.js, do not edit.
#include "lvgl/lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

`;

const mapRows = rasterize(112, 44, 80, -58, {cover: 0.35});
const mapPalette = [hex(colors.lcd), hex(colors.gridSea), hex(colors.land), hex(colors.gridLand)];
const highlightPalette = [[0, 0, 0, 0], [0, 0, 0, 0], hex(colors.highlightLand), hex(colors.highlightGrid)];
fs.writeFileSync(path.join(__dirname, "worldmap.c"),
                 fileHeader + toCArray("ae1200_worldmap", mapRows, mapPalette, 2) + "\n" +
                   toCArray("ae1200_worldmap_highlight", mapRows, highlightPalette, 2));

const dial = renderDial(100, colors, {ring: 49, minorIn: 46.5, majorIn: 45, disc: 43, circle: 30.7, circleWidth: 3, crossWidth: 2});
fs.writeFileSync(path.join(__dirname, "dial.c"), fileHeader + toCArray("ae1200_dial", dial.rows, dial.pal, 4));

const lcdImage = renderLcd(colors, lcdGeometry);
fs.writeFileSync(path.join(__dirname, "lcd.c"), fileHeader + toCArray("ae1200_lcd", lcdImage.rows, lcdImage.pal, 4));
