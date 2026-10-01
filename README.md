# PS5 Storage Restore 1.3 EN/VI

**Tiếng Việt** · [English](README.en.md)

Khôi phục đăng ký game PS5 đã cài trên bộ nhớ trong và SSD M.2 sau khi mất/reset database. Payload quét `PPSAxxxxx/app.pkg`, dựng metadata còn thiếu, tạo alias nullfs cho ổ ngoài và đăng ký qua API AppInst của hệ thống.

**Bản quyền NGÔ PHI PHƯƠNG x PSVIETHOA.COM**

Tải về: [Releases](../../releases) — `PS5_STORAGE_RESTORE.elf`, SHA256 `35d88926d143097136898ee2318c63f313992c1bb394df78f306cd49e390d5e2`.

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
| `build.py` | Cross-build PS5 trên Windows với LLVM/LLD |
| `tests/` | 23 parser + 14 metadata test dùng dữ liệu giả lập |
| `verification/console-summary-1.3.json` | Kết quả đã quan sát, không chứa database/log riêng |

## Build standalone trên Windows

Cần Python 3.12+, LLVM/Clang, `ld.lld`, `llvm-strip` trên PATH và PS5 payload SDK có json-c static. Bộ SDK đã dùng: pacbrew-repo v0.40.2, asset `ps5-payload-dev.tar.gz`, SHA256 `a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d`.

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

Bộ public kiểm tra 23 parser case và 14 metadata case: kích thước UINT32 tối đa bằng extent giả lập, kiểm tra vượt phạm vi, JSON hơn 2 MiB, streaming file 65 MiB cộng khối lẻ, backup nguyên byte, lỗi ghi/rename và chạy lại không ghi thêm.

## Chạy trên PS5

Đóng game và chờ tác vụ cài/cập nhật kết thúc. Chép ELF vào `/data/ps5_storage_restore/` rồi chạy một lần sau khi môi trường payload và M.2 sẵn sàng. Mở Thư viện trò chơi để kiểm tra. Log: `/data/ps5_storage_restore/restore.log`; backup param: `/data/ps5_storage_restore/backups/`.

Log cần có `START version=1.3-EN-VI-streaming`, `REGISTER <title> rc=0x00000000` và dòng `END`. Mount M.2 phải được tạo lại sau reboot; có thể đặt Recovery trong autoload hiện có sau payload nền tảng và khi ổ đã sẵn sàng. Giữ một Recovery. Không chép cấu hình ví dụ đè toàn bộ autoload của máy.

Payload xử lý package FIH/CNT đã cài, không khôi phục dữ liệu đã xóa hay format ổ. Thư mục đích bị chiếm hoặc không có dấu sở hữu sẽ bị bỏ qua; không tự che dữ liệu đó. Nếu phải xử lý thư mục metadata cũ, xác minh danh tính rồi giữ nguyên bằng rename trên cùng filesystem trước khi chạy lại. Chi tiết ở [docs/RECOVERY.md](docs/RECOVERY.md).

## Kiểm chứng đã quan sát

Standalone 1.3 đã chạy trên PS5 Pro FW 10.01 ngày 2026-10-01: **21/21 title đăng ký thành công, skipped=0**. FF7 Rebirth PPSA08668 và Spider-Man Remastered PPSA01467 đều `rc=0x00000000`, `external-nullfs`; ba vị trí param khớp package và hai database integrity_check=ok. Bảng icon của ba user ghi visible=1/installStatus=2. Alias app.pkg khớp kích thước và header nguồn M.2.

ELF đã thử trước khi chỉnh dòng bản quyền có SHA256 `9eab8e6d5bfb5147e4428ed145b676a861a979fc5869f8a67afd6ccd21841f9e`. Đã xác nhận mount/đăng ký trên console; chưa quan sát icon trên TV, mở game hoặc reboot. Source phát hành đổi dòng bản quyền thành NGÔ PHI PHƯƠNG x PSVIETHOA.COM, giữ nguyên logic fix; ELF build từ source này chỉ được kiểm chứng offline. Bộ dữ liệu riêng của dự án đã qua thêm 9 fixture parser và 2 replay package thật; dữ liệu game không được đưa vào repo public.

## Giấy phép và nguồn liên quan

Bản quyền **NGÔ PHI PHƯƠNG x PSVIETHOA.COM**, xem [COPYRIGHT.md](COPYRIGHT.md). Source Recovery fix phát hành theo GPL-3.0-only trong [LICENSE](LICENSE). Thông báo thư viện build ở `licenses/`; SDK và thư viện build được cài riêng.
