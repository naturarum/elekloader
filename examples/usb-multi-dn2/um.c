/* SPDX-License-Identifier: GPL-2.0-or-later */
/* usb-multi (Digitone II OS 1.12): a fourth USB CONFIG choice, USB MULTI/MIDI, with which the Digitone sends all 42 of
 * its DSP's output channels to the computer as a class-compliant USB Audio 2.0 device, 24-bit at 48 kHz, the 16
 * tracks first as stereo pairs. USB AUDIO/MIDI stays Elektron's stereo mode. For dn2-cfw's UM builds
 * (github.com/naturarum/dn2-cfw, notes/USB01-class-compliant-multichannel.md).
 * 0.1 ran on the unit 31 Oct (the 42 channels on a Mac; the sequencer lagged: see um_frame). 0.2: um_frame (no lag on
 * the unit). 0.3: the 42 channels as their own USB CONFIG choice, in place of USB AUDIO/MIDI's stereo. 0.4: every
 * packet inside its slot of the ring, whatever state the stock's sender left and whatever its rate says (um_next,
 * um_rate, um_target, um_run).
 *
 * The DSP already sends 42 channels every 32-sample block, in normal running: channels 0-13 in the SSI0 TDM words
 * (0x4e6e7100, two halves of 32 frames x 16 slots x 4 bytes, the 24-bit sample in the low three bytes), 14-41 in its
 * SPI reply (0x800053c0, 32 rows of 84 bytes, two 24-bit big-endian samples in each 6 bytes). Its output table
 * (DSP 0x25d760) puts the main mix on 0-1, track n's L and R on 2n and 2n + 1, the delay, reverb and chorus returns
 * on 34-39 and the inputs on 40-41. Stock USB AUDIO mode sends two of them (USB OUT's choice): the block handler
 * 0x40025e36 builds a stereo buffer and calls the sender 0x40003d00, which packs 5 to 7 frames a packet (the rate
 * re-estimated every 1,024 packets by 0x400036fc) into a ring of 16 x 56 bytes at 0x80009000, each packet a dTD
 * (0x400036c0) primed on endpoint index 7, EP 0x83 IN (0x40005a86). At the start of a stream, 0x40003b32 primes 11
 * silent packets of 6 stereo frames from the same ring.
 *
 * With USB MULTI/MIDI the stream carries all 42, in the order the owner asked for (channel one is track one):
 *     USB 1-32   tracks 1-16, L and R      (DSP 2-33)
 *     USB 33-34  the main mix              (DSP 0-1)
 *     USB 35-40  delay, reverb, chorus     (DSP 34-39)
 *     USB 41-42  the inputs                (DSP 40-41)
 * 3-byte little-endian subslots (the DSP's own 24 bits): a frame is 126 bytes, a packet at most 7 x 126 = 882,
 * one high-speed transaction. The ring is 15 packets of 882 bytes at 0x80009700-0x8000cb36: inside Overbridge's
 * packet ring (16 x 1,012 at 0x80008c00, mode 6 only), after the stock ring and the stock receive buffers
 * (0x80009380-0x80009700, audio from the computer, mode 5). The stock sender's state words are kept and used as it
 * uses them, so its rate estimate, restart and skip flags behave as before.
 *
 * The choice. USB CONFIG is the byte 0x405ce898 of the saved settings: 0 USB MIDI, 1 OVERBRIDGE, 2 USB AUDIO/MIDI;
 * USB MULTI/MIDI stores 3. The stock reads it through 0x400bb798, which turns anything above 2 into 2, so to every
 * stock reader 3 is USB AUDIO/MIDI, USB mode 5, and the stock code runs mode 5 as it always does; only this mod
 * tells the two apart. The start-up code 0x400cf216 reads the byte itself (1 -> mode 6, 2 -> mode 5, anything else
 * -> mode 2, USB MIDI): one branch makes it 2 or more -> mode 5. Elektron's OS reading a 3 starts in USB MIDI, and
 * USB CONFIG picked again sets it right. The descriptors of mode 5 live in the main OS's data, in RAM, and the stock
 * writes their run-time values once, at the USB start-up (0x4000677a, from 0x40006c52); um_on says which set they
 * hold, the stereo one or ours, and the stream follows it. A switch between USB AUDIO/MIDI and USB MULTI/MIDI keeps
 * the mode (5), so the stock 0x40006c16 would not restart USB: um_mode_set does, with the stock's own stop and start,
 * the new descriptors written in between, and the computer sees the device leave and come back.
 *
 * Sites (made and checked by dn2-cfw's scripts/um03_build.py):
 *  - 0x40027772 jsr 0x40003d00 (the block's stereo frames) -> um_send; 0x400279d4 (the silent path) -> um_mute;
 *  - 0x4000655e jsr 0x40003b32 (the stream's start) -> um_start; with um_on off, each calls the stock one;
 *  - 0x400040e6: move.l #0x40380000,d0, the dQH word of EP 0x83 IN (one transaction, 56 bytes) -> jsr um_dqh (that,
 *    or 0x43720000, 882 bytes, with um_on);
 *  - 0x40006880, the last write, cache push and rts of 0x4000677a -> jmp um_desc. 0x4000677a writes into the
 *    descriptors (each configuration's wTotalLength, string lengths), pushing each byte out of the data cache for
 *    the controller's DMA. um_desc does the stock's last write, then the descriptors for the choice: input terminal
 *    5 and AS interface 2 alt 1 carry 42 channels (bmChannelConfig stays 0: no spatial positions), the format 3-byte
 *    subslots of 24 bits, EP 0x83's wMaxPacketSize 882, or the stock's 2 channels, 4 bytes of 32 bits, 56. The stored
 *    image keeps Elektron's descriptor bytes: they lie in the tail after the panel MCU firmware, which the brick
 *    checker keeps byte for byte;
 *  - 0x400cf22c, the start-up's bne (the byte is not 2: USB MIDI) -> bgt (it is below 2);
 *  - the USB CFG menu (0x40104f42 builds it, three items, each four std::function: the name, the choice on YES, its
 *    check box, the choice on a turn): 0x40105220, after the third item -> jsr um_menu_hook, which adds the fourth
 *    (um_menu_add, the callbacks in um_stubs.s, shaped as the stock's); 0x40104d54, USB AUDIO/MIDI's check box, jsr
 *    0x400bb798 -> um_cfg_get (0-3, so a 3 does not tick it); 0x40104e02, USB AUDIO/MIDI's choice, jmp 0x40006c16
 *    -> um_mode_set.
 * USB CONFIG's other choices (OVERBRIDGE, USB MIDI) and their descriptors are untouched.
 */
typedef unsigned long u32;
typedef unsigned char u8;

#ifndef UM_HOST                   /* the stock OS's addresses (a host test supplies its own: dn2-cfw scripts/um01/test_um.c) */
#define DTD_GET  ((u32 (*)(void))0x400036c0)              /* the next free IN dTD (in d0) */
#define EP_PRIME ((int (*)(u32, u32))0x40005a86)          /* (endpoint index, dTD chain) */
#define RATE_EST ((u32 (*)(u32, u32 *))0x400036fc)        /* (last estimate, &frames per packet 16.16) */
#define ZERO     ((void (*)(void *, u32))0x4013419c)      /* memset 0 */
#define START_CB 0x400039feu                              /* the first priming dTD's completion handler */
#define START_2  ((void (*)(void))0x40003a92)             /* the rest of the stream's start */

#define W(a) (*(volatile u32 *)(a))
#define ON      W(0x4058eee8)      /* the host has the IN alternate setting on */
#define RESTART W(0x4058eed0)
#define SKIP    W(0x4058eec4)      /* frames to drop (sync) */
#define UNMUTE  W(0x4058eee4)
#define FILL    W(0x4058ee84)      /* frames in the packet being filled */
#define CUR     W(0x4058ee78)      /* that packet */
#define IDX     W(0x4058eea0)      /* the ring's next packet */
#define CNT     W(0x4058ee7c)      /* packets to the next rate estimate */
#define FRAC    W(0x4058ee80)
#define TARGET  W(0x40287a8c)      /* frames in this packet, 5 to 7 */
#define EST     W(0x40287a88)
#define RATE    (*(u32 *)0x40287a84)
#define FLAG_A  W(0x4058eedc)
#define FLAG_B  W(0x4058eed4)
#define START_P W(0x4058eebc)
#define TXPOS   W(0xfc045640)      /* the SSI DMA's place, as the block handler reads it */
#define TXHALF  0x4e6e8900u

#define SSI0  0x4e6e7100u
#define SPI0  0x800053c0u
#define RING  0x80009700u

#define STOCK_SEND  ((void (*)(const void *, int))0x40003d00)  /* the stereo sender (frames, count) */
#define STOCK_START ((void (*)(u32))0x40003b32)              /* the stereo stream's start */
#define CFG     (*(volatile signed char *)0x405ce898)       /* USB CONFIG as saved: 0 MIDI, 1 OVERBRIDGE, 2 AUDIO; 3 ours */
#define CFG_SET ((void (*)(volatile void *, const void *, u32))0x400bb65c)  /* (setting, bytes, n): set, mark to save */
#define MODE     W(0x4058f2a8)     /* the USB mode running: 2 USB MIDI, 5 USB AUDIO/MIDI, 6 Overbridge */
#define MODE_SET ((void (*)(u32))0x40006c16)                 /* a new mode: stop, the mode and its table, start */
#define USB_STOP  ((void (*)(void))0x40006bac)
#define USB_START ((void (*)(void))0x40006bc6)               /* the start, after the stop's 30 ticks */
#define NEW     ((void *(*)(u32))0x40120544)                 /* operator new */
#define ITEM    ((void (*)(void *, void *, void *, void *, void *, int, int))0x401173b8)  /* a menu item's constructor */
#define FN_DEL  ((void (*)(void *))0x40188366)               /* std::function's destructor */
#define MGR_NAME 0x40104b22u       /* USB AUDIO/MIDI's four std::function managers: a lambda without state, and three */
#define MGR_YES  0x40104b74u       /* that keep the menu's pointer (the same as ours) */
#define MGR_BOX  0x40104bd0u
#define MGR_TURN 0x40104c2cu

/* a byte of the descriptors, out of the data cache as 0x4000677a does (the controller reads them by DMA) */
static void um_poke(u32 a, u8 v)
{
    *(volatile u8 *)a = v;
    __asm__ volatile ("cpushl %%dc,(%0)" : : "a"(a) : "memory");
}
#define POKE um_poke
#endif
#define NPKT  15u
#define FRAME 126u
#define PKT   882u                 /* 7 frames */

/* One frame, 42 channels as 3-byte little-endian samples (126 bytes): USB 1-12 from SSI slots 2-13, 13-32 from SPI
   samples 0-19, 33-34 from SSI slots 0-1, 35-42 from SPI samples 20-27. The main OS's start-up (0x40000554) sets
   CACR 0xa50ce100 (data cache on, by default cache-inhibited) and ACR0 0x4007e020 (0x40000000-0x47ffffff copyback),
   so the SSI DMA buffers at 0x4e6e7100 are uncached and every read of them goes to memory: the SSI words are read
   once each, as 32-bit words (the sample in their low three bytes), and the order is code, not a table; the SPI row
   (two big-endian samples in each 6 bytes) and the ring are in SRAM. 0.1 read every SSI byte (42 uncached reads a
   frame, 1,344 a block) and the sequencer lagged on the unit (31 Oct). */
static inline void um_put(volatile u8 *d, u32 w)        /* byte stores: never merged into a word at an odd address */
{
    d[0] = (u8)w;
    d[1] = (u8)(w >> 8);
    d[2] = (u8)(w >> 16);
}

static inline void um_swap(volatile u8 *d, const u8 *s, int n)
{
    while (n-- > 0) {
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];
        d += 3;
        s += 3;
    }
}

#ifdef UM_HOST                              /* a host test: the unit's big-endian 32-bit read, from bytes */
#define SSI_WORD(s, i) ((u32)(s)[4 * (i) + 1] << 16 | (u32)(s)[4 * (i) + 2] << 8 | (u32)(s)[4 * (i) + 3])
#else
#define SSI_WORD(s, i) ((u32)((const volatile unsigned int *)(s))[i])
#endif

static inline void um_frame(volatile u8 *d, const u8 *ssi, const u8 *spi)
{
    int c;
    for (c = 0; c < 12; c++)                /* tracks 1-6: SSI slots 2-13 */
        um_put(d + 3 * c, SSI_WORD(ssi, 2 + c));
    um_swap(d + 36, spi, 20);               /* tracks 7-16: SPI samples 0-19 */
    um_put(d + 96, SSI_WORD(ssi, 0));       /* the main mix: SSI slots 0-1 */
    um_put(d + 99, SSI_WORD(ssi, 1));
    um_swap(d + 102, spi + 60, 8);          /* delay, reverb, chorus, the inputs: SPI samples 20-27 */
}

/* The next packet's slot. IDX is the stock sender's, which counts 16 slots: after its stream it can be 15, one past
   our ring (0.3 then wrote its first packet over 0x8000caae-0x8000ce20) */
static u32 um_next(void)
{
    u32 i = IDX < NPKT ? IDX : 0;
    IDX = i + 1 < NPKT ? i + 1 : 0;
    return RING + i * PKT;
}

/* Frames a packet, 16.16, from the stock's estimate (0x40003732, on the USB interrupt's start-of-frame list: timer
   ticks over USB microframes, the microframes counted in 13 bits, so a pause between two measurements longer than
   8,192 of them counts short and the estimate comes out too high; the first after a reset is taken unfiltered). Real
   rates are 6 frames a packet within a few hundred ppm (48 kHz over 8,000 microframes a second): outside 5.5 to 6.5
   the estimate is wrong and 6 goes instead, so the packets keep pace with the microframes */
static u32 um_rate(void)
{
    u32 r = RATE;
    return r < 0x58000u || r > 0x68000u ? 0x60000u : r;
}

/* Frames in a packet: the stock's rate gives 5 to 7, and nothing in it stops a wrong estimate from giving more, which
   its 56-byte slots barely feel but ours would: a packet of 7 x 126 = 882 bytes fills a slot and is wMaxPacketSize */
static u32 um_target(u32 t)
{
    return t < 5 ? 5 : t > PKT / FRAME ? PKT / FRAME : t;
}

/* CUR is one of our slots (the stock sender's packets lie in its own ring, 0x80009000-0x80009380) */
static int um_slot(u32 p)
{
    return p >= RING && p < RING + NPKT * PKT && (p - RING) % PKT == 0;
}

static void um_run(int n, int silent)
{
    u32 done = 0, k, j;
    const u8 *ssi, *spi;
    u8 *d;
    if (!ON)
        return;
    if (RESTART) {
        RESTART = 0;
        FILL = 0;
        TARGET = 6;
        FRAC = 0;
        EST = 0xffffffff;
        CNT = 0;
    }
    TARGET = um_target(TARGET);
    if (FILL >= TARGET || !um_slot(CUR))    /* a packet the stock began, or one past its size: a new one */
        FILL = 0;
    if ((int)SKIP > 0) {
        k = (int)SKIP < n ? SKIP : (u32)n;
        SKIP = SKIP - k;
        n -= k;
        done = k;
    }
    ssi = (const u8 *)(SSI0 + (TXPOS < TXHALF ? 0x800u : 0u));
    spi = (const u8 *)SPI0;
    while (n > 0) {
        k = TARGET - FILL;
        if ((int)k > n)
            k = n;
        if (FILL == 0)
            CUR = um_next();
        d = (u8 *)(CUR + FILL * FRAME);
        if (silent || !UNMUTE)
            ZERO(d, k * FRAME);
        else
            for (j = 0; j < k; j++, d += FRAME)
                um_frame(d, ssi + (done + j) * 64, spi + (done + j) * 84);
        done += k;
        n -= k;
        FILL = FILL + k;
        if (FILL == TARGET) {
            volatile u32 *t = (volatile u32 *)DTD_GET();
            u32 bytes = FILL * FRAME;
            t[2] = CUR;
            t[3] = (CUR + 0x1000) & ~(u32)0xfff;
            t[1] = bytes << 16 | 0x80;
            t[8] = bytes;
            t[9] = 0;
            EP_PRIME(7, (u32)t);
            FILL = 0;
            CNT = CNT - 1;
            if ((int)CNT < 1) {
                EST = RATE_EST(EST, (u32 *)&RATE);
                CNT = 0x400;
            }
            FRAC = FRAC + um_rate();
            TARGET = um_target(FRAC >> 16);
            FRAC = FRAC & 0xffff;
        }
    }
}

/* the descriptors and the stream are ours (USB MULTI/MIDI) or the stock's stereo ones (USB AUDIO/MIDI); um_dqh
   (um_stubs.s) reads it too */
u32 um_on;

/* in place of 0x40003d00: the block's frames (the stereo ones are not used with um_on) */
void um_send(const void *stereo, int n)
{
    if (!um_on) {
        STOCK_SEND(stereo, n);
        return;
    }
    um_run(n, 0);
}

void um_mute(const void *stereo, int n)
{
    if (!um_on) {
        STOCK_SEND(stereo, n);
        return;
    }
    um_run(n, 1);
}

/* in place of 0x40003b32: the stream's start, 11 silent packets of 6 frames, as the stock does with its own sizes */
void um_start(u32 p)
{
    int i;
    if (!um_on) {
        STOCK_START(p);
        return;
    }
    FLAG_A = 1;
    FLAG_B = 1;
    RESTART = 1;
    for (i = 0; i < 11; i++) {
        volatile u32 *t = (volatile u32 *)DTD_GET();
        u32 b = um_next();
        ZERO((void *)b, 6 * FRAME);
        t[2] = b;
        t[3] = (b + 0x1000) & ~(u32)0xfff;
        t[1] = (6 * FRAME) << 16 | 0x80;
        t[0] = 0xdead0001;
        t[8] = 6 * FRAME;
        t[9] = i == 0 ? START_CB : 0;
        EP_PRIME(7, (u32)t);
    }
    START_P = p;
    START_2();
}

/* mode 5's descriptors for the choice: ours (42 channels) or the stock's (2), and um_on with them */
static void um_apply(u32 on)
{
    um_on = on;
    POKE(0x402fc407u, on ? 42 : 2);            /* input terminal 5 (the Digitone's audio): bNrChannels */
    POKE(0x402fc494u, on ? 42 : 2);            /* AS interface 2 alt 1, AS_GENERAL: bNrChannels */
    POKE(0x402fc49eu, on ? 3 : 4);             /* FORMAT_TYPE I: bSubslotSize */
    POKE(0x402fc49fu, on ? 24 : 32);           /* bBitResolution */
    POKE(0x402fc4a4u, on ? PKT & 0xff : 56);   /* EP 0x83 IN: wMaxPacketSize, little-endian */
    POKE(0x402fc4a5u, on ? PKT >> 8 : 0);
}

/* in place of 0x4000677a's last instructions (0x40006880: move.b #0x22,(a0); cpushl; rts), reached by jmp with the
   caller's return address on the stack, once at the USB start-up: that write, then the descriptors for the choice */
void um_desc(void)
{
    POKE(0x402fc000u, 0x22);       /* the stock's own last write */
    um_apply(CFG == 3);
}

/* USB CONFIG as saved, 0-3 (the stock's 0x400bb798 gives 0-2): USB AUDIO/MIDI's check box, through 0x40104d54, and
   ours */
u32 um_cfg_get(void)
{
    int v = CFG;
    return v < 0 ? 0 : v > 3 ? 3 : (u32)v;
}

/* in place of the stock's 0x40006c16(mode) in USB AUDIO/MIDI's choice (0x40104e02, a jmp: the mode on the stack as
   its argument), and from USB MULTI/MIDI's: the descriptors for the choice, and USB restarted when the mode stays 5 */
void um_mode_set(u32 mode)
{
    u32 on = CFG == 3;
    if (mode == 5 && on != um_on) {
        if (MODE == 5) {           /* 0x40006c16 without its "same mode" return */
            USB_STOP();
            um_apply(on);
            USB_START();
            return;
        }
        um_apply(on);              /* not served in another mode */
    }
    MODE_SET(mode);
}

/* USB MULTI/MIDI chosen (YES, or a turn, on the fourth item): saved as 3, as the stock's 0x400bb7ba saves 0-2 */
void um_choose(void)
{
    unsigned char v = 3;
    CFG_SET(&CFG, &v, 1);
    um_mode_set(5);
}

/* The fourth item of the USB CFG menu, after USB AUDIO/MIDI, made as 0x40104f42 makes each of the three: four
   std::function (16 bytes as this OS lays them out: the lambda's pointer, a word, the manager, the invoker), the name,
   the choice on YES, the check box, the choice on a turn; the item (0x54 bytes) built from them, then handed to the
   menu (its vtable's third entry) and the four destroyed. The managers are USB AUDIO/MIDI's; the invokers ours. */
typedef struct {
    void *p;
    u32 x;
    u32 mgr;
    void (*inv)(void);
} um_fn;

const char um_name[] = "USB MULTI/MIDI";
void um_inv_name(void), um_inv_yes(void), um_inv_box(void), um_inv_turn(void);   /* um_stubs.s */

static void um_fn_make(um_fn *f, u32 size, void *menu, u32 mgr, void (*inv)(void))
{
    void **p = (void **)NEW(size);
    if (size == 4)
        *p = menu;
    f->p = p;
    f->x = 0;
    f->mgr = mgr;
    f->inv = inv;
}

void um_menu_add(void *menu)
{
    um_fn f[4];
    void *item;
    um_fn_make(&f[0], 1, menu, MGR_NAME, um_inv_name);
    um_fn_make(&f[1], 4, menu, MGR_YES, um_inv_yes);
    um_fn_make(&f[2], 4, menu, MGR_BOX, um_inv_box);
    um_fn_make(&f[3], 4, menu, MGR_TURN, um_inv_turn);
    item = NEW(0x54);
    ITEM(item, &f[0], &f[1], &f[2], &f[3], -1, 8);
    ((void (*)(void *, void *))(*(u32 **)menu)[2])(menu, item);
    FN_DEL(&f[3]);
    FN_DEL(&f[2]);
    FN_DEL(&f[1]);
    FN_DEL(&f[0]);
}
