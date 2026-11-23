#include "phone_service.h"
#include "phone_policy.h"
#include "ble_radio_owner.h"
#include <stdatomic.h>
#include <string.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_att.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"

/* 8c8e0001-6e9b-4b71-9e1d-42b47c574631, RX ...0002, TX ...0003. */
#define PHONE_UUID(n) BLE_UUID128_INIT(0x31,0x46,0x57,0x7c,0xb4,0x42,0x1d,0x9e,0x71,0x4b,0x9b,0x6e,n,0x00,0x8e,0x8c)
static const ble_uuid128_t service_uuid = PHONE_UUID(1), rx_uuid = PHONE_UUID(2), tx_uuid = PHONE_UUID(3);
static SemaphoreHandle_t lock;
static SemaphoreHandle_t api_gate;
static QueueHandle_t incoming;
static wf_phone_snapshot_t state;
static int64_t discovery_deadline, activity_at;
static uint16_t connection = BLE_HS_CONN_HANDLE_NONE, tx_handle;
static uint32_t generation;
static bool initialized, subscribed;
static atomic_bool start_requested, stop_requested;
void ble_store_config_init(void);
typedef struct { uint16_t length; uint8_t data[244]; } chunk_t;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static int gap_event(struct ble_gap_event *event, void *arg);

static int access_gatt(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr; (void)arg;
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn, &desc) || !desc.sec_state.encrypted ||
        !desc.sec_state.authenticated) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    chunk_t chunk = {0};
    if (OS_MBUF_PKTLEN(ctxt->om) > sizeof(chunk.data)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (ble_hs_mbuf_to_flat(ctxt->om, chunk.data, sizeof(chunk.data), &chunk.length))
        return BLE_ATT_ERR_UNLIKELY;
    const bool ok = xQueueSend(incoming, &chunk, 0) == pdTRUE;
    memset(&chunk, 0, sizeof(chunk));
    if (!ok) atomic_store(&stop_requested, true); /* Never accept a truncated command. */
    return ok ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
static const struct ble_gatt_svc_def services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &service_uuid.u,
     .characteristics = (struct ble_gatt_chr_def[]) {
        {.uuid = &rx_uuid.u, .access_cb = access_gatt,
         .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN},
        {.uuid = &tx_uuid.u, .access_cb = access_gatt, .val_handle = &tx_handle,
         .flags = BLE_GATT_CHR_F_NOTIFY}, {0}}}, {0}
};
static void synchronized(void)
{
    uint8_t address_type;
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &address_type);
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    if (!rc) rc = ble_gap_adv_set_fields(&fields);
    struct ble_hs_adv_fields scan = {0};
    scan.name = (uint8_t *)"WristFoundry";
    scan.name_len = 12;
    scan.name_is_complete = 1;
    if (!rc) rc = ble_gap_adv_rsp_set_fields(&scan);
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    xSemaphoreTake(lock, portMAX_DELAY);
    int64_t remaining = discovery_deadline - now_ms();
    xSemaphoreGive(lock);
    if (!rc && remaining > 0) rc = ble_gap_adv_start(address_type, NULL, (int32_t)remaining, &params, gap_event, NULL);
    else if (!rc) rc = BLE_HS_ETIMEOUT;
    xSemaphoreTake(lock, portMAX_DELAY);
    state.advertising = rc == 0; state.error = rc;
    xSemaphoreGive(lock);
    if (rc) atomic_store(&stop_requested, true);
}
static void reset_host(int reason)
{
    xSemaphoreTake(lock, portMAX_DELAY); state.error = reason; xSemaphoreGive(lock);
    atomic_store(&stop_requested, true);
}
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    xSemaphoreTake(lock, portMAX_DELAY);
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        state.advertising = false;
        if (event->connect.status == 0) {
            connection = event->connect.conn_handle; state.connected = true;
            activity_at = now_ms(); generation++;
        } else atomic_store(&stop_requested, true);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        state.connected = state.authenticated = state.active = false;
        connection = BLE_HS_CONN_HANDLE_NONE; subscribed = false; generation++;
        atomic_store(&stop_requested, true);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        state.advertising = false;
        if (!state.connected) atomic_store(&stop_requested, true);
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == tx_handle) subscribed = event->subscribe.cur_notify;
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc;
        state.authenticated = event->enc_change.status == 0 &&
            ble_gap_conn_find(connection, &desc) == 0 && desc.sec_state.encrypted && desc.sec_state.authenticated;
        if (!state.authenticated) atomic_store(&stop_requested, true);
        break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io io = {.action = event->passkey.params.action, .passkey = state.passkey};
        if (io.action != BLE_SM_IOACT_DISP || ble_sm_inject_io(connection, &io))
            atomic_store(&stop_requested, true);
        break;
    }
    default: break;
    }
    xSemaphoreGive(lock);
    return 0;
}
static void host_task(void *arg)
{
    (void)arg; nimble_port_run(); nimble_port_freertos_deinit();
}
static void stop_stack(void)
{
    if (!initialized) return;
    xSemaphoreTake(api_gate, portMAX_DELAY);
    ble_gap_adv_stop();
    xSemaphoreTake(lock, portMAX_DELAY);
    uint16_t conn = connection;
    state.active = state.authenticated = false;
    subscribed = false;
    xSemaphoreGive(lock);
    if (conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    if (nimble_port_stop() != 0) {
        xSemaphoreGive(api_gate);
        return; /* Keep ownership and retry; never sleep with live stack. */
    }
    nimble_port_deinit(); initialized = false;
    xSemaphoreTake(lock, portMAX_DELAY);
    memset(&state, 0, sizeof(state)); connection = BLE_HS_CONN_HANDLE_NONE;
    subscribed = false; generation++;
    xQueueReset(incoming);
    xSemaphoreGive(lock);
    xSemaphoreGive(api_gate);
    ble_radio_owner_release(BLE_RADIO_OWNER_PHONE);
    atomic_store(&stop_requested, false);
}
static void service_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (atomic_exchange(&stop_requested, false)) {
            stop_stack();
            if (initialized) atomic_store(&stop_requested, true);
        }
        if (atomic_exchange(&start_requested, false) && !initialized) {
            if (!ble_radio_owner_claim(BLE_RADIO_OWNER_PHONE)) {
                xSemaphoreTake(lock, portMAX_DELAY); state.error = ESP_ERR_INVALID_STATE; xSemaphoreGive(lock);
            } else {
                int rc = nimble_port_init();
                if (!rc) {
                    initialized = true;
                    ble_hs_cfg.sync_cb = synchronized; ble_hs_cfg.reset_cb = reset_host;
                    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
                    ble_hs_cfg.sm_bonding = 0; ble_hs_cfg.sm_mitm = 1; ble_hs_cfg.sm_sc = 1;
                    ble_hs_cfg.sm_our_key_dist = 0; ble_hs_cfg.sm_their_key_dist = 0;
                    ble_store_config_init();
                    ble_svc_gap_init(); ble_svc_gatt_init();
                    ble_svc_gap_device_name_set("WristFoundry");
                    rc = ble_gatts_count_cfg(services);
                    if (!rc) rc = ble_gatts_add_svcs(services);
                    xSemaphoreTake(lock, portMAX_DELAY);
                    state.busy = true; state.passkey = esp_random() % 1000000;
                    discovery_deadline = now_ms() + WF_PHONE_DISCOVERY_MS;
                    xSemaphoreGive(lock);
                    nimble_port_freertos_init(host_task);
                    if (rc) atomic_store(&stop_requested, true);
                } else ble_radio_owner_release(BLE_RADIO_OWNER_PHONE);
                xSemaphoreTake(lock, portMAX_DELAY); state.error = rc; xSemaphoreGive(lock);
            }
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        bool expired = initialized && ((state.connected && wf_phone_expired(now_ms(), activity_at, state.active)) ||
            (!state.connected && now_ms() >= discovery_deadline));
        xSemaphoreGive(lock);
        if (expired) atomic_store(&stop_requested, true);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
esp_err_t phone_service_init(void)
{
    lock = xSemaphoreCreateMutex(); api_gate = xSemaphoreCreateMutex();
    incoming = xQueueCreate(16, sizeof(chunk_t));
    if (!lock || !api_gate || !incoming) return ESP_ERR_NO_MEM;
    return xTaskCreate(service_task, "phone", 6144, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
bool phone_service_discover(void)
{
    if (!lock || phone_service_blocks_sleep()) return false;
    atomic_store(&start_requested, true); return true;
}
void phone_service_stop(void) { atomic_store(&start_requested, false); atomic_store(&stop_requested, true); }
void phone_service_get(wf_phone_snapshot_t *out)
{
    memset(out, 0, sizeof(*out)); if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY); *out = state;
    int64_t remaining = state.connected ? activity_at + (state.active ? WF_PHONE_IDLE_MS : WF_PHONE_HANDSHAKE_MS) - now_ms() : discovery_deadline - now_ms();
    out->seconds_remaining = state.busy && remaining > 0 ? (uint32_t)((remaining + 999) / 1000) : 0;
    xSemaphoreGive(lock);
}
bool phone_service_blocks_sleep(void)
{
    return ble_radio_owner_current() == BLE_RADIO_OWNER_PHONE || atomic_load(&start_requested);
}
bool phone_service_active(void) { wf_phone_snapshot_t s; phone_service_get(&s); return s.active; }
void phone_service_activity(bool handshake)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (state.authenticated && subscribed) {
        if (handshake && !state.active) { state.active = true; activity_at = now_ms(); }
        if (!handshake && state.active) activity_at = now_ms();
    }
    xSemaphoreGive(lock);
}
uint32_t phone_service_generation(void)
{
    if (!lock) return 0;
    xSemaphoreTake(lock, portMAX_DELAY); uint32_t g = generation; xSemaphoreGive(lock); return g;
}
int phone_service_read(uint8_t *data, size_t size)
{
    if (!incoming || size < 244) return 0;
    chunk_t chunk;
    if (xQueueReceive(incoming, &chunk, 0) != pdTRUE) return 0;
    memcpy(data, chunk.data, chunk.length); int n = chunk.length; memset(&chunk, 0, sizeof(chunk)); return n;
}
bool phone_service_send(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    while (size) {
        if (xSemaphoreTake(api_gate, pdMS_TO_TICKS(100)) != pdTRUE) return false;
        xSemaphoreTake(lock, portMAX_DELAY);
        uint16_t conn = connection;
        bool ok = state.authenticated && subscribed;
        xSemaphoreGive(lock);
        if (!ok) { xSemaphoreGive(api_gate); return false; }
        uint16_t mtu = ble_att_mtu(conn);
        if (mtu < 23) { xSemaphoreGive(api_gate); phone_service_stop(); return false; }
        size_t n = size < (size_t)(mtu - 3) ? size : (size_t)(mtu - 3);
        struct os_mbuf *om = ble_hs_mbuf_from_flat(bytes, n);
        if (!om || ble_gatts_notify_custom(conn, tx_handle, om)) {
            xSemaphoreGive(api_gate); phone_service_stop(); return false;
        }
        xSemaphoreGive(api_gate);
        bytes += n; size -= n;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    return true;
}
