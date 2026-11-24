"""Hardware identities shared by builds, releases and the editor."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILES = json.loads((ROOT / 'boards/profiles.json').read_text(encoding='utf-8'))
DEFAULT = 's3-16m8m'

def profile(name=DEFAULT):
    return {**PROFILES[name], 'id': name}

def data_partitions(board):
    return {'nvs': (0x9000, 0x6000), 'phy_init': (0xf000, 0x1000),
            'patches': (0x110000, 0x20000), 'userpatches': (0x130000, 0x20000),
            'waves': (0x150000, 0xa0000), 'samples': (0x200000, board['sampleBytes'])}

def editor_module():
    data = {key: {**value, 'id': key} for key, value in PROFILES.items()}
    return '// Generated from firmware/boards/profiles.json by tools/sync_boards.py.\nexport const boardProfiles = Object.freeze(' + json.dumps(data, indent=2) + ');\n'
