#!/usr/bin/env python3
"""Copy a native Opera UCI build into a Unity project's StreamingAssets."""

import argparse
import hashlib
import json
import platform
import shutil
import stat
import subprocess
from pathlib import Path


def main():
    root = Path(__file__).resolve().parent.parent
    systems = {"Darwin": "macOS", "Windows": "Windows", "Linux": "Linux"}
    machines = {"arm64": "arm64", "aarch64": "arm64", "x86_64": "x86_64", "amd64": "x86_64"}
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unity-project", required=True, type=Path)
    parser.add_argument("--binary", type=Path, help="Defaults to the host's Rust release binary")
    parser.add_argument("--platform", choices=systems.values(), default=systems.get(platform.system()))
    parser.add_argument("--arch", choices=["arm64", "x86_64"], default=machines.get(platform.machine().lower()))
    args = parser.parse_args()
    if not args.platform or not args.arch:
        parser.error("Specify the binary's platform and architecture.")
    project = args.unity_project.resolve()
    if not (project / "ProjectSettings/ProjectVersion.txt").is_file():
        parser.error("--unity-project must point to a Unity project root.")
    name = "opera-uci.exe" if args.platform == "Windows" else "opera-uci"
    source = args.binary or root / "rust/target/release" / name
    if not source.is_file():
        parser.error(f"Build the native engine first; binary missing: {source}")
    destination = project / "Assets/StreamingAssets/Opera" / f"{args.platform}-{args.arch}"
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / name
    shutil.copy2(source, target)
    if args.platform != "Windows":
        target.chmod(target.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    dirty = bool(subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=normal"], cwd=root, text=True).strip())
    metadata = {
        "packaged_from_checkout": revision + ("-dirty" if dirty else ""),
        "working_tree_dirty": dirty,
        "source_binary": str(source.resolve()),
        "platform": args.platform,
        "architecture": args.arch,
        "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
    }
    (destination / "engine.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Installed {target}\nSHA-256: {metadata['sha256']}")


if __name__ == "__main__":
    main()
