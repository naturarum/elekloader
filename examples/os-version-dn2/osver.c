/* SPDX-License-Identifier: GPL-2.0-or-later */
/* os-version (Digitone II OS 1.12): tell Overbridge and Transfer the unit runs OS 1.12, while the power-on screen and
 * TEST MODE still show the build's own name. For dn2-cfw (github.com/naturarum/dn2-cfw, notes/OB01-overbridge.md).
 *
 * A custom build names itself in the container's 4-byte version field (header +0x14, "1.12" on stock). The bootstrap
 * draws that field on the power-on screen and in TEST MODE, and the main OS reports it to the computer: Overbridge
 * (2.19.4) then logs "Device OS Version: CMP1 : 62" and its plugin refuses the unit ("Device is not running
 * compatible OS Version for this version of Overbridge"). The main OS reads the header only through its cache getter
 * 0x40134850 (0x4463fc2c, or 0 if the cache does not hold an ELE3 header for the Digitone II), at seven sites. Two of
 * them send the version out; the other five use only the build id at +8, which a custom build keeps stock:
 *   0x400cf1c8  the init task: copies +8..+0x17 ("0062        1.12") into the reply of USB vendor request 0xC0/0x02
 *   0x40126270  MidiRpcSoftwareVersionRequest: replies with +8 (4 bytes) and +0x0C (12 bytes)
 * At those two jsr sites osv_hdr returns a copy of the header with "1.12" in the version field instead. The flash,
 * the header cache and the other five readers are untouched, so the bootstrap still draws the build's name.
 *
 * Returns an integer: the stock callers read d0, and a pointer-typed C function would also return in a0.
 */
typedef unsigned long u32;

#define HDR_GET ((u32 (*)(void))0x40134850)   /* the header cache getter: returns in d0 only */

static unsigned char osv_copy[0x20] __attribute__((aligned(4)));

u32 osv_hdr(void)
{
    const unsigned char *h = (const unsigned char *)HDR_GET();
    int i;

    if (!h)
        return 0;
    for (i = 0; i < 0x20; i++)
        osv_copy[i] = h[i];
    osv_copy[0x14] = '1';
    osv_copy[0x15] = '.';
    osv_copy[0x16] = '1';
    osv_copy[0x17] = '2';
    return (u32)osv_copy;
}
