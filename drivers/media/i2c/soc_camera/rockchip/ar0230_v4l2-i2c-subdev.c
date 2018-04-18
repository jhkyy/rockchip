/*
 * ar0230 sensor driver
 *
 * Copyright (C) 2017 hacker@bainarm.cn
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf-core.h>
#include <linux/slab.h>
#include "ov_camera_module.h"

#define ar0230_DRIVER_NAME "ar0230"

#define ar0230_EXT_CLK                         24000000

static struct ov_camera_module ar0230;
static struct ov_camera_module_custom_config ar0230_custom_config;

struct reg_array
{
	u16 addr;
	u16 val;
};

// Helper marco
#define CHECK_RET(expr) \
	if ((ret = expr) < 0) return ret
/*
static const struct reg_array reg_array_pll[] = {
	// Input: 24MHz
	// Output: 40MHz (17帧配置)
	{0x302A, 12},	// VT_PIX_CLK_DIV
	{0x302C, 1},	// VT_SYS_CLK_DIV
	{0x302E, 1},	// PRE_PLL_CLK_DIV
	{0x3030, 20},	// PLL_MULTIPLIER
	{0x3036, 12},	// OP_PIX_CLK_DIV
	{0x3038, 1}	// OP_SYS_CLK_DIV
};
*/
static const struct reg_array reg_array_pll[] = {
	// Input: 24MHz
	// Output: 56MHz (22帧配置)
	{0x302A, 12},	// VT_PIX_CLK_DIV
	{0x302C, 1},	// VT_SYS_CLK_DIV
	{0x302E, 1},	// PRE_PLL_CLK_DIV
	{0x3030, 28},	// PLL_MULTIPLIER
	{0x3036, 12},	// OP_PIX_CLK_DIV
	{0x3038, 1}	// OP_SYS_CLK_DIV
};

static const struct reg_array reg_array_optimized_settings[] = {
	{0x2436, 0x000E}, {0x320C, 0x0180}, {0x320E, 0x0300}, {0x3210, 0x0500}, {0x3204, 0x0B6D}, {0x30FE, 0x0080},
	{0x3ED8, 0x7B99}, {0x3EDC, 0x9BA8}, {0x3EDA, 0x9B9B}, {0x3092, 0x006F}, {0x3EEC, 0x1C04}, {0x30BA, 0x779C},
	{0x3EF6, 0xA70F}, {0x3044, 0x0410}, {0x3ED0, 0xFF44}, {0x3ED4, 0x031F}, {0x30FE, 0x0080}, {0x3EE2, 0x8866},
	{0x3EE4, 0x6623}, {0x3EE6, 0x2263}, {0x30E0, 0x4283}, {0x30F0, 0x1283}
};

static const struct reg_array reg_array_sequencer_hdr[] = {
	{0x301A, 0x0059}, {0xB0A2, 0x0000}, {0xB0A6, 0x0000}, {0xB004, 0x0000}, {0xB008, 0x0000}, {0xB002, 0x0000}, 
	{0xB006, 0x0000}, {0xB040, 0x0000}, {0xB044, 0x0000}, {0xB18E, 0x0000}, {0xB1AC, 0x0000}, {0xB1D0, 0x0000}, 
	{0xA400, 0x0000}, {0xB082, 0x0000}, {0xB01A, 0x0000}, {0xB01E, 0x0000}, {0xB1AE, 0x0000}, {0xB1C6, 0x0000}, 
	{0xB1C4, 0x0000}, {0xB088, 0x0000}, {0x3088, 0x8000}, {0xB086, 0x0000}, {0x3086, 0x4558}, {0x3086, 0x729B}, 
	{0x3086, 0x4A31}, {0x3086, 0x4342}, {0x3086, 0x8E03}, {0x3086, 0x2A14}, {0x3086, 0x4578}, {0x3086, 0x7B3D}, 
	{0x3086, 0xFF3D}, {0x3086, 0xFF3D}, {0x3086, 0xEA2A}, {0x3086, 0x043D}, {0x3086, 0x102A}, {0x3086, 0x052A}, 
	{0x3086, 0x1535}, {0x3086, 0x2A05}, {0x3086, 0x3D10}, {0x3086, 0x4558}, {0x3086, 0x2A04}, {0x3086, 0x2A14}, 
	{0x3086, 0x3DFF}, {0x3086, 0x3DFF}, {0x3086, 0x3DEA}, {0x3086, 0x2A04}, {0x3086, 0x622A}, {0x3086, 0x288E}, 
	{0x3086, 0x0036}, {0x3086, 0x2A08}, {0x3086, 0x3D64}, {0x3086, 0x7A3D}, {0x3086, 0x0444}, {0x3086, 0x2C4B}, 
	{0x3086, 0x8F00}, {0x3086, 0x430C}, {0x3086, 0x2D63}, {0x3086, 0x4316}, {0x3086, 0x8E03}, {0x3086, 0x2AFC}, 
	{0x3086, 0x5C1D}, {0x3086, 0x5754}, {0x3086, 0x495F}, {0x3086, 0x5305}, {0x3086, 0x5307}, {0x3086, 0x4D2B}, 
	{0x3086, 0xF810}, {0x3086, 0x164C}, {0x3086, 0x0855}, {0x3086, 0x562B}, {0x3086, 0xB82B}, {0x3086, 0x984E}, 
	{0x3086, 0x1129}, {0x3086, 0x0429}, {0x3086, 0x8429}, {0x3086, 0x9460}, {0x3086, 0x5C19}, {0x3086, 0x5C1B}, 
	{0x3086, 0x4548}, {0x3086, 0x4508}, {0x3086, 0x4588}, {0x3086, 0x29B6}, {0x3086, 0x8E01}, {0x3086, 0x2AF8}, 
	{0x3086, 0x3E02}, {0x3086, 0x2AFA}, {0x3086, 0x3F09}, {0x3086, 0x5C1B}, {0x3086, 0x29B2}, {0x3086, 0x3F0C}, 
	{0x3086, 0x3E02}, {0x3086, 0x3E13}, {0x3086, 0x5C13}, {0x3086, 0x3F11}, {0x3086, 0x3E0B}, {0x3086, 0x5F2B}, 
	{0x3086, 0x902A}, {0x3086, 0xF22B}, {0x3086, 0x803E}, {0x3086, 0x043F}, {0x3086, 0x0660}, {0x3086, 0x29A2}, 
	{0x3086, 0x29A3}, {0x3086, 0x5F4D}, {0x3086, 0x192A}, {0x3086, 0xFA29}, {0x3086, 0x8345}, {0x3086, 0xA83E}, 
	{0x3086, 0x072A}, {0x3086, 0xFB3E}, {0x3086, 0x2945}, {0x3086, 0x8821}, {0x3086, 0x3E08}, {0x3086, 0x2AFA}, 
	{0x3086, 0x5D29}, {0x3086, 0x9288}, {0x3086, 0x102B}, {0x3086, 0x048B}, {0x3086, 0x1685}, {0x3086, 0x8D48}, 
	{0x3086, 0x4D4E}, {0x3086, 0x2B80}, {0x3086, 0x4C0B}, {0x3086, 0x603F}, {0x3086, 0x282A}, {0x3086, 0xF23F}, 
	{0x3086, 0x0F29}, {0x3086, 0x8229}, {0x3086, 0x8329}, {0x3086, 0x435C}, {0x3086, 0x155F}, {0x3086, 0x4D19}, 
	{0x3086, 0x2AFA}, {0x3086, 0x4558}, {0x3086, 0x8E00}, {0x3086, 0x2A98}, {0x3086, 0x3F06}, {0x3086, 0x1244}, 
	{0x3086, 0x4A04}, {0x3086, 0x4316}, {0x3086, 0x0543}, {0x3086, 0x1658}, {0x3086, 0x4316}, {0x3086, 0x5A43}, 
	{0x3086, 0x1606}, {0x3086, 0x4316}, {0x3086, 0x0743}, {0x3086, 0x168E}, {0x3086, 0x032A}, {0x3086, 0x9C45}, 
	{0x3086, 0x787B}, {0x3086, 0x3F07}, {0x3086, 0x2A9D}, {0x3086, 0x3E2E}, {0x3086, 0x4558}, {0x3086, 0x253E}, 
	{0x3086, 0x068E}, {0x3086, 0x012A}, {0x3086, 0x988E}, {0x3086, 0x0012}, {0x3086, 0x444B}, {0x3086, 0x0343}, 
	{0x3086, 0x2D46}, {0x3086, 0x4316}, {0x3086, 0xA343}, {0x3086, 0x165D}, {0x3086, 0x0D29}, {0x3086, 0x4488}, 
	{0x3086, 0x102B}, {0x3086, 0x0453}, {0x3086, 0x0D8B}, {0x3086, 0x1685}, {0x3086, 0x448E}, {0x3086, 0x032A}, 
	{0x3086, 0xFC5C}, {0x3086, 0x1D8D}, {0x3086, 0x6057}, {0x3086, 0x5449}, {0x3086, 0x5F53}, {0x3086, 0x0553}, 
	{0x3086, 0x074D}, {0x3086, 0x2BF8}, {0x3086, 0x1016}, {0x3086, 0x4C08}, {0x3086, 0x5556}, {0x3086, 0x2BB8}, 
	{0x3086, 0x2B98}, {0x3086, 0x4E11}, {0x3086, 0x2904}, {0x3086, 0x2984}, {0x3086, 0x2994}, {0x3086, 0x605C}, 
	{0x3086, 0x195C}, {0x3086, 0x1B45}, {0x3086, 0x4845}, {0x3086, 0x0845}, {0x3086, 0x8829}, {0x3086, 0xB68E}, 
	{0x3086, 0x012A}, {0x3086, 0xF83E}, {0x3086, 0x022A}, {0x3086, 0xFA3F}, {0x3086, 0x095C}, {0x3086, 0x1B29}, 
	{0x3086, 0xB23F}, {0x3086, 0x0C3E}, {0x3086, 0x023E}, {0x3086, 0x135C}, {0x3086, 0x133F}, {0x3086, 0x113E}, 
	{0x3086, 0x0B5F}, {0x3086, 0x2B90}, {0x3086, 0x2AF2}, {0x3086, 0x2B80}, {0x3086, 0x3E04}, {0x3086, 0x3F06}, 
	{0x3086, 0x6029}, {0x3086, 0xA229}, {0x3086, 0xA35F}, {0x3086, 0x4D1C}, {0x3086, 0x2AFA}, {0x3086, 0x2983}, 
	{0x3086, 0x45A8}, {0x3086, 0x3E07}, {0x3086, 0x2AFB}, {0x3086, 0x3E29}, {0x3086, 0x4588}, {0x3086, 0x243E}, 
	{0x3086, 0x082A}, {0x3086, 0xFA5D}, {0x3086, 0x2992}, {0x3086, 0x8810}, {0x3086, 0x2B04}, {0x3086, 0x8B16}, 
	{0x3086, 0x868D}, {0x3086, 0x484D}, {0x3086, 0x4E2B}, {0x3086, 0x804C}, {0x3086, 0x0B60}, {0x3086, 0x3F28}, 
	{0x3086, 0x2AF2}, {0x3086, 0x3F0F}, {0x3086, 0x2982}, {0x3086, 0x2983}, {0x3086, 0x2943}, {0x3086, 0x5C15}, 
	{0x3086, 0x5F4D}, {0x3086, 0x1C2A}, {0x3086, 0xFA45}, {0x3086, 0x588E}, {0x3086, 0x002A}, {0x3086, 0x983F}, 
	{0x3086, 0x064A}, {0x3086, 0x739D}, {0x3086, 0x0A43}, {0x3086, 0x160B}, {0x3086, 0x4316}, {0x3086, 0x8E03}, 
	{0x3086, 0x2A9C}, {0x3086, 0x4578}, {0x3086, 0x3F07}, {0x3086, 0x2A9D}, {0x3086, 0x3E12}, {0x3086, 0x4558}, 
	{0x3086, 0x3F04}, {0x3086, 0x8E01}, {0x3086, 0x2A98}, {0x3086, 0x8E00}, {0x3086, 0x9176}, {0x3086, 0x9C77}, 
	{0x3086, 0x9C46}, {0x3086, 0x4416}, {0x3086, 0x1690}, {0x3086, 0x7A12}, {0x3086, 0x444B}, {0x3086, 0x4A00}, 
	{0x3086, 0x4316}, {0x3086, 0x6343}, {0x3086, 0x1608}, {0x3086, 0x4316}, {0x3086, 0x5043}, {0x3086, 0x1665}, 
	{0x3086, 0x4316}, {0x3086, 0x6643}, {0x3086, 0x168E}, {0x3086, 0x032A}, {0x3086, 0x9C45}, {0x3086, 0x783F}, 
	{0x3086, 0x072A}, {0x3086, 0x9D5D}, {0x3086, 0x0C29}, {0x3086, 0x4488}, {0x3086, 0x102B}, {0x3086, 0x0453}, 
	{0x3086, 0x0D8B}, {0x3086, 0x1686}, {0x3086, 0x3E1F}, {0x3086, 0x4558}, {0x3086, 0x283E}, {0x3086, 0x068E}, 
	{0x3086, 0x012A}, {0x3086, 0x988E}, {0x3086, 0x008D}, {0x3086, 0x6012}, {0x3086, 0x444B}, {0x3086, 0x2C2C}, 
	{0x3086, 0x2C2C}
};

static const struct reg_array reg_array_ADACD_High_Conversion_Gain[] = {
	{0x3206, 0x1C0E},	// ADACD_NOISE_FLOOR1
	{0x3208, 0x4E39},	// ADACD_NOISE_FLOOR2
	{0x3202, 0x00B0},	// ADACD_NOISE_MODEL1
	{0x3200, 0x0002}	// ADACD_CONTROL
};

/* ======================================================================== */
/* Base sensor configs */
/* ======================================================================== */
static struct ov_camera_module_reg ar0230_init_tab_1928_1088_60fps[] = {
	{OV_CAMERA_MODULE_REG_TYPE_DATA, 0x302A, 12}, {OV_CAMERA_MODULE_REG_TYPE_DATA, 0x302C, 1},
	{OV_CAMERA_MODULE_REG_TYPE_DATA, 0x302E, 1}, {OV_CAMERA_MODULE_REG_TYPE_DATA, 0x3030, 20},
	{OV_CAMERA_MODULE_REG_TYPE_DATA, 0x3036, 12}, {OV_CAMERA_MODULE_REG_TYPE_DATA, 0x3038, 1},
};


static int ar0230_read(struct i2c_client *client, u16 addr, u16 *data)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	u16 __addr;
	u16 ret;

	// 16 bit addressable register
	__addr = cpu_to_be16(addr);

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *)&__addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 2;
	msg[1].buf   = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		dev_err(&client->dev, "Read failed at 0x%hx error %d", addr, ret);
	else
		ret = 0;

	*data = (buf[0] << 8) | buf[1];

	udelay(900);	// Needed for ar0230
	return ret;
}

static int ar0230_write(struct i2c_client *client, u16 addr, u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	u16 __addr, __data;
	int ret;

	// 16-bit addressable register
	__addr = cpu_to_be16(addr);
	__data = cpu_to_be16(data);

	buf[0] = __addr & 0xff;
	buf[1] = __addr >> 8;
	buf[2] = __data & 0xff;
	buf[3] = __data >> 8;
	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 4;
	msg.buf   = buf;

	// i2c_transfer returns message length, but function should return 0
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "Write failed at 0x%hx 0x%hx error %d\n", addr, data, ret);
	else
		ret = 0;

	udelay(900);	// Needed for ar0230
	return ret;
}

static int ar0230_write_bit(struct i2c_client *client, u16 addr, u8 bit_pos, u8 bit_val)
{
	int ret;
	u16 tmp;

	ret = ar0230_read(client, addr, &tmp);
	if (ret < 0)
		return ret;

	if (bit_val)
		tmp |= (1UL << bit_pos);
	else
		tmp &= ~(1UL << bit_pos);

	return ar0230_write(client, addr, tmp);
}

static int ar0230_init_from_array(struct i2c_client *client, const struct reg_array *regs, int cnt)
{
	int ret;
	u16 tmp;

	while (cnt--)
	{
		if (!(regs->addr & 0x8000))
			ret = ar0230_write(client, regs->addr, regs->val);
		else
			ret = ar0230_read(client, regs->addr & 0x7fff, &tmp);

		if (ret < 0)
			return ret;

		regs++;
	}
	return 0;
}

#define AR0230_INIT_FROM_ARRAY(c, a)	ar0230_init_from_array((c), (a), ARRAY_SIZE(a))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug Layer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static ssize_t ar0230_reg_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	u16 val;
	struct i2c_client *i2c_c = to_i2c_client(dev);
	struct ov_camera_module  *sensor = i2c_get_clientdata(i2c_c);

	ar0230_read(i2c_c, sensor->debug_reg_addr, &val);
	return snprintf(buf, PAGE_SIZE, "0x%04x=0x%04x\n", sensor->debug_reg_addr, val);
}

static ssize_t ar0230_reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int cnt;
	u32 addr, val;
	u16 val_tmp;
	struct i2c_client *i2c_c = to_i2c_client(dev);
	struct ov_camera_module  *sensor = i2c_get_clientdata(i2c_c);

	cnt = sscanf(buf, "0x%04x=0x%04x", &addr, &val);
	addr &= ~1UL;

	if (cnt >= 1)
		sensor->debug_reg_addr = (u16)addr;

	if (cnt == 1) {
		ar0230_read(i2c_c, addr, &val_tmp);
		dev_info(dev, "0x%04x=0x%04x\n", addr, val_tmp);
	}

	if (cnt >= 2)
		ar0230_write(i2c_c, sensor->debug_reg_addr, (u16)val);

	return count;
}
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR, ar0230_reg_show, ar0230_reg_store);

static int ar0230_set_hdr_mode(struct i2c_client *client)
{
	int ret;
	u16 tmp;

	// Reset
	CHECK_RET(ar0230_write(client, 0x301A, 0x0001));
	CHECK_RET(ar0230_write(client, 0x301A, 0x10D8));
	msleep(5);

	// HDR Mode Sequencer - Rev1.2
	CHECK_RET(AR0230_INIT_FROM_ARRAY(client, reg_array_sequencer_hdr));

	// AR0230 REV1.2 Optimized Settings
	CHECK_RET(ar0230_read(client, 0x3ED6, &tmp));
	tmp |= 0x00e0;
	tmp &= ~0x0040;
	CHECK_RET(ar0230_write(client, 0x3ED6, tmp));
	CHECK_RET(AR0230_INIT_FROM_ARRAY(client, reg_array_optimized_settings));

	CHECK_RET(ar0230_write(client, 0x301A, 0x10D8));	// RESET_REGISTER
	CHECK_RET(ar0230_write(client, 0x30B0, 0x0118));	// DIGITAL_TEST
	CHECK_RET(ar0230_write(client, 0x31AC, 0x100C));	// DATA_FORMAT_BITS

	// PLL_settings - Parallel
	CHECK_RET(AR0230_INIT_FROM_ARRAY(client, reg_array_pll));

	// Image size
	CHECK_RET(ar0230_write(client, 0x3002, 0));		// Y_ADDR_START
	CHECK_RET(ar0230_write(client, 0x3004, 0));		// X_ADDR_START
	CHECK_RET(ar0230_write(client, 0x3006, 1087));		// Y_ADDR_END
	CHECK_RET(ar0230_write(client, 0x3008, 1927));		// X_ADDR_END
	CHECK_RET(ar0230_write(client, 0x300A, 0x04e5/*AR0230_IMAGE_HEIGHT + 16*/));	// FRAME_LENGTH_LINES
	CHECK_RET(ar0230_write(client, 0x300C, 1118));	// LINE_LENGTH_PCK
	CHECK_RET(ar0230_write(client, 0x3012, 0x050));				// COARSE_INTEGRATION_TIME
	CHECK_RET(ar0230_write(client, 0x30A2, 0x0001));			// X_ODD_INC
	CHECK_RET(ar0230_write(client, 0x30A6, 0x0001));			// Y_ODD_INC
	CHECK_RET(ar0230_write(client, 0x30AE, 0x0001));			// X_ODD_INC_CB
	CHECK_RET(ar0230_write(client, 0x30A8, 0x0001));			// Y_ODD_INC_CB
	CHECK_RET(ar0230_write(client, 0x3040, 0x0000));			// READ_MODE
	CHECK_RET(ar0230_write(client, 0x31AE, 0x0301));			// SERIAL_FORMAT
	CHECK_RET(ar0230_write(client, 0x301C, 0x0003));			// MODE_SELECT MIRROR_ROW[2] MIRROR_COL[1]


	CHECK_RET(ar0230_write(client, 0x3058, 0x00ff));			// blue gain
	CHECK_RET(ar0230_write(client, 0x305a, 0x00a8));			// red gain

	// HDR Mode 16x Setup
	{
		CHECK_RET(ar0230_write(client, 0x3082, 0x0000));	// OPERATION_MODE_CTRL

		// HDR Mode Setup
		{
			CHECK_RET(ar0230_write(client, 0x3176, 0x0080));
			CHECK_RET(ar0230_write(client, 0x3178, 0x0080));
			CHECK_RET(ar0230_write(client, 0x317A, 0x0080));
			CHECK_RET(ar0230_write(client, 0x317C, 0x0080));

			// AR0230_Rev1_AWB_CCM
			// 2D Defect Correction
			CHECK_RET(ar0230_write_bit(client, 0x31E0, 9, 1));	// PIX_DEF_ID, PIX_DEF_ID_2D_SINGLE_EN[9], 1

			// ALTM Enabled
			{
				CHECK_RET(ar0230_write(client, 0x2420, 0x0000));	// ALTM_FSHARP_V
				CHECK_RET(ar0230_write(client, 0x2440, 0x0004));	// ALTM_CONTROL_DAMPER
				CHECK_RET(ar0230_write(client, 0x2442, 0x0080));	// ALTM_CONTROL_KEY_K0
				CHECK_RET(ar0230_write(client, 0x301E, 0x0000));	// DATA_PEDESTAL
				CHECK_RET(ar0230_write(client, 0x2450, 0x0000));	// ALTM_OUT_PEDESTAL
				CHECK_RET(ar0230_write(client, 0x320A, 0x0080));	// ADACD_PEDESTAL
				CHECK_RET(ar0230_write(client, 0x31D0, 0x0000));	// COMPANDING
				CHECK_RET(ar0230_write(client, 0x2400, 0x0002));	// ALTM_CONTROL
				CHECK_RET(ar0230_write(client, 0x2410, 0x0005));	// ALTM_POWER_GAIN
				CHECK_RET(ar0230_write(client, 0x2412, 0x001A));	// ALTM_POWER_OFFSET
				CHECK_RET(ar0230_write(client, 0x2444, 0x0000));	// ALTM_CONTROL_KEY_K01_LO
				CHECK_RET(ar0230_write(client, 0x2446, 0x0002));	// ALTM_CONTROL_KEY_K01_HI
				CHECK_RET(ar0230_write(client, 0x2438, 0x0010));	// ALTM_CONTROL_MIN_FACTOR
				CHECK_RET(ar0230_write(client, 0x243A, 0x0012));	// ALTM_CONTROL_MAX_FACTOR
				CHECK_RET(ar0230_write(client, 0x243C, 0xFFFF));	// ALTM_CONTROL_DARK_FLOOR
				CHECK_RET(ar0230_write(client, 0x243E, 0x0100));	// ALTM_CONTROL_BRIGHT_FLOOR
			}


			// ADACD_High_Conversion_Gain
			//CHECK_RET(AR0230_INIT_FROM_ARRAY(client, reg_array_ADACD_High_Conversion_Gain));

			// Motion Compensation On
			{
				CHECK_RET(ar0230_write(client, 0x318A, 0x0E74));	// HDR_MC_CTRL1
				CHECK_RET(ar0230_write(client, 0x318C, 0xC000));	// HDR_MC_CTRL2
				CHECK_RET(ar0230_write(client, 0x318E, 0x0200));	// HDR_MC_CTRL3
				CHECK_RET(ar0230_write(client, 0x3192, 0x0400));	// HDR_MC_CTRL5
				CHECK_RET(ar0230_write(client, 0x3198, 0x183C));	// HDR_MC_CTRL8, q1=60, q2=24

			}

			// DLO enable
			{
				CHECK_RET(ar0230_write(client, 0x3190, 0xE000));	// HDR_MC_CTRL4
				CHECK_RET(ar0230_write(client, 0x3194, 0x0BB8));	// HDR_MC_CTRL6
				CHECK_RET(ar0230_write(client, 0x3196, 0x0E74));	// HDR_MC_CTRL7
			}

			// HDR Mode High Conversion Gain
			{
				// Minimum analog gain for HCG = 1.0x analog Gain
				CHECK_RET(ar0230_write(client, 0x3060, 0x0022));	// ANALOG_GAIN

				CHECK_RET(ar0230_write(client, 0x3096, 0x0780));	// ROW_NOISE_ADJUST_TOP
				CHECK_RET(ar0230_write(client, 0x3098, 0x0780));	// ROW_NOISE_ADJUST_BTM

				// ADACD_High_Conversion_Gain
				CHECK_RET(AR0230_INIT_FROM_ARRAY(client, reg_array_ADACD_High_Conversion_Gain));

				CHECK_RET(ar0230_write(client, 0x3100, 0x0000));	// AECTRLREG
			}

			CHECK_RET(ar0230_write_bit(client, 0x30BA, 8, 1));	// DIGITAL_CTRL, COMBI_MODE[8] = 1
			CHECK_RET(ar0230_write_bit(client, 0x318E, 9, 1));	// HDR_MC_CTRL3, GAIN_BEFORE_DLO[9] = 1

			// Disable embedded data
			CHECK_RET(ar0230_write(client, 0x3064, 0x1802));
		}
	}

	CHECK_RET(ar0230_write(client, 0x301A, 0x10DC));	// RESET_REGISTER
	return ret;
}

static int ar0230_setup(struct i2c_client *client)
{
	int ret;

	CHECK_RET(ar0230_set_hdr_mode(client));

	return ret;
}


/* ======================================================================== */

static struct ov_camera_module_config ar0230_configs[] = {
	{
		.name = "1928x1088_60fps",
		.frm_fmt = {
			.width = 1928,
			.height = 1088,
			.code = MEDIA_BUS_FMT_SGRBG12_1X12
		},
		.frm_intrvl = {
			.interval = {
				.numerator = 1,
				.denominator = 60
			}
		},
		.auto_exp_enabled = false,
		.auto_gain_enabled = false,
		.auto_wb_enabled = false,
		.reg_table = (void *)ar0230_init_tab_1928_1088_60fps,
		.reg_table_num_entries =
			sizeof(ar0230_init_tab_1928_1088_60fps) /
			sizeof(ar0230_init_tab_1928_1088_60fps[0]),
		.v_blanking_time_us = 5000,
		PLTFRM_CAM_ITF_DVP_CFG(
			PLTFRM_CAM_ITF_BT601_12,
			PLTFRM_CAM_SIGNAL_HIGH_LEVEL,
			PLTFRM_CAM_SIGNAL_HIGH_LEVEL,
			PLTFRM_CAM_SDR_POS_EDG,
			ar0230_EXT_CLK)
	}
};

/*--------------------------------------------------------------------------*/

static int ar0230_write_aec(struct ov_camera_module *cam_mod)
{
	return 0;
}

/*--------------------------------------------------------------------------*/

static int ar0230_g_ctrl(struct ov_camera_module *cam_mod, u32 ctrl_id)
{
	int ret = 0;

	ov_camera_module_pr_info(cam_mod, "\n");

	switch (ctrl_id) {
	case V4L2_CID_GAIN:
	case V4L2_CID_EXPOSURE:
	case V4L2_CID_FLASH_LED_MODE:
		/* nothing to be done here */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (IS_ERR_VALUE(ret))
		ov_camera_module_pr_debug(cam_mod,
			"failed with error (%d)\n", ret);
	return ret;
}

/*--------------------------------------------------------------------------*/

static int ar0230_filltimings(struct ov_camera_module_custom_config *custom)
{
	return 0;
}

static int ar0230_g_timings(struct ov_camera_module *cam_mod,
	struct ov_camera_module_timings *timings)
{
	return 0;
}

static int ar0230_s_ctrl(struct ov_camera_module *cam_mod, u32 ctrl_id)
{
	int ret = 0;

	ov_camera_module_pr_info(cam_mod, "\n");

	switch (ctrl_id) {
	case V4L2_CID_GAIN:
	case V4L2_CID_EXPOSURE:
		ret = ar0230_write_aec(cam_mod);
		break;
	case V4L2_CID_FLASH_LED_MODE:
		/* nothing to be done here */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (IS_ERR_VALUE(ret))
		ov_camera_module_pr_debug(cam_mod,
			"failed with error (%d) 0x%x\n", ret, ctrl_id);
	return ret;
}

static int ar0230_s_ext_ctrls(struct ov_camera_module *cam_mod,
				 struct ov_camera_module_ext_ctrls *ctrls)
{
	int ret = 0;

	ov_camera_module_pr_info(cam_mod, "\n");

	/* Handles only exposure and gain together special case. */
	if (ctrls->count == 1)
		ret = ar0230_s_ctrl(cam_mod, ctrls->ctrls[0].id);
	else if ((ctrls->count == 3) &&
		 ((ctrls->ctrls[0].id == V4L2_CID_GAIN &&
		   ctrls->ctrls[1].id == V4L2_CID_EXPOSURE) ||
		  (ctrls->ctrls[1].id == V4L2_CID_GAIN &&
		   ctrls->ctrls[0].id == V4L2_CID_EXPOSURE)))
		ret = ar0230_write_aec(cam_mod);
	else
		ret = -EINVAL;

	if (IS_ERR_VALUE(ret))
		ov_camera_module_pr_debug(cam_mod,
			"failed with error (%d)\n", ret);
	return ret;
}

static int ar0230_set_flip(
	struct ov_camera_module *cam_mod,
	struct pltfrm_camera_module_reg reglist[],
	int len)
{
	return 0;
}

static int ar0230_check_camera_id(struct ov_camera_module *cam_mod)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cam_mod->sd);
	int ret = 0;
	u32 data;

	dev_info(&client->dev, "Probing AR0230 at address 0x%02x ... ", client->addr);
	ret |= ov_camera_module_read_reg(cam_mod, 2, 0x3000, &data);

	if (ret < 0 || data != 0x0056) {
		dev_err(&client->dev, "Failed! 0x%04x\n", data);
		return -ENODEV;
	}

	ar0230_setup(client);
	
	return 0;
	
}

static int ar0230_start_streaming(struct ov_camera_module *cam_mod)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cam_mod->sd);
	int ret = 0;

	ret = ar0230_write_bit(client, 0x301A, 2, true);
	if (IS_ERR_VALUE(ret))
		goto err;

	msleep(25);

	return 0;
err:
	dev_err(&client->dev, "failed with error (%d)\n", ret);
	return ret;
}

static int ar0230_stop_streaming(struct ov_camera_module *cam_mod)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cam_mod->sd);
	int ret = 0;

	ret = ar0230_write_bit(client, 0x301A, 2, false);
	if (IS_ERR_VALUE(ret))
		goto err;

	msleep(25);

	return 0;
err:
	dev_err(&client->dev, "failed with error (%d)\n", ret);
	return ret;
}

/* ======================================================================== */
/* This part is platform dependent */
/* ======================================================================== */
static struct v4l2_subdev_core_ops ar0230_camera_module_core_ops = {
	.g_ctrl = ov_camera_module_g_ctrl,
	.s_ctrl = ov_camera_module_s_ctrl,
	.s_ext_ctrls = ov_camera_module_s_ext_ctrls,
	.s_power = ov_camera_module_s_power,
	.ioctl = ov_camera_module_ioctl
};

static struct v4l2_subdev_video_ops ar0230_camera_module_video_ops = {
	.s_frame_interval = ov_camera_module_s_frame_interval,
	.s_stream = ov_camera_module_s_stream
};

static struct v4l2_subdev_pad_ops ar0230_camera_module_pad_ops = {
	.enum_frame_interval = ov_camera_module_enum_frameintervals,
	.get_fmt = ov_camera_module_g_fmt,
	.set_fmt = ov_camera_module_s_fmt,
};

static struct v4l2_subdev_ops ar0230_camera_module_ops = {
	.core = &ar0230_camera_module_core_ops,
	.video = &ar0230_camera_module_video_ops,
	.pad = &ar0230_camera_module_pad_ops
};

static struct ov_camera_module_custom_config ar0230_custom_config = {
	.start_streaming = ar0230_start_streaming,
	.stop_streaming = ar0230_stop_streaming,
	.s_ctrl = ar0230_s_ctrl,
	.s_ext_ctrls = ar0230_s_ext_ctrls,
	.g_ctrl = ar0230_g_ctrl,
	.g_timings = ar0230_g_timings,
	.check_camera_id = ar0230_check_camera_id,
	.set_flip = ar0230_set_flip,
	.configs = ar0230_configs,
	.num_configs = ARRAY_SIZE(ar0230_configs),
	.power_up_delays_ms = {20, 20, 0}
};

static int ar0230_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err;

	dev_info(&client->dev, "probing...\n");

	ar0230_filltimings(&ar0230_custom_config);
	v4l2_i2c_subdev_init(&ar0230.sd, client, &ar0230_camera_module_ops);

	ar0230.custom = ar0230_custom_config;

	// Debug interface
	ar0230.debug_reg_addr = 0x3000;
	err = device_create_file(&client->dev, &dev_attr_reg);

	// Reset
	ar0230_write(client, 0x301A, 0x0001);
	ar0230_write(client, 0x301A, 0x10D8);
	msleep(5);

	dev_info(&client->dev, "probing successful\n");
	return 0;
}

static int ar0230_remove(struct i2c_client *client)
{
	struct ov_camera_module *cam_mod = i2c_get_clientdata(client);

	dev_info(&client->dev, "removing device...\n");

	if (!client->adapter)
		return -ENODEV;	/* our client isn't attached */

	ov_camera_module_release(cam_mod);

	dev_info(&client->dev, "removed\n");
	return 0;
}

static const struct i2c_device_id ar0230_id[] = {
	{ ar0230_DRIVER_NAME, 0 },
	{ }
};

static const struct of_device_id ar0230_of_match[] = {
	{.compatible = "Semiconductor,ar0230-v4l2-i2c-subdev"},
	{},
};

MODULE_DEVICE_TABLE(i2c, ar0230_id);

static struct i2c_driver ar0230_i2c_driver = {
	.driver = {
		.name = ar0230_DRIVER_NAME,
		.of_match_table = ar0230_of_match
	},
	.probe = ar0230_probe,
	.remove = ar0230_remove,
	.id_table = ar0230_id,
};

module_i2c_driver(ar0230_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for ar0230");
MODULE_AUTHOR("Hacker");
MODULE_LICENSE("GPL");
