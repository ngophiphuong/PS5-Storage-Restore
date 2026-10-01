# Metadata và rollback

**Tiếng Việt** · [English](RECOVERY.en.md)

Payload ưu tiên package nội bộ, sau đó quét `/mnt/ext0/user/app` đến `/mnt/ext15/user/app`. Với ổ ngoài, alias `/user/app/<title>` trỏ về thư mục source bằng nullfs RW. Ownership marker nằm tại `/data/ps5_storage_restore/owned/<title>`.

Trước khi thay `param.json` khác nội dung, payload kiểm tra JSON hợp lệ và khớp cả titleId, contentId, contentVersion. Backup được ghi O_EXCL, fsync, đối chiếu nguyên byte và ghi journal trước khi thay bằng rename. Không ghi đè backup đã có. Lỗi trước bước thay giữ param cũ; backup được giữ nếu bước sau lỗi.

Để rollback param: đóng game, đọc cặp `original=` và `saved=` trong dòng `BACKUP`, giữ bản hiện tại rồi chép đúng backup về đúng original. Không dùng backup của title hay vị trí khác.

Một thư mục `/user/app/<title>` không có app.pkg vẫn có thể còn metadata từ bản cài cũ. Nó phải được kiểm tra đầy đủ trước khi di chuyển: đúng danh tính, chỉ còn dữ liệu sce_sys/icon, không phải mount đang dùng, không có game đang chạy. Giữ toàn bộ thư mục bằng rename sang tên dự phòng trên cùng filesystem. Payload không tự thực hiện bước này. Trên máy thử, xử lý thư mục cũ của PPSA01467 theo cách này đã gỡ chặn đăng ký.

Để rollback thư mục cũ, đóng game, gỡ alias nullfs do Recovery tạo rồi đổi tên thư mục dự phòng về tên gốc. Giữ source M.2 và backup param. Không xóa đệ quy qua `/user/app/<title>` khi đó là alias.

Bản sao database thu thập qua FTP chỉ là chứng cứ tại thời điểm đọc; tiện ích không chép database cũ đè lên hệ thống. Source public không chứa database, log console, token, file game hoặc game metadata trích xuất.
