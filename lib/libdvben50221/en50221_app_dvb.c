/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham (manu@kromtek.com)
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <string.h>
#include <dvbmisc.h>
#include "en50221_session.h"
#include "en50221_app_utils.h"
#include "en50221_app_dvb.h"
#include "asn_1.h"

// tags supported by this resource
#define TAG_TUNE            0x9f8400
#define TAG_REPLACE         0x9f8401
#define TAG_CLEAR_REPLACE       0x9f8402
#define TAG_ASK_RELEASE         0x9f8403

struct en50221_app_dvb_private {
        en50221_session_layer *sl;

        en50221_app_dvb_tune_callback tune_callback;
        void *tune_callback_arg;

        en50221_app_dvb_replace_callback replace_callback;
        void *replace_callback_arg;
};

static void en50221_app_dvb_resource_callback(void *arg,
                                             uint8_t slot_id,
                                             uint16_t session_number,
                                             uint32_t resource_id,
                                             uint8_t *data, uint32_t data_length);



en50221_app_dvb en50221_app_dvb_create(en50221_session_layer sl, en50221_app_rm rm)
{
    struct en50221_app_dvb_private *private = NULL;

    // create structure and set it up
    private = malloc(sizeof(struct en50221_app_dvb_private));
    if (private == NULL) {
        return NULL;
    }
    private->sl = sl;
    private->tune_callback = NULL;
    private->replace_callback = NULL;

    // register with the RM
    if (en50221_app_rm_register(rm, MKRID(32,1,1), en50221_app_dvb_resource_callback, private)) {
        free(private);
        return NULL;
    }

    // done
    return private;
}

void en50221_app_dvb_destroy(en50221_app_dvb dvb)
{
    struct en50221_app_dvb_private *private = (struct en50221_app_dvb_private *) dvb;

    free(private);
}

void en50221_app_dvb_register_tune_callback(en50221_app_dvb dvb,
                                            en50221_app_dvb_tune_callback callback, void *arg)
{
    struct en50221_app_dvb_private *private = (struct en50221_app_dvb_private *) dvb;

    private->tune_callback = callback;
    private->tune_callback_arg = arg;
}

void en50221_app_dvb_register_replace_callback(en50221_app_dvb dvb,
                                            en50221_app_dvb_replace_callback callback, void *arg)
{
    struct en50221_app_dvb_private *private = (struct en50221_app_dvb_private *) dvb;

    private->replace_callback = callback;
    private->replace_callback_arg = arg;
}

int en50221_app_dvb_ask_release(en50221_app_dvb dvb, uint16_t session_number)
{
    struct en50221_app_dvb_private *private = (struct en50221_app_dvb_private *) dvb;
    uint8_t data[3];

    data[0] = (TAG_ASK_RELEASE >> 16) & 0xFF;
    data[1] = (TAG_ASK_RELEASE >> 8) & 0xFF;
    data[2] = TAG_ASK_RELEASE & 0xFF;

    return en50221_sl_send_data(private->sl, session_number, data, 3);
}

static void en50221_app_dvb_parse_tune(struct en50221_app_dvb_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length < 9) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    if (data[0] != 8) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    uint8_t *tune_data = data+1;

    // parse it
    uint16_t network_id = (tune_data[0] << 8) | tune_data[1];
    uint16_t original_network_id = (tune_data[2] << 8) | tune_data[3];
    uint16_t transport_stream_id = (tune_data[4] << 8) | tune_data[5];
    uint16_t service_id = (tune_data[6] << 8) | tune_data[7];

    // tell the app
    if (private->tune_callback)
        private->tune_callback(private->tune_callback_arg, slot_id, session_number,
                               network_id, original_network_id, transport_stream_id, service_id);
}

static void en50221_app_dvb_parse_replace(struct en50221_app_dvb_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length < 6) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    if (data[0] != 5) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    uint8_t *replace_data = data+1;

    // parse it
    uint8_t replacement_ref = replace_data[0];
    uint16_t replace_pid = ((replace_data[1] & 0x1f)<<8) | replace_data[2];
    uint16_t replacement_pid = ((replace_data[3] & 0x1f)<<8) | replace_data[4];

    // tell the app
    if (private->replace_callback)
        private->replace_callback(private->replace_callback_arg, slot_id, session_number, 0,
                                  replacement_ref, replace_pid, replacement_pid);
}

static void en50221_app_dvb_parse_clear_replace(struct en50221_app_dvb_private *private,
                                                uint8_t slot_id, uint16_t session_number,
                                                uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length < 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    if (data[0] != 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    uint8_t *replace_data = data+1;

    // parse it
    uint8_t replacement_ref = replace_data[0];

    // tell the app
    if (private->replace_callback)
        private->replace_callback(private->replace_callback_arg, slot_id, session_number, 0,
                                  replacement_ref, 0, 0);
}

static void en50221_app_dvb_resource_callback(void *arg,
                                            uint8_t slot_id,
                                            uint16_t session_number,
                                            uint32_t resource_id,
                                            uint8_t *data, uint32_t data_length)
{
    struct en50221_app_dvb_private *private = (struct en50221_app_dvb_private *) arg;
    (void) resource_id;

    // get the tag
    if (data_length < 3) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

    switch(tag)
    {
        case TAG_TUNE:
            en50221_app_dvb_parse_tune(private, slot_id, session_number, data+3, data_length-3);
            break;
        case TAG_REPLACE:
            en50221_app_dvb_parse_replace(private, slot_id, session_number, data+3, data_length-3);
            break;
        case TAG_CLEAR_REPLACE:
            en50221_app_dvb_parse_clear_replace(private, slot_id, session_number, data+3, data_length-3);
            break;
        default:
            print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
            break;
    }
}