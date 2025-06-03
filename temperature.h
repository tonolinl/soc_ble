/*
 * temperature.h
 *
 *  Created on: 5 mai 2025
 *      Author: lilou
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

// Fonction pour lire la température du capteur et la convertir au format BLE.
sl_status_t read_temperature_ble_format(int16_t *temperature_ble);

#endif /* TEMPERATURE_H_ */
