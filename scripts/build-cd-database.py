"""Build the optional offline CD catalog from MusicBrainz's CC0 core dump.

Uses Python's standard library on the build machine; no database engine is
required on Windows 98. Downloaded sources and generated catalogs stay out of Git.
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import sqlite3
import struct
import tarfile
import time

TABLES = {'cdtoc', 'medium_cdtoc', 'medium', 'release', 'artist_credit', 'track'}
HEADER = struct.Struct('<8s6I')
ENTRY = struct.Struct('<4I')


def unpack_text(text):
    if text == r'\N':
        return ''
    result, i = [], 0
    escapes = {'n': '\n', 'r': '\r', 't': '\t', 'b': '\b', 'f': '\f', 'v': '\v'}
    while i < len(text):
        if text[i] == '\\' and i + 1 < len(text):
            i += 1
            result.append(escapes.get(text[i], text[i]))
        else:
            result.append(text[i])
        i += 1
    return ''.join(result)


def signature(offsets):
    value = 2166136261
    for number in [len(offsets)] + offsets:
        for byte in struct.pack('<I', number):
            value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def text_bytes(value):
    # Bound native allocations and preserve complete UTF-8 characters.
    value = ' '.join(unpack_text(value).replace('\0', '').split())
    return value.encode('utf-8')[:511].decode('utf-8', 'ignore').encode('utf-8') + b'\0'


def rows(folder, table):
    print('Reading', table, flush=True)
    with (folder / table).open(encoding='utf-8') as stream:
        for count, line in enumerate(stream, 1):
            if count % 1000000 == 0:
                print(table, count, 'rows scanned', flush=True)
            yield line.rstrip('\n').split('\t')


def extract(archive, folder):
    folder.mkdir(parents=True, exist_ok=True)
    if all((folder / table).is_file() for table in TABLES):
        print('Using previously extracted core tables', flush=True)
        return
    # Read a fixed allowlist, never extract arbitrary archive paths.
    with tarfile.open(archive, 'r|bz2') as source:
        for member in source:
            name = member.name.removeprefix('./')
            if name.startswith('mbdump/') and name[7:] in TABLES and member.isfile():
                target = folder / name[7:]
                print('Extracting', name, member.size, 'bytes', flush=True)
                with source.extractfile(member) as inp, target.with_suffix('.tmp').open('wb') as out:
                    shutil.copyfileobj(inp, out, 1024 * 1024)
                target.with_suffix('.tmp').replace(target)
    if not all((folder / table).is_file() for table in TABLES):
        raise ValueError('Core dump is missing required tables')


def import_tables(folder, scratch):
    db = sqlite3.connect(scratch)
    db.executescript('''PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;
        PRAGMA cache_size=-131072;
        DROP TABLE IF EXISTS toc; DROP TABLE IF EXISTS link;
        DROP TABLE IF EXISTS medium; DROP TABLE IF EXISTS releases;
        DROP TABLE IF EXISTS credit; DROP TABLE IF EXISTS tracks;
        CREATE TABLE toc(id INTEGER PRIMARY KEY,n INTEGER,lead INTEGER,offsets TEXT);
        CREATE TABLE link(toc INTEGER,medium INTEGER);
        CREATE TABLE medium(id INTEGER PRIMARY KEY,release INTEGER,position INTEGER,name TEXT,n INTEGER);
        CREATE TABLE releases(id INTEGER PRIMARY KEY,name TEXT,credit INTEGER);
        CREATE TABLE credit(id INTEGER PRIMARY KEY,name TEXT);
        CREATE TABLE tracks(medium INTEGER,position INTEGER,name TEXT,PRIMARY KEY(medium,position));
    ''')
    links, mediums, tocs = [], set(), set()
    for r in rows(folder, 'medium_cdtoc'):
        medium, toc = int(r[1]), int(r[2])
        links.append((toc, medium))
        mediums.add(medium)
        tocs.add(toc)
    db.executemany('INSERT INTO link VALUES (?,?)', links)
    del links
    db.executemany('INSERT INTO toc VALUES (?,?,?,?)',
                   ((int(r[0]), int(r[3]), int(r[4]), r[5]) for r in rows(folder, 'cdtoc')
                    if int(r[0]) in tocs and 1 <= int(r[3]) <= 99))
    releases = set()
    def medium_rows():
        for r in rows(folder, 'medium'):
            if int(r[0]) in mediums:
                releases.add(int(r[1]))
                yield int(r[0]), int(r[1]), int(r[2]), r[4], int(r[7])
    db.executemany('INSERT INTO medium VALUES (?,?,?,?,?)', medium_rows())
    credits = set()
    def release_rows():
        for r in rows(folder, 'release'):
            if int(r[0]) in releases:
                credits.add(int(r[3]))
                yield int(r[0]), r[2], int(r[3])
    db.executemany('INSERT INTO releases VALUES (?,?,?)', release_rows())
    db.executemany('INSERT INTO credit VALUES (?,?)',
                   ((int(r[0]), r[1]) for r in rows(folder, 'artist_credit') if int(r[0]) in credits))
    db.executemany('INSERT INTO tracks VALUES (?,?,?)',
                   ((int(r[3]), int(r[4]), r[6]) for r in rows(folder, 'track')
                    if int(r[3]) in mediums and int(r[4]) > 0 and r[11] == 'f'))
    db.executescript('CREATE INDEX link_toc ON link(toc); CREATE INDEX link_medium ON link(medium);')
    db.commit()
    return db


def write_catalog(db, output, snapshot):
    entries, seen, skipped = [], set(), 0
    query = '''SELECT t.n,t.lead,t.offsets,m.id,m.position,m.name,r.name,c.name
        FROM link l JOIN toc t ON t.id=l.toc JOIN medium m ON m.id=l.medium
        JOIN releases r ON r.id=m.release JOIN credit c ON c.id=r.credit
        ORDER BY t.id,m.id'''
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.with_suffix('.tmp').open('wb') as stream:
        stream.write(b'\0' * HEADER.size)
        for scanned, (n, lead, raw, medium, position, subtitle, album, artist) in enumerate(db.execute(query), 1):
            if scanned % 100000 == 0:
                print(scanned, 'layouts processed;', len(entries), 'unique entries;', stream.tell(), 'bytes', flush=True)
            offsets = [int(x) for x in raw.strip('{}').split(',')]
            names = list(db.execute('SELECT position,name FROM tracks WHERE medium=? ORDER BY position', (medium,)))
            if (len(offsets) != n or len(names) != n or [x[0] for x in names] != list(range(1, n + 1))
                    or offsets != sorted(set(offsets)) or lead <= offsets[-1]):
                skipped += 1
                continue
            duration = lead - offsets[0]
            offsets = [x - offsets[0] for x in offsets]
            title = album + ((' - ' + subtitle) if subtitle else (' (disc %d)' % position if position > 1 else ''))
            payload = struct.pack('<I', n) + struct.pack('<%dI' % n, *offsets)
            payload += text_bytes(title) + text_bytes(artist) + b''.join(text_bytes(x[1]) for x in names)
            identity = hashlib.sha256(struct.pack('<I', duration) + payload).digest()
            if identity in seen:
                continue
            seen.add(identity)
            if stream.tell() + len(payload) + (len(entries) + 1) * ENTRY.size >= 0x7fffffff:
                raise ValueError('Catalog exceeds the 2 GiB Win98 reader limit')
            entries.append((signature(offsets), duration, stream.tell(), len(payload)))
            stream.write(payload)
        index = stream.tell()
        for entry in sorted(entries):
            stream.write(ENTRY.pack(*entry))
        stream.seek(0)
        stream.write(HEADER.pack(b'G98CDDB1', len(entries), index, int(snapshot.replace('-', '')), 0, 0, 0))
    output.with_suffix('.tmp').replace(output)
    with output.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    result = {'snapshot': snapshot, 'records': len(entries), 'skipped': skipped,
              'bytes': output.stat().st_size, 'sha256': checksum,
              'source': 'MusicBrainz core database', 'license': 'CC0-1.0'}
    output.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=pathlib.Path, required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--work', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--snapshot', required=True)
    args = parser.parse_args()
    start = time.monotonic()
    with args.archive.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest().lower() != args.sha256.lower():
            raise ValueError('Source SHA-256 does not match the published checksum')
    args.work.mkdir(parents=True, exist_ok=True)
    marker = args.work / 'source.sha256'
    if marker.exists() and marker.read_text().strip() != args.sha256.lower():
        raise ValueError('Use a separate work directory for each source snapshot')
    marker.write_text(args.sha256.lower() + '\n')
    extract(args.archive, args.work / 'tables')
    db = import_tables(args.work / 'tables', args.work / 'catalog.sqlite')
    try:
        write_catalog(db, args.output, args.snapshot)
    finally:
        db.close()
    print('Completed in %.1f seconds' % (time.monotonic() - start), flush=True)


if __name__ == '__main__':
    main()
