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
#include "en50221_app_rm.h"
#include "asn_1.h"

// tags supported by this resource
#define TAG_PROFILE_ENQUIRY             0x9f8010
#define TAG_PROFILE                     0x9f8011
#define TAG_PROFILE_CHANGE              0x9f8012


struct en50221_app_rm_private {
        struct en50221_app_send_functions *funcs;

        en50221_app_rm_enq_callback enqcallback;
        void *enqcallback_arg;

        en50221_app_rm_reply_callback replycallback;
        void *replycallback_arg;

        en50221_app_rm_changed_callback changedcallback;
        void *changedcallback_arg;
};

static void en50221_app_rm_parse_profile_enq(struct en50221_app_rm_private *private,
                                             uint8_t slot_id, uint16_t session_number,
                                             uint8_t *data, uint32_t data_length);
static void en50221_app_rm_parse_profile_reply(struct en50221_app_rm_private *private,
                                               uint8_t slot_id, uint16_t session_number,
                                               uint8_t *data, uint32_t data_length);
static void en50221_app_rm_parse_profile_change(struct en50221_app_rm_private *private,
        uint8_t slot_id, uint16_t session_number,
        uint8_t *data, uint32_t data_length);


en50221_app_rm en50221_app_rm_create(struct en50221_app_send_functions *funcs)
{
    struct en50221_app_rm_private *private = NULL;

    // create structure and set it up
    private = malloc(sizeof(struct en50221_app_rm_private));
    if (private == NULL) {
        return NULL;
    }
    private->funcs = funcs;
    private->enqcallback = NULL;
    private->replycallback = NULL;
    private->changedcallback = NULL;

    // done
    return private;
}

void en50221_app_rm_destroy(en50221_app_rm rm)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;

    // free structure
    free(private);
}

void en50221_app_rm_register_enq_callback(en50221_app_rm rm,
                                          en50221_app_rm_enq_callback callback, void *arg)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;

    private->enqcallback = callback;
    private->enqcallback_arg = arg;
}

void en50221_app_rm_register_reply_callback(en50221_app_rm rm,
                                           en50221_app_rm_reply_callback callback, void *arg)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;

    private->replycallback = callback;
    private->replycallback_arg = arg;
}

void en50221_app_rm_register_changed_callback(en50221_app_rm rm,
                                              en50221_app_rm_changed_callback callback, void *arg)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;

    private->changedcallback = callback;
    private->changedcallback_arg = arg;
}

int en50221_app_rm_enq(en50221_app_rm rm, uint16_t session_number)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;
    uint8_t buf[10];

    // set up the tag
    buf[0] = (TAG_PROFILE_ENQUIRY >> 16) & 0xFF;
    buf[1] = (TAG_PROFILE_ENQUIRY >> 8) & 0xFF;
    buf[2] = TAG_PROFILE_ENQUIRY & 0xFF;

    // create the data and send it
    return private->funcs->send_data(private->funcs->arg, session_number, buf, 3);
}

int en50221_app_rm_reply(en50221_app_rm rm, uint16_t session_number,
                         uint32_t resource_id_count,
                         uint32_t *resource_ids)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;
    uint8_t buf[10];

    // set up the tag
    buf[0] = (TAG_PROFILE >> 16) & 0xFF;
    buf[1] = (TAG_PROFILE >> 8) & 0xFF;
    buf[2] = TAG_PROFILE & 0xFF;

    // encode the length field
    int length_field_len;
    if ((length_field_len = asn_1_encode(resource_id_count*4, buf+3, 3)) < 0) {
        return -1;
    }

    // build the iovecs
    struct iovec iov[2];
    iov[0].iov_base = buf;
    iov[0].iov_len = 3+length_field_len;
    iov[1].iov_base = (uint8_t*) resource_ids;
    iov[1].iov_len = resource_id_count * 4;

    // create the data and send it
    return private->funcs->send_datav(private->funcs->arg, session_number, iov, 2);
}

int en50221_app_rm_changed(en50221_app_rm rm, uint16_t session_number)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;
    uint8_t buf[10];

    // set up the tag
    buf[0] = (TAG_PROFILE_CHANGE >> 16) & 0xFF;
    buf[1] = (TAG_PROFILE_CHANGE >> 8) & 0xFF;
    buf[2] = TAG_PROFILE_CHANGE & 0xFF;

    // create the data and send it
    return private->funcs->send_data(private->funcs->arg, session_number, buf, 3);
}

int en50221_app_rm_message(en50221_app_rm rm,
                           uint8_t slot_id,
                           uint16_t session_number,
                           uint32_t resource_id,
                           uint8_t *data, uint32_t data_length)
{
    struct en50221_app_rm_private *private = (struct en50221_app_rm_private *) rm;
    (void) resource_id;

    // get the tag
    if (data_length < 3) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

    // dispatch it
    switch(tag)
    {
        case TAG_PROFILE_ENQUIRY:
            en50221_app_rm_parse_profile_enq(private, slot_id, session_number, data+3, data_length-3);
            break;
        case TAG_PROFILE:
            en50221_app_rm_parse_profile_reply(private, slot_id, session_number, data+3, data_length-3);
            break;
        case TAG_PROFILE_CHANGE:
            en50221_app_rm_parse_profile_change(private, slot_id, session_number, data+3, data_length-3);
            break;
        default:
            print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
            return -1;
    }

    return 0;
}


static void en50221_app_rm_parse_profile_enq(struct en50221_app_rm_private *private,
                                             uint8_t slot_id, uint16_t session_number,
                                             uint8_t *data, uint32_t data_length)
{
    (void)data;
    (void)data_length;

    if (private->enqcallback)
        private->enqcallback(private->enqcallback_arg, slot_id, session_number);
}

static void en50221_app_rm_parse_profile_reply(struct en50221_app_rm_private *private,
                                               uint8_t slot_id, uint16_t session_number,
                                               uint8_t *data, uint32_t data_length)
{
    // first of all, decode the length field
    uint16_t asn_data_length;
    int length_field_len;
    if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
        print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
        return;
    }

    // check it
    if (asn_data_length > (data_length-length_field_len)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return;
    }
    uint32_t resources_count = asn_data_length / 4;

    // inform observer
    if (private->replycallback)
        private->replycallback(private->replycallback_arg,
                               slot_id, session_number, resources_count, (uint32_t*) (data+length_field_len));
}

static void en50221_app_rm_parse_profile_change(struct en50221_app_rm_private *private,
                                                uint8_t slot_id, uint16_t session_number,
                                                uint8_t *data, uint32_t data_length)
{
    (void)data;
    (void)data_length;

    if (private->changedcallback)
        private->changedcallback(private->changedcallback_arg, slot_id, session_number);
}
