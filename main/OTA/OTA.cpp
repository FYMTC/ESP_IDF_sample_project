#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"


#if 0

#include "OTA.h"

#define OTA_BUFFER_SIZE 4096

void sdcard_ota_update() {
    // Mount SD card
    //mount_sdcard();

    // Open the firmware file on SD card
    FILE *f = fopen("/sdcard/firmware.bin", "rb");
    if (f == NULL) {
        ESP_LOGE("OTA", "Failed to open firmware file.");
        return;
    }

    // Get the OTA update partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE("OTA", "No OTA partition available.");
        fclose(f);
        return;
    }

    // Start OTA update
    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_begin failed: %s", esp_err_to_name(err));
        fclose(f);
        return;
    }

    // Read the firmware file and write to OTA partition
    uint8_t ota_buffer[OTA_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(ota_buffer, 1, OTA_BUFFER_SIZE, f)) > 0) {
        err = esp_ota_write(ota_handle, ota_buffer, bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE("OTA", "esp_ota_write failed: %s", esp_err_to_name(err));
            fclose(f);
            esp_ota_end(ota_handle);
            return;
        }
    }

    fclose(f);

    // Finalize OTA update
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_end failed: %s", esp_err_to_name(err));
        return;
    }

    // Set the new partition as boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI("OTA", "OTA update completed. Rebooting...");
    esp_restart();
}


#endif