// Dev server for the impacto web build: serves out/ and, for ?local=1 testing,
// the game folder under /gamedata/ with HTTP Range support plus a JSON listing.
//   node noberu-dev/wasm/serve.mjs [port] [build folder, default out]
//   (game folder: ../../gamedata/sgsteam). Pages are cross-origin isolated,
//   which the threaded build (out-mt) needs for SharedArrayBuffer.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const out = path.join(here, process.argv[3] || 'out');
const game = fs.realpathSync(path.join(here, '../../gamedata/sgsteam'));
const port = Number(process.argv[2] || 8765);
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
                '.json': 'application/json', '.data': 'application/octet-stream' };

// The archives, plus the 1080p movies and the English patch's replacements,
// movie subtitles, subtitle fonts and movie audio.
function listArchives() {
  const list = (sub, ...exts) => {
    const dir = path.join(game, sub);
    if (!fs.existsSync(dir)) return [];
    return fs.readdirSync(dir).filter((f) => exts.some((e) => f.toLowerCase().endsWith(e)))
      .map((f) => ({ path: sub + '/' + f, size: fs.statSync(path.join(dir, f)).size }));
  };
  return [...list('USRDIR', '.mpk'), ...list('USRDIR/movie/1920x1080', '.bk2'),
          ...list('languagebarrier/videos/1080p', '.bk2'),
          ...list('languagebarrier/subs', '.ass'),
          ...list('languagebarrier/subs/fonts', '.ttf', '.otf', '.ttc'),
          ...list('languagebarrier/audio', '.ogg')];
}

http.createServer((req, res) => {
  const url = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  if (url === '/gamedata.json') {
    res.writeHead(200, { 'content-type': 'application/json' });
    return res.end(JSON.stringify(listArchives()));
  }
  const file = url.startsWith('/gamedata/')
    ? path.join(game, url.slice('/gamedata/'.length))
    : path.join(out, url === '/' ? 'index.html' : url);
  if (!file.startsWith(game) && !file.startsWith(out)) { res.writeHead(403); return res.end(); }
  fs.stat(file, (err, st) => {
    if (err || !st.isFile()) { res.writeHead(404); return res.end(); }
    const headers = { 'content-type': types[path.extname(file)] || 'application/octet-stream',
                      'accept-ranges': 'bytes', 'cache-control': 'no-store',
                      'cross-origin-opener-policy': 'same-origin',
                      'cross-origin-embedder-policy': 'require-corp' };
    const range = /bytes=(\d+)-(\d*)/.exec(req.headers.range || '');
    if (range) {
      const start = Number(range[1]);
      const end = range[2] ? Number(range[2]) : st.size - 1;
      res.writeHead(206, { ...headers, 'content-range': `bytes ${start}-${end}/${st.size}`,
                           'content-length': end - start + 1 });
      return fs.createReadStream(file, { start, end }).pipe(res);
    }
    res.writeHead(200, { ...headers, 'content-length': st.size });
    fs.createReadStream(file).pipe(res);
  });
}).listen(port, () => console.log(`impacto web: http://localhost:${port}/`));
