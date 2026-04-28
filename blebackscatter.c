/**********************************************
 * BeagleBone PRU Backscatter
 * BLE advertisements backscatter
 * Authors : Ambuj Varshney < ambuj_varshney@it.uu.se >
 * (C) 2016 Uppsala Networked Objects (UNO)
 * Modified for Google FMDN advertisement
 ************************************************/

#include "blebackscatter.h"
#define BEAGLEBONE

// Control variable
static unsigned char count = 0;
// MAC address of the BLE backscatter device
static uint8_t arrMacAddress[BLE_MAC_ADDRESS];
// Payload buffer
static uint8_t payload[MAX_PAYLOAD];
// BLE packet instance
struct blePacket sblePacketInstance;

#ifdef BEAGLEBONE
#include "pru.h"
void *pru0DataMem;
void *pru1DataMem;
#define DELAY(ns) ((ns/5-6)/2)
unsigned long time, endtime;
#endif

// Forward declaration — defined in ble.c
uint8_t *get_fmdn_adv(const char *eid_hex);

// EID hex string — must match what's in ble.c
#define EID_HEX "5913715d278927cc947106e874066d926a283630"

/**** Convert byte to bit
 * Argument 1 : byte - Character to be converted to binary
 * Argument 2 : arr  - Binary representation MSB first
 **/
void bytetobit(uint8_t byt, uint8_t arr[8])
{
    int i;
    for (i = 0; i < 8; i++)
        arr[i] = (((byt << i) & 0x80)) >> 7;
}

int main(int argc, char **argv)
{
    unsigned int i, j;
    int prumemCnt = 0;
    uint8_t barr[8];

    time = 0;

#ifdef BEAGLEBONE
    if (geteuid()) {
        fprintf(stderr, "%s must be run as root to use prussdrv\n", argv[0]);
        return -1;
    }

    if (pru_setup()) {
        pru_cleanup();
        return -1;
    }

    uint8_t *pruMem8 = (uint8_t *) pru0DataMem;
    int rtn;
#endif

    // ── Build FMDN adv_data and copy into payload ─────────────────────────────
    uint8_t *result = get_fmdn_adv(EID_HEX);
    memcpy(payload, result, 31);

    // ── Static MAC address ────────────────────────────────────────────────────
    arrMacAddress[0] = 0x10;
    arrMacAddress[1] = 0x11;
    arrMacAddress[2] = 0x12;
    arrMacAddress[3] = 0x13;
    arrMacAddress[4] = 0x14;
    arrMacAddress[5] = 0x15;

    printf("MAC Address: ");
    for (int k = 0; k < BLE_MAC_ADDRESS; k++)
        printf("%02x ", arrMacAddress[k]);
    printf("\n");

    // ── Generate full BLE packet (PDU + CRC + whitening + bit swap) ───────────
    printf("Size of BLE Packet: %d\n", sizeof(sblePacketInstance));
    generate_ble_adv_payload(&sblePacketInstance, (char *)payload, (char *)arrMacAddress, 39);
    printf("Size of BLE Packet after function: %d\n", sizeof(sblePacketInstance));

#ifndef BEAGLEBONE
    printf("\nBLE payload is: ");
    printf("%x,", sblePacketInstance.u8preamble);
    printf("%x,", sblePacketInstance.access_address[0]);
    return 0;
#endif

#ifdef BEAGLEBONE
    // ── Push bits into PRU memory ─────────────────────────────────────────────
    bytetobit(sblePacketInstance.u8preamble, barr);
    POPULATE_PRU;

    bytetobit(sblePacketInstance.access_address[0], barr);
    POPULATE_PRU;
    bytetobit(sblePacketInstance.access_address[1], barr);
    POPULATE_PRU;
    bytetobit(sblePacketInstance.access_address[2], barr);
    POPULATE_PRU;
    bytetobit(sblePacketInstance.access_address[3], barr);
    POPULATE_PRU;

    for (j = 0; j < sblePacketInstance.u8PayloadLen; j++) {
        bytetobit(sblePacketInstance.blePDU[j], barr);
        POPULATE_PRU;
    }

    // Signal PRU that bits are done
    pruMem8[prumemCnt] = 0;

    printf("Executing PRU program\n");

    for (i = 0; i < 10000; i++) {
        if ((rtn = prussdrv_exec_program(PRU_NUM, "fsk.bin")) < 0) {
            fprintf(stderr, "prussdrv_exec_program() failed\n");
            return rtn;
        }
        printf("Loop %d\n", i);
        usleep(1000 * 20);
    }

    printf("waiting for interrupt from PRU0...\n");
    rtn = prussdrv_pru_wait_event(PRU_EVTOUT_0);
    printf("PRU program completed, event number %d\n", rtn);

    return pru_cleanup();
#endif
}