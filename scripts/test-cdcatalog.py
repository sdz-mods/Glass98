"""Exercise the actual native reader against a generated MusicBrainz fixture."""
import importlib.util
import pathlib
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('catalog', ROOT / 'scripts/build-cd-database.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)

with tempfile.TemporaryDirectory(prefix='cd-catalog-', dir=ROOT / 'build') as temporary:
    work = pathlib.Path(temporary)
    rows = {
        'medium_cdtoc': ['1\t10\t1', '2\t11\t1', '3\t10\t1'],
        'cdtoc': ['1\tid\tfreeid\t2\t30150\t{150,15150}'],
        'medium': ['10\t100\t1\t1\t\t0\tnow\t2', '11\t101\t1\t1\t\t0\tnow\t2'],
        'release': ['100\tuuid\tAlbum One\t1', '101\tuuid\tAlbum Two\t1'],
        'artist_credit': ['1\tArtist'],
        'track': [f'{medium}{pos}\tuuid\t1\t{medium}\t{pos}\t{pos}\t{title}\t1\t200000\t0\tnow\tf'
                  for medium in (10, 11) for pos, title in [(1, 'Caf\u00e9\\\\Live'), (2, 'Second')]],
    }
    for name, data in rows.items():
        (work / name).write_text('\n'.join(data) + '\n', encoding='utf-8')
    db = catalog.import_tables(work, work / 'scratch.sqlite')
    try:
        stats = catalog.write_catalog(db, work / 'CDMETA.DAT', '2026-09-26')
    finally:
        db.close()
    assert stats['records'] == 2, stats
    data = (work / 'CDMETA.DAT').read_bytes()
    (work / 'CDDATA.TXT').write_text('Test catalog notice')
    (work / 'CC0.TXT').write_text('Test license fixture')
    subprocess.run([str(ROOT / 'build/test-cdoptional.exe'), str(work)], cwd=work, check=True, timeout=10)

    def probe(choice=0, duration=30000):
        result = subprocess.run([str(ROOT / 'build/test-cdcatalog.exe'), str(work / 'CDMETA.DAT'),
                                 str(choice), str(duration)], capture_output=True, check=True, timeout=5)
        return result.stdout.decode('utf-8').splitlines()

    assert probe()[:3] == ['2', 'Album One', 'Artist']
    assert probe(1)[1] == 'Album Two'
    assert probe(200)[1] == 'Album One'
    assert probe(duration=29999)[0] == '2'
    assert probe(duration=29997)[0] == '0'
    assert probe()[3] == 'Caf\u00e9\\Live'
    subprocess.run([str(ROOT / 'build/test-cdcatalog.exe'), str(work / 'CDMETA.DAT'),
                    '0', '30000', 'integration'], check=True, stdout=subprocess.DEVNULL, timeout=5)
    for damaged in [b'', data[:20], b'INVALID!' + data[8:], data[:-1],
                    data[:8] + struct.pack('<I', 0xffffffff) + data[12:],
                    data[:-8] + struct.pack('<I', 0xfffffffe) + data[-4:]]:
        (work / 'CDMETA.DAT').write_bytes(damaged)
        assert probe()[0] == '-1'
    print('PASS: imported CD metadata, duplicate removal, multiple editions, UTF-8, frame tolerance, malformed files, MCI format restoration')
