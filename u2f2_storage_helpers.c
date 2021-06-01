/*
 *
 * Copyright 2019 The wookey project team <wookey@ssi.gouv.fr>
 *   - Ryad     Benadjila
 *   - Arnauld  Michelizza
 *   - Mathieu  Renard
 *   - Philippe Thierry
 *   - Philippe Trebuchet
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * the Free Software Foundation; either version 3 of the License, or (at
 * ur option) any later version.
 *
 * This package is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this package; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include "api/libu2f2.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "libc/malloc.h"

#include "u2f2_helpers.h"



/*
 * we have received a MAGIC_STORAGE_GET_METADATA command, with appid inside
 * return all the needed appid fields if appid found
 *
 * <------------ MAGIC_STORAGE_GET_METADATA
 * ------------> MAGIC_APPID_METADATA_STATUS (bool) (exists/doesnt exists)
 * if (exists):
 * ------------> MAGIC_APPID_METADATA_NAME (c[60])
 * ------------> MAGIC_APPID_METADATA_CTR   (u32)
 * ------------> MAGIC_APPID_METADATA_FLAGS (u32)
 * ------------> MAGIC_APPID_METADATA_ICON_TYPE (rgb|image|none)
 * if (rgb)
 * ------------> MAGIC_APPID_METADATA_COLOR (rgb: u8[3])
 * elif (icon)
 * ------------> MAGIC_APPID_METADATA_ICON_START (iconlen: u16)
 * ------------> MAGIC_APPID_METADATA_ICON (icon_trunk, upto 64)
 *  ...
 * ------------> MAGIC_APPID_METADATA_ICON (icon_trunk, upto 64)
 *
 * ------------> MAGIC_APPID_METADATA_END
 *
 */

/*
 * get back appid associated metadata. If the appid exists and has an icon, the appid_icon pointer is allocated
 * dynamically to the correct icon size (set in appid_info), otherwhise, it is set to NULL.
 */
mbed_error_t request_appid_metada(int msq, uint8_t *appid, fidostorage_appid_slot_t *appid_info, uint8_t    **appid_icon_p)
{
    log_printf("%s", __func__);
    mbed_error_t errcode = MBED_ERROR_NONE;
    if (appid == NULL || appid_info == NULL || appid_icon_p == NULL) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;
    }
    struct msgbuf msgbuf = { 0 };
    size_t msg_len = 0;
    ssize_t len;

    /* we know the appid, set the appid field localy */
    memcpy(appid_info->appid, appid, 32);
    /* sending get_metadata request */
    msgbuf.mtype = MAGIC_STORAGE_GET_METADATA;
    memcpy(&msgbuf.mtext.u8[0], appid, 32);
    msgsnd(msq, &msgbuf, 32, 0);
    /* read back appid status */
    msg_len = 1;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_STATUS, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata status, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    if (msgbuf.mtext.u8[0] != 0xff) {
        /* appid doesn't exists !*/
        log_printf("[u2f2] appid doesn't exist\n");
        errcode = MBED_ERROR_NOSTORAGE;
        goto end;
    }
    /* appid exists, get back metadata, starting with name */
    msg_len = 60;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_NAME, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata name, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    strncpy((char*)appid_info->name, &msgbuf.mtext.c[0], len);
    /* get back CTR */
    msg_len = 4;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_CTR, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata ctr, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    appid_info->ctr = msgbuf.mtext.u32[0];
    /* get back flags */
    msg_len = 4;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_FLAGS, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata flags, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    appid_info->flags = msgbuf.mtext.u32[0];
    /* get back icon_type */
    msg_len = 2;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_ICON_TYPE, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata icon_type, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    appid_info->icon_type = msgbuf.mtext.u16[0];
    /* depending on icon type, handling icon */
    uint16_t icon_len = 0;
    switch (appid_info->icon_type) {
        case ICON_TYPE_NONE:
            /* no icon */
            appid_icon_p = NULL;
            goto err;
            break;
        case ICON_TYPE_COLOR:
            /* icon is single RGB color */
            msg_len = 3;
            if (unlikely(msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_COLOR, 0) == -1)) {
                log_printf("[u2f2] failure while receiving metadata color, errno=%d\n", errno);
                errcode = MBED_ERROR_UNKNOWN;
                goto err;
            }
            memcpy(&appid_info->icon.rgb_color[0], &msgbuf.mtext.u8[0], 3);
            break;
        case ICON_TYPE_IMAGE:
            /* icon is RLE image */
            msg_len = 2;
            if (unlikely(msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_ICON_START, 0) == -1)) {
                log_printf("[u2f2] failure while receiving metadata icon start, errno=%d\n", errno);
                errcode = MBED_ERROR_UNKNOWN;
                goto err;
            }
            icon_len = msgbuf.mtext.u16[0];
            appid_info->icon_len = icon_len;
            /* now that we know the icon len, allocating it dynamically */
            if (wmalloc((void**)appid_icon_p, icon_len, ALLOC_NORMAL) != 0) {
                log_printf("[u2f2][warn] failure when allocating memory (%d bytes) for icon !!!\n");
                *appid_icon_p = NULL;
                /* we don't leave here as it would break the communication, instead, we set the
                 * icon to NULL and don't register locally the icon chunks.
                 * The task is responsible for checking the icon pointer and react */
            }
            /* how many requests to receive to fullfill icon ? */
            uint8_t *appid_icon = *appid_icon_p;
            uint16_t offset = 0;
            uint16_t len = 0;
            while (offset < icon_len) {

                msg_len = 64;
                if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_ICON, 0)) == -1)) {
                    log_printf("[u2f2] failure while receiving metadata icon, errno=%d\n", errno);
                    errcode = MBED_ERROR_UNKNOWN;
                    goto err;
                }
                if (offset + len > icon_len) {
                    log_printf("[u2f2] warn! the received icon is bigger than the declared size !\n");
                    errcode = MBED_ERROR_INVPARAM;
                    goto err;
                }
                /* we copy the icon chunk only if the icon allocation didn't fail */
                if (*appid_icon_p != NULL) {
                    memcpy(&appid_icon[offset], &msgbuf.mtext.u8[0], msg_len);
                }
                offset += msg_len;
            }
            break;
        default:
            errcode = MBED_ERROR_UNKNOWN;
            goto err;
            break;
    }
end:
    msg_len = 0;
    if (unlikely((len = msgrcv(msq, &msgbuf, msg_len, MAGIC_APPID_METADATA_END, 0)) == -1)) {
        log_printf("[u2f2] failure while receiving metadata end, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }

err:
    return errcode;
}

/*
 * here, MAGIC_STORAGE_GET_METADATA has just been received from msq and appid stored in argument. responding...
 */
mbed_error_t send_appid_metadata(int msq, uint8_t  *appid, fidostorage_appid_slot_t *appid_info, uint8_t    *appid_icon)
{
    log_printf("%s", __func__);
    mbed_error_t errcode = MBED_ERROR_NONE;
    if (appid == NULL) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;

    }
    struct msgbuf msgbuf = { 0 };
    size_t msg_len = 0;
    ssize_t len;

    msgbuf.mtype = MAGIC_APPID_METADATA_STATUS;
    /* send back appid status */
    if (appid_info == NULL) {
        /* if no appid_info previously populated, then we consider that the appid doesn't exist in the storage, sending 0 */
        log_printf("[u2f2] appid doesn't exist, sending 0x00\n");
        msgsnd(msq, &msgbuf, 1, 0);
        goto end;
    }
    /* or sending 'exists' status */
    msgbuf.mtext.u8[0] = 0xff;
    if (unlikely(msgsnd(msq, &msgbuf, 1, 0) == -1)) {
        log_printf("[u2f2] failure while sending metadata status, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }

    /* sending name */
    msgbuf.mtype = MAGIC_APPID_METADATA_NAME;
    msg_len = strlen((char*)appid_info->name);
    memcpy(&msgbuf.mtext.c[0], appid_info->name, msg_len);
    msgbuf.mtext.c[msg_len] = '\0';
    if (unlikely(msgsnd(msq, &msgbuf, msg_len + 1, 0) == -1)) {
        log_printf("[u2f2] failure while sending metadata name, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    /* sending CTR */
    msgbuf.mtype = MAGIC_APPID_METADATA_CTR;
    msgbuf.mtext.u32[0] = appid_info->ctr;
    if (unlikely(msgsnd(msq, &msgbuf, 4, 0) == -1)) {
        log_printf("[u2f2] failure while sending metadata CTR, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }
    /* sending flags */
    msgbuf.mtype = MAGIC_APPID_METADATA_FLAGS;
    msgbuf.mtext.u32[0] = appid_info->flags;
    if (unlikely(msgsnd(msq, &msgbuf, 4, 0) == -1)) {
        log_printf("[u2f2] failure while sending metadata flags, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }

    /* sending icon type */
    msgbuf.mtype = MAGIC_APPID_METADATA_ICON_TYPE;
    msgbuf.mtext.u16[0] = appid_info->icon_type;
    if (unlikely(msgsnd(msq, &msgbuf, 2, 0) == -1)) {
        log_printf("[u2f2] failure while sending metadata icon type, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }

    switch (appid_info->icon_type) {
        case ICON_TYPE_NONE:
            /* finished here */
            goto err;
            break;
        case ICON_TYPE_COLOR:
            msgbuf.mtype = MAGIC_APPID_METADATA_COLOR;
            memcpy(&msgbuf.mtext.u8[0], &appid_info->icon.rgb_color[0], 3);
            if (unlikely(msgsnd(msq, &msgbuf, 3, 0) == -1)) {
                log_printf("[u2f2] failure while sending metadata icon color, errno=%d\n", errno);
                errcode = MBED_ERROR_UNKNOWN;
                goto err;
            }
            break;
        case ICON_TYPE_IMAGE:
            if (appid_icon == NULL) {
                log_printf("[u2f2] an icon is to be sent, but icon arg is NULL!\n");
                errcode = MBED_ERROR_INVPARAM;
                goto err;
            }
            /* sending icon size first */
            msgbuf.mtype = MAGIC_APPID_METADATA_ICON_START;
            msgbuf.mtext.u16[0] = appid_info->icon_len;
            // XXX:
            if (unlikely(msgsnd(msq, &msgbuf, 2, 0) == -1)) {
                log_printf("[u2f2] failure while sending metadata icon start, errno=%d\n", errno);
                errcode = MBED_ERROR_UNKNOWN;
                goto err;
            }
            /* then icon data */
            uint16_t offset = 0;
            msgbuf.mtype = MAGIC_APPID_METADATA_ICON;
            while (offset < appid_info->icon_len) {
                size_t to_copy = ((appid_info->icon_len - offset) < 64) ? (appid_info->icon_len - offset): 64;
                memcpy(&msgbuf.mtext.u8[0], &appid_icon[offset], to_copy);
                if (unlikely(msgsnd(msq, &msgbuf, to_copy, 0) == -1)) {
                    log_printf("[u2f2] failure while sending metadata icon chunk, errno=%d\n", errno);
                    errcode = MBED_ERROR_UNKNOWN;
                    goto err;
                }
                offset += to_copy;
            }
            break;
        default:
            break;
    }
end:
    msg_len = 0;
    msgbuf.mtype = MAGIC_APPID_METADATA_END;
    if (unlikely((len = msgsnd(msq, &msgbuf, msg_len, 0)) == -1)) {
        log_printf("[u2f2] failure while sending metadata end, errno=%d\n", errno);
        errcode = MBED_ERROR_UNKNOWN;
        goto err;
    }

err:
    return errcode;
}


/*
 * we have received a MAGIC_STORAGE_SET_METADATA command, with appid inside
 * return all the needed appid fields if appid found
 *
 * <------------ MAGIC_STORAGE_SET_METADATA(mode) mode= fromscrath|templated
 * <------------ MAGIC_APPID_METADATA_IDENTIFIERS (appid,kh)
 * <------------ MAGIC_APPID_METADATA_NAME (c[60])
 * <------------ MAGIC_APPID_METADATA_CTR   (u32)
 * <------------ MAGIC_APPID_METADATA_FLAGS (u32)
 * <------------ MAGIC_APPID_METADATA_ICON_TYPE (rgb|image|none)
 * if (rgb)
 * <------------ MAGIC_APPID_METADATA_COLOR (rgb: u8[3])
 * elif (icon)
 * <------------ MAGIC_APPID_METADATA_ICON_START (iconlen: u16)
 * <------------ MAGIC_APPID_METADATA_ICON (icon_trunk, upto 64)
 *  ...
 * <------------ MAGIC_APPID_METADATA_ICON (icon_trunk, upto 64)
 *
 * <------------ MAGIC_APPID_METADATA_END
 *
 */


