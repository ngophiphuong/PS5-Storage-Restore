# PS5 Storage Restore 1.3 EN/VI

**Tiếng Việt** · [English](README.en.md)

Khôi phục đăng ký game PS5 đã cài trên bộ nhớ trong và SSD M.2 sau khi mất/reset database. Payload quét `PPSAxxxxx/app.pkg`, dựng metadata còn thiếu, tạo alias nullfs cho ổ ngoài và đăng ký qua API AppInst của hệ thống.

**Bản quyền NGÔ PHI PHƯƠNG x PSVIETHOA.COM**

Tải về: [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`, SHA256 `17eabf322889f4ad5edf8577192f43b256dda440dfb15463981d7efa1e01db95`.

## Bản fix 1.8

Một lần chạy là xong, không cần chạy lại mỗi lần khởi động.

Alias nullfs của các bản trước ghi `_org_path = /user/app/<id>` vào `appinfo.db`. Đường dẫn đó chỉ tồn tại khi alias còn mount, mà mount không sống qua reboot và không có gì tự dựng lại, nên lần bật máy sau hệ thống báo dữ liệu hỏng cho tới khi chạy lại Recovery. Title do trình cài gói đặt thì ghi đường dẫn thiết bị thật cộng `_m2_device_id=1`, và chạy qua reboot không cần ai đụng vào.

- 1.8 ghi thẳng hai trường đó vào `appinfo.db`: `_org_path` thành `/mnt/extN/user/app/<id>` và `_m2_device_id` thành 1.
- Hỏi database trước khi làm gì: title đã có hàng thì **không** tạo alias, không gọi AppInst, chỉ sửa vị trí. Title chưa có hàng (đúng lúc mất database) vẫn đi đường alias + AppInst để tạo hàng trước.
- Sao lưu nguyên byte `appinfo.db` ra `/data/ps5_storage_restore/backups/appinfo-<pid>.db` trước khi ghi; không sao lưu được thì không ghi gì.
- `PRAGMA integrity_check` trước và sau. Chỉ `UPDATE` hàng có sẵn, không `INSERT`, không `DELETE`, không đụng cột khác.
- Đọc lại `_org_path` sau khi ghi; lệch thì bỏ qua title đó và giữ nguyên hiện trạng.
- Alias đang mount và thư mục `/user/app/<id>` **không bị đụng tới**. Alias tự thành thừa ở lần boot sau.

Không dùng `sceAppInstUtilAppInstallTitleDir` với base của ổ: nó nhận tham số, trả `0x00000000` và không đổi gì, kiểm bằng cách gọi rồi reboot. Gọi nó khi `appinfo` đã ghi đường dẫn thiết bị thì trả `0x80f50004`.

## Bản fix 1.4

- `contentVersion` không còn nằm trong điều kiện nhận dạng. Chỉ `titleId` và `contentId` phải khớp tuyệt đối; `param.json` trong `app.pkg` là nguồn chuẩn cho `contentVersion`.
- Sidecar `sce_sys/param.json` cũ hơn package được đồng bộ tiến lên. Package cũ hơn sidecar bị từ chối (`param-content-version-older-in-package`), không bao giờ hạ cấp.
- `contentVersion` không đọc được bị từ chối riêng (`param-content-version-invalid`), không gộp vào lỗi nhận dạng.
- Thêm dòng log `PARAM_VERSION path=... existing=... package=...` mỗi lần đồng bộ.
- Lý do bỏ qua vì sai danh tính đổi tên thành `param-title-or-content-id-mismatch-or-invalid`.

Lý do: bản 1.3 bắt `contentVersion` phải giống, nhưng đó đúng là trường mà bản vá game luôn làm đổi. Trên máy thử, ASTRO BOT PPSA21567 được cập nhật 01.018.000 → 01.019.000 ngày 07/10/2026; `app.pkg` và cả hai thư mục appmeta đã sang 01.019.000 còn `sce_sys/param.json` trên M.2 vẫn là 01.018.000, nên 1.3 bỏ qua title này.

## Bản fix 1.3

- Sửa `param.json` cũ khi `titleId`, `contentId`, `contentVersion` khớp hoàn toàn. Sao lưu nguyên byte trước khi thay file.
- Bỏ trần 32/64 MiB mỗi file và 256 MiB tổng metadata; dữ liệu nhị phân đọc/ghi/đối chiếu theo khối 64 KiB.
- Bỏ trần 1 MiB cho `param.json`; JSON được parse theo khối.
- Offset PKG và bộ đếm đọc/ghi 64 bit. Kích thước entry CNT vẫn theo trường uint32 của định dạng, tối đa 4.294.967.295 byte/entry.
- Giữ kiểm tra phạm vi PKG, đường dẫn, flags và danh tính title. Tối đa 512 metadata được chọn và 65.535 entry CNT; độ sâu JSON theo json-c.
- Thông báo EN/VI tự chọn theo ngôn ngữ hệ thống. Log ghi đường dẫn và nguyên nhân lỗi.

FF7 Rebirth bị bỏ qua vì `param.json` sidecar cũ khác package. Spider-Man Remastered có trophy 53.313.776 byte, vượt trần parser cũ; trên máy thử còn có thư mục metadata cũ tại `/user/app/PPSA01467` chặn alias M.2. Thư mục cũ được giữ nguyên bằng rename sau khi xác minh danh tính, rồi chạy lại payload.

## Source

| Đường dẫn | Vai trò |
| --- | --- |
| `src/storage_restore.c` | Payload standalone: quét, mount, đăng ký và thông báo |
| `src/pkg_reader.h` | Parser FIH/CNT và kiểm tra bảng metadata |
| `src/restore_metadata.h` | Streaming, đối chiếu, backup và thay `param.json` |
| `src/restore_native.h` | Ghi `_org_path`/`_m2_device_id` vào `appinfo.db` kèm backup và kiểm tra |
| `build.py` | Cross-build PS5 trên Windows với LLVM/LLD |
| `tests/` | 23 parser + 23 metadata test dùng dữ liệu giả lập |
| `verification/console-summary-1.4.json` | Kết quả 1.4 đã quan sát, không chứa database/log riêng |
| `verification/console-summary-1.3.json` | Kết quả 1.3, giữ lại để đối chiếu |

## Build standalone trên Windows

Cần Python 3.12+, LLVM/Clang, `ld.lld`, `llvm-strip` trên PATH và PS5 payload SDK có json-c và sqlite3 static (`libjson-c.a`, `libsqlite3.a`). Bộ SDK đã dùng: pacbrew-repo v0.40.2, asset `ps5-payload-dev.tar.gz`, SHA256 `a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d`.

```powershell
$env:PS5_SDK = 'C:\ps5-payload-sdk'
python .\build.py
```

SDK phải có `target/include`, `target/lib/crt1.o`, `target/user/homebrew/include/json-c`, `target/user/homebrew/lib/libjson-c.a` và `ldscripts/elf_x86_64.x`. Output: `PS5_STORAGE_RESTORE.elf`, `BUILD.json`, ELF debug và map trong `build/`. Build kiểm tra ELF64 little-endian, ET_DYN/x86-64, segment và entrypoint; giữ ELF đầu ra cũ theo SHA256. Mỗi build mới bắt đầu với runtime verification = false.

## Kiểm thử host trên Windows

Cần Clang, CMake và Ninja. json-c 0.19 được tải từ nguồn release đã ghim SHA256; bộ test dùng fixture JSON tự tạo.

```powershell
python -m pip install --target build/host-deps ninja
python tests/build_host_jsonc.py
python tests/test_parser.py
python tests/test_metadata.py
```

Bộ public kiểm tra 23 parser case và 23 metadata case: kích thước UINT32 tối đa bằng extent giả lập, kiểm tra vượt phạm vi, JSON hơn 2 MiB, streaming file 65 MiB cộng khối lẻ, backup nguyên byte, lỗi ghi/rename, chạy lại không ghi thêm, và nhóm ca `contentVersion` của 1.4: đồng bộ tiến lên, chặn hạ cấp, hạ cấp ở nhóm cuối, version viết thiếu nhóm, sáu dạng chuỗi version không đọc được.

## Chạy trên PS5

Đóng game và chờ tác vụ cài/cập nhật kết thúc. Chép ELF vào `/data/ps5_storage_restore/` rồi chạy một lần sau khi môi trường payload và M.2 sẵn sàng. Mở Thư viện trò chơi để kiểm tra. Log: `/data/ps5_storage_restore/restore.log`; backup param: `/data/ps5_storage_restore/backups/`.

Log cần có `START version=1.8-EN-VI-native`, `REGISTER <title> rc=0x00000000` và dòng `END`. Mount M.2 phải được tạo lại sau reboot; có thể đặt Recovery trong autoload hiện có sau payload nền tảng và khi ổ đã sẵn sàng. Giữ một Recovery. Không chép cấu hình ví dụ đè toàn bộ autoload của máy.

Payload xử lý package FIH/CNT đã cài, không khôi phục dữ liệu đã xóa hay format ổ. Thư mục đích bị chiếm hoặc không có dấu sở hữu sẽ bị bỏ qua; không tự che dữ liệu đó. Nếu phải xử lý thư mục metadata cũ, xác minh danh tính rồi giữ nguyên bằng rename trên cùng filesystem trước khi chạy lại. Chi tiết ở [docs/RECOVERY.md](docs/RECOVERY.md).

## Kiểm chứng đã quan sát

Việc ghi hai trường vị trí đã được kiểm chứng trên PS5 Pro FW 10.01: sau khi `_org_path` của 20 title M.2 được đổi sang `/mnt/ext1/user/app/<id>` và `_m2_device_id` thành 1, máy tắt bật lại giữ nguyên cả 20 giá trị và game mở được **mà không chạy payload nào lúc boot**. `integrity_check` của `app.db` và `appinfo.db` đều ok.

Bản 1.8 thực hiện đúng hai thao tác ghi đó. Lần chạy ngày 2026-10-18 với ELF SHA256 `17eabf322889f4ad5edf8577192f43b256dda440dfb15463981d7efa1e01db95`:

```
END registered=22 found=22 skipped=0 created=0 updated=0
NATIVE_SUMMARY applied=20 kept=0
DB_INTEGRITY ok        (truoc va sau khi ghi)
```

**Chưa quan sát:** kết quả sau reboot của trạng thái do chính 1.8 ghi, và nhánh "chưa có hàng trong database" — tức đúng kịch bản mất database mà payload này sinh ra để cứu. Trên máy thử mọi title đều đã có hàng nên nhánh đó chưa chạy lần nào. Icon trên TV, mở game và reboot của riêng bản 1.8 cũng chưa xác nhận.

Standalone **1.4** đã chạy trên PS5 Pro FW 10.01 ngày 2026-10-10: **21/21 title đăng ký thành công, skipped=0, created=6, updated=1**. ELF của lần chạy đó build từ đúng source trong repo này, SHA256 `d61a3864e647f09965aa0d0675b99f0b5110fbe52bdf5b6ee6db7dd5dd25f2f7`.

ASTRO BOT PPSA21567 là title mà 1.3 bỏ qua. Sau 1.4: `PARAM_VERSION existing=01.018.000 package=01.019.000 package-newer`, `REGISTER rc=0x00000000 mode=external-nullfs`. `param.json` ở cả ba vị trí (`sce_sys` trên M.2, `/user/appmeta`, `/system_data/priv/appmeta`) byte-identical với `param.json` trong `app.pkg`; backup bản cũ 01.018.000 giữ nguyên byte trong `/data/ps5_storage_restore/backups/`. Alias `/user/app/PPSA21567/app.pkg` khớp kích thước nguồn M.2. `app.db` và `appinfo.db` `integrity_check=ok` cả trước và sau. Param của các title khác không bị thay đổi.

**Chưa quan sát:** icon trên TV, mở game và reboot. Không có bước nào sửa autoload, chạy game hay chép database lên console.

Bản 1.3 trước đó chạy trên cùng máy ngày 2026-10-01, cũng 21/21 và skipped=0, với ELF `9eab8e6d5bfb5147e4428ed145b676a861a979fc5869f8a67afd6ccd21841f9e` build trước khi sửa dòng bản quyền. Bộ dữ liệu riêng của dự án đã qua thêm 9 fixture parser và 2 replay package thật; dữ liệu game không được đưa vào repo public.

## Giấy phép và nguồn liên quan

Bản quyền **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, xem [COPYRIGHT.md](COPYRIGHT.md). Source Recovery fix phát hành theo GPL-3.0-only trong [LICENSE](LICENSE). Thông báo thư viện build ở `licenses/`; SDK và thư viện build được cài riêng.
