/**
 * MIT License
 *
 * Copyright (c) 2018-2023 Mahyar Koshkouei
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Please note that at least two parts of source code within this project was
 * taken from the SameBoy project at https://github.com/LIJI32/SameBoy/ which at
 * the time of this writing is released under the MIT License. Occurrences of
 * this code is marked as being taken from SameBoy with a comment.
 * SameBoy, and code marked as being taken from SameBoy,
 * is Copyright (c) 2015-2019 Lior Halphon.
 */

#ifndef PEANUT_GB_H
#define PEANUT_GB_H

#if defined(__has_include)
# if __has_include("version.all")
#  include "version.all"	/* Version information */
# endif
#else
/* Stub __has_include for later. */
# define __has_include(x) 0
#endif

#include <stdint.h>	/* Required for int types */
#include <time.h>	/* Required for tm struct */

/**
* If PEANUT_GB_IS_LITTLE_ENDIAN is positive, then Peanut-GB will be configured
* for a little endian platform. If 0, then big endian.
*/
#if !defined(PEANUT_GB_IS_LITTLE_ENDIAN)
/* If endian is not defined, then attempt to detect it. */
# if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
/* Building for a big endian platform. */
#   define PEANUT_GB_IS_LITTLE_ENDIAN 0
#  else
#   define PEANUT_GB_IS_LITTLE_ENDIAN 1
#  endif /* __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ */
# elif defined(_WIN32)
/* We assume that Windows is always little endian by default. */
#  define PEANUT_GB_IS_LITTLE_ENDIAN 1
# elif !defined(PEANUT_GB_IS_LITTLE_ENDIAN)
#  error "Could not detect target platform endian. Please define PEANUT_GB_IS_LITTLE_ENDIAN"
# endif
#endif /* !defined(PEANUT_GB_IS_LITTLE_ENDIAN) */

#if PEANUT_GB_IS_LITTLE_ENDIAN == 0
# error "Peanut-GB only supports little endian targets"
/* This is because the logic has been written with assumption of little
 * endian byte order. */
#endif

/** Definitions for compile-time setting of features. **/
/**
 * Sound support must be provided by an external library. When audio_read() and
 * audio_write() functions are provided, define ENABLE_SOUND to a non-zero value
 * before including peanut_gb.h in order for these functions to be used.
 */
#ifndef ENABLE_SOUND
# define ENABLE_SOUND 0
#endif

/* Enable LCD drawing. On by default. May be turned off for testing purposes. */
#ifndef ENABLE_LCD
# define ENABLE_LCD 1
#endif

/* Enable 16 bit colour palette. If disabled, only four colour shades are set in
 * pixel data. */
#ifndef PEANUT_GB_12_COLOUR
# define PEANUT_GB_12_COLOUR 0
#endif

/* Adds more code to improve LCD rendering accuracy. */
#ifndef PEANUT_GB_HIGH_LCD_ACCURACY
# define PEANUT_GB_HIGH_LCD_ACCURACY 1
#endif

/* Use intrinsic functions. This may produce smaller and faster code. */
#ifndef PEANUT_GB_USE_INTRINSICS
# define PEANUT_GB_USE_INTRINSICS 1
#endif

/* Only include function prototypes. At least one file must *not* have this
 * defined. */
// #define PEANUT_GB_HEADER_ONLY

/** Internal source code. **/
/* Interrupt masks */
#define VBLANK_INTR	0x01
#define LCDC_INTR	0x02
#define TIMER_INTR	0x04
#define SERIAL_INTR	0x08
#define CONTROL_INTR	0x10
#define ANY_INTR	0x1F

/* Memory section sizes for DMG */
#define WRAM_SIZE	0x2000
#define VRAM_SIZE	0x2000
#define HRAM_IO_SIZE	0x0100
#define OAM_SIZE	0x00A0

/* Memory addresses */
#define ROM_0_ADDR      0x0000
#define ROM_N_ADDR      0x4000
#define VRAM_ADDR       0x8000
#define CART_RAM_ADDR   0xA000
#define WRAM_0_ADDR     0xC000
#define WRAM_1_ADDR     0xD000
#define ECHO_ADDR       0xE000
#define OAM_ADDR        0xFE00
#define UNUSED_ADDR     0xFEA0
#define IO_ADDR         0xFF00
#define HRAM_ADDR       0xFF80
#define INTR_EN_ADDR    0xFFFF

/* Cart section sizes */
#define ROM_BANK_SIZE   0x4000
#define WRAM_BANK_SIZE  0x1000
#define CRAM_BANK_SIZE  0x2000
#define VRAM_BANK_SIZE  0x2000

/* DIV Register is incremented at rate of 16384Hz.
 * 4194304 / 16384 = 256 clock cycles for one increment. */
#define DIV_CYCLES          256

/* Serial clock locked to 8192Hz on DMG.
 * 4194304 / (8192 / 8) = 4096 clock cycles for sending 1 byte. */
#define SERIAL_CYCLES       4096

/* Calculating VSYNC. */
#define DMG_CLOCK_FREQ      4194304.0
#define SCREEN_REFRESH_CYCLES 70224.0
#define VERTICAL_SYNC       (DMG_CLOCK_FREQ/SCREEN_REFRESH_CYCLES)

/* Real Time Clock is locked to 1Hz. */
#define RTC_CYCLES	((uint_fast32_t)DMG_CLOCK_FREQ)

/* SERIAL SC register masks. */
#define SERIAL_SC_TX_START  0x80
#define SERIAL_SC_CLOCK_SRC 0x01

/* STAT register masks */
#define STAT_LYC_INTR       0x40
#define STAT_MODE_2_INTR    0x20
#define STAT_MODE_1_INTR    0x10
#define STAT_MODE_0_INTR    0x08
#define STAT_LYC_COINC      0x04
#define STAT_MODE           0x03
#define STAT_USER_BITS      0xF8

/* LCDC control masks */
#define LCDC_ENABLE         0x80
#define LCDC_WINDOW_MAP     0x40
#define LCDC_WINDOW_ENABLE  0x20
#define LCDC_TILE_SELECT    0x10
#define LCDC_BG_MAP         0x08
#define LCDC_OBJ_SIZE       0x04
#define LCDC_OBJ_ENABLE     0x02
#define LCDC_BG_ENABLE      0x01

/** LCD characteristics **/
/* There are 154 scanlines. LY < 154. */
#define LCD_VERT_LINES      154
#define LCD_WIDTH           160
#define LCD_HEIGHT          144
/* PPU cycles through modes every 456 cycles. */
#define LCD_LINE_CYCLES     456
#define LCD_MODE0_HBLANK_MAX_DRUATION	204
#define LCD_MODE0_HBLANK_MIN_DRUATION	87
#define LCD_MODE2_OAM_SCAN_DURATION	80
#define LCD_MODE3_LCD_DRAW_MIN_DURATION	172
#define LCD_MODE3_LCD_DRAW_MAX_DURATION	289
#define LCD_MODE1_VBLANK_DURATION	(LCD_LINE_CYCLES * (LCD_VERT_LINES - LCD_HEIGHT))
#define LCD_FRAME_CYCLES		(LCD_LINE_CYCLES * LCD_VERT_LINES)
/* The following assumes that Hblank starts on cycle 0. */
/* Mode 2 (OAM Scan) starts on cycle 204 (although this is dependent on the
 * duration of Mode 3 (LCD Draw). */
#define LCD_MODE_2_CYCLES   LCD_MODE0_HBLANK_MAX_DRUATION
/* Mode 3 starts on cycle 284. */
#define LCD_MODE_3_CYCLES   (LCD_MODE_2_CYCLES + LCD_MODE2_OAM_SCAN_DURATION)
/* Mode 0 starts on cycle 376. */
#define LCD_MODE_0_CYCLES   (LCD_MODE_3_CYCLES + LCD_MODE3_LCD_DRAW_MIN_DURATION)

#define LCD_MODE2_OAM_SCAN_START	0
#define LCD_MODE2_OAM_SCAN_END		(LCD_MODE2_OAM_SCAN_DURATION)
#define LCD_MODE3_LCD_DRAW_END		(LCD_MODE2_OAM_SCAN_END + LCD_MODE3_LCD_DRAW_MIN_DURATION)
#define LCD_MODE0_HBLANK_END		(LCD_MODE3_LCD_DRAW_END + LCD_MODE0_HBLANK_MAX_DRUATION)
#if LCD_MODE0_HBLANK_END != LCD_LINE_CYCLES
#error "LCD length not equal"
#endif

/* VRAM Locations */
#define VRAM_TILES_1        (0x8000 - VRAM_ADDR)
#define VRAM_TILES_2        (0x8800 - VRAM_ADDR)
#define VRAM_BMAP_1         (0x9800 - VRAM_ADDR)
#define VRAM_BMAP_2         (0x9C00 - VRAM_ADDR)
#define VRAM_TILES_3        (0x8000 - VRAM_ADDR + VRAM_BANK_SIZE)
#define VRAM_TILES_4        (0x8800 - VRAM_ADDR + VRAM_BANK_SIZE)

/* Interrupt jump addresses */
#define VBLANK_INTR_ADDR    0x0040
#define LCDC_INTR_ADDR      0x0048
#define TIMER_INTR_ADDR     0x0050
#define SERIAL_INTR_ADDR    0x0058
#define CONTROL_INTR_ADDR   0x0060

/* SPRITE controls */
#define NUM_SPRITES         0x28
#define MAX_SPRITES_LINE    0x0A
#define OBJ_PRIORITY        0x80
#define OBJ_FLIP_Y          0x40
#define OBJ_FLIP_X          0x20
#define OBJ_PALETTE         0x10

/* Joypad buttons */
#define JOYPAD_A            0x01
#define JOYPAD_B            0x02
#define JOYPAD_SELECT       0x04
#define JOYPAD_START        0x08
#define JOYPAD_RIGHT        0x10
#define JOYPAD_LEFT         0x20
#define JOYPAD_UP           0x40
#define JOYPAD_DOWN         0x80

#define ROM_HEADER_CHECKSUM_LOC	0x014D

/* Local macros. */
#ifndef MIN
# define MIN(a, b)          ((a) < (b) ? (a) : (b))
#endif

#define PEANUT_GB_ARRAYSIZE(array)    (sizeof(array)/sizeof(array[0]))

/** Allow setting deprecated functions and variables. */
#if (defined(__GNUC__) && __GNUC__ >= 6) || (defined(__clang__) && __clang_major__ >= 4)
# define PGB_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
# define PGB_DEPRECATED(msg)
#endif

#if !defined(__has_builtin)
/* Stub __has_builtin if it isn't available. */
# define __has_builtin(x) 0
#endif

/* The PGB_UNREACHABLE() macro tells the compiler that the code path will never
 * be reached, allowing for further optimisation. */
#if !defined(PGB_UNREACHABLE)
# if __has_builtin(__builtin_unreachable)
#  define PGB_UNREACHABLE() __builtin_unreachable()
# elif defined(_MSC_VER) && _MSC_VER >= 1200
#  /* __assume is not available before VC6. */
#  define PGB_UNREACHABLE() __assume(0)
# else
#  define PGB_UNREACHABLE() abort()
# endif
#endif /* !defined(PGB_UNREACHABLE) */

#if !defined(PGB_UNLIKELY)
# if __has_builtin(__builtin_expect)
#  define PGB_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
# else
#  define PGB_UNLIKELY(expr) (expr)
# endif
#endif /* !defined(PGB_UNLIKELY) */
#if !defined(PGB_LIKELY)
# if __has_builtin(__builtin_expect)
#  define PGB_LIKELY(expr) __builtin_expect(!!(expr), 1)
# else
#  define PGB_LIKELY(expr) (expr)
# endif
#endif /* !defined(PGB_LIKELY) */

#if PEANUT_GB_USE_INTRINSICS
/* If using MSVC, only enable intrinsics for x86 platforms*/
# if defined(_MSC_VER) && __has_include("intrin.h") && \
	(defined(_M_IX86_FP) || defined(_M_AMD64) || defined(_M_X64))
/* Define intrinsic functions for MSVC. */
#  include <intrin.h>
#  define PGB_INTRIN_SBC(x,y,cin,res) _subborrow_u8(cin,x,y,&res)
#  define PGB_INTRIN_ADC(x,y,cin,res) _addcarry_u8(cin,x,y,&res)
# endif /* MSVC */

/* Check for intrinsic functions in GCC and Clang. */
# if __has_builtin(__builtin_sub_overflow)
#  define PGB_INTRIN_SBC(x,y,cin,res) __builtin_sub_overflow(x,y+cin,&res)
#  define PGB_INTRIN_ADC(x,y,cin,res) __builtin_add_overflow(x,y+cin,&res)
# endif
#endif /* PEANUT_GB_USE_INTRINSICS */

#if defined(PGB_INTRIN_SBC)
# define PGB_INSTR_SBC_R8(r,cin)						\
	{									\
		uint8_t temp;							\
		gb->cpu_reg.f.f_bits.c = PGB_INTRIN_SBC(gb->cpu_reg.a,r,cin,temp);\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;\
		gb->cpu_reg.f.f_bits.n = 1;					\
		gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
		gb->cpu_reg.a = temp;						\
	}

# define PGB_INSTR_CP_R8(r)							\
	{									\
		uint8_t temp;							\
		gb->cpu_reg.f.f_bits.c = PGB_INTRIN_SBC(gb->cpu_reg.a,r,0,temp);\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;\
		gb->cpu_reg.f.f_bits.n = 1;					\
		gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
	}
#else
# define PGB_INSTR_SBC_R8(r,cin)						\
	{									\
		uint16_t temp = gb->cpu_reg.a - (r + cin);			\
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
		gb->cpu_reg.f.f_bits.n = 1;					\
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
		gb->cpu_reg.a = (temp & 0xFF);					\
	}

# define PGB_INSTR_CP_R8(r)							\
	{									\
		uint16_t temp = gb->cpu_reg.a - r;				\
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
		gb->cpu_reg.f.f_bits.n = 1;					\
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
	}
#endif  /* PGB_INTRIN_SBC */

#if defined(PGB_INTRIN_ADC)
# define PGB_INSTR_ADC_R8(r,cin)						\
	{									\
		uint8_t temp;							\
		gb->cpu_reg.f.f_bits.c = PGB_INTRIN_ADC(gb->cpu_reg.a,r,cin,temp);\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
		gb->cpu_reg.f.f_bits.n = 0;					\
		gb->cpu_reg.f.f_bits.z = (temp == 0x00);			\
		gb->cpu_reg.a = temp;						\
	}
#else
# define PGB_INSTR_ADC_R8(r,cin)						\
	{									\
		uint16_t temp = gb->cpu_reg.a + r + cin;			\
		gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;		\
		gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0; \
		gb->cpu_reg.f.f_bits.n = 0;					\
		gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);		\
		gb->cpu_reg.a = (temp & 0xFF);					\
	}
#endif /* PGB_INTRIN_ADC */

#define PGB_INSTR_INC_R8(r)							\
	r++;									\
	gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x00);				\
	gb->cpu_reg.f.f_bits.n = 0;						\
	gb->cpu_reg.f.f_bits.z = (r == 0x00)

#define PGB_INSTR_DEC_R8(r)							\
	r--;									\
	gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x0F);				\
	gb->cpu_reg.f.f_bits.n = 1;						\
	gb->cpu_reg.f.f_bits.z = (r == 0x00)

#define PGB_INSTR_XOR_R8(r)							\
	gb->cpu_reg.a ^= r;							\
	gb->cpu_reg.f.reg = 0;							\
	gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)

#define PGB_INSTR_OR_R8(r)							\
	gb->cpu_reg.a |= r;							\
        gb->cpu_reg.f.reg = 0;							\
	gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)

#define PGB_INSTR_AND_R8(r)							\
	gb->cpu_reg.a &= r;							\
	gb->cpu_reg.f.reg = 0;							\
	gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00);			\
	gb->cpu_reg.f.f_bits.h = 1

#if PEANUT_GB_IS_LITTLE_ENDIAN
# define PEANUT_GB_GET_LSB16(x) (x & 0xFF)
# define PEANUT_GB_GET_MSB16(x) (x >> 8)
# define PEANUT_GB_GET_MSN16(x) (x >> 12)
# define PEANUT_GB_U8_TO_U16(h,l) ((l) | ((h) << 8))
#else
# define PEANUT_GB_GET_LSB16(x) (x >> 8)
# define PEANUT_GB_GET_MSB16(x) (x & 0xFF)
# define PEANUT_GB_GET_MSN16(x) ((x & 0xF0) >> 4)
# define PEANUT_GB_U8_TO_U16(h,l) ((h) | ((l) << 8))
#endif

struct cpu_registers_s
{
/* Change register order if big endian.
 * Macro receives registers in little endian order. */
#if PEANUT_GB_IS_LITTLE_ENDIAN
# define PEANUT_GB_LE_REG(x,y) x,y
#else
# define PEANUT_GB_LE_REG(x,y) y,x
#endif
	/* Define specific bits of Flag register. */
	union {
		struct {
			uint8_t  : 4; /* Unused. */
			uint8_t c: 1; /* Carry flag. */
			uint8_t h: 1; /* Half carry flag. */
			uint8_t n: 1; /* Add/sub flag. */
			uint8_t z: 1; /* Zero flag. */
		} f_bits;
		uint8_t reg;
	} f;
	uint8_t a;

	union
	{
		struct
		{
			uint8_t PEANUT_GB_LE_REG(c,b);
		} bytes;
		uint16_t reg;
	} bc;

	union
	{
		struct
		{
			uint8_t PEANUT_GB_LE_REG(e,d);
		} bytes;
		uint16_t reg;
	} de;

	union
	{
		struct
		{
			uint8_t PEANUT_GB_LE_REG(l,h);
		} bytes;
		uint16_t reg;
	} hl;

	/* Stack pointer */
	union
	{
		struct
		{
			uint8_t PEANUT_GB_LE_REG(p, s);
		} bytes;
		uint16_t reg;
	} sp;

	/* Program counter */
	union
	{
		struct
		{
			uint8_t PEANUT_GB_LE_REG(c, p);
		} bytes;
		uint16_t reg;
	} pc;
#undef PEANUT_GB_LE_REG
};

struct count_s
{
	uint_fast16_t lcd_count;	/* LCD Timing */
	uint_fast16_t div_count;	/* Divider Register Counter */
	uint_fast16_t tima_count;	/* Timer Counter */
	uint_fast16_t serial_count;	/* Serial Counter */
	uint_fast32_t rtc_count;	/* RTC Counter */
	uint_fast32_t lcd_off_count;	/* Cycles LCD has been disabled */
};

#if ENABLE_LCD
	/* Bit mask for the shade of pixel to display */
	#define LCD_COLOUR	0x03

# if PEANUT_GB_12_COLOUR
	/**
	* Bit mask for whether a pixel is OBJ0, OBJ1, or BG. Each may have a different
	* palette when playing a DMG game on CGB.
	*/
	#define LCD_PALETTE_OBJ	0x10
	#define LCD_PALETTE_BG	0x20
	/**
	* Bit mask for the two bits listed above.
	* LCD_PALETTE_ALL == 0b00 --> OBJ0
	* LCD_PALETTE_ALL == 0b01 --> OBJ1
	* LCD_PALETTE_ALL == 0b10 --> BG
	* LCD_PALETTE_ALL == 0b11 --> NOT POSSIBLE
	*/
	#define LCD_PALETTE_ALL 0x30
# endif
#endif

/**
 * Errors that may occur during emulation.
 */
enum gb_error_e
{
	GB_UNKNOWN_ERROR = 0,
	GB_INVALID_OPCODE = 1,
	GB_INVALID_READ = 2,
	GB_INVALID_WRITE = 3,

	/* GB_HALT_FOREVER is deprecated and will no longer be issued as an
	 * error by Peanut-GB. */
	GB_HALT_FOREVER PGB_DEPRECATED("Error no longer issued by Peanut-GB") = 4,

	GB_INVALID_MAX = 5
};

/**
 * Errors that may occur during library initialisation.
 */
typedef enum gb_init_error_e
{
	GB_INIT_NO_ERROR = 0,
	GB_INIT_CARTRIDGE_UNSUPPORTED,
	GB_INIT_INVALID_CHECKSUM,

	GB_INIT_INVALID_MAX
} gb_init_error_e;

/**
 * Return codes for serial receive function, mainly for clarity.
 */
enum gb_serial_rx_ret_e
{
	GB_SERIAL_RX_SUCCESS = 0,
	GB_SERIAL_RX_NO_CONNECTION = 1
};

union cart_rtc
{
	struct
	{
		uint8_t sec;
		uint8_t min;
		uint8_t hour;
		uint8_t yday;
		uint8_t high;
	} reg;
	uint8_t bytes[5];
};

/**
 * Emulator context.
 *
 * Only values within the `direct` struct may be modified directly by the
 * front-end implementation. Other variables must not be modified.
 */
typedef struct gb_s
{
	/**
	 * Return byte from ROM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in ROM
	 */
	uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Return byte from cart RAM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \return		byte at address in RAM
	 */
	uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t addr);

	/**
	 * Write byte to cart RAM at given address.
	 *
	 * \param gb_s	emulator context
	 * \param addr	address
	 * \param val	value to write to address in RAM
	 */
	void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t addr,
				  const uint8_t val);

	/**
	 * Notify front-end of error.
	 *
	 * \param gb_s		emulator context
	 * \param gb_error_e	error code
	 * \param addr		address of where error occurred
	 */
	void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t addr);

	/* Transmit one byte and return the received byte. */
	void (*gb_serial_tx)(struct gb_s*, const uint8_t tx);
	enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*, uint8_t* rx);

	/* Read byte from boot ROM at given address. */
	uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t addr);

	struct
	{
		bool gb_halt	: 1;
		bool gb_ime	: 1;
		/* gb_frame is set when 0.016742706298828125 seconds have
		 * passed. It is likely that a new frame has been drawn since
		 * then, but it is possible that the LCD was switched off and
		 * nothing was drawn. */
		bool gb_frame	: 1;
		bool lcd_blank	: 1;
		/* Set if MBC3O cart is used. */
		bool cart_is_mbc3O : 1;
	};

	/* Cartridge information:
	 * Memory Bank Controller (MBC) type. */
	int8_t mbc;
	/* Whether the MBC has internal RAM. */
	uint8_t cart_ram;
	/* Number of ROM banks in cartridge. */
	uint16_t num_rom_banks_mask;
	/* Number of RAM banks in cartridge. Ignore for MBC2. */
	uint8_t num_ram_banks;

	uint16_t selected_rom_bank;
	/* WRAM and VRAM bank selection not available. */
	uint8_t cart_ram_bank;
	uint8_t enable_cart_ram;
	/* Cartridge ROM/RAM mode select. */
	uint8_t cart_mode_select;

	union cart_rtc rtc_latched, rtc_real;

	struct cpu_registers_s cpu_reg;
	//struct gb_registers_s gb_reg;
	struct count_s counter;

	/* TODO: Allow implementation to allocate WRAM, VRAM and Frame Buffer. */
	uint8_t wram[WRAM_SIZE];
	uint8_t vram[VRAM_SIZE];
	uint8_t oam[OAM_SIZE];
	uint8_t hram_io[HRAM_IO_SIZE];

	struct
	{
		/**
		 * Draw line on screen.
		 *
		 * \param gb_s		emulator context
		 * \param pixels	The 160 pixels to draw.
		 * 			Bits 1-0 are the colour to draw.
		 * 			Bits 5-4 are the palette, where:
		 * 				OBJ0 = 0b00,
		 * 				OBJ1 = 0b01,
		 * 				BG = 0b10
		 * 			Other bits are undefined.
		 * 			Bits 5-4 are only required by front-ends
		 * 			which want to use a different colour for
		 * 			different object palettes. This is what
		 * 			the Game Boy Color (CGB) does to DMG
		 * 			games.
		 * \param line		Line to draw pixels on. This is
		 * guaranteed to be between 0-144 inclusive.
		 */
		void (*lcd_draw_line)(struct gb_s *gb);

		/* Palettes */
		uint8_t bg_palette[4];
		uint8_t sp_palette[8];

		uint8_t window_clear;
		uint8_t WY;

		/* Only support 30fps frame skip. */
		bool frame_skip_count : 1;
		bool interlace_count : 1;
	} display;

	/**
	 * Variables that may be modified directly by the front-end.
	 * This method seems to be easier and possibly less overhead than
	 * calling a function to modify these variables each time.
	 *
	 * None of this is thread-safe.
	 */
	struct
	{
		/* Set to enable interlacing. Interlacing will start immediately
		 * (at the next line drawing).
		 */
		bool interlace : 1;
		bool frame_skip : 1;

		union
		{
			struct
			{
				/* Using this bitfield is deprecated due to
				 * portability concerns. It is recommended to
				 * use the JOYPAD_* defines instead.
				 */
				bool a		: 1;
				bool b		: 1;
				bool select	: 1;
				bool start	: 1;
				bool right	: 1;
				bool left	: 1;
				bool up		: 1;
				bool down	: 1;
			} joypad_bits;
			uint8_t joypad;
		};

		/* Implementation defined data. Set to NULL if not required. */
		void *priv;
	} direct;
} gb_s;

#ifndef PEANUT_GB_HEADER_ONLY

#define IO_JOYP	0x00
#define IO_SB	0x01
#define IO_SC	0x02
#define IO_DIV	0x04
#define IO_TIMA	0x05
#define IO_TMA	0x06
#define IO_TAC	0x07
#define IO_IF	0x0F
#define IO_LCDC	0x40
#define IO_STAT	0x41
#define IO_SCY	0x42
#define IO_SCX	0x43
#define IO_LY	0x44
#define IO_LYC	0x45
#define	IO_DMA	0x46
#define	IO_BGP	0x47
#define	IO_OBP0	0x48
#define IO_OBP1	0x49
#define IO_WY	0x4A
#define IO_WX	0x4B
#define IO_BOOT	0x50
#define IO_IE	0xFF

#define IO_TAC_RATE_MASK	0x3
#define IO_TAC_ENABLE_MASK	0x4

/* LCD Mode defines. */
#define IO_STAT_MODE_HBLANK		0
#define IO_STAT_MODE_VBLANK		1
#define IO_STAT_MODE_OAM_SCAN		2
#define IO_STAT_MODE_LCD_DRAW		3
#define IO_STAT_MODE_VBLANK_OR_TRANSFER_MASK 0x1

#if ENABLE_LCD
struct sprite_data {
	uint8_t sprite_number;
	uint8_t x;
};
#endif
#endif // PEANUT_GB_HEADER_ONLY

/** Function prototypes: Required functions **/
/**
 * Initialises the emulator context to a known state. Call this before calling
 * any other peanut-gb function.
 * To reset the emulator, you can call gb_reset() instead.
 *
 * \param gb	Allocated emulator context. Must not be NULL.
 * \param gb_rom_read Pointer to function that reads ROM data. ROM banking is
 * 		already handled by Peanut-GB. Must not be NULL.
 * \param gb_cart_ram_read Pointer to function that reads Cart RAM. Must not be
 * 		NULL.
 * \param gb_cart_ram_write Pointer to function to writes to Cart RAM. Must not
 * 		be NULL.
 * \param gb_error Pointer to function that is called when an unrecoverable
 *		error occurs. Must not be NULL. Returning from this
 *		function is undefined and will result in SIGABRT.
 * \param priv	Private data that is stored within the emulator context. Set to
 * 		NULL if unused.
 * \returns	0 on success or an enum that describes the error.
 */
enum gb_init_error_e gb_init(struct gb_s *gb,
			     uint8_t (*gb_rom_read)(struct gb_s*, const uint_fast32_t),
			     uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t),
			     void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t),
			     void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t),
			     void *priv);

/**
 * Executes the emulator and runs for the duration of time equal to one frame.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
void gb_run_frame(struct gb_s *gb);

/**
 * Internal function used to step the CPU. Used mainly for testing.
 * Use gb_run_frame() instead.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
void __gb_step_cpu(struct gb_s *gb);

/** Function prototypes: Optional Functions **/
/**
 * Reset the emulator, like turning the Game Boy off and on again.
 * This function can be called at any time.
 *
 * \param	An initialised emulator context. Must not be NULL.
 */
void gb_reset(struct gb_s *gb);

/**
 * Initialises the display context of the emulator. Only available when
 * ENABLE_LCD is defined to a non-zero value.
 * The pixel data sent to lcd_draw_line comes with both shade and layer data.
 * The first two least significant bits are the shade data (black, dark, light,
 * white). Bits 4 and 5 are layer data (OBJ0, OBJ1, BG), which can be used to
 * add more colours to the game in the same way that the Game Boy Color does to
 * older Game Boy games.
 * This function can be called at any time.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param lcd_draw_line Pointer to function that draws the 2-bit pixel data on the line
 *		"line". Must not be NULL.
 */
#if ENABLE_LCD
void gb_init_lcd(struct gb_s *gb, void (*lcd_draw_line)(struct gb_s *gb));
#endif

/**
 * Initialises the serial connection of the emulator. This function is optional,
 * and if not called, the emulator will assume that no link cable is connected
 * to the game.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param gb_serial_tx Pointer to function that transmits a byte of data over
 *		the serial connection. Must not be NULL.
 * \param gb_serial_rx Pointer to function that receives a byte of data over the
 *		serial connection. If no byte is received,
 *		return GB_SERIAL_RX_NO_CONNECTION. Must not be NULL.
 */
void gb_init_serial(struct gb_s *gb,
		    void (*gb_serial_tx)(struct gb_s*, const uint8_t),
		    enum gb_serial_rx_ret_e (*gb_serial_rx)(struct gb_s*,
			    uint8_t*));

/**
 * Obtains the save size of the game (size of the Cart RAM). Required by the
 * frontend to allocate enough memory for the Cart RAM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param ram_size Pointer to size_t variable that will be set to the size of
 *		the Cart RAM in bytes. Must not be NULL.
 *		If the Cart RAM is not battery backed, this will be set to 0.
 *		If the Cart RAM size is invalid or unknown, this will not be
 *		set.
 * \returns	0 on success, or -1 if the RAM size is invalid or unknown.
 */
int gb_get_save_size_s(struct gb_s *gb, size_t *ram_size);

/**
 * Deprecated. Use gb_get_save_size_s() instead.
 * Obtains the save size of the game (size of the Cart RAM). Required by the
 * frontend to allocate enough memory for the Cart RAM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \returns	Size of the Cart RAM in bytes. 0 if Cartridge has not battery
 *		backed RAM.
 *		0 is also returned on invalid or unknown RAM size.
 */
uint_fast32_t gb_get_save_size(struct gb_s *gb);

/**
 * Calculates and returns a hash of the game header in the same way the Game
 * Boy Color does for colourising old Game Boy games. The frontend can use this
 * hash to automatically set a colour palette.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \returns	Hash of the game header.
 */
uint8_t gb_colour_hash(struct gb_s *gb);

/**
 * Returns the title of ROM.
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param title_str Allocated string at least 16 characters.
 * \returns	Pointer to start of string, null terminated.
 */
const char* gb_get_rom_name(struct gb_s* gb, char *title_str);

/**
 * Deprecated. Will be removed in the next major version.
 * RTC is ticked internally and this function has no effect.
 */
void gb_tick_rtc(struct gb_s *gb);

/**
 * Set initial values in RTC.
 * Should be called after gb_init().
 *
 * \param gb	An initialised emulator context. Must not be NULL.
 * \param time	Time structure with date and time.
 */
void gb_set_rtc(struct gb_s *gb, const struct tm * const time);

/**
 * Use boot ROM on reset. gb_reset() must be called for this to take affect.
 * \param gb 	An initialised emulator context. Must not be NULL.
 * \param gb_bootrom_read Function pointer to read boot ROM binary.
 */
void gb_set_bootrom(struct gb_s *gb,
	uint8_t (*gb_bootrom_read)(struct gb_s*, const uint_fast16_t));

/* Undefine CPU Flag helper functions. */
#undef PEANUT_GB_CPUFLAG_MASK_CARRY
#undef PEANUT_GB_CPUFLAG_MASK_HALFC
#undef PEANUT_GB_CPUFLAG_MASK_ARITH
#undef PEANUT_GB_CPUFLAG_MASK_ZERO
#undef PEANUT_GB_CPUFLAG_BIT_CARRY
#undef PEANUT_GB_CPUFLAG_BIT_HALFC
#undef PEANUT_GB_CPUFLAG_BIT_ARITH
#undef PEANUT_GB_CPUFLAG_BIT_ZERO
#undef PGB_SET_CARRY
#undef PGB_SET_HALFC
#undef PGB_SET_ARITH
#undef PGB_SET_ZERO
#undef PGB_GET_CARRY
#undef PGB_GET_HALFC
#undef PGB_GET_ARITH
#undef PGB_GET_ZERO
#endif //PEANUT_GB_H