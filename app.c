/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "temperature.h"
#include "sl_sensor_rht.h"
#include "gatt_db.h"
#include "sl_bt_api.h"
#include "sl_sleeptimer.h"
#include "sl_simple_led_instances.h"
#include "irradiance.h"
#include "sl_sensor_light.h"

#define TEMPERATURE_TIMER_SIGNAL (1<<0)
#define LUMINOSITE_TIMER_SIGNAL (1<<1)

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// Déclaration de la variable qui stockera la température convertie.
static int16_t temperature_ble = 0;

//Déclaration des variables utiles au Timer
static sl_sleeptimer_timer_handle_t timer_handle_temperature;
static int timer_step_temp = 0;
static uint8_t connection = 0;

// Déclaration des variables relative au capteur de luminosité
static int16_t lum_ble = 0;
sl_sleeptimer_timer_handle_t timer_handle_luminosite;
static int timer_step_lum = 0;



/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  sl_sensor_rht_init(); //initialisation de la fonction de recupération de la temperature

  sl_simple_led_init_instances(); // Initialisation des LEDs

  sl_sensor_light_init(); //Initialisation du capteur de luminosité

  app_log_info("%s\n",__FUNCTION__);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  //////////////////////////////////////////////////////////////////////////////
}
//Fonction d'interrutpion de la température
void timer_callback_temperature(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;
    timer_step_temp++;
    app_log_info("Timer temperature step %d\n", timer_step_temp);

    sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);

}
//Fonction d'interrutpion de la luminosité
void timer_callback_luminosite(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;
    timer_step_lum++;
    app_log_info("Timer luminosite step %d\n", timer_step_lum);

    sl_bt_external_signal(LUMINOSITE_TIMER_SIGNAL);
}



/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: connection_opened\n", __FUNCTION__);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: connection_closed\n", __FUNCTION__);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      // Désactive le capteur de température lorsque la connexion est fermée
      sl_sensor_rht_deinit();
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    case sl_bt_evt_gatt_server_user_read_request_id:
      app_log_info("user_read_request_ok\n");

      /******************TEMPERATURE****************************/
      // Vérifie si la lecture concerne la caractéristique température
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature)
      {
          app_log_info("Lecture caracteristique : temperature id: %d\n",evt->data.evt_gatt_server_user_read_request.characteristic );
          read_temperature_ble_format(&temperature_ble); // Appel de la fonction pour lire la température et la convertir au format BLE.
          // Envoyer la réponse GATT
          sc = sl_bt_gatt_server_send_user_read_response(
               evt->data.evt_gatt_server_user_read_request.connection,  // Connexion active
               evt->data.evt_gatt_server_user_read_request.characteristic,   // ID caractéristique (température)
               0,                                                        // Pas d'erreur
               sizeof(temperature_ble),                                  // Taille de la donnée (2 octets)
               (const uint8_t *)&temperature_ble,                        // Pointeur vers la valeur
               NULL                                                      // Taille réellement envoyée
               );
          app_assert_status(sc);
          if (sc == SL_STATUS_OK) {
              app_log_info("Reponse GATT envoyee avec succes.\n");
          } else {
            app_log_info("Echec de l'envoi de la reponse GATT.\n");
          }
      }

      /******************LUMINOSITE****************************/
      // Vérifie si la lecture concerne la caractéristique luminosité
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_irradiance_0)
      {
          app_log_info("Lecture caracteristique : luminosite id: %d\n",evt->data.evt_gatt_server_user_read_request.characteristic );
          read_luminosite_ble_format(&lum_ble); // Appel de la fonction pour lire la température et la convertir au format BLE.
          // Envoyer la réponse GATT
          sc = sl_bt_gatt_server_send_user_read_response(
               evt->data.evt_gatt_server_user_read_request.connection,  // Connexion active
               evt->data.evt_gatt_server_user_read_request.characteristic,   // ID caractéristique (irradiance)
               0,                                                        // Pas d'erreur
               sizeof(lum_ble),                                  // Taille de la donnée
              (const uint8_t *)&lum_ble,                        // Pointeur vers la valeur
               NULL                                                      // Taille réellement envoyée
               );
          app_assert_status(sc);
          if (sc == SL_STATUS_OK) {
              app_log_info("Reponse GATT envoyee avec succes.\n");
          } else {
              app_log_info("Echec de l'envoi de la reponse GATT.\n");
          }
       }

      break;


    case sl_bt_evt_gatt_server_characteristic_status_id :
      app_log_info("gatt_server_characteristic_status_id ok\n");

      /******************TEMPERATURE****************************/
      // Vérifie si la lecture concerne la caractéristique température
      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_temperature)
      {
          app_log_info("Notification de la caracteristique : temperature id: %d\n",evt->data.evt_gatt_server_characteristic_status.characteristic);
          // Vérification si la notification a été activée ou désactivée
          if (evt->data.evt_gatt_server_characteristic_status.client_config_flags ==  sl_bt_gatt_server_notification && evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config ) {
              app_log_info("status_flags: 0x%02X & valeur de notify : 0x%02X\n", evt->data.evt_gatt_server_characteristic_status.status_flags, evt->data.evt_gatt_server_characteristic_status.client_config_flags);

              connection = evt->data.evt_gatt_server_characteristic_status.connection; //sauvegarde la connection en cours

              // Démarrage du timer à 1Hz (1000 ms)
                  sc = sl_sleeptimer_start_periodic_timer_ms(
                      &timer_handle_temperature,
                      1000,             // 1000 ms = 1 Hz
                      timer_callback_temperature,   // fonction appelée à chaque expiration
                      NULL,             // pas de données passées dans ce cas
                      0,                // priorité normale
                      0);               // options par défaut

                  app_log_info("Notify active pour la temperature, timer demarre\n");
          } else {
              app_log_info("status_flags: 0x%02X & valeur de notify : 0x%02X\n", evt->data.evt_gatt_server_characteristic_status.status_flags, evt->data.evt_gatt_server_characteristic_status.client_config_flags);
              sc = sl_sleeptimer_stop_timer(&timer_handle_temperature);
              app_log_info("Notification desactivee pour la temperature, timer arrete\n");
          }
      }

      /******************LUMINOSITE****************************/
      // Vérifie si la lecture concerne la caractéristique irradiance
      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_irradiance_0)
      {
          app_log_info("Notification de la caracteristique : luminosite id: %d\n", evt->data.evt_gatt_server_characteristic_status.characteristic);
          // Vérification si la notification a été activée ou désactivée
          if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == sl_bt_gatt_notification &&
              evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config)
          {
              connection = evt->data.evt_gatt_server_characteristic_status.connection;

              sc = sl_sleeptimer_start_periodic_timer_ms(
                  &timer_handle_luminosite,
                  1000,
                  timer_callback_luminosite,
                  NULL,
                  0,
                  0);

              app_log_info("Notify active pour la luminosite, timer demarre\n");
          }
          else
          {
              sc = sl_sleeptimer_stop_timer(&timer_handle_luminosite);
              app_log_info("Notification desactivee pour la luminosite, timer arrete\n");
          }
      }


      break;

    case sl_bt_evt_system_external_signal_id:

      /******************TEMPERATURE****************************/
        if (evt->data.evt_system_external_signal.extsignals == TEMPERATURE_TIMER_SIGNAL) {
            // Lire la température et la convertir au format BLE
            read_temperature_ble_format(&temperature_ble);

            // Envoyer la notification
            sc = sl_bt_gatt_server_send_notification(connection,
                                                     gattdb_temperature,
                                                     sizeof(temperature_ble),
                                                     (const uint8_t *)&temperature_ble);
            if (sc == SL_STATUS_OK) {
                app_log_info("Notification temperature envoyee.\n");
            } else {
                app_log_info("Erreur envoi notification\n");
            }
        }

        /******************LUMINOSITE****************************/
        if (evt->data.evt_system_external_signal.extsignals == LUMINOSITE_TIMER_SIGNAL) {
            // Lire la luminosité et la convertir au format BLE
            read_luminosite_ble_format(&lum_ble);

            sc = sl_bt_gatt_server_send_notification(connection,
                                                     gattdb_irradiance_0,
                                                     sizeof(lum_ble),
                                                     (const uint8_t *)&lum_ble);
            if (sc == SL_STATUS_OK) {
                app_log_info("Notification luminosite envoyee.\n");
            } else {
                app_log_info("Erreur envoi notification luminosite\n");
            }
        }

    break;


    case sl_bt_evt_gatt_server_user_write_request_id:
      app_log_info("user write request ok\n");
      // Vérifier qu’il s’agit bien de la caractéristique "Digital"
          if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_digital_0) {
              app_log_info("Write sur la caracteristique Digital\n");

              app_log_info("Opcode recu: 0x%02X\n", evt->data.evt_gatt_server_user_write_request.att_opcode);

              // Lire la valeur écrite
              uint8_t value = evt->data.evt_gatt_server_user_write_request.value.data[0];
              app_log_info("Valeur recue : %d\n", value);


              // Si la valeur est non nulle => Active => LEDs ON
                      if (value==1) {
                          for (uint8_t i = 0; i < SL_SIMPLE_LED_COUNT; i++) {
                              sl_led_turn_on(sl_simple_led_array[i]);
                          }
                          app_log_info("LEDs allumees\n");
                      } else if (value==0) {
                          for (uint8_t i = 0; i < SL_SIMPLE_LED_COUNT; i++) {
                              sl_led_turn_off(sl_simple_led_array[i]);
                          }
                          app_log_info("LEDs eteintes\n");
                      }
                      else {
                          app_log_info("Ecriture Inactive ou Active seulement accepte");
                      }
           // Write Request (0x12) = nécessite une réponse
           if (evt->data.evt_gatt_server_user_write_request.att_opcode == 0x12) {
           sl_bt_gatt_server_send_user_write_response(evt->data.evt_gatt_server_user_write_request.connection,
                                                      evt->data.evt_gatt_server_user_write_request.characteristic,
                                                      SL_STATUS_OK);
           }
      }
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

