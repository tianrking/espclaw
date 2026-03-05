/*
 * ESPClaw - tool/tool_network.c
 * Network tools: WiFi scan, network info, etc.
 * Included directly by tool_registry.c (not a standalone TU).
 */
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "util/json_util.h"
#include "net/wifi_manager.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations */
static bool tool_wifi_scan(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_get_network_info(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

/*
 * WiFi Scan Tool
 * Scans nearby APs and returns results as JSON-like string.
 * Note: This will temporarily disconnect from current AP (2-5 seconds).
 */
static bool tool_wifi_scan(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300,
            },
        },
    };

    ESP_LOGI(TAG, "Starting WiFi scan...");
    bool was_connected = wifi_mgr_is_connected();

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: WiFi scan failed: %s", esp_err_to_name(err));
        return false;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        snprintf(result_buf, result_sz, "No WiFi networks found.");
        return true;
    }

    if (ap_count > 20) ap_count = 20;

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        snprintf(result_buf, result_sz, "Error: Out of memory");
        return false;
    }

    err = esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    if (err != ESP_OK) {
        free(ap_list);
        snprintf(result_buf, result_sz, "Error: Failed to get scan results: %s", esp_err_to_name(err));
        return false;
    }

    int pos = 0;
    pos += snprintf(result_buf + pos, result_sz - pos, "Found %d WiFi networks:\n", ap_count);
    
    for (int i = 0; i < ap_count && pos < (int)result_sz - 100; i++) {
        wifi_ap_record_t *ap = &ap_list[i];
        
        const char *auth = "Open";
        switch (ap->authmode) {
            case WIFI_AUTH_WEP:        auth = "WEP"; break;
            case WIFI_AUTH_WPA_PSK:    auth = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK:   auth = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK:   auth = "WPA3"; break;
            default: break;
        }

        pos += snprintf(result_buf + pos, result_sz - pos,
            "%d. %s (Ch:%d, RSSI:%d, %s)\n",
            i + 1,
            ap->ssid[0] ? (char *)ap->ssid : "[Hidden]",
            ap->primary,
            ap->rssi,
            auth);
    }

    free(ap_list);

    if (was_connected) {
        pos += snprintf(result_buf + pos, result_sz - pos, 
            "\n(Reconnecting to previous network...)");
    }

    ESP_LOGI(TAG, "WiFi scan complete: %d APs found", ap_count);
    return true;
}

/*
 * Helper: format IP address to string
 */
static int fmt_ip(char *buf, size_t len, uint32_t ip)
{
    return snprintf(buf, len, "%d.%d.%d.%d",
        (int)((ip >> 0)  & 0xFF),
        (int)((ip >> 8)  & 0xFF),
        (int)((ip >> 16) & 0xFF),
        (int)((ip >> 24) & 0xFF));
}

/*
 * Get Network Info Tool
 * Returns current network status: IP, gateway, MAC, WiFi signal, etc.
 */
static bool tool_get_network_info(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    
    int pos = 0;
    
    /* Get WiFi connection info */
    wifi_ap_record_t ap_info;
    bool wifi_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
    
    if (!wifi_connected) {
        snprintf(result_buf, result_sz, "WiFi not connected.");
        return true;
    }
    
    /* Get network interface info */
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        netif = esp_netif_get_handle_from_ifkey("wlan0");
    }
    
    /* IP address info */
    esp_netif_ip_info_t ip_info;
    esp_netif_dns_info_t dns_info;
    
    bool has_ip = (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK);
    bool has_dns = (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK);
    
    /* Build result */
    pos += snprintf(result_buf + pos, result_sz - pos, "Network Status:\n");
    
    /* SSID */
    pos += snprintf(result_buf + pos, result_sz - pos, 
        "SSID: %s\n", ap_info.ssid[0] ? (char *)ap_info.ssid : "N/A");
    
    /* Signal strength */
    pos += snprintf(result_buf + pos, result_sz - pos, 
        "Signal: %d dBm (%s)\n", ap_info.rssi,
        ap_info.rssi > -50 ? "Excellent" :
        ap_info.rssi > -60 ? "Good" :
        ap_info.rssi > -70 ? "Fair" : "Weak");
    
    /* Channel */
    pos += snprintf(result_buf + pos, result_sz - pos, 
        "Channel: %d\n", ap_info.primary);
    
    /* BSSID (MAC of AP) */
    pos += snprintf(result_buf + pos, result_sz - pos, 
        "AP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
        ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
    
    /* IP Address */
    if (has_ip) {
        char ip_str[20], gw_str[20], mask_str[20];
        fmt_ip(ip_str, sizeof(ip_str), ip_info.ip.addr);
        fmt_ip(gw_str, sizeof(gw_str), ip_info.gw.addr);
        fmt_ip(mask_str, sizeof(mask_str), ip_info.netmask.addr);
        
        pos += snprintf(result_buf + pos, result_sz - pos, "IP: %s\n", ip_str);
        pos += snprintf(result_buf + pos, result_sz - pos, "Gateway: %s\n", gw_str);
        pos += snprintf(result_buf + pos, result_sz - pos, "Netmask: %s\n", mask_str);
    } else {
        pos += snprintf(result_buf + pos, result_sz - pos, "IP: Not assigned\n");
    }
    
    /* DNS - handle ESP-IDF 5.5 structure */
    if (has_dns) {
        char dns_str[20];
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        fmt_ip(dns_str, sizeof(dns_str), dns_info.ip.u_addr.ip4.addr);
#else
        fmt_ip(dns_str, sizeof(dns_str), dns_info.ip.addr);
#endif
        pos += snprintf(result_buf + pos, result_sz - pos, "DNS: %s\n", dns_str);
    }
    
    /* Local MAC address */
    uint8_t local_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, local_mac);
    pos += snprintf(result_buf + pos, result_sz - pos,
        "Local MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        local_mac[0], local_mac[1], local_mac[2],
        local_mac[3], local_mac[4], local_mac[5]);

    ESP_LOGI(TAG, "Network info retrieved");
    return true;
}
