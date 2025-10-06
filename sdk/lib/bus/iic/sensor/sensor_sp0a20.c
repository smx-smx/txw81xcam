#include "sys_config.h"
#include "typesdef.h"
#include "lib/video/dvp/cmos_sensor/csi.h"
#include "tx_platform.h"
#include "list.h"
#include "dev.h"
#include "hal/i2c.h"

#if DEV_SENSOR_SP0A20


SENSOR_INIT_SECTION static const unsigned char SP0A20InitTable[CMOS_INIT_LEN]= 
{
    // --- Page 0: basic enable + timing ---------------------------------
    0xFD,0x00,
    0x12,0x02,          // enable core blocks (safe)
    0x13,0x2F,          // misc control (kept from your working stub)

    // Frame timing for 640x480 @ ~10fps on 24MHz (matches your “works” case)
    0x03,0x01,          // HB[11:8]
    0x04,0xC2,          // HB[7:0]  (0x1C2)
    0x09,0x02,          // VB[11:8]
    0x0A,0xF4,          // VB[7:0]  (0x2F4)

    // DVP sync polarity/packing (BK-style defaults)
    0x32,0x15,          // BT.601: separate HS/VS; pos-pol (try flip later if needed)
    0x35,0x40,          // YUV422 on DVP (YUYV order)
    0x33,0xEF,          // enable ISP data path
    0x5F,0x51,          // route to DVP pins

    // --- Page 1: clocking, output path, black level, AE/AWB seeds -------
    0xFD,0x01,
    0xF0,0x00,          // clock gating: on
    0xF7,0x4B,          // PLL/clock divider (safe default seen on SP parts)
    0xF2,0x49,          // output select: DVP path, 8-bit, no MIPI

    // Black level clamp (mild)
    0x1F,0x10,
    0x20,0x1F,

    // AE/AGC: conservative open loop start so image isn’t green/magenta
    0x26,0x80, 0x27,0x40,  // max/min AGC
    0x28,0x00, 0x29,0x20,  // exposure lines min
    0x2A,0x00, 0x2B,0x03,  // exposure lines step
    0x2C,0x00, 0x2D,0x20,  // integration clamp
    0x30,0x00, 0x31,0x00,  // AEC speed/hold = slow & safe

    // AWB grey targets (neutral)
    0xEB,0x70,           // outdoor
    0xEC,0x70,           // indoor
    0xED,0x05, 0xEE,0x0A,// hysteresis

    // Pre-gains (unity)
    0x6D,0x10, 0x6E,0x10, 0x6F,0x10, 0x70,0x10,   // Y/UV pre-gains
    0x71,0x10, 0x72,0x10, 0x73,0x10, 0x74,0x10,

    // Slight chroma desat to avoid purple/green cast while AWB settles
    0x75,0x08, 0x76,0x08, 0x77,0x08, 0x78,0x08,   // sat curve low
    0x79,0x18, 0x7A,0x18, 0x7B,0x18, 0x7C,0x18,   // sat curve high

    // --- Page 2: gamma (soft S curve) & minor ISP knobs ------------------
    0xFD,0x02,
    // 9-point gamma, gentle – avoids color crush that can look magenta/green
    0x86,0x20,
    0x6D,0x12, 0x6E,0x1A, 0x6F,0x1E, 0x70,0x24,
    0x71,0x12, 0x72,0x1A, 0x73,0x1E, 0x74,0x24,

    // Luma contrast neutral
    0x54,0x0A,          // contrast mid
    0x5B,0x05, 0x5C,0xA0,// edge/sharpness mild

    // --- Back to Page 0: re-enable stream --------------------------------
    0xFD,0x00,
    0x12,0x18,          // stream on (ISP + DVP)

    -1, -1
};


SENSOR_OP_SECTION const _Sensor_Adpt_ sp0a20_cmd= 
{	
	.typ = 1, //YUV
	.pixelw = 640,
	.pixelh= 480,
	.hsyn = 1,
	.vsyn = 1,
	.rduline = 0,//
	.rawwide = 1,//10bit
	.colrarray = 2,//0:_RGRG_ 1:_GRGR_,2:_BGBG_,3:_GBGB_
	.init = (uint8 *)SP0A20InitTable,
	.rotate_adapt = {0},
	.hvb_adapt = {0x80,0x0a,0x80,0x0a},
	. mclk = 24000000,
	.p_fun_adapt = {NULL,NULL,NULL},
	.p_xc7016_adapt = {NULL},	
};

const _Sensor_Ident_ sp0a20_init=
{
	0x2B,0x42,0x43,0x01,0x01,0x02
};


#endif