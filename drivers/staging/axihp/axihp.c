/*
 * Copyright (C) 2017 Red Pitaya.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */
 


#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/clk/zynq.h>

struct axihp{
	struct regmap *slcr_regmap;
	struct device *dev;
	
};

#define to_axihp_data(p)		\
	container_of((p), struct axihp, dev)

static struct regmap *slcr_regmap;

/* Match table for of_platform binding */
static const struct of_device_id axihp_of_match[] = {
	{ .compatible = "axihp",},
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, axihp_of_match);

#define SLCR_UNLOCK_MAGIC		0xDF0D
#define SLCR_UNLOCK_OFFSET		0x8   /* SCLR unlock register */
#define SLCR_PS_RST_CTRL_OFFSET		0x200 /* PS Software Reset Control */
#define SLCR_FPGA_RST_CTRL_OFFSET	0x240 /* FPGA Software Reset Control */

#define SLCR 0xF8000000
#define AXI_HP0 0XF8008000
#define AXI_HP1 0XF8009000
#define AXI_HP2 0XF800A000
#define AXI_HP3 0XF800B000
#define AXI_HP0W 0XF8008014
#define AXI_HP1W 0XF8009014
#define AXI_HP2W 0XF800A014
#define AXI_HP3W 0XF800B014
#define LOCK	0XF8000004
#define AXI_HP_X_OFFSET 0x14



static int axihp_probe(struct platform_device *pdev)
{
	struct axihp *ahp;
	int ret;
	u32 /*n32BitEn=0,*/ reg=0, raw=0;

	slcr_regmap = syscon_regmap_lookup_by_phandle(pdev,"syscon");
	
	if(!slcr_regmap) return 0;
	
	ahp = devm_kzalloc(&pdev->dev, sizeof(*ahp), GFP_KERNEL);
	if (!ahp)
		return -ENOMEM;
		
	ahp->slcr_regmap=slcr_regmap;
	
	ahp->dev = &pdev->dev;

	platform_set_drvdata(pdev, ahp);
	
//get reg property out of devicetree 	
	ret = of_property_read_u32(pdev->dev.of_node, "reg", &reg);
	if (ret) {
		dev_err(&pdev->dev, "No AXI_HP reg in device tree.\n");
		return ret;
	}
	
//get raw value out of device tree 
	ret = of_property_read_u32(pdev->dev.of_node, "raw", &raw);
	if (ret) {
		dev_err(&pdev->dev, "No AXI_HP raw value in device tree.\n");
		return ret;

	} 

//some day you can get booleann for n32BitEn out of device tree but 
//currently device tree does not support boolean and we will have to do
//with  raw value and not deal with every specific read modify write 
//for every bit in this register, altho integar value for 32bit enable would work as well

//	ret = of_property_read_u32(pdev->dev.of_node, "n32BitEn", &n32BitEn);
//	if (ret) {
//		dev_err(&pdev->dev, "No AXI_HP width in device tree.\n");
//		return ret;
//	}

	//First we will have to unlock  SLCR settings by writing UNLOCK_KEY = 0XDF0D to 0XF8000008
	regmap_write(ahp->slcr_regmap, 0x8, 0xDF0D);
		
	//level shifter comes next but I do not think that this is neccecary sice that is alredy set with fsbl
	//EMIT_MASKWRITE(0XF8000900, 0x0000000FU ,0x0000000FU)
	
	//then set appropriate bit to 0 for reseting apropraite bus interface
	//this might be done by reset-zynq driver in some way but I have not figred out specific interrface for that or just hard form syscom slcr driver...
	
	switch(reg){
		case AXI_HP0:
		case AXI_HP0W:{
			//FPGA0_OUT_RST bit 0 of 0xf8000240
			regmap_update_bits(ahp->slcr_regmap, SLCR_FPGA_RST_CTRL_OFFSET, 1,0);
		
		}break;
		case AXI_HP1:
		case AXI_HP1W:{
			//FPGA1_OUT_RST bit 1 of 0xf8000240	
			regmap_update_bits(ahp->slcr_regmap, SLCR_FPGA_RST_CTRL_OFFSET, 2,0);
		
		}break;
		case AXI_HP2:
		case AXI_HP2W:{	
			//FPGA2_OUT_RST bit 2 of 0xf8000240
			regmap_update_bits(ahp->slcr_regmap, SLCR_FPGA_RST_CTRL_OFFSET, 4,0);
		
		}break;
		case AXI_HP3:
		case AXI_HP3W:{
			//FPGA3_OUT_RST bit 3 of 0xf8000240
			regmap_update_bits(ahp->slcr_regmap, SLCR_FPGA_RST_CTRL_OFFSET, 8,0);
		
		}break;
	}
	// write raw walue to specified register offseted by base register of slcr
	regmap_write(ahp->slcr_regmap, reg-SLCR, raw);

	
	// then lock it back with writing 0x0000767B to 0XF8000004 
	regmap_write(ahp->slcr_regmap, 0x4, 0x767B);

	dev_info(&pdev->dev, "AXI HP bus enabled and set to: %u.\n", raw);

	return 0;
}

static int axihp_remove(struct platform_device *pdev)
{
	struct axihp *st = platform_get_drvdata(pdev);

	//here disable specific bus just reverse it all
	//or do nothing hm what is better? 
	 devm_kfree(pdev,st);

	return 0;
}

static struct platform_driver axihp_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = axihp_of_match,
	},
	.probe		= axihp_probe,
	.remove		= axihp_remove,
};

module_platform_driver(axihp_driver);

MODULE_AUTHOR("Uros Golob <uros.golob@redpitaya.com>");
MODULE_DESCRIPTION("axihp config 32 or 64bit width");
MODULE_LICENSE("GPL v2");
