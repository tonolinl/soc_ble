/*
 * irradiance.h
 *
 *  Created on: 2 juin 2025
 *      Author: lilou
 */

#ifndef IRRADIANCE_H_
#define IRRADIANCE_H_

// Fonction pour lire la température du capteur et la convertir au format BLE.
sl_status_t read_luminosite_ble_format(int16_t *lum_ble);

#endif /* IRRADIANCE_H_ */
