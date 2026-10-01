# Metadata and rollback

[Tiếng Việt](RECOVERY.md) · **English**

The payload handles internal packages first, then scans `/mnt/ext0/user/app` through `/mnt/ext15/user/app`. For an external drive, the alias `/user/app/<title>` points back at the source directory through nullfs RW. The ownership marker lives at `/data/ps5_storage_restore/owned/<title>`.

Before replacing a `param.json` whose contents differ, the payload checks that the JSON is valid and that `titleId`, `contentId` and `contentVersion` all match. The backup is written with `O_EXCL`, fsynced, compared byte for byte and journalled before the replacement happens via rename. An existing backup is never overwritten. A failure before the replacement step leaves the old param in place; the backup is kept if a later step fails.

To roll a param back: close every game, read the `original=` and `saved=` pair from the `BACKUP` line, keep the current file aside, and copy that exact backup back to that exact original path. Never use a backup belonging to another title or another location.

A `/user/app/<title>` directory with no `app.pkg` may still hold metadata from an older install. It must be checked in full before it is moved: correct identity, nothing left but `sce_sys`/icon data, not a mount currently in use, no game running. Preserve the whole directory with a rename to a fallback name on the same filesystem. The payload does not do this step for you. On the test console, handling the stale PPSA01467 directory this way unblocked registration.

To roll a stale directory back, close every game, remove the nullfs alias that Recovery created, then rename the fallback directory to its original name. Keep the M.2 source and the param backups. Never delete recursively through `/user/app/<title>` while that path is an alias.

A database copy collected over FTP is only evidence as of the moment it was read; this utility never copies an old database over the system's own. The public source contains no database, no console log, no token, no game file and no extracted game metadata.
