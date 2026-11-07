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

const unsigned char ov6946_i2c_addr = 0x6c; /* I2C Address of Ov6946 */
const unsigned int ov6946_addr_byte = 2;
const unsigned int ov6946_data_byte = 1;
static int g_fd[ISP_MAX_PIPE_NUM] = { [0 ...(ISP_MAX_PIPE_NUM - 1)] = -1 };

extern ISP_SNS_STATE_S *g_pastOv6946[ISP_MAX_PIPE_NUM];

extern ISP_SNS_COMMBUS_U g_aunOv6946BusInfo[];
#if 0
static ISP_SNS_COMMBUS_U g_aunOv6946BusInfo[ISP_MAX_PIPE_NUM] = {
    [0] = { .s8I2cDev = 2 }, //确定pipe0的i2c总线号
    [1] = { .s8I2cDev = -1 }, 
    [2] = { .s8I2cDev = -1 }, 
    [3] = { .s8I2cDev = 2 }, 
    [4 ... ISP_MAX_PIPE_NUM - 1] = { .s8I2cDev = -1 }
};
#endif
#define OV6946_1M_30FPS_LINEAR_MODE  (1)
#define OV6946_1M_30FPS_2t1_DOL_MODE (2)

int ov6946_i2c_init(VI_PIPE ViPipe)
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
    u8DevNum = g_aunOv6946BusInfo[ViPipe].s8I2cDev;

    snprintf(acDevFile, sizeof(acDevFile), "/dev/i2c-%u", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0) {
        ISP_ERR_TRACE("Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (ov6946_i2c_addr >> 1));
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_SLAVE_FORCE error!\n");
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return ret;
    }
#endif

    return HI_SUCCESS;
}

int ov6946_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0) {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int ov6946_read_register(VI_PIPE ViPipe, int addr)
{
    return HI_SUCCESS;
}
int ov6946_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (g_fd[ViPipe] < 0) {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = ov6946_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = ov6946_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = ov6946_data_byte;

    ret = ioctl(g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data);

    if (ret) {
        ISP_ERR_TRACE("GPIO-I2C write faild!\n");
        return ret;
    }
#else
    int idx = 0;
    int ret;
    char buf[8];
    if (ov6946_addr_byte == 2) {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    } else {
        buf[idx] = addr & 0xff;
        idx++;
    }

    if (ov6946_data_byte == 2) {
        buf[idx] = (data >> 8) & 0xff;
        idx++;
        buf[idx] = data & 0xff;
        idx++;
    } else {
        buf[idx] = data & 0xff;
        idx++;
    }

    ret = write(g_fd[ViPipe], buf, (ov6946_addr_byte + ov6946_data_byte));
    if (ret < 0) {
        ISP_ERR_TRACE("I2C_WRITE DATA error!\n");
        return HI_FAILURE;
    }

#endif
    return HI_SUCCESS;
}

void ov6946_standby(VI_PIPE ViPipe)
{
    return;
}

void ov6946_restart(VI_PIPE ViPipe)
{
    return;
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void ov6946_linear_1M30_10bit_init(VI_PIPE ViPipe);
void ov6946_DOL_2t1_1M30_10bit_init(VI_PIPE ViPipe);

void ov6946_default_reg_init(VI_PIPE ViPipe)
{
    HI_U32 i = 0;
    for (i = 0; i < g_pastOv6946[ViPipe]->astRegsInfo[0].u32RegNum; i++) {
        ov6946_write_register(ViPipe, g_pastOv6946[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_pastOv6946[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
    }
}
void ov6946_init(VI_PIPE ViPipe)
{
    HI_U8 u8ImgMode;
    HI_BOOL bInit;

    bInit = g_pastOv6946[ViPipe]->bInit;
    u8ImgMode = g_pastOv6946[ViPipe]->u8ImgMode;
    /* 1. sensor i2c init */
    ov6946_i2c_init(ViPipe);

    if (bInit == HI_FALSE) {
        /* 2.  sensor registers init */
        if (u8ImgMode == OV6946_1M_30FPS_LINEAR_MODE) { /* 1K@30fps Linear */
            ov6946_linear_1M30_10bit_init(ViPipe);
        } else if (u8ImgMode == OV6946_1M_30FPS_2t1_DOL_MODE) { /* 4K@30fps DOL2 */
            ov6946_DOL_2t1_1M30_10bit_init(ViPipe);
        }
    } else {

    /* When sensor switch mode(linear<->WDR or resolution), config different registers(if possible) */
        /* 2.  sensor registers init */
        if (u8ImgMode == OV6946_1M_30FPS_LINEAR_MODE) { /* 4K@30fps Linear */
            ov6946_linear_1M30_10bit_init(ViPipe);
        } else if (u8ImgMode == OV6946_1M_30FPS_2t1_DOL_MODE) { /* 4K@30fps DOL2 */
            ov6946_DOL_2t1_1M30_10bit_init(ViPipe);
        }
    }

    g_pastOv6946[ViPipe]->bInit = HI_TRUE;

    return;
}

void ov6946_exit(VI_PIPE ViPipe)
{
    ov6946_i2c_exit(ViPipe);

    return;
}

void ov6946_linear_1M30_10bit_init(VI_PIPE ViPipe)
{
#if  1 
    ov6946_write_register(ViPipe, 0x0103, 0x01);
    ov6946_write_register(ViPipe, 0x3025, 0x02);
    ov6946_write_register(ViPipe, 0x3026, 0x1c);
    ov6946_write_register(ViPipe, 0x3205, 0x01);
    ov6946_write_register(ViPipe, 0x0100, 0x01);
    ov6946_write_register(ViPipe, 0x3024, 0x06);
    ov6946_write_register(ViPipe, 0x3209, 0x03);
    ov6946_write_register(ViPipe, 0x3701, 0x40);
    ov6946_write_register(ViPipe, 0x3702, 0x4c);
    ov6946_write_register(ViPipe, 0x3003, 0x32);
    ov6946_write_register(ViPipe, 0x3004, 0x01);
    ov6946_write_register(ViPipe, 0x3204, 0x87);
    ov6946_write_register(ViPipe, 0x3028, 0xb0);
    ov6946_write_register(ViPipe, 0x3027, 0x20);
    ov6946_write_register(ViPipe, 0x5a40, 0x05);
    ov6946_write_register(ViPipe, 0x3a19, 0x3e);
    ov6946_write_register(ViPipe, 0x5a00, 0x04);
    ov6946_write_register(ViPipe, 0x4009, 0x18);
    ov6946_write_register(ViPipe, 0x4005, 0x1a);
    ov6946_write_register(ViPipe, 0x3020, 0x09);
    ov6946_write_register(ViPipe, 0x3021, 0x30);
    ov6946_write_register(ViPipe, 0x3022, 0x1f);
    ov6946_write_register(ViPipe, 0x3023, 0x40);
    ov6946_write_register(ViPipe, 0x3024, 0x14);
    ov6946_write_register(ViPipe, 0x3a0f, 0x4c);
    ov6946_write_register(ViPipe, 0x3a10, 0x44);
    ov6946_write_register(ViPipe, 0x3a1b, 0x52);
    ov6946_write_register(ViPipe, 0x3a1e, 0x3c);
    ov6946_write_register(ViPipe, 0x3a05, 0x28);
    ov6946_write_register(ViPipe, 0x3203, 0x03);
    ov6946_write_register(ViPipe, 0x4052, 0x01);
    ov6946_write_register(ViPipe, 0x302a, 0x01);
    ov6946_write_register(ViPipe, 0x4708, 0x03);
    ov6946_write_register(ViPipe, 0x4706, 0x20);
    ov6946_write_register(ViPipe, 0x5680, 0x00);
    ov6946_write_register(ViPipe, 0x5681, 0x50);
    ov6946_write_register(ViPipe, 0x5682, 0x0e);
    ov6946_write_register(ViPipe, 0x5683, 0x20);
    ov6946_write_register(ViPipe, 0x5684, 0x0f);
    ov6946_write_register(ViPipe, 0x5685, 0x00);
    ov6946_write_register(ViPipe, 0x5686, 0x0b);
    ov6946_write_register(ViPipe, 0x5687, 0x00);
    ov6946_write_register(ViPipe, 0x5688, 0x01);
    ov6946_write_register(ViPipe, 0x5689, 0x11);
    ov6946_write_register(ViPipe, 0x568a, 0x11);
    ov6946_write_register(ViPipe, 0x568b, 0x11);
    ov6946_write_register(ViPipe, 0x568c, 0x11);
    ov6946_write_register(ViPipe, 0x568d, 0x11);
    ov6946_write_register(ViPipe, 0x568e, 0x11);
    ov6946_write_register(ViPipe, 0x568f, 0x11);
    ov6946_write_register(ViPipe, 0x5690, 0x01);
    ov6946_write_register(ViPipe, 0x0100, 0x00);
    ov6946_write_register(ViPipe, 0x0100, 0x01);
    ov6946_write_register(ViPipe, 0x5007, 0x03);
    ov6946_write_register(ViPipe, 0x3701, 0x41);
    //ov6946_write_register(ViPipe, 0x5001, 0x92);
    //ov6946_write_register(ViPipe, 0x4709, 0x02);
    /*white balanc*/    
    ov6946_write_register(ViPipe, 0x5186, 0x12);
    ov6946_write_register(ViPipe, 0x5187, 0x0f);
    ov6946_write_register(ViPipe, 0x5188, 0x04);//05偏绿
    ov6946_write_register(ViPipe, 0x5189, 0x00);
    ov6946_write_register(ViPipe, 0x518a, 0x04);
    ov6946_write_register(ViPipe, 0x518b, 0x30);

    ov6946_write_register(ViPipe, 0x3205, 0x00);
    ov6946_write_register(ViPipe, 0x3500, 0x00);
    ov6946_write_register(ViPipe, 0x3501, 0x31);
    ov6946_write_register(ViPipe, 0x3502, 0x00);
    ov6946_write_register(ViPipe, 0x3503, 0x03);
    ov6946_write_register(ViPipe, 0x3a26, 0x1c);
    ov6946_write_register(ViPipe, 0x3714, 0x00);
    ov6946_write_register(ViPipe, 0x3715, 0x00);
    ov6946_write_register(ViPipe, 0x4706, 0x20);

    //ov6946_write_register(ViPipe, 0x3a19, 0x7e);
    //ov6946_write_register(ViPipe, 0x3a1b, 0x28);
    //ov6946_write_register(ViPipe, 0x3a0f, 0x26);
    //ov6946_write_register(ViPipe, 0x3a10, 0x18);
    //ov6946_write_register(ViPipe, 0x3a1e, 0x16);
    //ov6946_write_register(ViPipe, 0x3a05, 0x70);

#endif
#if 0      //ov官方
	ov6946_write_register(ViPipe,0x0103, 0x01);

    ov6946_write_register(ViPipe,0x3025, 0x02);
    ov6946_write_register(ViPipe,0x3026, 0x1c);
    ov6946_write_register(ViPipe,0x3003, 0x32);
    ov6946_write_register(ViPipe,0x3004, 0x01);

    ov6946_write_register(ViPipe,0x3205, 0x06);
    ov6946_write_register(ViPipe,0x0100, 0x01);
    ov6946_write_register(ViPipe,0x3024, 0x04);
    ov6946_write_register(ViPipe,0x3209, 0x03);
    ov6946_write_register(ViPipe,0x3701, 0x40);
    ov6946_write_register(ViPipe,0x3702, 0x4c);
    ov6946_write_register(ViPipe,0x3204, 0x87);
    ov6946_write_register(ViPipe,0x3028, 0xb0);
    ov6946_write_register(ViPipe,0x3027, 0x20);
    ov6946_write_register(ViPipe,0x5a40, 0x05);
    ov6946_write_register(ViPipe,0x3a19, 0x3e);
    ov6946_write_register(ViPipe,0x5a00, 0x04);
    ov6946_write_register(ViPipe,0x4009, 0x18);
    ov6946_write_register(ViPipe,0x4005, 0x1a);
    ov6946_write_register(ViPipe,0x3020, 0x09);
    ov6946_write_register(ViPipe,0x3024, 0x00);
    ov6946_write_register(ViPipe,0x3a0f, 0x58);
    ov6946_write_register(ViPipe,0x3a10, 0x48);
    ov6946_write_register(ViPipe,0x3a1b, 0x64);
    ov6946_write_register(ViPipe,0x3a1e, 0x40);
    ov6946_write_register(ViPipe,0x4052, 0x01);
    ov6946_write_register(ViPipe,0x302a, 0x01);
    delay_ms(33);
    ov6946_write_register(ViPipe,0x3205, 0x00);

#endif
    printf("===OV6946 1M30fps 10bit LINE Init OK!===\n");
    return;
}

void ov6946_DOL_2t1_1M30_10bit_init(VI_PIPE ViPipe)
{
    ov6946_write_register(ViPipe, 0x5780, 0x3e);
    ov6946_write_register(ViPipe, 0x5781, 0x0f);
    ov6946_write_register(ViPipe, 0x5782, 0x44);
    ov6946_write_register(ViPipe, 0x5783, 0x02);
    ov6946_write_register(ViPipe, 0x5784, 0x01);
    ov6946_write_register(ViPipe, 0x5785, 0x01);
    ov6946_write_register(ViPipe, 0x5786, 0x00);
    ov6946_write_register(ViPipe, 0x5787, 0x04);
    ov6946_write_register(ViPipe, 0x5788, 0x02);
    ov6946_write_register(ViPipe, 0x5789, 0x0f);
    ov6946_write_register(ViPipe, 0x578a, 0xfd);
    ov6946_write_register(ViPipe, 0x578b, 0xf5);
    ov6946_write_register(ViPipe, 0x578c, 0xf5);
    ov6946_write_register(ViPipe, 0x578d, 0x03);
    ov6946_write_register(ViPipe, 0x578e, 0x08);
    ov6946_write_register(ViPipe, 0x578f, 0x0c);
    ov6946_write_register(ViPipe, 0x5790, 0x08);
    ov6946_write_register(ViPipe, 0x5791, 0x04);
    ov6946_write_register(ViPipe, 0x5792, 0x00);
    ov6946_write_register(ViPipe, 0x5793, 0x52);
    ov6946_write_register(ViPipe, 0x5794, 0xa3);
    //ov6946_write_register(ViPipe, 0x4709, 0x03);
    // Sensor registers used for normal image
#if 0
    ov6946_write_register(ViPipe, 0x304E, 0x00);
    ov6946_write_register(ViPipe, 0x304F, 0x00);

    ov6946_write_register(ViPipe, 0x3074, 0xB0);
    ov6946_write_register(ViPipe, 0x3075, 0x00);

    ov6946_write_register(ViPipe, 0x308E, 0xB1);
    ov6946_write_register(ViPipe, 0x308F, 0x00);

    ov6946_write_register(ViPipe, 0x30B6, 0x00);
    ov6946_write_register(ViPipe, 0x30B7, 0x00);

    ov6946_write_register(ViPipe, 0x3116, 0x00);
    ov6946_write_register(ViPipe, 0x3080, 0x02);
    ov6946_write_register(ViPipe, 0x309B, 0x02);
#endif

    ov6946_default_reg_init(ViPipe);
    delay_ms(1);
    //ov6946_write_register(ViPipe, 0x3000, 0x00);  // Standby Cancel
    //delay_ms(20);
    //ov6946_write_register(ViPipe, 0x3002, 0x00);
    //delay_ms(320);  // wait for image stablization

    printf("===OV6946 2M30fps 12bit DOL 2t1 Init OK!===\n");
    return;
}
