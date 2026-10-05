// Dev server for the impacto web build: serves out/ and, for ?local=1 testing,
// the game folder under /gamedata/ with HTTP Range support plus a JSON listing.
//   node noberu-dev/wasm/serve.mjs [port]     (game folder: ../../gamedata/sgsteam)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const out = path.join(here, 'out');
const game = fs.realpathSync(path.join(here, '../../gamedata/sgsteam'));
const port = Number(process.argv[2] || 8765);
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
                '.json': 'application/json', '.data': 'application/octet-stream' };

function listArchives() {
  const dir = path.join(game, 'USRDIR');
  return fs.readdirSync(dir).filter((f) => f.endsWith('.mpk'))
    .map((f) => ({ path: 'USRDIR/' + f, size: fs.statSync(path.join(dir, f)).size }));
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
                      'accept-ranges': 'bytes', 'cache-control': 'no-store' };
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
