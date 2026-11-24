"""Package and publish the static Modulox editor and firmware images."""

import argparse
from datetime import datetime, timezone
from hashlib import file_digest
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile
from urllib.request import urlopen
from uuid import uuid4
import zipfile


PUBLISH_ROOT = Path("/srv/modulox-deploy")
PROFILES = (
    ("", "modulox-brain-s3", "ab-1.5m-v1", 16 * 1024**2),
    ("s3-32m16m/", "modulox-brain-s3-32m16m", "ab-1.5m-32m-v1", 32 * 1024**2),
)
METADATA = "deployment.json"
MAX_ARCHIVE = 64 * 1024**2
MAX_CONTENTS = 300 * 1024**2
MAX_FILES = 2000
COMMIT = re.compile(r"[0-9a-f]{40}")
RELEASE = re.compile(r"[0-9]{14}-[0-9a-f]{12}-[0-9a-f]{8}")
FILE_PATH = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*(?:/[A-Za-z0-9][A-Za-z0-9_.-]*)*")
STATIC_ASSETS = {
    ".html", ".css", ".mjs", ".js", ".json", ".bin", ".pcm", ".wav", ".txt",
    ".png", ".jpg", ".jpeg", ".webp", ".svg", ".ico", ".woff", ".woff2", ".wasm",
}


def asset_path(name):
    if not FILE_PATH.fullmatch(name) or Path(name).suffix.lower() not in STATIC_ASSETS:
        raise ValueError(f"Invalid static asset path: {name}")
    return name


def checksum(path, algorithm="sha256"):
    with path.open("rb") as source:
        return file_digest(source, algorithm).hexdigest()


def validate_site(directory):
    for name in ("index.html", "app.mjs", "style.css", "i18n.mjs", "presets/assets/index.json"):
        if not (directory / name).is_file():
            raise ValueError(f"Missing editor asset: {name}")
    presets = directory / "presets"
    preset_names = sorted(
        path.name for path in presets.iterdir()
        if path.is_file() and path.name != "index.json" and re.fullmatch(r"[a-z0-9-]+\.json", path.name)
    )
    if not preset_names or json.loads((presets / "index.json").read_text(encoding="utf-8")) != preset_names:
        raise ValueError("Preset index is out of date. Run node editor/build-web.mjs.")

    for prefix, model, layout, flash in PROFILES:
        for kind in ("firmware", "factory"):
            manifest = directory / kind / prefix / "latest.json"
            release = json.loads(manifest.read_text(encoding="utf-8"))
            if (release["model"], release["layout"], release["chip"]) != (model, layout, "esp32s3"):
                raise ValueError(f"Wrong hardware profile in {kind}/{prefix}latest.json")
            name = release["file"]
            if not re.fullmatch(r"[a-z0-9][a-z0-9.-]+\.bin", name):
                raise ValueError(f"Invalid firmware name in {kind}/{prefix}latest.json")
            image = manifest.parent / name
            size = image.stat().st_size
            if size != release["size"] or checksum(image) != release["sha256"]:
                raise ValueError(f"Size or SHA-256 mismatch for {kind}/{prefix}{name}")
            if kind == "factory":
                if size != flash or checksum(image, "md5") != release["md5"]:
                    raise ValueError(f"Invalid full-flash image for {model}")
            elif not 0 < size <= 0x180000:
                raise ValueError(f"Application image exceeds its OTA slot for {model}")


def package(directory, archive, commit):
    if not COMMIT.fullmatch(commit):
        raise ValueError("Expected a full Git commit ID")
    validate_site(directory)
    paths = sorted(directory.rglob("*"))
    files = []
    for path in paths:
        if path.is_symlink():
            raise ValueError(f"Static source contains a symlink: {path}")
        if path.is_file():
            name = asset_path(path.relative_to(directory).as_posix())
            if name == METADATA:
                raise ValueError(f"Reserved deployment asset: {name}")
            files.append((path, name))
    if len(files) + 1 > MAX_FILES or sum(path.stat().st_size for path, _ in files) > MAX_CONTENTS:
        raise ValueError("Static package exceeds the publication limit")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=5) as output:
        for path, name in files:
            output.write(path, name)
        output.writestr(METADATA, json.dumps({"commit": commit}) + "\n")
    if archive.stat().st_size > MAX_ARCHIVE:
        raise ValueError("Compressed package exceeds the publication limit")
    print(f"Prepared {len(files)} assets from {commit}.")


def extract_package(stream, directory):
    with zipfile.ZipFile(stream) as archive:
        entries = archive.infolist()
        if len(entries) > MAX_FILES or sum(item.file_size for item in entries) > MAX_CONTENTS:
            raise ValueError("Static package exceeds the publication limit")
        for item in entries:
            name = asset_path(item.filename)
            if stat.S_IFMT(item.external_attr >> 16) not in (0, stat.S_IFREG):
                raise ValueError(f"Non-regular archive asset: {name}")
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as source, target.open("xb") as destination:
                shutil.copyfileobj(source, destination)
    metadata = json.loads((directory / METADATA).read_text(encoding="utf-8"))
    commit = metadata["commit"]
    if not COMMIT.fullmatch(commit):
        raise ValueError("Invalid deployment commit")
    validate_site(directory)
    return commit


def publish(stream):
    import fcntl

    releases = PUBLISH_ROOT / "releases"
    live = PUBLISH_ROOT / "www/modulox/configurateur"
    with (releases / ".lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        data = stream.read(MAX_ARCHIVE + 1)
        if not data or len(data) > MAX_ARCHIVE:
            raise ValueError("Invalid compressed package size")
        # Unpack and verify in isolation before changing the site's pointer.
        with tempfile.TemporaryDirectory(prefix=".incoming-", dir=releases) as staging:
            directory = Path(staging) / "site"
            directory.mkdir(mode=0o755)
            commit = extract_package(io.BytesIO(data), directory)
            suffix = uuid4().hex[:8]
            name = datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S") + "-" + commit[:12] + "-" + suffix
            destination = releases / name
            os.rename(directory, destination)
            pointer = live.with_name(".configurateur-" + suffix)
            pointer.symlink_to("../../releases/" + name, target_is_directory=True)
            os.replace(pointer, live)
        # Keep the active publication and its two most recent predecessors.
        previous = sorted(
            (path for path in releases.iterdir()
             if RELEASE.fullmatch(path.name) and path.is_dir() and not path.is_symlink() and path != destination),
            key=lambda path: path.stat().st_mtime_ns,
            reverse=True,
        )
        for obsolete in previous[2:]:
            shutil.rmtree(obsolete)
    print(f"Published {commit} as {name}.")


def verify(archive, base):
    with zipfile.ZipFile(archive) as release:
        commit = json.loads(release.read(METADATA))["commit"]
        paths = [METADATA, "index.html", "app.mjs", "style.css", "presets/index.json"]
        for prefix, _, _, _ in PROFILES:
            for kind in ("firmware", "factory"):
                folder = kind + "/" + prefix
                manifest = folder + "latest.json"
                paths.extend((manifest, folder + json.loads(release.read(manifest))["file"]))
        for path in paths:
            with release.open(path) as expected, urlopen(base.rstrip("/") + "/" + path + "?deployment=" + commit, timeout=45) as served:
                if file_digest(expected, "sha256").hexdigest() != file_digest(served, "sha256").hexdigest():
                    raise ValueError(f"Published asset differs from the verified package: {path}")
            print(f"Verified {path}")
    print(f"HTTPS publication verified for {commit}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    builder = commands.add_parser("package")
    builder.add_argument("directory", type=Path)
    builder.add_argument("archive", type=Path)
    builder.add_argument("commit")
    commands.add_parser("publish")
    checker = commands.add_parser("verify")
    checker.add_argument("archive", type=Path)
    checker.add_argument("base")
    arguments = parser.parse_args()
    if arguments.operation == "package":
        package(arguments.directory, arguments.archive, arguments.commit)
    elif arguments.operation == "verify":
        verify(arguments.archive, arguments.base)
    else:
        if os.environ.get("SSH_ORIGINAL_COMMAND") != "publish":
            parser.error("This SSH key only accepts the publish command")
        os.umask(0o022)
        publish(sys.stdin.buffer)


if __name__ == "__main__":
    main()
