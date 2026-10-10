# PS5 Storage Restore 1.3 EN/VI

[Tiếng Việt](README.md) · **English**

Restores the registration of PS5 games already installed on internal storage and on an M.2 SSD after the application database is lost or reset. The payload scans `PPSAxxxxx/app.pkg`, rebuilds the missing metadata, creates a nullfs alias for external drives, and registers each title through the system AppInst API.

**Copyright NGÔ PHI PHƯƠNG x PSVIETHOA.COM**

Download: [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`, SHA256 `d61a3864e647f09965aa0d0675b99f0b5110fbe52bdf5b6ee6db7dd5dd25f2f7`.

## What fix 1.4 changes

- `contentVersion` is no longer part of the identity check. Only `titleId` and `contentId` must match exactly; the `param.json` inside `app.pkg` owns `contentVersion`.
- A `sce_sys/param.json` sidecar older than the package is synced forward. A package older than the sidecar is refused (`param-content-version-older-in-package`) — never a downgrade.
- An unparsable `contentVersion` is refused on its own (`param-content-version-invalid`) instead of being folded into the identity failure.
- New log line `PARAM_VERSION path=... existing=... package=...` on every sync.
- The identity failure reason is renamed to `param-title-or-content-id-mismatch-or-invalid`.

Why: 1.3 required `contentVersion` to be equal, but that is precisely the field a game update changes. On the test console ASTRO BOT PPSA21567 was updated 01.018.000 → 01.019.000 on 2026-10-07; `app.pkg` and both appmeta copies moved to 01.019.000 while the M.2 `sce_sys/param.json` stayed at 01.018.000, so 1.3 skipped the title.

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
| `tests/` | 23 parser + 23 metadata tests on synthetic data |
| `verification/console-summary-1.4.json` | Observed 1.4 results; contains no private database or log |
| `verification/console-summary-1.3.json` | The 1.3 results, kept for comparison |

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

The public suite covers 23 parser cases and 23 metadata cases: the maximum UINT32 size against a synthetic extent, out-of-range checks, JSON larger than 2 MiB, streaming a 65 MiB file plus an odd trailing chunk, byte-exact backup, write/rename failures, a second run that writes nothing more, and the 1.4 `contentVersion` group: syncing forward, refusing a downgrade, a downgrade in a later group, a version written in fewer groups, and six unparsable version strings.

## Running it on the PS5

Close every game and wait for installs and updates to finish. Copy the ELF to `/data/ps5_storage_restore/` and run it once, after the payload environment and the M.2 drive are ready. Open the game library to check the result. Log: `/data/ps5_storage_restore/restore.log`; param backups: `/data/ps5_storage_restore/backups/`.

The log must contain `START version=1.4-EN-VI-streaming`, one `REGISTER <title> rc=0x00000000` per title, and a final `END` line. The M.2 mount has to be recreated after every reboot; Recovery can be placed in an existing autoload chain after the platform payload and once the drive is ready. Keep exactly one Recovery entry. Do not copy an example configuration over the console's whole autoload setup.

The payload handles installed FIH/CNT packages. It does not undelete data and does not format a drive. A destination directory that is occupied or carries no ownership marker is skipped; the payload never hides that data by itself. If a stale metadata directory has to be dealt with, verify its identity and then preserve it with a rename on the same filesystem before running again. Details in [docs/RECOVERY.en.md](docs/RECOVERY.en.md).

## Observed verification

Standalone **1.4** ran on a PS5 Pro, firmware 10.01, on 2026-10-10: **21/21 titles registered, skipped=0, created=6, updated=1**. The ELF of that run was built from exactly the source in this repository, SHA256 `d61a3864e647f09965aa0d0675b99f0b5110fbe52bdf5b6ee6db7dd5dd25f2f7`.

ASTRO BOT PPSA21567 is the title 1.3 skipped. Under 1.4: `PARAM_VERSION existing=01.018.000 package=01.019.000 package-newer`, then `REGISTER rc=0x00000000 mode=external-nullfs`. The `param.json` in all three locations (`sce_sys` on the M.2, `/user/appmeta`, `/system_data/priv/appmeta`) is byte-identical to the `param.json` inside `app.pkg`; the byte-exact backup of the old 01.018.000 file is kept in `/data/ps5_storage_restore/backups/`. The alias `/user/app/PPSA21567/app.pkg` matches the size of the M.2 source. `app.db` and `appinfo.db` both reported `integrity_check=ok` before and after. No other title's param file changed.

**Not observed:** the icon on the TV, launching a game, and a reboot. No step changed autoload, launched a game, or copied a database onto the console.

1.3 ran earlier on the same console on 2026-10-01, also 21/21 with skipped=0, using ELF `9eab8e6d5bfb5147e4428ed145b676a861a979fc5869f8a67afd6ccd21841f9e` built before the copyright line was edited. The project's private data set also passed 9 extra parser fixtures and 2 real-package replays; no game data is included in this public repository.

## License and related sources

Copyright **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, see [COPYRIGHT.md](COPYRIGHT.md). The Recovery fix source is released under GPL-3.0-only in [LICENSE](LICENSE). Build-dependency notices live in `licenses/`; the SDK and the build libraries are installed separately.
