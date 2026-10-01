# PS5 Storage Restore 1.3 EN/VI

[Tiếng Việt](README.md) · **English**

Restores the registration of PS5 games already installed on internal storage and on an M.2 SSD after the application database is lost or reset. The payload scans `PPSAxxxxx/app.pkg`, rebuilds the missing metadata, creates a nullfs alias for external drives, and registers each title through the system AppInst API.

**Copyright NGÔ PHI PHƯƠNG x PSVIETHOA.COM**

Download: [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`, SHA256 `35d88926d143097136898ee2318c63f313992c1bb394df78f306cd49e390d5e2`.

## What fix 1.3 changes

- Repairs a stale `param.json` only when `titleId`, `contentId` and `contentVersion` all match. A byte-exact backup is written before the file is replaced.
- Removes the 32/64 MiB per-file and 256 MiB total metadata ceilings; binary data is read, written and compared in 64 KiB chunks.
- Removes the 1 MiB ceiling on `param.json`; the JSON is parsed in chunks.
- PKG offsets and read/write counters are 64-bit. A CNT entry size is still the format's `uint32` field, so the per-entry maximum stays 4,294,967,295 bytes.
- Keeps the PKG range, path, flags and title-identity checks. At most 512 metadata files are selected and 65,535 CNT entries parsed; JSON depth follows json-c.
- EN/VI notifications are picked from the system language. The log records the path and the reason for every failure.

FF7 Rebirth used to be skipped because its stale sidecar `param.json` differed from the package. Spider-Man Remastered has a 53,313,776-byte trophy file, which exceeded the old parser ceiling; on the test console a stale metadata directory at `/user/app/PPSA01467` also blocked the M.2 alias. That stale directory was preserved with a rename after its identity was verified, and the payload was then run again.

## Source

| Path | Role |
| --- | --- |
| `src/storage_restore.c` | Standalone payload: scan, mount, register and notify |
| `src/pkg_reader.h` | FIH/CNT parser and metadata-table validation |
| `src/restore_metadata.h` | Streaming, comparison, backup and `param.json` replacement |
| `build.py` | PS5 cross-build on Windows with LLVM/LLD |
| `tests/` | 23 parser + 14 metadata tests on synthetic data |
| `verification/console-summary-1.3.json` | Observed results; contains no private database or log |

## Standalone build on Windows

Requires Python 3.12+, LLVM/Clang, `ld.lld` and `llvm-strip` on PATH, plus a PS5 payload SDK that provides static json-c. The SDK used here: pacbrew-repo v0.40.2, asset `ps5-payload-dev.tar.gz`, SHA256 `a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d`.

```powershell
$env:PS5_SDK = 'C:\ps5-payload-sdk'
python .\build.py
```

The SDK must contain `target/include`, `target/lib/crt1.o`, `target/user/homebrew/include/json-c`, `target/user/homebrew/lib/libjson-c.a` and `ldscripts/elf_x86_64.x`. Output: `PS5_STORAGE_RESTORE.elf`, `BUILD.json`, and the debug ELF plus link map in `build/`. The build checks for ELF64 little-endian, ET_DYN/x86-64, the segment layout and the entrypoint, and keeps the previous output ELF under its SHA256. Every new build starts with runtime verification set to false.

## Host tests on Windows

Requires Clang, CMake and Ninja. json-c 0.19 is downloaded from a release source pinned by SHA256; the tests use synthetic JSON fixtures.

```powershell
python -m pip install --target build/host-deps ninja
python tests/build_host_jsonc.py
python tests/test_parser.py
python tests/test_metadata.py
```

The public suite covers 23 parser cases and 14 metadata cases: the maximum UINT32 size against a synthetic extent, out-of-range checks, JSON larger than 2 MiB, streaming a 65 MiB file plus an odd trailing chunk, byte-exact backup, write/rename failures, and a second run that writes nothing more.

## Running it on the PS5

Close every game and wait for installs and updates to finish. Copy the ELF to `/data/ps5_storage_restore/` and run it once, after the payload environment and the M.2 drive are ready. Open the game library to check the result. Log: `/data/ps5_storage_restore/restore.log`; param backups: `/data/ps5_storage_restore/backups/`.

The log must contain `START version=1.3-EN-VI-streaming`, one `REGISTER <title> rc=0x00000000` per title, and a final `END` line. The M.2 mount has to be recreated after every reboot; Recovery can be placed in an existing autoload chain after the platform payload and once the drive is ready. Keep exactly one Recovery entry. Do not copy an example configuration over the console's whole autoload setup.

The payload handles installed FIH/CNT packages. It does not undelete data and does not format a drive. A destination directory that is occupied or carries no ownership marker is skipped; the payload never hides that data by itself. If a stale metadata directory has to be dealt with, verify its identity and then preserve it with a rename on the same filesystem before running again. Details in [docs/RECOVERY.en.md](docs/RECOVERY.en.md).

## Observed verification

Standalone 1.3 ran on a PS5 Pro, firmware 10.01, on 2026-10-01: **21/21 titles registered, skipped=0**. FF7 Rebirth PPSA08668 and Spider-Man Remastered PPSA01467 both returned `rc=0x00000000` in `external-nullfs` mode; the param file matched the package in all three locations and both databases reported `integrity_check=ok`. The icon tables of three users recorded `visible=1` / `installStatus=2`. The `app.pkg` alias matched the size and header of the M.2 source.

The ELF used for that run, built before the copyright line was edited, has SHA256 `9eab8e6d5bfb5147e4428ed145b676a861a979fc5869f8a67afd6ccd21841f9e`. Mounting and registration were confirmed on the console; the icon on the TV, launching a game and a reboot were **not** observed. The published source changes the copyright line to NGÔ PHI PHƯƠNG x PSVIETHOA.COM and keeps the fix logic unchanged; the ELF built from this source has only been verified offline. The project's private data set also passed 9 extra parser fixtures and 2 real-package replays; no game data is included in this public repository.

## License and related sources

Copyright **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, see [COPYRIGHT.md](COPYRIGHT.md). The Recovery fix source is released under GPL-3.0-only in [LICENSE](LICENSE). Build-dependency notices live in `licenses/`; the SDK and the build libraries are installed separately.
