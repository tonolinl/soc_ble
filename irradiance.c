/*
 * irradiance.c
 *
 *  Created on: 2 juin 2025
 *      Author: lilou
 */

#include "app_log.h"
#include "sl_status.h"
#include "sl_sensor_light.h"   // pour accéder au capteur de lumière
#include "irradiance.h"

#define LUX_TO_IRRADIANCE_FACTOR 0.001472f

sl_status_t read_luminosite_ble_format(int16_t *lum_ble) {
    sl_status_t sc;
    float lux = 0.0f;
    float uvi = 0.0f;

    // Lire la luminosité et l'UV index
    sc = sl_sensor_light_get(&lux, &uvi);
    if (sc != SL_STATUS_OK) {
        app_log_error("Erreur de lecture du capteur de luminosité\n");
        return sc;
    }

    float irradiance = lux * LUX_TO_IRRADIANCE_FACTOR;
    int16_t irradiance_val = (int16_t)(irradiance * 10);  // Partie entière + décimale

    *lum_ble = irradiance_val;

    app_log_info("Irradiance : %d,%d W/m²\n",
                 irradiance_val / 10,
                 irradiance_val % 10);

    return SL_STATUS_OK;
}

