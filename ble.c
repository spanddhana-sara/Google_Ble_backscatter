/**********************************************
 * BeagleBone PRU Backscatter
 * BLE advertisements backscatter
 * Modified for Google Find My Device Network (FMDN)
 * Authors : Ambuj Varshney < ambuj_varshney@it.uu.se >
 * (C) 2016 Uppsala Networked Objects (UNO)
 ************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "blebackscatter.h"

// ─── Your 20-byte EID (40 hex chars) ─────────────────────────────────────────
#define EID_HEX "5913715d278927cc947106e874066d926a283630"

// ─── FMDN raw ADV payload (31 bytes) ─────────────────────────────────────────
//
//  [0]     0x02    AD length
//  [1]     0x01    AD type: Flags
//  [2]     0x06    LE General Discoverable | BR/EDR Not Supported
//  [3]     0x19    AD length (25 bytes follow)
//  [4]     0x16    AD type: Service Data — 16-bit UUID
//  [5]     0xAA    } UUID 0xFEAA little-endian (Google FMDN service UUID)
//  [6]     0xFE    }
//  [7]     0x40    FMDN frame type:
//                    0x40 = normal
//                    0x41 = unwanted tracking protection enabled
//  [8–27]  EID     20-byte Ephemeral Identifier — filled by get_fmdn_adv()
//  [28]    0x00    Hashed flags (not implemented)
//  [29]    0x00    Padding
//  [30]    0x00    Padding
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t adv_data[31] = {
    // AD struct 1: Flags
    0x02,                   /* Length */
    0x01,                   /* AD type: Flags */
    0x06,                   /* LE General Discoverable | BR/EDR Not Supported */

    // AD struct 2: FMDN Service Data
    0x19,                   /* Length (25 bytes follow) */
    0x16,                   /* AD type: Service Data - 16-bit UUID */
    0xAA, 0xFE,             /* Google FMDN UUID 0xFEAA, little-endian */
    0x40,                   /* FMDN frame type */

    // [8..27] EID — 20 bytes, filled at runtime
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    // [28] Hashed flags
    0x00,

    // [29..30] Padding
    0x00, 0x00,
};

// ─── CRC computation ──────────────────────────────────────────────────────────
void btLeCrc(const uint8_t *data, uint8_t len, uint8_t *dst) {
    uint8_t v, t, d;
    while (len--) {
        d = *data++;
        for (v = 0; v < 8; v++, d >>= 1) {
            t = dst[0] >> 7;
            dst[0] <<= 1;
            if (dst[1] & 0x80) dst[0] |= 1;
            dst[1] <<= 1;
            if (dst[2] & 0x80) dst[1] |= 1;
            dst[2] <<= 1;
            if (t != (d & 1)) {
                dst[2] ^= 0x5B;
                dst[1] ^= 0x06;
            }
        }
    }
}

// ─── Bit swap ─────────────────────────────────────────────────────────────────
uint8_t swapbits(uint8_t a) {
    uint8_t v = 0;
    if (a & 0x80) v |= 0x01;
    if (a & 0x40) v |= 0x02;
    if (a & 0x20) v |= 0x04;
    if (a & 0x10) v |= 0x08;
    if (a & 0x08) v |= 0x10;
    if (a & 0x04) v |= 0x20;
    if (a & 0x02) v |= 0x40;
    if (a & 0x01) v |= 0x80;
    return v;
}

// ─── BLE whitening ────────────────────────────────────────────────────────────
void btLeWhiten(uint8_t *data, uint8_t len, uint8_t whitenCoeff) {
    uint8_t m;
    while (len--) {
        for (m = 1; m; m <<= 1) {
            if (whitenCoeff & 0x80) {
                whitenCoeff ^= 0x11;
                (*data) ^= m;
            }
            whitenCoeff <<= 1;
        }
        data++;
    }
}

// ─── BLE advertisement payload generator ─────────────────────────────────────
void generate_ble_adv_payload(struct blePacket *bPacket, char adv_data[], char mac_address[], uint8_t whiten_channel)
{
    unsigned char ctr = 0;
    uint8_t crc[3] = {0x55, 0x55, 0x55};
    unsigned char i;

    // Preamble
    bPacket->u8preamble = 0xAA;

    // BLE advertising access address (fixed for all BLE advertisements)
    bPacket->access_address[0] = swapbits(0xD6);
    bPacket->access_address[1] = swapbits(0xBE);
    bPacket->access_address[2] = swapbits(0x89);
    bPacket->access_address[3] = swapbits(0x8E);

    // PDU header
    bPacket->blePDU[ctr++] = 0x42;     // PDU type: ADV_NONCONN_IND
    bPacket->blePDU[ctr++] = 0x25;     // Length: 6 (MAC) + 31 (adv) = 37 = 0x25

    // MAC address
    for (i = 0; i < BLE_MAC_ADDRESS; i++)
        bPacket->blePDU[ctr++] = mac_address[i];

    // adv_data — exactly 31 bytes, copied by index (safe for 0x00 bytes)
    for (i = 0; i < 31; i++)
        bPacket->blePDU[ctr++] = adv_data[i];

    printf("PDU length without CRC: %d\n", ctr);

    // CRC
    btLeCrc(bPacket->blePDU, ctr, crc);
    for (i = 0; i < 3; i++)
        bPacket->blePDU[ctr++] = swapbits(crc[i]);

    printf("BLE PDU before whitening: ");
    for (int m = 0; m < ctr; m++)
        printf("%02x ", bPacket->blePDU[m]);
    printf("\n");
    printf("PDU length with CRC: %d\n", ctr);

    // Whitening
    btLeWhiten(bPacket->blePDU, ctr, swapbits(whiten_channel) | 2);

    // Bit swap entire PDU
    for (i = 0; i < ctr; i++)
        bPacket->blePDU[i] = swapbits(bPacket->blePDU[i]);

    bPacket->u8PayloadLen = ctr;

    printf("BLE PDU Length: %d\n", ctr);
    printf("\n--- BLE Packet ---\n");
    printf("Preamble: %02x\n", bPacket->u8preamble);
    printf("Access Address: ");
    for (int r = 0; r < 4; r++)
        printf("%02x ", bPacket->access_address[r]);
    printf("\n");
    printf("BLE PDU: ");
    for (int j = 0; j < bPacket->u8PayloadLen; j++)
        printf("%02x ", bPacket->blePDU[j]);
    printf("\n-------------------\n");
}

// ─── Inject EID into adv_data and return pointer ─────────────────────────────
uint8_t *get_fmdn_adv(const char *eid_hex) {
    for (int i = 0; i < 20; i++)
        sscanf(eid_hex + 2 * i, "%2hhx", &adv_data[8 + i]);

    printf("[FMDN] adv_data (31 bytes): ");
    for (int i = 0; i < 31; i++)
        printf("%02x ", adv_data[i]);
    printf("\n");

    return adv_data;
}