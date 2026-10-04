#!/usr/bin/env python3
"""Collect notices from checksum-verified Cargo.lock archives; never run crate code."""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import tomllib


ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "tools/wellen_reader/Cargo.lock"
OUTPUT = ROOT / "third_party/wellen-licenses"


def digest(data):
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


def locked_packages():
    return [p for p in tomllib.loads(LOCK.read_text(encoding="utf-8"))["package"]
            if "source" in p]


def collect(cache_dirs):
    packages = []
    contents = {}
    for package in locked_packages():
        name = f"{package['name']}-{package['version']}"
        if package["source"] != "registry+https://github.com/rust-lang/crates.io-index":
            raise ValueError(f"Review the non-crates.io dependency before collecting: {name}")
        archive = next((p / f"{name}.crate" for p in cache_dirs
                        if (p / f"{name}.crate").is_file()), None)
        if archive is None:
            raise ValueError(f"Missing {name}.crate; supply its Cargo registry cache with --cache-dir")
        data = archive.read_bytes()
        if hashlib.sha256(data).hexdigest() != package["checksum"]:
            raise ValueError(f"Cargo.lock checksum mismatch: {name}")
        licenses = []
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
            manifest = tomllib.loads(tar.extractfile(f"{name}/Cargo.toml").read().decode("utf-8"))
            metadata = manifest["package"]
            if (metadata["name"], metadata["version"]) != (package["name"], package["version"]):
                raise ValueError(f"Archive identity mismatch: {name}")
            declared_file = metadata.get("license-file")
            for member in tar.getmembers():
                path = PurePosixPath(member.name)
                if not member.isfile() or path.parts[0] != name or ".." in path.parts:
                    continue
                relative = PurePosixPath(*path.parts[1:])
                if not (relative.name.lower().startswith(
                        ("license", "licence", "copying", "copyright", "notice", "unlicense"))
                        or str(relative) == declared_file):
                    continue
                text = tar.extractfile(member).read()
                text.decode("utf-8")  # Do not copy binary payloads as notices.
                destination = f"{name}/{relative}"
                contents[destination] = text.replace(b"\r\n", b"\n")
                licenses.append({"path": destination, "sha256Lf": digest(text)})
        if not licenses:
            raise ValueError(f"No license text found: {name}; review upstream before releasing")
        packages.append({
            "name": package["name"], "version": package["version"],
            "license": metadata.get("license"),
            "archiveUrl": f"https://static.crates.io/crates/{package['name']}/{name}.crate",
            "archiveSha256": package["checksum"], "files": sorted(licenses, key=lambda f: f["path"]),
        })
    manifest = {"schema": 1, "cargoLockSha256Lf": digest(LOCK.read_bytes()), "packages": packages}
    # Validate every archive before replacing any maintained notice.
    for name, data in contents.items():
        destination = OUTPUT / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    lines = ["# Wellen reader dependency notices", "",
             "Collected from the exact registry archives identified by Cargo.lock.",
             "Archive checksums were verified before copying license texts; no crate code was executed.",
             "This inventory covers every locked registry package, including build-time and non-Windows dependencies.",
             "It does not assert that every listed crate is present in every binary.", "",
             "Regenerate after a lockfile change with Python 3.11 or newer:", "",
             "```text", "python scripts/collect-wellen-licenses.py --cache-dir <cargo-registry-cache>",
             "python scripts/collect-wellen-licenses.py --check", "```", "",
             "Repeat --cache-dir for additional archive caches. The collector does not download or execute packages.",
             "Keep this directory with redistributed wave-wellen-reader binaries.", "",
             "| Package | License expression from package metadata | Full texts |",
             "| --- | --- | --- |"]
    for package in packages:
        links = ", ".join(f"[{Path(f['path']).name}]({f['path']})" for f in package["files"])
        lines.append(f"| {package['name']} {package['version']} | {package['license']} | {links} |")
    (OUTPUT / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def check():
    manifest = json.loads((OUTPUT / "manifest.json").read_text(encoding="utf-8"))
    if manifest["cargoLockSha256Lf"] != digest(LOCK.read_bytes()):
        raise ValueError("Cargo.lock changed; regenerate and review the Wellen notices")
    expected = {(p["name"], p["version"], p["checksum"]) for p in locked_packages()}
    actual = {(p["name"], p["version"], p["archiveSha256"]) for p in manifest["packages"]}
    if expected != actual:
        raise ValueError("The notice inventory does not cover the locked dependencies")
    for package in manifest["packages"]:
        if not package["files"]:
            raise ValueError(f"Missing license texts for {package['name']}")
        for entry in package["files"]:
            path = (OUTPUT / entry["path"]).resolve()
            if not path.is_relative_to(OUTPUT.resolve()) or digest(path.read_bytes()) != entry["sha256Lf"]:
                raise ValueError(f"Notice missing or changed: {entry['path']}")
    print(f"Verified notices for {len(actual)} locked registry packages.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", type=Path, action="append", default=[])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if not args.check:
        if not args.cache_dir:
            parser.error("collection requires at least one --cache-dir")
        collect(args.cache_dir)
    check()


if __name__ == "__main__":
    main()
