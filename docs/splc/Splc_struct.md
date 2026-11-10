# Splc_struct.md — cấu trúc dữ liệu và API của SimplePLC mà port cần biết

> Nguồn chính thức: `simple_plc/docs/SimplePLC_App_MCU_Structs_v2.0_Self_Describing_Profile.md` (wire),
> `simple_plc/docs/firmware/MCU_CONFORMANCE_SPECIFICATION_V2_0.md` (hành vi), `simple_plc/docs/SPLC_SPEC_ZIGBEE_006_ICD.md` +
> `simple_plc/docs/zigbee_intergation.md` (Zigbee). File này là TÓM TẮT để agent không phải đọc 60 KB mỗi lần.
> Mâu thuẫn → tin code (`simple_plc/simple_plc/**`), rồi báo người dùng. Số/ký hiệu dưới đây đã đối chiếu với tài liệu và header ngày 09/11/2026.

## 1. Mô hình

- SimplePLC = Rule Engine `IF trigger THEN action` chạy trên **bảng tag phẳng**: `g_tag_value[MAX_TAGS]` (int32 mỗi tag; float32 lưu bit) + `g_tag_table[]` (kind, channel).
  Scan loop mỗi `PLC_SCAN_INTERVAL_MS` (10 ms). **Mô hình kéo**: ghi một tag chỉ ghi RAM; bước khác đọc lại sau. Không heap, struct cố định.
- **Đơn luồng, không khoá** khi `SX_OS_USE_FREERTOS=1`: `plc_engine_init()` rồi `plc_engine_poll()` từ ĐÚNG MỘT task. Task khác không được đụng tag/rule/draft/retain.
  Không log từ ISR. `splc_link_input()` từ đúng một ngữ cảnh.
- App (SimplePLC.Studio, .NET) nói **Modbus** với MCU: FC03 đọc, FC06/FC16 ghi (không dùng coil). 32-bit = 2 thanh ghi, **High Word trước**;
  2×uint8 = 1 thanh ghi, byte cao trước; CRC RTU little-endian; CRC rule/retain = **CRC-16/MODBUS trên byte wire** (`rule_table_wire_crc16()`).
  Giới hạn: FC03 ≤ 125 reg, FC16 ≤ 123 reg. Exception `0x02` địa chỉ sai, `0x03` giá trị sai.

## 2. Tag

`TagKind`: NONE=0, DI=1, DO=2, AI=3, VFLAG=4, VREG=5, MB_COIL=6, MB_HOLDING=7, VREG_RETAIN=8, COUNTER=9.
Layout theo board qua `SPLC_TagLayout` (không có tag sentinel ở index 0), thứ tự nhóm: **DI, DO, AI, VFLAG, VREG, VREG_RETAIN, COUNTER**.
Kiểu dữ liệu tag (bảng `0x0080..0x00BF`, chỉ `SPLC_REAL_ENABLE=1`): 0 chưa khai = INT32, 1 BOOL, 2 INT32, 3 REAL.

```c
typedef struct {               /* board_dtu_zigbee.c: s_tag_layout */
    uint16_t di_count, do_count, ai_count, vflag_count, vreg_count, vreg_retain_count, counter_count;
} SPLC_TagLayout;              /* SPLC_TAG_LAYOUT_TOTAL(...) = tổng; SPLC_BOARD_CHECK_TAG_COUNT kiểm = MAX_TAGS */
```
**DTU**: DI/DO/AI = 0; VFLAG 32, VREG 32, VREG_RETAIN 32, COUNTER 8 ⇒ **104 tag** (`MAX_TAGS` ở `board/board_tag_define.h`, mục `BOARD_DTU_ZIGBEE`).
Truy cập từ layer ≥ 3 qua `tag_read()/tag_write()/tag_get_kind()`. Cửa sổ Modbus tag `0x0900..0x09FF`: 2 reg/tag, tag ≥ `MAX_TAGS` đọc 0, ghi bị từ chối `0x02`.

## 3. Rule (32 byte = 16 thanh ghi, `rule_format_version = 7`)

```c
typedef struct SPLC_PACKED {
    union { int32_t threshold_lo; float threshold_lo_f; };  /* [0..1] ngưỡng dưới / HHmm bắt đầu (Time Window) */
    union { int32_t threshold_hi; float threshold_hi_f; };  /* [2..3] ngưỡng trên / HHmm kết thúc */
    uint32_t for_ms;                                       /* [4..5] dwell / chu kỳ */
    union { int32_t action_param; float action_param_f; }; /* [6..7] */
    uint16_t trigger_tag;      /* [8]  */
    uint16_t action_tag;       /* [9]  */
    uint16_t guard_tag;        /* [10] bit15 NEGATE, bit0..14 index; "không guard" = 0x7FFF (KHÔNG phải 0) */
    uint8_t  enabled;          /* [11H] */
    uint8_t  trigger_type;     /* [11L] ON_CHANGE, ON_RISE, ON_FALL, TIME_WINDOW, INTERVAL */
    uint8_t  compare_op;       /* [12H] NONE, EQ, NEQ, GT, LT, GTE, LTE, BETWEEN */
    uint8_t  action_type;      /* [12L] SET_TAG, TOGGLE_TAG, INC_COUNTER, WRITE_REMOTE*, LOG_EVENT*, SEND_ALARM*, ADD_TAG, SCALE_TAG  (*chưa làm) */
    uint8_t  flags;            /* [13H] chỉ REAL=1: bit0 TRIGGER_FLOAT, bit1 ACTION_FLOAT, 0x03 = FLOAT_DATA (SCALE_TAG float cần cả hai) */
    uint8_t  reserved[5];      /* [13L..15] ghi 0 */
} SPLC_RuleRecord_t;           /* sizeof == 32; không thêm rule_id; định danh rule = vị trí; MAX_RULES ≤ 100 */
```
State machine: `IDLE → TRIGGERED → COMPARED → DWELLING → GUARD_CHECK → FIRE/BLOCKED`. Nạp rule: ghi `RULE_COUNT_STAGED` + `EXPECTED_CRC16` → ghi staging (≤ 16 reg mỗi FC16 cho gói USB FS 64 B)
→ đọc `CONFIG_STATUS` → ghi `0xA5A5` vào `0xA000` → đọc version. Lưu Flash A/B (`plc_rule_flash`), một CRC phủ cả bản ghi.

## 4. Bản đồ thanh ghi (tóm tắt)

| Địa chỉ | R/W | Khối |
|---|---|---|
| `0x0000..0x0009` | R | `SPLC_DeviceDescriptor_t` (20 B): device_class, device_variant, hw ver ×3, fw ver ×3, `protocol_version=2`, `rule_format_version=7` |
| `0x0010` | R | số rule active |
| `0x0020..0x0029` | R | `SPLC_DeviceResourceInfo_t` (20 B): `wire_profile=2`, max_rules, runtime_tag_count, di/do/ai/vflag/vreg/vreg_retain/counter_count — App tự dựng TagCatalog từ đây |
| `0x0080..0x00BF` | R/W | bảng kiểu tag (REAL=1; nháp + COMMIT) |
| `0x0100..0x073F` | R | rule table đang chạy (16 reg/rule) |
| `0x0800..0x0809` | R | `SPLC_DeviceHealth_t` (uptime, reset_reason, health_flags, cpu/ram %, scan_time_ms, max_scan_time_ms) |
| `0x0810..0x0813` | R/W | RTC: `epoch_utc_s` (32), `tz_offset_min` (int16), `status_flags` RO (0x1 SYNCED, 0x2 HW_PRESENT, 0x4 BATTERY_LOW) |
| `0x0900..0x09FF` | R (W khi DIAG) | runtime tag |
| `0x0A00` / `0x0A01..02` | W / R | System Command (REBOOT, FACTORY_RESET, CLEAR_RULES, CLEAR_RETAIN) / status, error |
| `0x0A08..0x0A09` | R | **HardwareCapabilities** 32-bit (xem dưới) |
| `0x0A0A` | R | số cổng RS-485 vật lý (= `SPLC_MBM_PORT_COUNT`, KHÔNG tính Zigbee) |
| `0x0A20..0x0A24` | R/W | Diag Control (ENTER/HEARTBEAT/EXIT, lease 1000..60000 ms, mặc định 3000) |
| `0x0B00..0x0B7F` | R/W | 8 Timer + 8 Counter (FB) |
| `0x0C00..0x0D7F`, `0x0E00..0x0E5F` | R/W | MQTT Telemetry / Broker (`SPLC_MQTT_ENABLE`) |
| `0x0D80..0x0DFF` | R/W | MQTT Subscribe (`SPLC_MQTT_SUB_ENABLE`) |
| `0x0F00..0x0F43` | W/R | nạp chứng chỉ TLS (`SPLC_MQTT_TLS_CERT_ENABLE`) |
| `0x1000..0x10FF` | R/W | **MB_READ** 4 khối × 64 reg (`SPLC_MBM_ENABLE`; nháp + COMMIT) |
| `0x1100..0x11FF` | R/W | **MB_WRITE** 4 khối × 64 reg (`SPLC_MBW_ENABLE`) |
| `0x1300..0x13FF` | R/W | Wi-Fi (`SPLC_WIFI_ENABLE`) |
| `0x1400..0x143F` | R/W | bảng cổng RS-485: 4 × 16 reg (baud/parity/data/stop/gap/retry; vai Master/Slave) |
| `0x1600..0x16BF` | — | DTU Gateway (HTTP/TCP/routing) — **lib CHƯA có**, chờ đội App |
| `0x1700..0x173F` | R/W | **Zigbee** (mục 6) |
| `0x9000..0x9005`, `0x9010..0x964F`, `0xA000/0xA001` | | staging handshake, staging rule buffer, commit `0xA5A5`, active version |

**Capability `0x0A08`**: `0x0001` BASE_LOGIC, `0x0002` RETENTIVE, `0x0004` RTC, `0x0008` ETHERNET, `0x0010` MQTT_PUB, `0x0020` MQTT_SUB, `0x0040` SD_DATALOG,
`0x0080` MQTT_TLS_CERT, `0x0100` MODBUS_MASTER, `0x4000` MODBUS_SLAVE, `0x8000` ZIGBEE (chỉ khi build bật Zigbee **và** port đã `splc_zigbee_register()` lúc boot).
App đọc `0x0A08` trước khi Deploy; thiếu cờ cho khối nó dùng ⇒ Studio dừng Deploy. **Đừng đổi cờ ngoài thiết kế.**

Quy trình nạp 6 bước: đọc `0x0A08` → ghi ngoại vi (`0x0B00, 0x0B40, 0x0E00, 0x0C00, 0x0D80, 0x1000, 0x1100, 0x1200`, nháp) → staging rule `0x9010` → CRC `0x9003` → COMMIT `0xA000=0xA5A5`.
COMMIT từ chối khối MB_READ/MB_WRITE trỏ vào cổng vai Slave: `CONFIG_ERROR_CODE = 7`.

## 5. Struct/API phía port (những gì agent thực sự code)

Đăng ký kiểu `netif_add` (struct được COPY; gọi trước `plc_engine_init()`, từ ngữ cảnh engine):

```c
/* App link: modbus_link.h */
typedef struct {
    void *ctx;
    int32_t (*output)(const uint8_t *buf, uint16_t len, void *ctx);  /* core→peer, không chặn; n=đã nhận (0=bận, thử lại), <0=lỗi cứng */
    void    (*process)(void *ctx);   /* tùy chọn; gọi mỗi scan và khi read/write chờ; nơi cho TinyUSB tud_task() */
    uint8_t  unit_id;                /* 1..247 cho RTU/ASCII */
} splc_link_ops_t;
splc_link_err_t splc_link_register(const splc_link_ops_t *ops);   /* OK=0, ERR_ARG=-1, ERR_REGISTERED=-2, ERR_NOT_READY=-3 */
uint16_t        splc_link_input(const uint8_t *buf, uint16_t len); /* peer→core; MỘT ngữ cảnh duy nhất; ISR-safe; ring SPLC_LINK_RX_BUF_SIZE=512 */
bool            splc_link_get_transport(modbus_transport_t *out);  /* kind = NONE: board PHẢI gán kind (RTU) */
```
```c
/* Transport Modbus (modbus_transport.h): TCP dùng cái này, add_transport khi SPLC_MODBUS_MAX_TRANSPORTS > 1 */
typedef struct {
    void *ctx;
    int32_t (*read)(uint8_t *buf, uint16_t count, int32_t timeout_ms, void *ctx);   /* trả số byte; 0 = timeout, <0 = lỗi cứng */
    int32_t (*write)(const uint8_t *buf, uint16_t count, int32_t timeout_ms, void *ctx); /* trả count; <0 = lỗi */
    void    (*process)(void *ctx);                                                  /* có thể NULL */
    modbus_transport_kind_t kind;   /* NONE=0 (bị từ chối), RTU=1, TCP=2, ASCII=3 (cần LIB=MB) */
    uint8_t unit_id;
} modbus_transport_t;                /* bool plc_modbus_cfg_add_transport(const modbus_transport_t *); */
```
```c
/* Ống byte RS-485 / Zigbee (modbus_serial.h) */
typedef struct {
    int  (*read)(void *ctx, uint8_t *buf, uint16_t max_len);     /* KHÔNG chặn; trả số byte, 0 = chưa có. BẮT BUỘC */
    int  (*write)(void *ctx, const uint8_t *buf, uint16_t len, uint32_t timeout_ms); /* 0 = xong; <0 lỗi. NULL được nếu có write_start+tx_done */
    void (*set_tx)(void *ctx, bool tx);                          /* tùy chọn: điều khiển DE */
    void *ctx;  uint32_t baudrate;                               /* tốc độ THẬT, lib dùng tính khoảng lặng/timeout */
    int  (*write_start)(void *ctx, const uint8_t *buf, uint16_t len);  /* SPLC_MBM_ENABLE: gửi không chặn, 0 = đã bắt đầu */
    bool (*tx_done)(void *ctx);                                  /* true khi bit cuối đã ra khỏi UART; poll mỗi scan */
    void (*write_abort)(void *ctx);
    uint8_t frame_sync;                                          /* LIB=MB, phía slave: 0 SYNC_TIMING (UART thật), 1 SYNC_LENGTH (ống không giữ thời gian byte, vd module radio) */
} modbus_serial_port_t;
/* RS-485: */ void splc_mbm_set_rs485_port(uint8_t port_index /*0..COUNT-1 = cổng App - 1*/, const modbus_serial_port_t *port,
                                           int (*configure)(void *ctx, const plc_mbm_serial_t *cfg) /* NULL = bỏ qua line settings của App */);
/* DTU board file chuyển tiếp: */ bool splc_dtu_set_rs485_port(uint8_t port_index, const modbus_serial_port_t *, plc_mbm_configure_fn);
```
Gửi **chặn trong `write_start`** (mode 3) là cấu hình đã chạy trên board Zigbee-IO; gửi bằng ngắt (`HAL_UART_Transmit_IT`) đã FAIL ở đó (quyết định 46, nguyên nhân gốc chưa rõ).
```c
/* RTC ngoài (SPLC_RTC_EXTERNAL=1): plc_rtc.h */
typedef struct { void *ctx; bool (*get_epoch)(void *ctx, uint32_t *epoch_utc_s); bool (*set_epoch)(void *ctx, uint32_t epoch_utc_s);
                 bool (*is_synced)(void *ctx); /* có thể NULL */ } plc_rtc_ops_t;
bool plc_rtc_register(const plc_rtc_ops_t *ops);   /* + g_rtc_caps.hw_present do port đặt */
/* MQTT publish (host cung cấp): */
typedef int (*splc_mqtt_publish_fn)(const char *topic, const char *payload, size_t len, uint8_t qos, bool retain, void *ctx); /* không chặn; 0 ok, <0 bận */
bool splc_mqtt_register(splc_mqtt_publish_fn fn, void *ctx);   /* payload NUL-terminated, buffer bị dùng lại ngay: sao chép nếu cần giữ */
```
```c
/* Zigbee (plc_zigbee.h) */
#define SPLC_ZIGBEE_ROLE_SLAVE 0x01U   /* Router = Modbus Slave: BẮT BUỘC có read+write */
#define SPLC_ZIGBEE_ROLE_MASTER 0x02U  /* Coordinator = Modbus Master */
typedef struct { void *ctx; uint8_t roles;
  bool (*status)(void *ctx, splc_zigbee_status_t *out);        /* tùy chọn, không chặn */
  bool (*validate)(void *ctx, const splc_zigbee_config_t *draft); /* tùy chọn; false = từ chối COMMIT (mã 2); KHÔNG gọi khi nạp Flash lúc boot */
  int  (*read)(void *ctx, uint8_t *buf, uint16_t max_len);
  int  (*write)(void *ctx, const uint8_t *buf, uint16_t len, uint32_t timeout_ms);
  uint32_t baudrate; } splc_zigbee_ops_t;
bool splc_zigbee_register(const splc_zigbee_ops_t *ops);       /* false nếu ops NULL hoặc không có bit vai */
/* splc_zigbee_config_t: enable, role_slave, security, antenna_ext, channel, pan_id, local_addr, dest_addr, power_level, baud_rate_idx,
   parity, data_bits, stop_bits, response_timeout_ms, inter_frame_delay_ms, retry_count, slave_id, name[], net_key[], key_set */
/* splc_zigbee_status_t: link_state, rssi_dbm, lqi, tx_packets, rx_packets, tx_errors, rx_errors, error_code (PLC_ZIGBEE_ERR_*) */
```
Board hook (`board.h`): `board_get_tag_layout()`, `board_hw_init()` (gọi từ `splc_board_init()`), `board_get_modbus_transport()`.
Thứ tự `plc_engine_init()`: `tag_table_load_from_flash(&layout)` → FB init → tagtype init → rule table load (Flash) → `retain_store_restore()` → PVD callback →
`board_init()` (`board_hw_init`) → `plc_modbus_cfg_init()` (lấy transport; **link phải đã đăng ký**).

## 6. Zigbee `0x1700..0x173F` (ICD v2.0.0, 64 reg, big-endian)

`+0` CONTROL_FLAGS (bit0 ENABLE, bit1 ROLE 0=Master/1=Slave, bit2 FRAMING **phải 0**=RTU, bit3 SECURITY, bit4 ANTENNA 0 PCB/1 SMA, bit5 PRESERVE_KEY cờ lệnh, bit6..15 = 0),
`+1` CHANNEL 11..26, `+2` PAN 1..0xFFFE, `+3` LOCAL 0..0xFFFD, `+4` DEST (0xFFFF = broadcast), `+5` POWER 0..4, `+6` BAUD_IDX 0..4 (9600, 19200, 38400, 57600, 115200),
`+7` PARITY 0/1/2, `+8` DATA_BITS = 8, `+9` STOP 1..2, `+10` RESP_TIMEOUT 50..5000 ms, `+11` INTER_FRAME 10..500 ms, `+12` RETRY 0..5, `+13` SLAVE_ID 1..247 (Slave; 236/237 do port từ chối),
`+14..15` dự phòng, `+16..+23` NET_KEY (**chỉ ghi**, 16 B, đọc luôn 0), `+24..+31` NETWORK_NAME (≤15 ký tự in được + NUL), `+32..35` RO 0, `+36` STATUS_FLAGS (KEY_SET),
`+37..+44` LINK_STATE, RSSI, LQI, TX/RX packets, TX/RX errors, ERROR_CODE. Nháp + COMMIT; ảnh Flash 32 reg (`+32..+63` không lưu); kiểm trường chỉ khi `ENABLE=1`.
Qua Zigbee **chỉ Modbus RTU** (không ASCII). DTU Master = Coordinator, board IO Slave = Router. Zigbee không phải "cổng RS-485 số 5".
Module DRF1609H: **Slave ID 236/237 là lệnh riêng của module** (DRF 5.4/5.5) — port từ chối trong `validate`, `status` báo `PLC_ZIGBEE_ERR_RESTRICTED_ID` (3).

## 7. Flash dữ liệu của lib (STM32H5, sector 8 KB)

Vùng dữ liệu = **5 sector cuối** tính từ `END = 0x08000000 + SPLC_FLASH_SIZE_KB*1024`: sector cuối = Rule Table A, kế = Rule Table B, 3 sector trước = Retain (log xoay vòng, record 208 B).
DTU (`SPLC_FLASH_SIZE_KB 2048`, END=`0x08200000`): A=`0x081FE000`, B=`0x081FC000`, Retain=`0x081FA000/0x081F8000/0x081F6000`. Firmware phải nằm dưới `END - 40 KB` = `0x081F6000`.
`splc_flash_define.h` kiểm lúc biên dịch: size bội của 16 KB, 128..2048 KB, 5 sector trọn trong bank 2. Mọi thao tác Flash ngoài `plc_retain.c` bọc `retain_flash_op_begin/end()`.
**Linker host `FLASH LENGTH = 2048K` chồng lên vùng này** — xem `Rule_Agent.md` mục 4.4.

## 8. Option `splcopts.h` quan trọng (đầy đủ: `config/splc_opt.h`, mẫu `config/splcopts_template.h`)

`SPLC_PLATFORM`, `SPLC_BOARD`, `SPLC_FLASH_SIZE_KB`, `SX_OS_USE_FREERTOS`, `PLC_SCAN_INTERVAL_MS`, `MAX_RULES`, `SPLC_PORT_USB_CDC`, `SPLC_MODBUS_UNIT_ID` (mặc định 1),
`SPLC_MODBUS_MAX_TRANSPORTS`, `SPLC_LINK_RX_BUF_SIZE` (lũy thừa 2, ≥256, mặc định 512), `SPLC_MODBUS_LIB` (`SPLC_MODBUS_LIB_NANO`=1 mặc định, `_MB`=2), `SPLC_MBM_ENABLE`, `SPLC_MBM_PORT_COUNT` (1..4; DTU ≤ 3),
`SPLC_MBW_ENABLE` (cần MBM), `SPLC_MODBUS_SLAVE_ENABLE` (cần MBM), `SPLC_REAL_ENABLE`, `SPLC_MQTT_ENABLE`, `SPLC_MQTT_SUB_ENABLE`, `SPLC_RTC_BUILTIN`/`SPLC_RTC_EXTERNAL`, `SPLC_LOGGER_BUILTIN`,
`SPLC_UART_BUILTIN`, `SPLC_ZIGBEE_ENABLE`, `SPLC_INDICATOR_ENABLE`, `SPLC_LOG_LEVEL` (0..4). CMake đọc `SPLC_BOARD`, `SPLC_PLATFORM`, `SPLC_LOGGER_BUILTIN`, `SPLC_PORT_USB_CDC`, `SPLC_RTC_BUILTIN` thẳng từ file này **lúc configure** ⇒ đổi xong phải xoá thư mục build.
Cờ `STM32H5_PLATFORM`, `BOARD_*`, `SX_NO_OS` được SUY RA, không đặt tay. Tuỳ chọn của port sản phẩm (chỉ port đọc): `SPLC_TCP_PORT_NUMBER`, `SPLC_HOST_RS485_TX_TIMEOUT_MS`.