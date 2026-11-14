# PS5 Storage Restore 1.9

**Tiếng Việt** · [English](README.en.md)

Dựng lại đăng ký game PS5 đã cài trên SSD M.2 và bộ nhớ trong sau khi mất database. Payload quét `PPSAxxxxx/app.pkg`, dựng metadata còn thiếu, rồi ghi đăng ký trỏ thẳng vào ổ chứa game.

**Bản quyền NGÔ PHI PHƯƠNG x PSVIETHOA.COM** — GPL-3.0-only

**Tải về:** [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`
SHA256 `83daa52e684054d810a0cb7fe3af37ed4220629a5cfd80e430a7acd502ee00f6`

## Chạy một lần là xong

Các bản trước đăng ký game M.2 qua alias nullfs, nên database ghi `_org_path = /user/app/<id>`. Alias không sống qua reboot → mỗi lần bật máy game báo "Dữ liệu đã bị hỏng" cho tới khi chạy lại payload.

1.8 ghi đúng hai trường mà bản cài gốc dùng:

```
_org_path      -> /mnt/extN/user/app/<id>
_m2_device_id  -> 1
```

Đường dẫn thiết bị thì hệ thống tự mount lúc boot. Chạy một lần, **không cần thêm vào autoload**. Chỉ chạy lại khi mất database hoặc thêm game mới.

## Cách dùng

1. Đóng hết game, chờ tác vụ cài/cập nhật xong.
2. Chép ELF vào `/data/ps5_storage_restore/`, chạy một lần sau khi môi trường payload và M.2 sẵn sàng.
3. Mở Thư viện trò chơi kiểm tra.

Log: `/data/ps5_storage_restore/restore.log`. Cần thấy `START version=1.9-EN-VI-native`, các dòng `NATIVE <title> _org_path=/mnt/extN/...`, `DB_INTEGRITY ok` hai lần, và `END`.

Thấy `NATIVE_ABORT` hoặc `NATIVE_FAILED` thì database **không bị sửa** — chép bản backup trong `/data/ps5_storage_restore/backups/` về là xong.

## ⚠ Đừng chạy Rebuild Database

Rebuild Database **xoá luôn dữ liệu** của fPKG nằm ở `/user/app` (bộ nhớ trong), không chỉ xoá đăng ký. Mất `app.pkg` thì không payload nào cứu được, phải cài lại.

Game trên M.2 (`/mnt/extN/user/app`) ngoài tầm rebuild — chỉ mất đăng ký, và đó đúng là thứ payload này dựng lại.

Mất database thì chạy payload này, đừng chạy Rebuild Database.

## An toàn

- Sao lưu nguyên byte `appinfo.db` trước khi ghi; không sao lưu được thì không ghi gì.
- `PRAGMA integrity_check` trước và sau.
- Chỉ `UPDATE` hàng có sẵn. Không `INSERT`, không `DELETE`, không đụng cột khác, không đụng `app.db`.
- Đọc lại `_org_path` sau khi ghi; lệch thì giữ nguyên hiện trạng title đó.
- Alias đang mount và thư mục `/user/app/<id>` không bị đụng tới.
- Thay `param.json` cũ chỉ khi `titleId` + `contentId` khớp, có backup nguyên byte trước.

## Source

| Đường dẫn | Vai trò |
| --- | --- |
| `src/storage_restore.c` | Quét, mount, đăng ký, thông báo |
| `src/pkg_reader.h` | Parser FIH/CNT |
| `src/restore_metadata.h` | Streaming, đối chiếu, backup, thay `param.json` |
| `src/restore_native.h` | Ghi `_org_path`/`_m2_device_id` vào `appinfo.db` |
| `build.py` | Cross-build PS5 trên Windows với LLVM/LLD |
| `tests/` | 23 parser + 23 metadata test, dữ liệu giả lập |
| `verification/` | Kết quả đã quan sát trên máy thật |

## Build

Cần Python 3.12+, LLVM/Clang, `ld.lld`, `llvm-strip` trên PATH, và PS5 payload SDK có `libjson-c.a` + `libsqlite3.a`.

```powershell
$env:PS5_SDK = 'C:\ps5-payload-sdk'
python .\build.py          # ra PS5_STORAGE_RESTORE.elf + BUILD.json
python tests\build_host_jsonc.py
python tests\test_parser.py
python tests\test_metadata.py
```

## Kiểm chứng

Chạy trên PS5 Pro FW 10.01, 14/11/2026:

```
END registered=40 found=42 skipped=2 created=19
NATIVE_SUMMARY applied=38 kept=0
DB_INTEGRITY ok (truoc va sau)
```

Trước đó bản 1.8 chỉ ra `registered=12 found=42 skipped=30`. Chốt bảo vệ cũ đòi **vừa** có dấu sở hữu **vừa** rỗng, mà `/data/ps5_storage_restore/owned/` đã bị xoá sạch, nên mọi title còn sót thư mục rỗng ở `/user/app/` bị khoá vĩnh viễn — 28 trong 30 ca là thư mục rỗng. 1.9 cho dùng thư mục rỗng; chỉ thư mục còn file mới cần là của mình.

Hai title vẫn bị bỏ qua có `icon0.png` + `sce_sys` thật ở `/user/app/<id>`; từ chối là đúng, xử theo [docs/RECOVERY.md](docs/RECOVERY.md).

Lần chạy 18/10/2026 với bản trước đã xác nhận phần đường dẫn thiết bị: tắt hẳn máy bật lại, alias 0/22, **20/20 game giữ đường dẫn thiết bị và mở được, không chạy payload lúc boot**.

**Chưa kiểm chứng:** reboot sau lần chạy 1.9.

**Chưa hỗ trợ:** fPKG PS4 (`CUSAxxxxx`). Bộ lọc chỉ nhận `PPSA`, và game PS4 không có `sce_sys` nên pipeline metadata hiện tại không áp dụng được.

Chi tiết từng lần chạy trong `verification/console-summary-*.json`.

## Giấy phép

Bản quyền **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, xem [COPYRIGHT.md](COPYRIGHT.md). GPL-3.0-only, toàn văn trong [LICENSE](LICENSE). Thông báo thư viện phụ thuộc ở `licenses/`.
