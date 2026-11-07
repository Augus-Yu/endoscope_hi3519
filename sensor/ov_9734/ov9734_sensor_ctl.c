/*
* Copyright (C) Hisilicon Technologies Co., Ltd. 2012-2019. All rights reserved.
* Description:
* Author: Hisilicon multimedia software group
* Create: 2011/06/28
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"

#ifdef HI_GPIO_I2C
#include "gpioi2c_ex.h"
#else
#include "hi_i2c.h"
#endif

const unsigned char ov9734_i2c_addr = 0x6c; /* I2C Address of Ov9734 */
const unsigned int ov9734_addr_byte = 2;
const unsigned int ov9734_data_byte = 1;
static int g_fd[ISP_MAX_PIPE_NUM] = { [0 ...(ISP_MAX_PIPE_NUM - 1)] = -1 };

extern ISP_SNS_STATE_S *g_pastOv9734[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U g_aunOv9734BusInfo[];

#define OV9734_1M_30FPS_LINEAR_MODE  (1)
#define OV9734_1M_30FPS_2t1_DOL_MODE (2)

int ov9734_i2c_init(VI_PIPE ViPipe)
{
    char acDevFile[16] = { 0 };
    HI_U8 u8DevNum;

    if (g_fd[ViPipe] >= 0) {
        return HI_SUCCESS;
    }
#ifdef HI_GPIO_I2C
    int ret;

    g_fd[ViPipe] = open("/dev/gpioi2c_ex", O_RDONLY, S_IRUSR);
    if (g_fd[ViPipe] < 0) {
        ISP_ERR_TRACE("Open gpioi2c_ex error!\n");
        return HI_FAILURE;
    }
#else
    int ret;
    u8DevNum = g_aunOv9734BusInfo[ViPipe].s8I2cDev;

    snprintf(acDevFile, sizeof(acDevFile), "/dev/i2c-%u", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0) {
        ISP_ERR_TRACE("Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (ov9734_i2c_addr >> 1));
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_SLAVE_FORCE error!\n");
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return ret;
    }
#endif

    return HI_SUCCESS;
}

int ov9734_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0) {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int ov9734_read_register(VI_PIPE ViPipe, int addr)
{
    return HI_SUCCESS;
}
int ov9734_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (g_fd[ViPipe] < 0) {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = ov9734_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = ov9734_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = ov9734_data_byte;

    ret = ioctl(g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data);

    if (ret) {
        ISP_ERR_TRACE("GPIO-I2C write faild!\n");
        return ret;
    }
#else
    int idx = 0;
    int ret;
    char buf[8];
    if (ov9734_addr_byte == 2) {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    } else {
        buf[idx] = addr & 0xff;
        idx++;
    }

    if (ov9734_data_byte == 2) {
        buf[idx] = (data >> 8) & 0xff;
        idx++;
        buf[idx] = data & 0xff;
        idx++;
    } else {
        buf[idx] = data & 0xff;
        idx++;
    }

    ret = write(g_fd[ViPipe], buf, (ov9734_addr_byte + ov9734_data_byte));
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_WRITE DATA error!\n");
        return HI_FAILURE;
    }

#endif
    return HI_SUCCESS;
}

void ov9734_standby(VI_PIPE ViPipe)
{
    return;
}

void ov9734_restart(VI_PIPE ViPipe)
{
    return;
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void ov9734_linear_1M30_10bit_init(VI_PIPE ViPipe);
void ov9734_DOL_2t1_1M30_10bit_init(VI_PIPE ViPipe);

void ov9734_default_reg_init(VI_PIPE ViPipe)
{
    HI_U32 i = 0;
    for (i = 0; i < g_pastOv9734[ViPipe]->astRegsInfo[0].u32RegNum; i++) {
        ov9734_write_register(ViPipe, g_pastOv9734[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_pastOv9734[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
    }
}
void ov9734_init(VI_PIPE ViPipe)
{
    HI_U8 u8ImgMode;
    HI_BOOL bInit;

    bInit = g_pastOv9734[ViPipe]->bInit;
    u8ImgMode = g_pastOv9734[ViPipe]->u8ImgMode;

    /* 1. sensor i2c init */
    ov9734_i2c_init(ViPipe);

    if (bInit == HI_FALSE) {
        /* 2.  sensor registers init */
        if (u8ImgMode == OV9734_1M_30FPS_LINEAR_MODE) { /* 1K@30fps Linear */
            ov9734_linear_1M30_10bit_init(ViPipe);
        } else if (u8ImgMode == OV9734_1M_30FPS_2t1_DOL_MODE) { /* 4K@30fps DOL2 */
            ov9734_DOL_2t1_1M30_10bit_init(ViPipe);
        }
    } else {

    /* When sensor switch mode(linear<->WDR or resolution), config different registers(if possible) */
        /* 2.  sensor registers init */
        if (u8ImgMode == OV9734_1M_30FPS_LINEAR_MODE) { /* 4K@30fps Linear */
            ov9734_linear_1M30_10bit_init(ViPipe);
        } else if (u8ImgMode == OV9734_1M_30FPS_2t1_DOL_MODE) { /* 4K@30fps DOL2 */
            ov9734_DOL_2t1_1M30_10bit_init(ViPipe);
        }
    }

    g_pastOv9734[ViPipe]->bInit = HI_TRUE;

    return;
}

void ov9734_exit(VI_PIPE ViPipe)
{
    ov9734_i2c_exit(ViPipe);

    return;
}

void ov9734_linear_1M30_10bit_init(VI_PIPE ViPipe)
{
   
    ov9734_write_register(ViPipe, 0x0103, 0x01);
    ov9734_write_register(ViPipe, 0x0100, 0x00);
    ov9734_write_register(ViPipe, 0x3001, 0x00);
    ov9734_write_register(ViPipe, 0x3002, 0x00);
    ov9734_write_register(ViPipe, 0x3007, 0x00);
    ov9734_write_register(ViPipe, 0x3010, 0x00);
    ov9734_write_register(ViPipe, 0x3011, 0x08);
    ov9734_write_register(ViPipe, 0x3014, 0x22);
    ov9734_write_register(ViPipe, 0x301e, 0x15);
    ov9734_write_register(ViPipe, 0x3030, 0x19);
    ov9734_write_register(ViPipe, 0x3080, 0x02);
    ov9734_write_register(ViPipe, 0x3081, 0x3c);
    ov9734_write_register(ViPipe, 0x3082, 0x04);
    ov9734_write_register(ViPipe, 0x3083, 0x00);
    ov9734_write_register(ViPipe, 0x3084, 0x02);
    ov9734_write_register(ViPipe, 0x3085, 0x01);
    ov9734_write_register(ViPipe, 0x3086, 0x01);
    ov9734_write_register(ViPipe, 0x3089, 0x01);
    ov9734_write_register(ViPipe, 0x308a, 0x00);
    ov9734_write_register(ViPipe, 0x3103, 0x01);
    ov9734_write_register(ViPipe, 0x3600, 0x55);
    ov9734_write_register(ViPipe, 0x3601, 0x02);
    ov9734_write_register(ViPipe, 0x3605, 0x22);
    ov9734_write_register(ViPipe, 0x3611, 0xe7);
    ov9734_write_register(ViPipe, 0x3654, 0x10);
    ov9734_write_register(ViPipe, 0x3655, 0x77);
    ov9734_write_register(ViPipe, 0x3656, 0x77);
    ov9734_write_register(ViPipe, 0x3657, 0x07);
    ov9734_write_register(ViPipe, 0x3658, 0x22);
    ov9734_write_register(ViPipe, 0x3659, 0x22);
    ov9734_write_register(ViPipe, 0x365a, 0x02);
    ov9734_write_register(ViPipe, 0x3784, 0x05);
    ov9734_write_register(ViPipe, 0x3785, 0x55);
    ov9734_write_register(ViPipe, 0x37c0, 0x07);
    ov9734_write_register(ViPipe, 0x3800, 0x00);
    ov9734_write_register(ViPipe, 0x3801, 0x04);
    ov9734_write_register(ViPipe, 0x3802, 0x00);
    ov9734_write_register(ViPipe, 0x3803, 0x04);
    ov9734_write_register(ViPipe, 0x3804, 0x05);
    ov9734_write_register(ViPipe, 0x3805, 0x0b);
    ov9734_write_register(ViPipe, 0x3806, 0x02);
    ov9734_write_register(ViPipe, 0x3807, 0xdb);
    ov9734_write_register(ViPipe, 0x3808, 0x05);
    ov9734_write_register(ViPipe, 0x3809, 0x00);
    ov9734_write_register(ViPipe, 0x380a, 0x02);
    ov9734_write_register(ViPipe, 0x380b, 0xd0);
    ov9734_write_register(ViPipe, 0x380c, 0x05);
    ov9734_write_register(ViPipe, 0x380d, 0xc6);
    ov9734_write_register(ViPipe, 0x380e, 0x03);
    ov9734_write_register(ViPipe, 0x380f, 0x22);
    ov9734_write_register(ViPipe, 0x3810, 0x00);
    ov9734_write_register(ViPipe, 0x3811, 0x04);
    ov9734_write_register(ViPipe, 0x3812, 0x00);
    ov9734_write_register(ViPipe, 0x3813, 0x04);
    ov9734_write_register(ViPipe, 0x3816, 0x00);
    ov9734_write_register(ViPipe, 0x3817, 0x00);
    ov9734_write_register(ViPipe, 0x3818, 0x00);
    ov9734_write_register(ViPipe, 0x3819, 0x04);
    ov9734_write_register(ViPipe, 0x3820, 0x18);
    ov9734_write_register(ViPipe, 0x3821, 0x00);
    ov9734_write_register(ViPipe, 0x382c, 0x06);
    ov9734_write_register(ViPipe, 0x3500, 0x00);
    ov9734_write_register(ViPipe, 0x3501, 0x31);
    ov9734_write_register(ViPipe, 0x3502, 0x00);
    ov9734_write_register(ViPipe, 0x3503, 0x03);
    ov9734_write_register(ViPipe, 0x3504, 0x00);
    ov9734_write_register(ViPipe, 0x3505, 0x00);
    ov9734_write_register(ViPipe, 0x3509, 0x10);
    ov9734_write_register(ViPipe, 0x350a, 0x00);
    ov9734_write_register(ViPipe, 0x350b, 0x40);
    ov9734_write_register(ViPipe, 0x3d00, 0x00);
    ov9734_write_register(ViPipe, 0x3d01, 0x00);
    ov9734_write_register(ViPipe, 0x3d02, 0x00);
    ov9734_write_register(ViPipe, 0x3d03, 0x00);
    ov9734_write_register(ViPipe, 0x3d04, 0x00);
    ov9734_write_register(ViPipe, 0x3d05, 0x00);
    ov9734_write_register(ViPipe, 0x3d06, 0x00);
    ov9734_write_register(ViPipe, 0x3d07, 0x00);
    ov9734_write_register(ViPipe, 0x3d08, 0x00);
    ov9734_write_register(ViPipe, 0x3d09, 0x00);
    ov9734_write_register(ViPipe, 0x3d0a, 0x00);
    ov9734_write_register(ViPipe, 0x3d0b, 0x00);
    ov9734_write_register(ViPipe, 0x3d0c, 0x00);
    ov9734_write_register(ViPipe, 0x3d0d, 0x00);
    ov9734_write_register(ViPipe, 0x3d0e, 0x00);
    ov9734_write_register(ViPipe, 0x3d0f, 0x00);
    ov9734_write_register(ViPipe, 0x3d80, 0x00);
    ov9734_write_register(ViPipe, 0x3d81, 0x00);
    ov9734_write_register(ViPipe, 0x3d82, 0x38);
    ov9734_write_register(ViPipe, 0x3d83, 0xa4);
    ov9734_write_register(ViPipe, 0x3d84, 0x00);
    ov9734_write_register(ViPipe, 0x3d85, 0x00);
    ov9734_write_register(ViPipe, 0x3d86, 0x1f);
    ov9734_write_register(ViPipe, 0x3d87, 0x03);
    ov9734_write_register(ViPipe, 0x3d8b, 0x00);
    ov9734_write_register(ViPipe, 0x3d8f, 0x00);
    ov9734_write_register(ViPipe, 0x4001, 0xe0);
    ov9734_write_register(ViPipe, 0x4009, 0x0b);
    ov9734_write_register(ViPipe, 0x4300, 0x03);
    ov9734_write_register(ViPipe, 0x4301, 0xff);
    ov9734_write_register(ViPipe, 0x4304, 0x00);
    ov9734_write_register(ViPipe, 0x4305, 0x00);
    ov9734_write_register(ViPipe, 0x4309, 0x00);
    ov9734_write_register(ViPipe, 0x4600, 0x00);
    ov9734_write_register(ViPipe, 0x4601, 0x80);
    ov9734_write_register(ViPipe, 0x4800, 0x00);
    ov9734_write_register(ViPipe, 0x4805, 0x00);
    ov9734_write_register(ViPipe, 0x4821, 0x50);
    ov9734_write_register(ViPipe, 0x4823, 0x50);
    ov9734_write_register(ViPipe, 0x4837, 0x2d);
    ov9734_write_register(ViPipe, 0x4a00, 0x00);
    ov9734_write_register(ViPipe, 0x4f00, 0x80);
    ov9734_write_register(ViPipe, 0x4f01, 0x10);
    ov9734_write_register(ViPipe, 0x4f02, 0x00);
    ov9734_write_register(ViPipe, 0x4f03, 0x00);
    ov9734_write_register(ViPipe, 0x4f04, 0x00);
    ov9734_write_register(ViPipe, 0x4f05, 0x00);
    ov9734_write_register(ViPipe, 0x4f06, 0x00);
    ov9734_write_register(ViPipe, 0x4f07, 0x00);
    ov9734_write_register(ViPipe, 0x4f08, 0x00);
    ov9734_write_register(ViPipe, 0x4f09, 0x00);
    ov9734_write_register(ViPipe, 0x5000, 0x2f);
    ov9734_write_register(ViPipe, 0x500c, 0x00);
    ov9734_write_register(ViPipe, 0x500d, 0x00);
    ov9734_write_register(ViPipe, 0x500e, 0x00);
    ov9734_write_register(ViPipe, 0x500f, 0x00);
    ov9734_write_register(ViPipe, 0x5010, 0x00);
    ov9734_write_register(ViPipe, 0x5011, 0x00);
    ov9734_write_register(ViPipe, 0x5012, 0x00);
    ov9734_write_register(ViPipe, 0x5013, 0x00);
    ov9734_write_register(ViPipe, 0x5014, 0x00);
    ov9734_write_register(ViPipe, 0x5015, 0x00);
    ov9734_write_register(ViPipe, 0x5016, 0x00);
    ov9734_write_register(ViPipe, 0x5017, 0x00);
    ov9734_write_register(ViPipe, 0x5080, 0x00);
    ov9734_write_register(ViPipe, 0x5180, 0x01);
    ov9734_write_register(ViPipe, 0x5181, 0x00);
    ov9734_write_register(ViPipe, 0x5182, 0x01);
    ov9734_write_register(ViPipe, 0x5183, 0x00);
    ov9734_write_register(ViPipe, 0x5184, 0x01);
    ov9734_write_register(ViPipe, 0x5185, 0x00);
    ov9734_write_register(ViPipe, 0x5708, 0x06);
    ov9734_write_register(ViPipe, 0x380f, 0x2a);
#if 1
	ov9734_write_register(ViPipe, 0x5780, 0x3e),
	ov9734_write_register(ViPipe, 0x5781, 0x0f),
	ov9734_write_register(ViPipe, 0x5782, 0x44),
	ov9734_write_register(ViPipe, 0x5783, 0x02),
	ov9734_write_register(ViPipe, 0x5784, 0x01),
	ov9734_write_register(ViPipe, 0x5785, 0x01),
	ov9734_write_register(ViPipe, 0x5786, 0x00),
	ov9734_write_register(ViPipe, 0x5787, 0x04),
	ov9734_write_register(ViPipe, 0x5788, 0x02),
	ov9734_write_register(ViPipe, 0x5789, 0x0f),
	ov9734_write_register(ViPipe, 0x578a, 0xfd),
	ov9734_write_register(ViPipe, 0x578b, 0xf5),
	ov9734_write_register(ViPipe, 0x578c, 0xf5),
	ov9734_write_register(ViPipe, 0x578d, 0x03),
	ov9734_write_register(ViPipe, 0x578e, 0x08),
	ov9734_write_register(ViPipe, 0x578f, 0x0c),
	ov9734_write_register(ViPipe, 0x5790, 0x08),
	ov9734_write_register(ViPipe, 0x5791, 0x04),
	ov9734_write_register(ViPipe, 0x5792, 0x00),
	ov9734_write_register(ViPipe, 0x5793, 0x52),
	ov9734_write_register(ViPipe, 0x5794, 0xa3),

	ov9734_write_register(ViPipe, 0x5000, 0x3f),

	ov9734_write_register(ViPipe, 0x0100, 0x01),
#endif
    printf("===OV9734 1M30fps 10bit LINE Init OK!===\n");
    return;
}

void ov9734_DOL_2t1_1M30_10bit_init(VI_PIPE ViPipe)
{
    ov9734_write_register(ViPipe, 0x5780, 0x3e);
    ov9734_write_register(ViPipe, 0x5781, 0x0f);
    ov9734_write_register(ViPipe, 0x5782, 0x44);
    ov9734_write_register(ViPipe, 0x5783, 0x02);
    ov9734_write_register(ViPipe, 0x5784, 0x01);
    ov9734_write_register(ViPipe, 0x5785, 0x01);
    ov9734_write_register(ViPipe, 0x5786, 0x00);
    ov9734_write_register(ViPipe, 0x5787, 0x04);
    ov9734_write_register(ViPipe, 0x5788, 0x02);
    ov9734_write_register(ViPipe, 0x5789, 0x0f);
    ov9734_write_register(ViPipe, 0x578a, 0xfd);
    ov9734_write_register(ViPipe, 0x578b, 0xf5);
    ov9734_write_register(ViPipe, 0x578c, 0xf5);
    ov9734_write_register(ViPipe, 0x578d, 0x03);
    ov9734_write_register(ViPipe, 0x578e, 0x08);
    ov9734_write_register(ViPipe, 0x578f, 0x0c);
    ov9734_write_register(ViPipe, 0x5790, 0x08);
    ov9734_write_register(ViPipe, 0x5791, 0x04);
    ov9734_write_register(ViPipe, 0x5792, 0x00);
    ov9734_write_register(ViPipe, 0x5793, 0x52);
    ov9734_write_register(ViPipe, 0x5794, 0xa3);

    // Sensor registers used for normal image
#if 0
    ov9734_write_register(ViPipe, 0x304E, 0x00);
    ov9734_write_register(ViPipe, 0x304F, 0x00);

    ov9734_write_register(ViPipe, 0x3074, 0xB0);
    ov9734_write_register(ViPipe, 0x3075, 0x00);

    ov9734_write_register(ViPipe, 0x308E, 0xB1);
    ov9734_write_register(ViPipe, 0x308F, 0x00);

    ov9734_write_register(ViPipe, 0x30B6, 0x00);
    ov9734_write_register(ViPipe, 0x30B7, 0x00);

    ov9734_write_register(ViPipe, 0x3116, 0x00);
    ov9734_write_register(ViPipe, 0x3080, 0x02);
    ov9734_write_register(ViPipe, 0x309B, 0x02);
#endif

    ov9734_default_reg_init(ViPipe);
    delay_ms(1);
    //ov9734_write_register(ViPipe, 0x3000, 0x00);  // Standby Cancel
    //delay_ms(20);
    //ov9734_write_register(ViPipe, 0x3002, 0x00);
    //delay_ms(320);  // wait for image stablization

    printf("===OV9734 2M30fps 12bit DOL 2t1 Init OK!===\n");
    return;
}
