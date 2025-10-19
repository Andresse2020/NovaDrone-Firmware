#include "i_comm.h"
#include <string.h>
#include <stdio.h>
#include "i_led.h"
#include "i_time.h"

/* --------------------------------------------------------------------------
 * Service: envoi périodique et réception CAN
 * - Envoie un message toutes les 500 ms
 * - Lit les messages reçus sur IComm_Control
 * - Renvoie les messages reçus sur IComm_Release
 * -------------------------------------------------------------------------- */

void service_release_comm_test(void)
{
    static uint32_t last_tick = 0;
    static const char *periodic_msg = "Hello!!\r";
    uint8_t rx_data[64] = {0};
    char debug_buf[128];

    uint32_t now = ITime->getTick();

    /* ------------------ Envoi périodique ------------------ */
    if ((now - last_tick) >= 500)
    {
        last_tick = now;

        if (IComm_Release != NULL && IComm_Release->tx_ready())
        {
            if (IComm_Release->send(DISPLAY, (const uint8_t*)periodic_msg, strlen(periodic_msg)) == COMM_OK)
            {
                ILED->toggle(LED_STATUS); // Indique que le message a été envoyé
            }
        }
    }

    /* ------------------ Réception ------------------ */
    if (IComm_Release != NULL && IComm_Release->rx_available())
    {
        memset(rx_data, 0, sizeof(rx_data));

        if (IComm_Release->receive(rx_data, sizeof(rx_data)) == COMM_OK)
        {

            /* Formater le message pour Remote */
            int ln = snprintf(debug_buf, sizeof(debug_buf), "Received (%u): %s\r\n", (unsigned)strlen((char*)rx_data), (char*)rx_data);

            /* Envoi vers Remote si prêt */
            if (IComm_Release != NULL && IComm_Release->tx_ready())
            {
                IComm_Release->send(DISPLAY, (uint8_t*)debug_buf, ln);
            }
        }
    }
}
