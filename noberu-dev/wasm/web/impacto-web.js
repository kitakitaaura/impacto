// Test page glue for the impacto (STEINS;GATE Steam) browser build.
// The user's folder becomes Module.noberuFiles: path -> File, read on demand by
// src/io/webfilestream.cpp. /saves is IndexedDB-backed.
(() => {
  const DEV_FILE = '/tmp/devinput.txt';
  const status = document.getElementById('status');
  const say = (text) => { status.textContent = text; };

  function filesFromFolder(list) {
    const files = new Map();
    for (const file of list) {
      // "<picked folder>/USRDIR/bg.mpk" -> "/gamedata/sgsteam/USRDIR/bg.mpk"
      const rel = file.webkitRelativePath.split('/').slice(1).join('/');
      const lower = rel.toLowerCase();
      const wanted = (lower.startsWith('usrdir/') && lower.endsWith('.mpk')) ||
        ((lower.startsWith('usrdir/movie/1920x1080/') ||
          lower.startsWith('languagebarrier/videos/1080p/')) && lower.endsWith('.bk2')) ||
        // LanguageBarrier's movie subtitles, their fonts and movie audio.
        (lower.startsWith('languagebarrier/subs/') && lower.endsWith('.ass')) ||
        (lower.startsWith('languagebarrier/audio/') && lower.endsWith('.ogg')) ||
        (lower.startsWith('languagebarrier/subs/fonts/') && /\.(ttf|otf|ttc)$/.test(lower));
      if (!wanted) continue;
      files.set('/gamedata/sgsteam/' + rel, file);
    }
    return files;
  }

  async function start(files) {
    document.getElementById('picker').hidden = true;
    const canvas = document.getElementById('canvas');
    canvas.hidden = false;
    say('loading engine...');
    const config = window.impactoModule = {
      canvas,
      noberuFiles: files,
      print: (t) => console.log(t),
      printErr: (t) => console.warn(t),
      preRun: [(m) => {
        // ?mute=1: start silent (handy while testing).
        if (new URLSearchParams(location.search).has('mute')) m.ENV.IMPACTO_MUTE = '1';
        // ?dev=1: DevInput through an in-memory file, see impactoDev below.
        if (new URLSearchParams(location.search).has('dev')) m.ENV.IMPACTO_DEV_INPUT = DEV_FILE;
        // Loose files (movies) get empty stand-ins so folder mounts can list
        // them; reads go to the real File (src/io/webfilestream.cpp).
        for (const path of files.keys()) {
          if (path.toLowerCase().endsWith('.mpk')) continue;
          m.FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
          m.FS.writeFile(path, new Uint8Array(0));
        }
        m.FS.mkdir('/saves');
        m.FS.mount(m.IDBFS, {}, '/saves');
      }],
      onGameLoaded: () => say(''),
      onSaveWritten: () => module.FS.syncfs(false, (err) => {
        if (err) console.warn('save sync failed', err);
      }),
    };
    window.noberuStats?.marks && (window.noberuStats.marks.engineLoad = Math.round(performance.now()));
    const module = await createImpacto(config);
    window.noberuStats?.marks && (window.noberuStats.marks.engineReady = Math.round(performance.now()));
    // impactoDev('state') -> the reply line (src/devinput.h lists commands).
    window.impactoDev = async (cmd, prefix = 'Dev', timeoutMs = 5000) => {
      const { FS } = module;
      const read = (path) => { try { return FS.readFile(path, { encoding: 'utf8' }); } catch { return ''; } };
      const seen = read(DEV_FILE + '.out').length;
      FS.writeFile(DEV_FILE, read(DEV_FILE) + cmd + '\n');
      for (const end = Date.now() + timeoutMs; Date.now() < end;) {
        await new Promise((r) => setTimeout(r, 30));
        const line = read(DEV_FILE + '.out').slice(seen).split('\n')
          .find((l) => l.startsWith(prefix));
        if (line) return line;
      }
      return null;
    };
    // Restore saves before main() so the engine sees them at boot.
    await new Promise((resolve) => module.FS.syncfs(true, (err) => {
      if (err) console.warn('save restore failed', err);
      resolve();
    }));
    say('starting...');
    canvas.focus();
    // userconfig.toml lives with the saves so settings persist too.
    window.noberuStats?.marks && (window.noberuStats.marks.main = Math.round(performance.now()));
    module.callMain(['-bp', '/web/basepaths.lua', '-gc', '/web/gamedefinitions.lua',
                     '-uc', '/saves/userconfig.toml', '-g', 'sgsteam']);
  }

  // Dev mode (?local=1 with serve.mjs): File-like objects whose slice() fetches
  // byte ranges from the dev server, so the engine path is the same as with a
  // picked folder.
  // Start-up profile for ?local=1 runs: reads (count, bytes, ms waiting)
  // and timestamps since navigation (window.noberuStats).
  const stats = window.noberuStats = { reads: 0, bytes: 0, readMs: 0, marks: {} };
  const mark = (name) => { stats.marks[name] = Math.round(performance.now()); };
  class RangeFile {
    constructor(url, size) { this.url = new URL(url, location.href).href; this.size = size; }
    slice(start, end) {
      const url = this.url;
      return { arrayBuffer: async () => {
        const t = performance.now();
        const res = await fetch(url, { headers: { Range: `bytes=${start}-${end - 1}` } });
        const data = await res.arrayBuffer();
        stats.reads++; stats.bytes += data.byteLength; stats.readMs += performance.now() - t;
        return data;
      } };
    }
  }
  if (new URLSearchParams(location.search).has('local')) {
    fetch('gamedata.json').then((r) => r.json()).then((list) => {
      const files = new Map(list.map(({ path, size }) =>
        ['/gamedata/sgsteam/' + path, new RangeFile('gamedata/' + path, size)]));
      start(files).catch((err) => { console.error(err); say(String(err)); });
    });
  }

  document.getElementById('folder').addEventListener('change', (e) => {
    const files = filesFromFolder(e.target.files);
    if (!files.size) { say('No USRDIR/*.mpk files in that folder.'); return; }
    say(`${files.size} archives found`);
    start(files).catch((err) => { console.error(err); say(String(err)); });
  });
})();
