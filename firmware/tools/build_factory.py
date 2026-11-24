"""Build a clean factory image from a board-specific build and the 40 factory presets.

No device access. Unlike a raw clone this never copies unit-specific calibration,
MAC metadata or personal patches. The existing verified 16/8 image is retained.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from board_profiles import ROOT, PROFILES, DEFAULT, profile
from release import validate_layout, boot_a_metadata

def run(*args):
    subprocess.run([sys.executable, *map(str, args)], check=True, capture_output=True)

def generate_nvs(destination, namespace, blobs, size):
    source = destination.with_suffix('.csv')
    with source.open('w', newline='', encoding='utf-8') as output:
        writer = csv.writer(output)
        writer.writerow(['key', 'type', 'encoding', 'value'])
        writer.writerow([namespace, 'namespace', '', ''])
        for key, value in blobs:
            writer.writerow([key, 'data', 'hex2bin', value.hex()])
    run('-m', 'esp_idf_nvs_partition_gen', 'generate', source, destination, hex(size))
    return destination.read_bytes()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', choices=PROFILES, default='s3-32m16m')
    args = parser.parse_args()
    board = profile(args.model)
    build = ROOT / ('build' if args.model == DEFAULT else 'build-' + args.model)
    release_dir = ROOT / 'releases' / (ROOT / 'version.txt').read_text().strip()
    if args.model != DEFAULT: release_dir /= args.model
    release = json.loads((release_dir / 'latest.json').read_text())
    app = (release_dir / release['file']).read_bytes()
    if release['model'] != board['model'] or hashlib.sha256(app).hexdigest() != release['sha256']:
        raise ValueError('Release does not match the board build')
    if len(app) > 0x100000: raise ValueError('Factory fallback partition is too small')
    table = (build / 'partition_table/partition-table.bin').read_bytes()
    validate_layout(table, board)
    config = (ROOT / ('sdkconfig' if args.model == DEFAULT else 'sdkconfig.' + args.model)).read_text()
    page_size = 512 if args.model != DEFAULT else 256
    if f'CONFIG_SPIFFS_PAGE_SIZE={page_size}\n' not in config:
        raise ValueError('SPIFFS geometry mismatch')
    if board['sampleBytes'] // page_size >= 65535:
        raise ValueError('SPIFFS page index overflow')
    work = build / 'factory-assets'
    work.mkdir(exist_ok=True)
    presets_root = ROOT.parent / 'editor/dist/presets'
    presets = [(p.name, json.loads(p.read_text(encoding='utf-8'))) for p in sorted(presets_root.glob('*.json')) if p.name != 'index.json']
    if len(presets) != 40: raise ValueError('Expected 40 factory presets')
    patches = [('u' + hashlib.sha256(('modulox.factory.v1:' + name).encode()).hexdigest()[:13],
                json.dumps(patch, ensure_ascii=False, separators=(',', ':')).encode()) for name, patch in presets]
    userpatches = generate_nvs(work / 'userpatches.bin', 'patches', patches, 0x20000)
    assets = presets_root / 'assets'
    index = json.loads((assets / 'index.json').read_text())
    def pcm(entry):
        data = (assets / (entry['id'] + '.pcm')).read_bytes()
        if hashlib.sha256(data).hexdigest()[:14] != entry['id']: raise ValueError('Invalid asset hash')
        return data
    waves = []
    for entry in index['waves']:
        data = pcm(entry)
        if len(data) != 2048: raise ValueError('Invalid wave size')
        name = entry['name'].encode('ascii')
        if len(name) > 31: raise ValueError('Invalid wave name')
        waves.append((entry['id'], name.ljust(32, b'\0') + data))
    wave_data = generate_nvs(work / 'waves.bin', 'cycles', waves, 0xa0000)
    # A fresh directory avoids accidentally packaging obsolete sample files.
    import tempfile
    with tempfile.TemporaryDirectory(prefix='samples-', dir=work) as folder:
        folder = Path(folder)
        for entry in index['samples']:
            (folder / entry['id']).write_bytes(pcm(entry))
            (folder / (entry['id'] + '.name')).write_text(entry['name'], encoding='ascii')
        idf = Path(os.environ.get('IDF_PATH', Path.home() / 'esp/v5.4.1/esp-idf'))
        run(idf / 'components/spiffs/spiffsgen.py', hex(board['sampleBytes']), folder, work / 'samples.bin', '--page-size', page_size)
    image = bytearray(b'\xff' * board['flashBytes'])
    def put(offset, data):
        if offset + len(data) > len(image): raise ValueError('Factory image overflow')
        image[offset:offset + len(data)] = data
    put(0, (build / 'bootloader/bootloader.bin').read_bytes())
    put(0x8000, table)
    for offset in [0x10000, board['ota0'], board['ota1']]: put(offset, app)
    put(0x110000, (build / 'patches.bin').read_bytes())
    put(0x130000, userpatches)
    put(0x150000, wave_data)
    put(0x1f0000, boot_a_metadata())
    put(0x200000, (work / 'samples.bin').read_bytes())
    destination = release_dir / 'factory'
    destination.mkdir(exist_ok=True)
    filename = f"modulox-usine-{release['version']}.bin"
    (destination / filename).write_bytes(image)
    manifest = {'format': 'modulox.full-flash', 'version': 1, 'model': board['model'], 'hardwareProfile': args.model,
                'chip': 'esp32s3', 'layout': board['layout'], 'file': filename, 'offset': 0,
                'size': len(image), 'flashBytes': board['flashBytes'], 'psramBytes': board['psramBytes'],
                'sha256': hashlib.sha256(image).hexdigest(), 'md5': hashlib.md5(image).hexdigest(),
                'firmwareVersion': release['version'], 'buildId': release['buildId'], 'bootSlot': 'ota_0',
                'hardwareValidated': False}
    (destination / 'latest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    public = ROOT.parent / 'editor/dist/factory'
    if args.model != DEFAULT: public /= args.model
    public.mkdir(parents=True, exist_ok=True)
    import shutil
    for name in [filename, 'latest.json']: shutil.copyfile(destination / name, public / name)
    print(f"Factory {args.model}: {len(image)} bytes, 40 presets, {len(waves)} waves, {len(index['samples'])} samples")
    print('Built and checked offline; first boot on this hardware still needs validation.')

if __name__ == '__main__': main()
