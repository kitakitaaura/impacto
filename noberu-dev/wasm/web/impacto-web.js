// Test page glue for the impacto (STEINS;GATE Steam) browser build.
// The user's folder becomes Module.noberuFiles: path -> File, read on demand by
// src/io/webfilestream.cpp. /saves is IndexedDB-backed.
(() => {
  const status = document.getElementById('status');
  const say = (text) => { status.textContent = text; };

  function filesFromFolder(list) {
    const files = new Map();
    for (const file of list) {
      // "<picked folder>/USRDIR/bg.mpk" -> "/gamedata/sgsteam/USRDIR/bg.mpk"
      const rel = file.webkitRelativePath.split('/').slice(1).join('/');
      if (!rel.startsWith('USRDIR/') || !rel.toLowerCase().endsWith('.mpk')) continue;
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
        m.FS.mkdir('/saves');
        m.FS.mount(m.IDBFS, {}, '/saves');
      }],
      onGameLoaded: () => say(''),
      onSaveWritten: () => module.FS.syncfs(false, (err) => {
        if (err) console.warn('save sync failed', err);
      }),
    };
    const module = await createImpacto(config);
    // Restore saves before main() so the engine sees them at boot.
    await new Promise((resolve) => module.FS.syncfs(true, (err) => {
      if (err) console.warn('save restore failed', err);
      resolve();
    }));
    say('starting...');
    canvas.focus();
    // userconfig.toml lives with the saves so settings persist too.
    module.callMain(['-bp', '/web/basepaths.lua', '-gc', '/web/gamedefinitions.lua',
                     '-uc', '/saves/userconfig.toml', '-g', 'sgsteam']);
  }

  // Dev mode (?local=1 with serve.mjs): File-like objects whose slice() fetches
  // byte ranges from the dev server, so the engine path is the same as with a
  // picked folder.
  class RangeFile {
    constructor(url, size) { this.url = url; this.size = size; }
    slice(start, end) {
      const url = this.url;
      return { arrayBuffer: async () => {
        const res = await fetch(url, { headers: { Range: `bytes=${start}-${end - 1}` } });
        return res.arrayBuffer();
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
