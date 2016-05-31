/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Nokia Fame board display support
 * Copyright (C) 2016, AngeloGioacchino Del Regno <kholk11@gmail.com>
 *
 * Based on mipi_orise.c
 * May contain portions of code (C) FIH Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_orise.h"
#include <mach/vreg.h>
#include <linux/gpio.h>

#define NOK_PANEL_RACE 0xe3

static struct dsi_buf orise_tx_buf;
static struct dsi_buf orise_rx_buf;
static int mipi_orise_lcd_init(void);

static struct msm_panel_common_pdata *mipi_orise_pdata = NULL;
static int display_initialize = 0;
static char gPanelModel = 0;

/* Nokia RACE WVGA DSI CMD Panel (480x800x24) START */
static char dsi_enter_sleep[2] = {0x10, 0x00};		/* DTYPE_DCS_WRITE */
static char dsi_exit_sleep[2] = {0x11, 0x00};		/* DTYPE_DCS_WRITE */

static char race_set_pixel_format[2] = {0x3A, 0x70};	/* DTYPE_DCS_WRITE1 */
static char race_ctrl_display[2] = {0x53, 0x2C};	/* DTYPE_DCS_WRITE1 */
static char race_lcm_brightness[2] = {0x51, 0x80};	/* DTYPE_DCS_WRITE1 */
static char race_cabc_on[2] = {0x55, 0x01};		/* DTYPE_DCS_WRITE1 */
static char race_cabc_off[2] = {0x55, 0x00};		/* DTYPE_DCS_WRITE1 */
static char race_disp_on[2] = {0x29, 0x00};		/* DTYPE_DCS_WRITE */
static char race_disp_off[2] = {0x28, 0x00};		/* DTYPE_DCS_WRITE */

static char race_unknown_cmd[2] = {0xff, 0x78};		/* DTYPE_GEN_LWRITE? */


static struct dsi_cmd_desc nok_panel_race_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(dsi_exit_sleep), dsi_exit_sleep},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(race_unknown_cmd), race_unknown_cmd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_set_pixel_format), race_set_pixel_format},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_ctrl_display), race_ctrl_display},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(race_lcm_brightness), race_lcm_brightness},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_cabc_on), race_cabc_on},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_set_pixel_format), race_set_pixel_format},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_disp_on), race_disp_on},
};

static struct dsi_cmd_desc nok_panel_race_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_cabc_off), race_cabc_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(race_disp_off), race_disp_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(dsi_enter_sleep), dsi_enter_sleep},
};

static struct dsi_cmd_desc race_backlight_cmd = {
	DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(race_lcm_brightness), race_lcm_brightness
};

/* Nokia RACE WVGA DSI CMD Panel (480x800x24) END   */

static char dsi_panelid_cmd_DA[2] = {0xDA, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc orise_ReadDA = {
	DTYPE_DCS_READ, 1, 0, 1, 20,
	sizeof(dsi_panelid_cmd_DA), dsi_panelid_cmd_DA
};

static int mipi_orise_manufacture_id(struct msm_fb_data_type *mfd)
{
	char retDA = 0;

	struct dsi_buf *tp = &orise_tx_buf;
	struct dsi_buf *rp = &orise_rx_buf;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	mipi_dsi_cmds_rx(mfd, tp, rp, &orise_ReadDA, 1);
	retDA = *((char *) rp->data);

	printk(KERN_ALERT "[DISPLAY] Panel ID <0x%02x>\n", retDA);
	return retDA;
}

static int mipi_orise_lcd_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_fb_data_type *mfd = NULL;
	pr_info("[DISPLAY] +%s\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (unlikely(display_initialize))
		return 0;

	if (unlikely(gPanelModel == 0)) {
		gPanelModel = mipi_orise_manufacture_id(mfd);
	}

	switch (gPanelModel) {
		case NOK_PANEL_RACE:
			rc = mipi_dsi_cmds_tx(&orise_tx_buf, nok_panel_race_on_cmds,
					ARRAY_SIZE(nok_panel_race_on_cmds));
			break;
		default:
			printk(KERN_ERR "[DISPLAY] illegal PID <0x%02x>\n", gPanelModel);
			break;
	}
    if(gPanelModel)
	{
		printk(KERN_ALERT "[DISPLAY] dsi commands done, rc <%d>, PID <0x%02x>\n",
			rc, gPanelModel);

		display_initialize = 1;
    }

	if (rc > 0)
		rc = 0;

	return rc;
}

static int mipi_orise_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = NULL;

	printk(KERN_ALERT "[DISPLAY] Enter %s\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (unlikely(!display_initialize))
		return 0;

	mipi_set_tx_power_mode(0);
	mipi_dsi_cmds_tx(&orise_tx_buf, nok_panel_race_off_cmds,
			ARRAY_SIZE(nok_panel_race_off_cmds));
	mipi_set_tx_power_mode(1);

	display_initialize = 0;

	return 0;
}

extern int fih_wled_set(int level);
static void mipi_orise_lcd_backlight(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	if (unlikely(!display_initialize))
		return;

	pr_info("BACKLIGHT: Setting level %d\n", mfd->bl_level);

	race_lcm_brightness[1] = mfd->bl_level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &race_backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq);
/*
	down(&mfd->dma->mutex);
	mipi_set_tx_power_mode(0);
	mipi_dsi_cmds_tx(&orise_tx_buf, race_backlight_cmd,
				ARRAY_SIZE(race_backlight_cmd));
	mipi_set_tx_power_mode(1);
	up(&mfd->dma->mutex);
*/
#if 0
	write_display_brightness[1] = BKL_PWM[mfd->bl_level];  /* Duty_Cycle */

	down(&mfd->dma->mutex);
	mipi_set_tx_power_mode(0);
	mipi_dsi_cmds_tx(&orise_tx_buf, orise_video_bkl_cmds,
			ARRAY_SIZE(orise_video_bkl_cmds));
	mipi_set_tx_power_mode(1);
	up(&mfd->dma->mutex);
#endif
}
static int __devinit mipi_orise_lcd_probe(struct platform_device *pdev)
{
	printk(KERN_ALERT "[DISPLAY] Enter %s\n", __func__);

	if (pdev->id == 0) {
		mipi_orise_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_orise_lcd_probe,
	.driver = {
		.name   = "mipi_orise",
	},
};

static struct msm_fb_panel_data orise_panel_data = {
	.on		= mipi_orise_lcd_on,
	.off	= mipi_orise_lcd_off,
	.set_backlight = mipi_orise_lcd_backlight,
};

static int ch_used[3];

int mipi_orise_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret = 0;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_orise_lcd_init();
	if (ret) {
		pr_err("mipi_orise_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_orise", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	orise_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &orise_panel_data,
		sizeof(orise_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_orise_lcd_init(void)
{
	mipi_dsi_buf_alloc(&orise_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&orise_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
