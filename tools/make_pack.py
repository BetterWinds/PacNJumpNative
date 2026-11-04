#!/usr/bin/env python3
"""Builds a game-data pack from your own copy of the original Android APK.

    python make_pack.py com.namcobandaigames.pacnjump_and.apk            -> pnj.pack
    python make_pack.py game.apk -o /somewhere/pnj.pack
    python make_pack.py game.apk --dir out_assets                        -> loose files instead of a pack (debugging)

The engine ships no art, sound or level data. Everything it needs from the game is read from the pack at start-up:
  tex/<name>.png     sprite sheets (the APK's own hdpi PNGs, copied byte for byte)
  audio/<name>.ogg   sound effects and music (the APK's own ogg files; wakawaka needs ffmpeg, see below)
  maps/<name>.map    the 42 hand-made pickup layouts (res/raw/*.map, copied byte for byte)
  lang/language.*    string tables (the Help pages are read from lang/language.eng)
  meta/atlas.txt     the 224 sprite rectangles, read out of classes.dex by dexatlas.py ("tex x y w h tw" per line)
  meta.json          source APK hash, tool version, what was found

Needs only Python 3. ffmpeg is optional: it converts the power-up jingle (wakawaka.3gp, AAC) to ogg.
Without it that one sound is skipped and the stage music keeps playing while powered up.

Pack format (little endian): "PNJPACK1", u32 count, count x { u16 name_len, name, u32 offset, u32 size }, then the file data.
"""
import argparse, hashlib, json, os, shutil, struct, subprocess, sys, tempfile, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dexatlas

TOOL_VERSION = 2
TEXTURES = ['bnglogo', 'bndr_logo', 'sysparts', 'stages', 'charaparts'] + ['stage_%d_%s' % (i, k) for i in range(1, 9) for k in ('map', 'cha')]
SFX = ['balloon', 'bound', 'cancel', 'candle', 'decide', 'eat01', 'eat02', 'eat03', 'eat04', 'eat05', 'eat06', 'eat07', 'eat08', 'eat_enemy',
       'eat_item', 'eat_pow', 'gameover', 'hat', 'hiscore', 'jump', 'mappy_jump', 'miss', 'select', 'st8_warning']
RAW_RENAME = {'breeak': 'break'}                       # the APK's own spelling
MAP_COUNT = 42


def png_size(b):
    return struct.unpack('>II', b[16:24]) if b[:8] == b'\x89PNG\r\n\x1a\n' else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('apk'); ap.add_argument('-o', '--out', default='pnj.pack'); ap.add_argument('--dir', help='write loose files here instead of a pack')
    a = ap.parse_args()
    if not os.path.isfile(a.apk): sys.exit('not found: ' + a.apk)
    z = zipfile.ZipFile(a.apk)
    names = set(z.namelist())
    files = {}; notes = []; missing = []

    if 'AndroidManifest.xml' not in names or 'res/drawable-hdpi/sysparts.png' not in names:
        sys.exit('This does not look like the Pac\'N-Jump Android APK (no res/drawable-hdpi/sysparts.png).')

    for t in TEXTURES:
        p = 'res/drawable-hdpi/%s.png' % t
        if p in names:
            b = z.read(p); sz = png_size(b)
            if t not in ('bnglogo', 'bndr_logo') and sz != (1024, 1024): sys.exit('%s is %s, expected 1024x1024: this is not the hdpi build the engine expects' % (p, sz))
            files['tex/%s.png' % t] = b
        elif t not in ('bnglogo', 'bndr_logo'): missing.append(p)

    for n in sorted(names):
        if n.startswith('res/raw/') and n.endswith('.map'): files['maps/%s.map' % n[8:-4]] = z.read(n)
    if len(files) and sum(k.startswith('maps/') for k in files) != MAP_COUNT: missing.append('expected %d level maps, found %d' % (MAP_COUNT, sum(k.startswith('maps/') for k in files)))

    for n in sorted(names):
        if n.startswith('res/raw/') and n.endswith('.ogg'):
            base = n[8:-4]
            if base.startswith('pack_action_bgm'): base = base[len('pack_action_'):]        # pack_action_bgm1 -> bgm1
            files['audio/%s.ogg' % RAW_RENAME.get(base, base)] = z.read(n)
    for s in SFX:
        if 'audio/%s.ogg' % s not in files: missing.append('audio/%s.ogg' % s)

    ww = 'res/raw/wakawaka.3gp'
    if ww in names:
        ff = shutil.which('ffmpeg')
        if ff:
            with tempfile.TemporaryDirectory() as d:
                src = os.path.join(d, 'w.3gp'); dst = os.path.join(d, 'w.ogg'); open(src, 'wb').write(z.read(ww))
                r = subprocess.run([ff, '-v', 'error', '-y', '-i', src, '-ar', '44100', '-ac', '2', '-c:a', 'libvorbis', '-q:a', '5', dst], capture_output=True)
                if r.returncode == 0 and os.path.exists(dst): files['audio/wakawaka.ogg'] = open(dst, 'rb').read()
                else: notes.append('wakawaka.3gp: ffmpeg could not convert it (%s)' % r.stderr.decode(errors='ignore').strip()[:120])
        else: notes.append('wakawaka.3gp skipped: ffmpeg not found (stage music will keep playing while powered up)')

    for n in sorted(names):
        if n.startswith('assets/language.'): files['lang/' + n[7:]] = z.read(n)

    if 'classes.dex' in names:
        try: files['meta/atlas.txt'] = dexatlas.atlas_text(z.read('classes.dex'))
        except dexatlas.DexError as e: missing.append('meta/atlas.txt (%s)' % e)
    else: missing.append('classes.dex')
    if 'lang/language.eng' not in files: missing.append('lang/language.eng')

    meta = {'tool_version': TOOL_VERSION, 'apk_sha256': hashlib.sha256(open(a.apk, 'rb').read()).hexdigest(), 'apk_size': os.path.getsize(a.apk),
            'files': len(files), 'missing': missing, 'notes': notes}
    files['meta.json'] = json.dumps(meta, indent=1).encode()

    if a.dir:
        for k, b in files.items():
            p = os.path.join(a.dir, k); os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'wb').write(b)
        print('wrote %d files to %s' % (len(files), a.dir))
    else:
        keys = sorted(files); head = bytearray(b'PNJPACK1' + struct.pack('<I', len(keys)))
        toc_len = sum(2 + len(k.encode()) + 8 for k in keys); off = len(head) + toc_len; toc = bytearray()
        for k in keys:
            kb = k.encode(); toc += struct.pack('<H', len(kb)) + kb + struct.pack('<II', off, len(files[k])); off += len(files[k])
        with open(a.out, 'wb') as f:
            f.write(head); f.write(toc)
            for k in keys: f.write(files[k])
        print('wrote %s: %d files, %.1f MB' % (a.out, len(keys), os.path.getsize(a.out) / 1e6))
    for n in notes: print('note:', n)
    if missing:
        print('WARNING, not found in this APK:'); [print('  ', m) for m in missing]; sys.exit(2)


if __name__ == '__main__':
    main()
