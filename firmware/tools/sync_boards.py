"""Generate the editor's hardware registry from the build profiles."""
from board_profiles import ROOT, editor_module

if __name__ == '__main__':
    (ROOT.parent / 'editor/dist/board-profiles.mjs').write_text(editor_module(), encoding='utf-8')
