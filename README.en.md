# PS5 Storage Restore 1.9

[Tiếng Việt](README.md) · **English**

Rebuilds the registration of PS5 games installed on an M.2 SSD or internal storage after the application database is lost. The payload scans `PPSAxxxxx/app.pkg`, rebuilds the missing metadata, then records the registration against the drive the game actually lives on.

**Copyright NGÔ PHI PHƯƠNG x PSVIETHOA.COM** — GPL-3.0-only

**Download:** [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`
SHA256 `83daa52e684054d810a0cb7fe3af37ed4220629a5cfd80e430a7acd502ee00f6`

## One run is enough

Earlier releases registered an M.2 title through a nullfs alias, so the database recorded `_org_path = /user/app/<id>`. The alias does not survive a reboot, so every boot reported the data as corrupt until the payload ran again.

1.8 writes the two fields a real install uses:

```
_org_path      -> /mnt/extN/user/app/<id>
_m2_device_id  -> 1
```

The system mounts the device path itself at boot. Run it once — **no autoload entry needed**. Run it again only after a database loss or when adding a game.

## How to use

1. Close every game and wait for installs and updates to finish.
2. Copy the ELF to `/data/ps5_storage_restore/` and run it once, after the payload environment and the M.2 drive are ready.
3. Open the game library to check.

Log: `/data/ps5_storage_restore/restore.log`. It must contain `START version=1.9-EN-VI-native`, the `NATIVE <title> _org_path=/mnt/extN/...` lines, `DB_INTEGRITY ok` twice, and `END`.

If `NATIVE_ABORT` or `NATIVE_FAILED` appears the database was **not** modified — restoring the copy from `/data/ps5_storage_restore/backups/` is enough.

## ⚠ Do not run Rebuild Database

Rebuild Database **deletes the data** of an fPKG sitting in `/user/app` (internal storage), not just its registration. Once `app.pkg` is gone no payload can bring it back; the game has to be installed again.

A title on the M.2 (`/mnt/extN/user/app`) is outside its reach — only the registration is lost, which is exactly what this payload rebuilds.

If the database is lost, run this payload instead of Rebuild Database.

## Safety

- `appinfo.db` is copied byte for byte before any write; if the copy fails, nothing is written.
- `PRAGMA integrity_check` runs before and after.
- Only existing rows are updated. No inserts, no deletes, no other column touched, `app.db` untouched.
- `_org_path` is read back after writing; a mismatch leaves that title as it was.
- The mounted alias and `/user/app/<id>` are not touched.
- A stale `param.json` is replaced only when `titleId` and `contentId` match, after a byte-exact backup.

## Source

| Path | Role |
| --- | --- |
| `src/storage_restore.c` | Scan, mount, register, notify |
| `src/pkg_reader.h` | FIH/CNT parser |
| `src/restore_metadata.h` | Streaming, comparison, backup, `param.json` replacement |
| `src/restore_native.h` | Writes `_org_path`/`_m2_device_id` into `appinfo.db` |
| `build.py` | PS5 cross-build on Windows with LLVM/LLD |
| `tests/` | 23 parser + 23 metadata cases on synthetic data |
| `verification/` | Results observed on real hardware |

## Build

Requires Python 3.12+, LLVM/Clang, `ld.lld` and `llvm-strip` on PATH, plus a PS5 payload SDK providing `libjson-c.a` and `libsqlite3.a`.

```powershell
$env:PS5_SDK = 'C:\ps5-payload-sdk'
python .\build.py          # produces PS5_STORAGE_RESTORE.elf + BUILD.json
python tests\build_host_jsonc.py
python tests\test_parser.py
python tests\test_metadata.py
```

## Verification

Ran on a PS5 Pro, firmware 10.01, on 2026-11-14:

```
END registered=40 found=42 skipped=2 created=19
NATIVE_SUMMARY applied=38 kept=0
DB_INTEGRITY ok (before and after)
```

1.8 produced `registered=12 found=42 skipped=30` on the same console. Its destination guard required **both** an ownership marker **and** an empty directory, and `/data/ps5_storage_restore/owned/` had been cleared, so every leftover mount point under `/user/app/` was permanently unusable - 28 of the 30 cases were empty directories. 1.9 accepts an empty directory; only one that still holds files has to be ours.

The two titles still skipped have a real `icon0.png` and `sce_sys` in `/user/app/<id>`; refusing them is correct, handle those per [docs/RECOVERY.en.md](docs/RECOVERY.en.md).

The device-path behaviour was confirmed by the run on 2026-10-18: after a full power cycle, 0/22 aliases mounted and **20/20 titles kept the device path and launched, with no payload run at boot**.

**Not verified:** a reboot after the 1.9 run.

**Not supported:** PS4 fPKG (`CUSAxxxxx`). The title filter only accepts `PPSA`, and PS4 titles have no `sce_sys`, so the current metadata pipeline does not apply.

Per-run detail is in `verification/console-summary-*.json`.

## License

Copyright **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, see [COPYRIGHT.md](COPYRIGHT.md). GPL-3.0-only, full text in [LICENSE](LICENSE). Dependency notices in `licenses/`.
