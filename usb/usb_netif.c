/**
  ******************************************************************************
  * @file    tusbnetif.c
  * @brief   TinyUSB (RNDIS/ECM) network interface driver for lwIP in FreeRTOS
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "logger.h"
#include "app_config.h"
#include "usb_netif.h"

static const char *TAG = "USB_NETIF";

/* Private define ------------------------------------------------------------*/
#define INTERFACE_THREAD_STACK_SIZE    (1024 * 4)
#define TIME_WAITING_FOR_INPUT         (portMAX_DELAY)

#define IFNAME0 'u'
#define IFNAME1 's'

#define INIT_IP4(a, b, c, d) {PP_HTONL(LWIP_MAKEU32(a, b, c, d))}

/* Private variables ---------------------------------------------------------*/
static struct netif usb_netif_data;
static osSemaphoreId_t UsbRxPktSemaphore = NULL;

TaskHandle_t UsbIfHandle = NULL;

/* MAC Address của TinyUSB Adapter (Local Administered) */
uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

/* Mạng cấu hình cho USB Interface */
static const ip4_addr_t ipaddr  = INIT_IP4(192, 168, 4, 1);
static const ip4_addr_t netmask = INIT_IP4(255, 255, 255, 0);
static const ip4_addr_t gateway = INIT_IP4(0, 0, 0, 0);

/* DHCP Entries cấp phát IP cho PC/Smartphone */
static dhcp_entry_t dhcp_entries[] = {
    {{0}, INIT_IP4(192, 168, 4, 2), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 4, 3), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 4, 4), 24 * 60 * 60},
};

static const dhcp_config_t dhcp_config = {
    .router = INIT_IP4(0, 0, 0, 0),
    .port   = 67,
    .dns    = INIT_IP4(192, 168, 4, 1),
    "usb",
    TU_ARRAY_SIZE(dhcp_entries),
    dhcp_entries
};

/* Private function prototypes -----------------------------------------------*/
static void usb_ethernetif_input(void *argument);
static err_t low_level_output(struct netif *netif, struct pbuf *p);
static void low_level_init(struct netif *netif);

/*******************************************************************************
                       LL Driver Interface (lwIP --> USB)
*******************************************************************************/

/**
  * @brief  Khởi tạo phần cứng cấp thấp cho USB Network Interface
  */
static void low_level_init(struct netif *netif)
{
  /* Cấu hình MAC Address (Đổi Bit LSbit để không trùng MAC với Host) */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;

  LOGI(TAG, "USB MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
       netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
       netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

  /* Maximum Transfer Unit */
  netif->mtu = CFG_TUD_NET_MTU;

  /* Cờ tính năng mạng */
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  /* Semaphore tín hiệu nhận gói tin từ USB */
  UsbRxPktSemaphore = xSemaphoreCreateBinary();

  /* Task quản lý vòng lặp tud_task() và xử lý packet USB */
  xTaskCreate(usb_ethernetif_input, "usb_netif_task", INTERFACE_THREAD_STACK_SIZE, netif, configMAX_PRIORITIES - 2, &UsbIfHandle);
}

/**
  * @brief  Đẩy gói tin dữ liệu từ lwIP ra cổng USB RNDIS/ECM
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  (void) netif;

  if (!tud_ready())
  {
    return ERR_USE;
  }

  /* Kiểm tra driver USB có sẵn sàng nhận độ dài dữ liệu này không */
  if (tud_network_can_xmit(p->tot_len))
  {
    tud_network_xmit(p, 0);
    return ERR_OK;
  }

  /* Trả về ERR_MEM nếu bộ đệm USB đang bận để lwIP thử lại sau, KHÔNG treo vòng lặp blocking */
  return ERR_MEM;
}
  static bool dns_proc(const char *name, ip4_addr_t *addr) {
    if (strcmp(name, "tiny.usb") == 0) {
      *addr = ipaddr;
      return true;
    }
    return false;
  }
/**
  * @brief Task đồng bộ sự kiện USB và đẩy dữ liệu nhận được vào lwIP
  */
static void usb_ethernetif_input(void *argument)
{
  struct netif *netif = (struct netif *) argument;

  /* Khởi tạo Stack TinyUSB */
  LOGI(TAG, "USB Stack Initializing...");
  tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  /* Khởi tạo các Service DHCP & DNS trên cổng USB */
  while (dhserv_init(&dhcp_config) != ERR_OK)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }


  while (dnserv_init(IP_ADDR_ANY, 53, dns_proc) != ERR_OK)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  LOGI(TAG, "USB Network Stack Started Successfully");

  for (;;)
  {
    /* 1. Luôn cập nhật trạng thái USB Stack dưới sự bảo vệ của Lock Core lwIP */
    LOCK_TCPIP_CORE();
    tud_task();
    UNLOCK_TCPIP_CORE();

    /* 2. Nhường CPU cho các Task khác thực thi */
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/**
  * @brief Khởi tạo Network Interface cho TinyUSB (gọi bởi netifapi_netif_add)
  */
err_t tusb_netif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "stm32_usb";
#endif

  // MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

  netif->output     = etharp_output;
  netif->linkoutput = low_level_output;

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif

  low_level_init(netif);

  return ERR_OK;
}

/*******************************************************************************
                       TinyUSB Callbacks Interface
*******************************************************************************/

/**
  * @brief Callback nhận dữ liệu từ TinyUSB Driver
  */
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  struct netif *netif = &usb_netif_data;

  if (size == 0)
  {
    return true;
  }

  /* Cấp phát pbuf từ lwIP RAM Pool */
  struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

  if (p == NULL)
  {
    LOGE(TAG, "Failed to allocate pbuf size: %d", size);
    return false;
  }

  /* Copy dữ liệu từ USB Buffer sang pbuf */
  pbuf_take(p, src, size);

  /* BẮT BUỘC: Khóa Lock Core trước khi nạp dữ liệu vào lwIP Stack */
  LOCK_TCPIP_CORE();
  err_t err = netif->input(p, netif);
  UNLOCK_TCPIP_CORE();

  if (err != ERR_OK)
  {
    LOGE(TAG, "netif->input failed with error: %d", err);
    pbuf_free(p);
  }

  /* Báo cho TinyUSB hoàn thành và sẵn sàng nhận gói tiếp theo */
  tud_network_recv_renew();

  return true;
}

/**
  * @brief Callback copy dữ liệu truyền đi từ pbuf sang USB hardware buffer
  */
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  struct pbuf *p = (struct pbuf *) ref;
  (void) arg;

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

/* Các Callback quản lý trạng thái kết nối USB */
void tud_mount_cb(void)
{
  LOCK_TCPIP_CORE();
  netif_set_link_up(&usb_netif_data);
  netif_set_up(&usb_netif_data);
  UNLOCK_TCPIP_CORE();
  LOGI(TAG, "USB Network Cable Plugged (Link UP)");
}

void tud_umount_cb(void)
{
  LOCK_TCPIP_CORE();
  netif_set_link_down(&usb_netif_data);
  netif_set_down(&usb_netif_data);
  UNLOCK_TCPIP_CORE();
  LOGI(TAG, "USB Network Cable Unplugged (Link DOWN)");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  LOCK_TCPIP_CORE();
  netif_set_link_down(&usb_netif_data);
  UNLOCK_TCPIP_CORE();
}

void tud_resume_cb(void)
{
  if (tud_mounted())
  {
    LOCK_TCPIP_CORE();
    netif_set_link_up(&usb_netif_data);
    UNLOCK_TCPIP_CORE();
  }
}

/*******************************************************************************
                       Public API Initialization
*******************************************************************************/

/**
  * @brief  Hàm đăng ký interface TinyUSB với lwIP stack (Gọi tại app_main/main)
  */
void usb_netif_init(void)
{
  /* Đăng ký netif với lwIP thông qua netifapi để đảm bảo Thread safety */
  netif_add(&usb_netif_data, &ipaddr, &netmask, &gateway, NULL, tusb_netif_init, tcpip_input);
  // netif_set_default(&usb_netif_data);
}