from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re


EDITOR = Path(__file__).resolve().parent
EXAMPLES = {
    "/examples/01-basic.json": "01-basic.json",
    "/examples/02-triple.json": "02-triple.json",
}


class EditorHandler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".mjs": "text/javascript"}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=EDITOR / "dist", **kwargs)

    def translate_path(self, path):
        example = EXAMPLES.get(path)
        if example:
            return str(EDITOR.parent / "firmware" / "patches" / example)
        return super().translate_path(path)

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/presets/index.json":
            files = sorted(p.name for p in (EDITOR / "dist" / "presets").glob("*.json")
                           if p.name != "index.json" and re.fullmatch(r"[a-z0-9-]+\.json", p.name))
            content = json.dumps(files).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)
            return
        super().do_GET()

    def list_directory(self, path):
        self.send_error(404)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    with ThreadingHTTPServer(("127.0.0.1", 8765), EditorHandler) as server:
        print("Ouvre http://127.0.0.1:8765/ dans Chrome ou Edge.", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
