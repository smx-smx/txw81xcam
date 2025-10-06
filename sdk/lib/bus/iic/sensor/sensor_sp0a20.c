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
    // Early misc + select page 0
    0xAC,0x8A,  0x5E,0x00, 0x5F,0x00, 0x60,0x00, 0x61,0x00, 0x62,0x00,
    0xFD,0x00,

    // Basic clocks/gating
    0x1C,0x28,          // ISP/clock gating (safe default)
    0x12,0x18,          // ensure ISP/sensor blocks enabled (stream bits held ON)

    // 640x480 timing (minimal HB/VB; tweak later if needed)
    0x03,0x01, 0x04,0x32,   // HB
    0x09,0x01, 0x0A,0x46,   // VB

    // --- Page 1: effects off, DVP routing, U/V order select ---
    0xFD,0x01,
    0xF4,0x00,          // effects = normal
    0xFB,0x25,          // pipeline/clk helper (seen in vendor init)
    0xF2,0x49,          // DVP route enable
    0x5D,0x10,          // U/V ordering on DVP mux (bit0 cleared)

    // Optional pad/drive defaults (benign, used across SP*):
    0xD3,0x08, 0xD4,0x08, 0xD5,0x10, 0xD6,0x10, 0xD7,0x08, 0xD8,0x08, 0xD9,0x10,

    // --- Back to Page 0: explicit BT.601/YUV422 + func enable + stream ---
    0xFD,0x00,
    0x31,0x10,          // sync clamp/mode helper (present in vendor tail)
    0x32,0x15,          // HS/VS/PCLK polarity/mode (BK72xx-friendly default)
    0x34,0x66,          // sync/timing nibble (alt to 0x76; preferred on many boards)
    0x35,0x40,          // YUV422 output enable
    0x33,0xEF,          // enable ISP/path
    0x5F,0x51,          // route to DVP (not MIPI)
    0x1B,0x07,          // additional enable bit seen in vendor tail
    0x12,0x18,          // (re)assert stream ON at the very end

    -1,-1
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