# Linux UniKey

Fork của [ibus-unikey](https://github.com/vn-input/ibus-unikey), dùng lõi xử lý tiếng Việt mã nguồn mở của UniKey. Bản này tập trung sửa độ trễ, mất chữ và lỗi autocomplete khi gõ tiếng Việt trên Ubuntu X11.

## Điểm khác biệt

- Gõ trực tiếp vào ứng dụng, không dùng chuỗi preedit bị bôi đen hoặc gạch chân.
- Chuyển ASCII, Backspace và ký tự tiếng Việt qua cùng luồng sự kiện IBus để giữ đúng thứ tự khi ứng dụng lag.
- Dùng layout XKB riêng cho 134 ký tự tiếng Việt; không dùng clipboard hay `CommitText`.
- Sửa lỗi Chrome omnibox mất chữ đầu, nhân đôi chữ đầu và sai dấu khi autocomplete xuất hiện.
- Giữ trạng thái UniKey qua các `reset`/`focus` phát sinh trong lúc Chrome và GTK nhận ký tự.
- Hỗ trợ Telex, VNI và các tùy chọn có sẵn của ibus-unikey.

## Tương thích

Đã kiểm tra trên Ubuntu 22.04, GNOME Xorg, IBus, Google Chrome và ứng dụng GTK 3.

Wayland chưa được hỗ trợ. Layout phát Unicode hiện dùng XKB và keycode X11 dành riêng. Chọn phiên đăng nhập **Ubuntu on Xorg** trước khi dùng.

Chỉ bật một bộ gõ tiếng Việt. Tắt hoặc gỡ ibus-unikey bản Ubuntu, IBus Bamboo, Fcitx/Fcitx5 hay engine tiếng Việt khác để tránh nhận một phím hai lần.

## Cài đặt

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config gettext ibus libibus-1.0-dev libgtk-3-dev libx11-dev

git clone https://github.com/thientranvan/linux_unikey.git
cd linux_unikey

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build

gsettings set org.freedesktop.ibus.engine.unikey direct-forward true
ibus-daemon --replace --cache=refresh --panel disable --xim --daemonize
sleep 1
ibus engine Unikey
```

Mở **Settings → Keyboard → Input Sources**, thêm **Vietnamese (Unikey)** nếu engine chưa xuất hiện trong danh sách.

Kiểm tra chế độ gõ trực tiếp:

```bash
gsettings get org.freedesktop.ibus.engine.unikey direct-forward
```

Kết quả cần là `true`.

## Cập nhật

```bash
git pull --ff-only
cmake --build build -j"$(nproc)"
sudo cmake --install build
ibus-daemon --replace --cache=refresh --panel disable --xim --daemonize
sleep 1
ibus engine Unikey
```

## Kiểm tra

Test GTK mô phỏng ứng dụng đứng 500 ms trong lúc nhận phím:

```bash
sudo apt install python3-gi gir1.2-gtk-3.0
tests/gtk_forward_smoke.py
```

Test Chrome omnibox cần Chrome đang chạy và các công cụ X11:

```bash
sudo apt install xclip x11-utils
python3 tests/chrome_omnibox_smoke.py
```

Kết quả mặc định:

```text
con mèo mà trèo cây cau hỏi thăm chú chuột đi đâu vắng nhà
```

Các ca hồi quy chữ đầu:

```bash
FIRST_KEY_DELAY=1 SMOKE_SEQUENCE='nawngs' SMOKE_EXPECTED='nắng' python3 tests/chrome_omnibox_smoke.py
FIRST_KEY_DELAY=1 SMOKE_SEQUENCE='awn gif truwa nay' SMOKE_EXPECTED='ăn gì trưa nay' python3 tests/chrome_omnibox_smoke.py
FIRST_KEY_DELAY=1 SMOKE_SEQUENCE='awngs' SMOKE_EXPECTED='ắng' python3 tests/chrome_omnibox_smoke.py
```

## Cấu trúc chính

- `ukengine/`: lõi xử lý UniKey.
- `src/engine.cpp`: engine IBus và đường phát phím trực tiếp.
- `data/xkb/symbols/unikey`: ánh xạ XKB cho ký tự tiếng Việt.
- `tests/`: smoke test Chrome và GTK.

## Giấy phép

Mã dự án dùng [GPL-3.0](LICENSE). Lõi nhúng trong `ukengine/` giữ giấy phép và thông báo bản quyền riêng tại [ukengine/LICENSE](ukengine/LICENSE).
