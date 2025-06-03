/*
 * temperature.c
 *
 *  Created on: 5 mai 2025
 *      Author: lilou
 */

#include "app_log.h"
#include "sl_status.h"
#include "sl_sensor_rht.h"
#include "temperature.h"

sl_status_t read_temperature_ble_format(int16_t *temperature_ble) {
    sl_status_t sc;
    int32_t temperature;
    uint32_t humidity;

    // Lire la température et l'humidité avec la fonction existante.
    sc = sl_sensor_rht_get(&humidity, &temperature);
    if (sc != SL_STATUS_OK) {
        app_log_error("Erreur de lecture du capteur de température\n");
        return sc;
    }

    // Conversion en centi-degrés (pour BLE).
    *temperature_ble = (int16_t)(temperature / 10);  // temperature en centi-degré celsus

    // Log formaté : exemple => 25,00°C
    app_log_info("Temperature : %ld,%2ld degC\n",temperature / 1000,           // partie entière
                (temperature % 1000) / 10);   // deux chiffres après la virgule

    return SL_STATUS_OK;
}
