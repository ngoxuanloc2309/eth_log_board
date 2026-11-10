# Rule_Agent.md — luật làm việc cho agent port SimplePLC vào host DTU

> Đọc file này TRƯỚC. Rồi đọc theo thứ tự: `DTU_host_map.md` (host là gì) → `splc_how_porting.md` (nối thế nào)
> → `Splc_struct.md` (struct/API) → trong repo splc: `Readme.md`, `simple_plc/docs/architecture.md`,
> `simple_plc/docs/handoff.md` (mục 2 quyết định, mục 3 quy trình, mục 6 bài học — file 240 KB, đọc có chọn lọc).
> Nguồn gốc các file này: phiên làm việc 09/11/2026. Mục nào ghi **[ĐÃ ĐỌC CODE]** = đã đối chiếu với code thật;
> **[CHƯA KIỂM]** = chưa build/chạy, đừng coi là sự thật.

## 1. Bạn là ai, làm gì

Bạn viết/sửa **code port** để thư viện `SimplePLC` (lib, rule engine IF–THEN trên bảng tag) chạy trong firmware host
**DTU** (STM32H563, build CMake + FreeRTOS + lwIP), rồi **debug** đến khi chạy
được. Lib chỉ sở hữu LOGIC; phần cứng thuộc host (trừ USB, xem mục 4). Người dùng là chủ dự án, tự push git; bạn không push.

Bố cục thư mục dự kiến trong host (người dùng chốt 09/11):
```
X.Link-DTU/
  libs/simple_plc/        
  port/splc
```

## 2. Luật bắt buộc (từ handoff mục 2/3 và người dùng)

1. **Không sửa file CubeMX tự sinh, kể cả khối `USER CODE`** (quyết định 9). Cần hành vi khác → chặn ở lớp thấp
   (linker `--wrap`, lớp `sx_*`, hoặc nhờ người dùng đổi `.ioc`). Thấy bug thật trong file sinh (vd. linker) → **báo và HỎI trước khi sửa**.
2. **Việc kiến trúc lớn hỏi bằng lựa chọn** (`ask_user_input_v0` nếu có công cụ; không thì liệt kê 2–3 phương án ngắn), từng quyết định một.
   Quyết định liên quan hợp đồng App: kiểm xem đội App đã trả lời chưa.
3. **Option tắt (`X=0`) phải ra binary GIỐNG HỆT bản trước** — so md5 của `.bin`. Option mới: thêm vào `config/splc_opt.h`
   (mặc định + `#error` kiểm giá trị) và `config/splcopts_template.h`; không rải `#define` cấu hình vào header layer.
4. **Tên không được trùng host** (quyết định 21). File/symbol mới của sản phẩm mang tiền tố `splc_`; kiểm trùng bằng
   basename header + `nm` trước khi coi là xong. Cảnh báo đã biết: host và lib đều có **`sx_uart.h`** với API KHÁC NHAU (xem `DTU_host_map.md`).
5. **Việc chưa chạy board thì nói rõ là chưa test.** Nói rõ mức chắc chắn khi nêu nguyên nhân; kiểm chứng bằng dữ liệu thật,
   đừng đoán. Số đo (FLASH/RAM) không cộng rời — đọc map thật (bài học 21).
6. **Trao đổi tiếng Việt, code/comment tiếng Anh.**
7. Mỗi mốc: code → build ARM (host thật) → người dùng nạp board + chạy test → cập nhật tài liệu. Không tuyên bố "xong" khi mới đọc diff.
8. Không tự đổi quyết định đã chốt (mục 4); nghi ngờ thì hỏi.

## 3. Mục tiêu và trạng thái (09/11/2026)

Mục tiêu: DTU chạy SimplePLC với **App Studio nạp rule qua USB-CDC và Modbus TCP**, 3 cổng RS-485 (MB_READ/MB_WRITE/slave),
RTC của host, MQTT qua backend của host, cấu hình Zigbee `0x1700`. Hiện trạng:

| Hạng mục | Trạng thái |
|---|---|
| `board_dtu_zigbee.{h,c}`, `DTU_Zigbee/port/*` | đã sửa theo host `ec5cf52` ngày 09/11; **[CHƯA KIỂM]** chưa build, chưa link, chưa nạp board |
| Lib `use_mb_new`, Zigbee `0x1700` theo ICD v2.0.0 | kiểm bằng host giả (365 kiểm tra), **chưa build ARM đầy đủ**, chưa chạy board |
| `plc_indicator` | lib đã code; trên DTU đang TẮT (host có `indicator_app`) |
| Zigbee Slave (Modbus qua UART9) | **chưa có** — chờ quyết định UART9 (mục 4) |

## 4. Quyết định đã chốt cho DTU / còn mở

**Đã chốt (người dùng):**
- Link App = **USB-CDC + Modbus TCP**. USB dùng **TinyUSB có sẵn của lib** (`SPLC_PORT_USB_CDC 1`, module `simple_plc/port/usb/`),
  CDC instance 0 của lib. Modbus TCP là transport thứ hai (`SPLC_MODBUS_MAX_TRANSPORTS 2`), `port/splc_tcp_port.c`.
- **Host sở hữu logger và mọi UART** (mốc 3, 07/11): lib chỉ là logic. `SPLC_LOGGER_BUILTIN 0`, `SPLC_UART_BUILTIN 0`;
  log của lib đi vào `log_func()` của host (`libs/logger`, `logger_init` ở `app_freertos.c`).
- `SPLC_MODBUS_LIB MB` (master qua Zigbee chỉ có ở MB), `SPLC_BOARD_DTU_ZIGBEE`, `SPLC_FLASH_SIZE_KB 2048`, 3 cổng RS-485.
- Không sửa file CubeMX.

**ĐÃ LỖI THỜI trong handoff (đừng làm theo):**
- Handoff mục 100(e) "hướng B: HOST giữ USB, `#error` nếu `SPLC_PORT_USB_CDC=1`" → **bị thay** bằng quyết định trên.
  Lý do: host `eth_log_board` HEAD `ec5cf52` ("board dtu real", 09/11) đã bỏ TinyUSB `usb/`, ECM, `SynaptiX/`; chỉ còn HAL PCD
  (`Core/Src/usb.c`), UART qua `board/dev/uart/sx_uart.*`, TCP qua `applications/tcp/`. Các mô tả "host dùng ECM/TinyUSB/`uart_*()`/`board.h`/`RF_Gateway.ioc`" đều sai với host hiện tại. **[ĐÃ ĐỌC CODE]**
- Port DTU cũ dùng `uart_read_bytes/uart_write_bytes/board.h` (SynaptiX) → đã viết lại sang `sx_uart_*`.

**Còn mở — HỎI người dùng, đừng tự chọn:**
1. **Ai sở hữu UART9** (module DRF1609H). `rf_app` của host đọc mọi byte (`sx_uart_read(SX_UART_PORT_UART9)`) để parse dòng `:...` và gửi
   lệnh INS. Lib muốn một ống byte Modbus RTU nhị phân trên cùng UART. Hai bên cùng đọc ⇒ cướp byte nhau. Phương án: **A** (đang áp dụng)
   `rf_app` giữ UART9, port chỉ khai vai Master, KHÔNG có ống byte ⇒ chưa có Modbus Slave qua Zigbee; **B** `rf_app` nhường UART9 khi ở chế độ
   Modbus (cần cơ chế chuyển chế độ); **C** `rf_app` parse rồi chuyển khung không phải `:` sang lib qua ring buffer.
2. **LED**: host `indicator_app` + `dtu_board.c` đang điều khiển PC6/PC7/PC8 (LED2/1/0, active HIGH). Hiện `SPLC_INDICATOR_ENABLE 0`.
   Bật cả hai ⇒ tranh chân. Muốn bật thì phải thống nhất ai là chủ.
3. **Đánh số cổng RS-485 theo nhãn vỏ máy.** Hiện App cổng 1/2/3 = UART7/USART3/USART10 (thứ tự `settings` và enum `sx_uart` của host);
   board file cũ ghi USART3/UART7/USART10. Hỏi nhãn in trên vỏ.
4. **Linker host**: `STM32H563xx_FLASH.ld` khai `FLASH LENGTH = 2048K`, trong khi lib dành 5 sector cuối (40 KB, `0x081F6000..0x081FFFFF`)
   cho Rule Table A/B + Retain. Firmware lớn lên chạm vùng này sẽ bị lib xoá/ghi. Cần giảm `LENGTH` còn `2008K` (hoặc đủ nhỏ) — **file linker do Cube sinh: HỎI người dùng trước.** **[ĐÃ ĐỌC CODE; chưa đo kích thước ảnh thật]**
5. Câu hỏi `ROLE` gửi App (`zigbee_intergation.md` mục 5b): ROLE là vai Modbus hay vai mạng Zigbee.
6. Cổng TCP `502` là GIẢ ĐỊNH (`SPLC_TCP_PORT_NUMBER`); hỏi App Studio dùng cổng nào (lần trước nối TCP cổng 5020 trên gateway cũ).

## 5. Việc cho agent (theo thứ tự đề xuất)

**T0 — Dựng baseline host.** Build host `eth_log_board` NGUYÊN BẢN trước khi nối lib (bài học 21), ghi FLASH/RAM và md5 `.bin`.

**T1 — Nối build.** Theo `splc_how_porting.md` mục 2: đặt lib vào `libs/splc`, port vào `porting/`, `git submodule update --init --depth 1
simple_plc/libs/tinyusb` (TinyUSB là submodule của repo splc; CMake lib báo lỗi nếu thiếu). Sửa `porting/CMakeLists.txt` cho đúng đường dẫn mới
(file đang tham chiếu `../Zigbee_IO/port` — xem mục T1a). Mục tiêu: **link ra `.elf`**, 0 lỗi, 0 cảnh báo mới.
- T1a. `splc_host_zigbee_port.c` include `splc_zigbee_port.h` và CMake bắt buộc `Zigbee_IO/port/{splc_zigbee_port.h,splc_indicators_port.c}`.
  Với `SPLC_INDICATOR_ENABLE 0` có thể bỏ phụ thuộc này: chép `splc_zigbee_port.h` hoặc khai prototype `splc_zigbee_port_ms_since_rx()`
  vào `porting/port/`, bỏ `splc_indicators_port.c` khỏi danh sách. Hỏi người dùng nếu muốn giữ cả repo `Zigbee_IO/` trong `libs/splc`.
- T1b. Kiểm va chạm tên: `nm` trên `.elf`/`.o` tìm symbol trùng (đặc biệt `sx_uart_*`, `sx_get_tick_ms`, `USB_DRD_FS_IRQHandler`, `HAL_*Callback`).
  `sx_get_tick_ms` do lib định nghĩa ở `platforms/stm32/stm32h5/time/stm32h5_time.c`: **[CHƯA KIỂM]** file này vẫn được build khi `SPLC_UART_BUILTIN 0`.

**T2 — USB.** Người dùng đổi `.ioc`: tắt sinh handler ngắt `USB_DRD_FS` (NVIC, bỏ "Generate IRQ handler") để `board_dtu_zigbee.c` định nghĩa
`USB_DRD_FS_IRQHandler()` gọi `splc_usb_irq()`. Kiểm: `HAL_PCD_MspInit` (clock 48 MHz, chân PA11/PA12, NVIC) vẫn chạy khi giữ `MX_USB_PCD_Init()` rồi
TinyUSB `stm32_fsdev` `tusb_init`; xung đột `hpcd_USB_DRD_FS` với TinyUSB (cả hai đều khởi tạo ngoại vi USB) — nếu có, báo người dùng:
phương án là bỏ hẳn USB khỏi `.ioc` và để lib tự bật clock/chân (cần viết thêm, HỎI). Ưu tiên ngắt USB phải hợp lệ với `configMAX_SYSCALL_INTERRUPT_PRIORITY`. Test: `test/test_diag.py`, `test_fb.py` qua COM của CDC.

**T3 — RS-485 (`splc_host_rs485_port.c`).** Yêu cầu host: 3 UART phải cài ở chế độ INTERRUPT/DMA có RX buffer (hiện `settings_install_uarts()` cài POLLING,
`rx_buffer_size 0` ⇒ `sx_uart_get_rx_bytes_available()` = -1 ⇒ port không đọc được). HỎI người dùng trước khi sửa `app_freertos.c`
(file dự án, không phải file Cube nhưng là code host). Sau đó test `test/test_mbm_live.py` (slave giả) và đo DE bằng logic analyzer.
Nếu host đổi baud lúc chạy (`settings`), port cần `configure` hook hoặc đọc lại baud — hiện chỉ đọc `huart->Init.BaudRate` lúc init.

**T4 — TCP + RTC + MQTT.** `splc_tcp_port.c` (lwIP socket, 1 client, kết nối mới thay cũ). Kiểm thứ tự khởi động: lwIP đã lên trước khi `splc_port_start()`.
`splc_rtc_port.c` dùng `hrtc`; **MX_RTC_Init của host có thể đặt lại giờ mỗi lần boot** (lib built-in RTC dùng `--wrap` để chặn việc này; ở đường
`SPLC_RTC_EXTERNAL=1` không có) ⇒ kiểm giờ sau reset, báo người dùng nếu mất. MQTT: host cắm backend bằng `splc_mqtt_port_set_backend()`
(host dùng `mqtt_client_posix` — đọc cách publish; backend CHƯA viết).

**T5 — Gọi lib từ host.** Thêm ĐÚNG MỘT dòng `splc_port_start();` vào luồng khởi tạo của host (sau khi lwIP/logger/UART đã sẵn sàng; xem `DTU_host_map.md`).
Task engine: stack 2048 từ + ưu tiên `idle+2` là **đoán**, đo `uxTaskGetStackHighWaterMark()` sau khi COMMIT/Flash.

**T6 — Zigbee.** Sau khi người dùng chốt UART9 (mục 4.1). Với B/C: viết `read/write` trên `sx_uart` + `rf_app` handover, đặt `roles = SLAVE|MASTER`, `validate`
(Slave ID 236/237), `status`. Test `test/test_zigbee_cfg.py`, `test_zigbee_slave_read.py` khi có board.

**T7 — Khi tất cả chạy:** đo FLASH/RAM thật, so với baseline T0; ghi vào handoff mục 100 (bản mới thay mục (e)).

## 6. Debug playbook (triệu chứng → kiểm gì trước)

| Triệu chứng | Kiểm |
|---|---|
| Link lỗi `multiple definition of USB_DRD_FS_IRQHandler` | `.ioc` còn sinh handler ngắt USB (T2) |
| Link lỗi `undefined splc_dtu_set_rs485_port` / `splc_*_port_init` | module sản phẩm phải vào exe bằng **OBJECT library** (bài học 87), không STATIC; `board_dtu_zigbee.c` chỉ được CMake chọn khi `SPLC_BOARD_DTU_ZIGBEE` |
| Lỗi biên dịch kiểu `sx_uart_t undeclared` / sai chữ ký `sx_uart_read` | include nhầm `sx_uart.h` của lib; port dùng `"dev/uart/sx_uart.h"` của host |
| Link lỗi `undefined sx_os_*` | thiếu `$<TARGET_OBJECTS:splc_platform_freertos>` + `-DSPLC_OS_INCLUDE_DIRS` (library mode không tự gắn) |
| App không thấy thiết bị qua USB | `log_func` có `USB CDC initialized`? `tud_mounted()`; ngắt USB có chạy; `splc_link_rx_dropped()`; unit id = `SPLC_MODBUS_UNIT_ID` |
| App qua TCP không nối | `LWIP_SOCKET 1` ✔; cổng 502 vs cổng Studio; `splc_tcp_port_attach()` có gọi sau `plc_engine_init()`; log `socket()/bind()` errno |
| MB_READ trả `err_code 1` | cổng chưa đăng ký (`log_warn` "RS485 port N not registered") hoặc COMMIT đang trỏ cổng vai Slave (mã lỗi 7) |
| MB_READ timeout, không byte về | UART ở POLLING (T3); DE không lên/hạ; baud lib ≠ baud thật; RE/DE nối chung nên không echo — nếu thấy echo thì board khác giả định |
| Scan overrun / App chậm | `write_start` chặn đúng thời gian khung (255 B ở 9600 ≈ 300 ms, quyết định 46); `splc_engine` task bị Flash erase chặn |
| Reset ngẫu nhiên khi COMMIT | stack task engine; Flash vùng dữ liệu chạm firmware (mục 4.4); PVD |
| Zigbee `0x1700` COMMIT bị từ chối | `CONFIG_ERROR_CODE` (`zigbee_intergation.md` mục 5); Slave ID 236/237; role không khai |

## 7. Báo cáo cuối mỗi lượt (bắt buộc)

Nêu: (1) file đã đổi + đường dẫn đích + file xoá; (2) đã kiểm gì **thực sự chạy** (lệnh, kết quả) và gì mới chỉ đọc code; (3) việc người dùng phải làm (host/`.ioc`/nạp board);
(4) quyết định cần người dùng chốt, dạng lựa chọn ngắn. Không dùng từ "hoàn tất" cho thứ chưa link được.