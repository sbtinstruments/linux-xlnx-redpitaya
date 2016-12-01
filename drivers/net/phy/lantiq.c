/*
 * Copyright (C) 2012 Daniel Schwierzeck <daniel.schwierzeck@xxxxxxxxxxxxxx>
 * Copyright (C) 2016 Hauke Mehrtens <hauke@xxxxxxxxxx>
 *
 * PHY MDIO register interface:
 * Copyright (C) 2013 Ales Bardorfer <ales.bardorfer@redpitaya.com>
 *
 * LED blinking configuration:
 * Copyright (C) 2013 Tomaz Beltram <tomaz.beltram@i-tech.si>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/sysfs.h>

#define LANTIQ_MDIO_IMASK		0x19	/* interrupt mask */
#define LANTIQ_MDIO_ISTAT		0x1A	/* interrupt status */

#define MII_MMDCTRL		0x0d
#define MII_MMDDATA		0x0e

#define LANTIQ_MDIO_INIT_WOL		BIT(15)	/* Wake-On-LAN */
#define LANTIQ_MDIO_INIT_MSRE		BIT(14)
#define LANTIQ_MDIO_INIT_NPRX		BIT(13)
#define LANTIQ_MDIO_INIT_NPTX		BIT(12)
#define LANTIQ_MDIO_INIT_ANE		BIT(11)	/* Auto-Neg error */
#define LANTIQ_MDIO_INIT_ANC		BIT(10)	/* Auto-Neg complete */
#define LANTIQ_MDIO_INIT_ADSC		BIT(5)	/* Link auto-downspeed detect */
#define LANTIQ_MDIO_INIT_MPIPC		BIT(4)
#define LANTIQ_MDIO_INIT_MDIXC		BIT(3)
#define LANTIQ_MDIO_INIT_DXMC		BIT(2)	/* Duplex mode change */
#define LANTIQ_MDIO_INIT_LSPC		BIT(1)	/* Link speed change */
#define LANTIQ_MDIO_INIT_LSTC		BIT(0)	/* Link state change */
#define LANTIQ_MDIO_INIT_MASK		(LANTIQ_MDIO_INIT_LSTC | \
					 LANTIQ_MDIO_INIT_ADSC)

#define ADVERTISED_MPD			BIT(10)	/* Multi-port device */

/* LED Configuration */
#define LANTIQ_MMD_LEDCH			0x01E0
/* Inverse of SCAN Function */
#define  LANTIQ_MMD_LEDCH_NACS_NONE		0x0000
#define  LANTIQ_MMD_LEDCH_NACS_LINK		0x0001
#define  LANTIQ_MMD_LEDCH_NACS_PDOWN		0x0002
#define  LANTIQ_MMD_LEDCH_NACS_EEE		0x0003
#define  LANTIQ_MMD_LEDCH_NACS_ANEG		0x0004
#define  LANTIQ_MMD_LEDCH_NACS_ABIST		0x0005
#define  LANTIQ_MMD_LEDCH_NACS_CDIAG		0x0006
#define  LANTIQ_MMD_LEDCH_NACS_TEST		0x0007
/* Slow Blink Frequency */
#define  LANTIQ_MMD_LEDCH_SBF_F02HZ		0x0000
#define  LANTIQ_MMD_LEDCH_SBF_F04HZ		0x0010
#define  LANTIQ_MMD_LEDCH_SBF_F08HZ		0x0020
#define  LANTIQ_MMD_LEDCH_SBF_F16HZ		0x0030
/* Fast Blink Frequency */
#define  LANTIQ_MMD_LEDCH_FBF_F02HZ		0x0000
#define  LANTIQ_MMD_LEDCH_FBF_F04HZ		0x0040
#define  LANTIQ_MMD_LEDCH_FBF_F08HZ		0x0080
#define  LANTIQ_MMD_LEDCH_FBF_F16HZ		0x00C0
/* LED Configuration */
#define LANTIQ_MMD_LEDCL			0x01E1
/* Complex Blinking Configuration */
#define  LANTIQ_MMD_LEDCH_CBLINK_NONE		0x0000
#define  LANTIQ_MMD_LEDCH_CBLINK_LINK		0x0001
#define  LANTIQ_MMD_LEDCH_CBLINK_PDOWN		0x0002
#define  LANTIQ_MMD_LEDCH_CBLINK_EEE		0x0003
#define  LANTIQ_MMD_LEDCH_CBLINK_ANEG		0x0004
#define  LANTIQ_MMD_LEDCH_CBLINK_ABIST		0x0005
#define  LANTIQ_MMD_LEDCH_CBLINK_CDIAG		0x0006
#define  LANTIQ_MMD_LEDCH_CBLINK_TEST		0x0007
/* Complex SCAN Configuration */
#define  LANTIQ_MMD_LEDCH_SCAN_NONE		0x0000
#define  LANTIQ_MMD_LEDCH_SCAN_LINK		0x0010
#define  LANTIQ_MMD_LEDCH_SCAN_PDOWN		0x0020
#define  LANTIQ_MMD_LEDCH_SCAN_EEE		0x0030
#define  LANTIQ_MMD_LEDCH_SCAN_ANEG		0x0040
#define  LANTIQ_MMD_LEDCH_SCAN_ABIST		0x0050
#define  LANTIQ_MMD_LEDCH_SCAN_CDIAG		0x0060
#define  LANTIQ_MMD_LEDCH_SCAN_TEST		0x0070
/* Configuration for LED Pin x */
#define LANTIQ_MMD_LED0H			0x01E2
/* Fast Blinking Configuration */
#define  LANTIQ_MMD_LEDxH_BLINKF_MASK		0x000F
#define  LANTIQ_MMD_LEDxH_BLINKF_NONE		0x0000
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK10		0x0001
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK100	0x0002
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK10X	0x0003
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK1000	0x0004
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK10_0	0x0005
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK100X	0x0006
#define  LANTIQ_MMD_LEDxH_BLINKF_LINK10XX	0x0007
#define  LANTIQ_MMD_LEDxH_BLINKF_PDOWN		0x0008
#define  LANTIQ_MMD_LEDxH_BLINKF_EEE		0x0009
#define  LANTIQ_MMD_LEDxH_BLINKF_ANEG		0x000A
#define  LANTIQ_MMD_LEDxH_BLINKF_ABIST		0x000B
#define  LANTIQ_MMD_LEDxH_BLINKF_CDIAG		0x000C
/* Constant On Configuration */
#define  LANTIQ_MMD_LEDxH_CON_MASK		0x00F0
#define  LANTIQ_MMD_LEDxH_CON_NONE		0x0000
#define  LANTIQ_MMD_LEDxH_CON_LINK10		0x0010
#define  LANTIQ_MMD_LEDxH_CON_LINK100		0x0020
#define  LANTIQ_MMD_LEDxH_CON_LINK10X		0x0030
#define  LANTIQ_MMD_LEDxH_CON_LINK1000		0x0040
#define  LANTIQ_MMD_LEDxH_CON_LINK10_0		0x0050
#define  LANTIQ_MMD_LEDxH_CON_LINK100X		0x0060
#define  LANTIQ_MMD_LEDxH_CON_LINK10XX		0x0070
#define  LANTIQ_MMD_LEDxH_CON_PDOWN		0x0080
#define  LANTIQ_MMD_LEDxH_CON_EEE		0x0090
#define  LANTIQ_MMD_LEDxH_CON_ANEG		0x00A0
#define  LANTIQ_MMD_LEDxH_CON_ABIST		0x00B0
#define  LANTIQ_MMD_LEDxH_CON_CDIAG		0x00C0
#define  LANTIQ_MMD_LEDxH_CON_COPPER		0x00D0
#define  LANTIQ_MMD_LEDxH_CON_FIBER		0x00E0
/* Configuration for LED Pin x */
#define LANTIQ_MMD_LED0L			0x01E3
/* Pulsing Configuration */
#define  LANTIQ_MMD_LEDxL_PULSE_MASK		0x000F
#define  LANTIQ_MMD_LEDxL_PULSE_NONE		0x0000
#define  LANTIQ_MMD_LEDxL_PULSE_TXACT		0x0001
#define  LANTIQ_MMD_LEDxL_PULSE_RXACT		0x0002
#define  LANTIQ_MMD_LEDxL_PULSE_COL		0x0004
/* Slow Blinking Configuration */
#define  LANTIQ_MMD_LEDxL_BLINKS_MASK		0x00F0
#define  LANTIQ_MMD_LEDxL_BLINKS_NONE		0x0000
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK10		0x0010
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK100	0x0020
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK10X	0x0030
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK1000	0x0040
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK10_0	0x0050
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK100X	0x0060
#define  LANTIQ_MMD_LEDxL_BLINKS_LINK10XX	0x0070
#define  LANTIQ_MMD_LEDxL_BLINKS_PDOWN		0x0080
#define  LANTIQ_MMD_LEDxL_BLINKS_EEE		0x0090
#define  LANTIQ_MMD_LEDxL_BLINKS_ANEG		0x00A0
#define  LANTIQ_MMD_LEDxL_BLINKS_ABIST		0x00B0
#define  LANTIQ_MMD_LEDxL_BLINKS_CDIAG		0x00C0
#define LANTIQ_MMD_LED1H			0x01E4
#define LANTIQ_MMD_LED1L			0x01E5
#define LANTIQ_MMD_LED2H			0x01E6
#define LANTIQ_MMD_LED2L			0x01E7
#define LANTIQ_MMD_LED3H			0x01E8
#define LANTIQ_MMD_LED3L			0x01E9

#define PHY_ID_PHY11G_1_3			0x030260D1
#define PHY_ID_PHY22F_1_3			0x030260E1
#define PHY_ID_PHY11G_1_4			0xD565A400
#define PHY_ID_PHY22F_1_4			0xD565A410
#define PHY_ID_PHY11G_1_5			0xD565A401
#define PHY_ID_PHY22F_1_5			0xD565A411
#define PHY_ID_PHY11G_VR9			0xD565A409
#define PHY_ID_PHY22F_VR9			0xD565A419


#define MMD_DEVAD		0x1f
#define MMD_ACTYPE_SHIFT	14
#define MMD_ACTYPE_ADDRESS	(0 << MMD_ACTYPE_SHIFT)
#define MMD_ACTYPE_DATA		(1 << MMD_ACTYPE_SHIFT)
#define MMD_ACTYPE_DATA_PI	(2 << MMD_ACTYPE_SHIFT)
#define MMD_ACTYPE_DATA_PIWR	(3 << MMD_ACTYPE_SHIFT)

#define MDIO_ADDR_MAX           0x1f
#define MDIO_VAL_MAX            0xffff

/* MMD LED configuration registers */
#define INT_LED0H 0x1e2
#define INT_LED0L 0x1e3
#define INT_LED1H 0x1e4
#define INT_LED1L 0x1e5

/* LED function bits */
#define INT_LED_TX 0x01
#define INT_LED_RX 0x02
#define INT_LED_LINK10 0x1
#define INT_LED_LINK100 0x2
#define INT_LED_LINK1000 0x4

/* Simple "address-value" interface to PHY MDIO registers */
static ssize_t show_mdio_addr_attr(struct device *dev,
                                   struct device_attribute *devattr,
                                   char *buf);
static ssize_t set_mdio_addr_attr (struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count);
static ssize_t show_mdio_val_attr (struct device *dev,
                                   struct device_attribute *devattr,
                                   char *buf);
static ssize_t set_mdio_val_attr  (struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count);

static DEVICE_ATTR(mdio_addr, S_IWUSR | S_IRUGO,
                   show_mdio_addr_attr, set_mdio_addr_attr);
static DEVICE_ATTR(mdio_val, S_IWUSR | S_IRUGO,
                   show_mdio_val_attr, set_mdio_val_attr);

struct attribute_group attrs;

static struct attribute *phy11g_attrs[] = {
	&dev_attr_mdio_addr.attr,
	&dev_attr_mdio_val.attr,
	NULL
};

// TODO: Make these part of device insted of globals.
unsigned short g_addr;
struct phy_device* g_phydev = 0;

/** 
 * Get MDIO register address of the PHY to act upon by "mdio_val" sysfs entry.
 */
static ssize_t show_mdio_addr_attr(struct device *dev,
                                   struct device_attribute *devattr,
                                   char *buf)
{
        return sprintf(buf, "0x%02x\n", g_addr);
}

/** 
 * Set MDIO register address of the PHY to act upon by "mdio_val" sysfs entry.
 */
static ssize_t set_mdio_addr_attr(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	unsigned long addr;
	int err;

	err = kstrtoul(buf, 0, &addr);
	if (err)
		return err;

	if (addr > MDIO_ADDR_MAX) {
		dev_err(dev,
			"MDIO address 0x%08lx out of range [0x0 - 0x%02x].\n",
                        addr, MDIO_ADDR_MAX);
		return -EINVAL;
	}
        
        g_addr = addr;

	return count;
}

/** 
 * Get value of MDIO register, pointed to by "mdio_addr" sysfs entry.
 */
static ssize_t show_mdio_val_attr(struct device *dev,
                                  struct device_attribute *devattr,
                                  char *buf)
{
    unsigned short val;

    val = phy_read(g_phydev, g_addr);

    return sprintf(buf, "0x%04x\n", val);
}

/** 
 * Set value of MDIO register, pointed to by "mdio_addr" sysfs entry.
 */
static ssize_t set_mdio_val_attr(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	if (val > MDIO_VAL_MAX) {
		dev_err(dev,
			"requested MDIO value 0x%08lx out of range.\n",
                        val);
		return -EINVAL;
	}

        err = phy_write(g_phydev, g_addr, val);
	if (err)
		return err;

	return count;
}

static __maybe_unused int lantiq_gphy_mmd_read(struct phy_device *phydev,
						u16 regnum)
{
	phy_write(phydev, MII_MMDCTRL, MMD_ACTYPE_ADDRESS | MMD_DEVAD);
	phy_write(phydev, MII_MMDDATA, regnum);
	phy_write(phydev, MII_MMDCTRL, MMD_ACTYPE_DATA | MMD_DEVAD);

	return phy_read(phydev, MII_MMDDATA);
}

static __maybe_unused int lantiq_gphy_mmd_write(struct phy_device *phydev,
						u16 regnum, u16 val)
{
	phy_write(phydev, MII_MMDCTRL, MMD_ACTYPE_ADDRESS | MMD_DEVAD);
	phy_write(phydev, MII_MMDDATA, regnum);
	phy_write(phydev, MII_MMDCTRL, MMD_ACTYPE_DATA | MMD_DEVAD);
	phy_write(phydev, MII_MMDDATA, val);

	return 0;
}

static int lantiq_gphy_config_init(struct phy_device *phydev)
{
	int err;

	dev_dbg(&phydev->mdio.dev, "%s\n", __func__);

        /* Set LED0 blinking on rx/tx. */
        lantiq_gphy_mmd_write(phydev, INT_LED0H, 0);
        lantiq_gphy_mmd_write(phydev, INT_LED0L,
                 INT_LED_RX | INT_LED_TX);

        /* Set LED1 binking on link speed: slow=10M, fast=100M, on=1G. */
        lantiq_gphy_mmd_write(phydev, INT_LED1H,
                 INT_LED_LINK1000 << 4 | INT_LED_LINK100);
        lantiq_gphy_mmd_write(phydev, INT_LED1L,
                 INT_LED_LINK10 << 4);

	/* Mask all interrupts */
	err = phy_write(phydev, LANTIQ_MDIO_IMASK, 0);
	if (err)
		return err;

	/* Clear all pending interrupts */
	phy_read(phydev, LANTIQ_MDIO_ISTAT);

        /* Set SGMII RX & TX timing skew to 2 ns & 2.5 ns respectively. */
        /* Set MII power supply to 2V5. */
        err = phy_write(phydev, 0x17, 0x4d00);
	if (err)
		return err;

        /* Disable all 10M modes due to Xilinx EMACPS driver bug - #3120. */
        err = phy_write(phydev, 0x04, 0x0581);
        if (err)
                return err;

        /* Register sysfs interface */
        // TODO: Are there any register/unregister functions available for
        //       PHY drivers? This should really be moved there.
        // TODO: Clanup! But not strictly necessary,
        //       since we do not run as a module.
        if (!g_phydev) {
            attrs.attrs = phy11g_attrs;
            err = sysfs_create_group(&phydev->mdio.dev.kobj, &attrs);
            if (err)
                return(err);
            g_phydev = phydev;
        }

	return 0;
}

static int lantiq_gphy14_config_aneg(struct phy_device *phydev)
{
	int reg, err;

	/* Advertise as multi-port device, see IEEE802.3-2002 40.5.1.1 */
	/* This is a workaround for an errata in rev < 1.5 devices */
	reg = phy_read(phydev, MII_CTRL1000);
	reg |= ADVERTISED_MPD;
	err = phy_write(phydev, MII_CTRL1000, reg);
	if (err)
		return err;

	return genphy_config_aneg(phydev);
}

static int lantiq_gphy_ack_interrupt(struct phy_device *phydev)
{
	int reg;

	/*
	 * Possible IRQ numbers:
	 * - IM3_IRL18 for GPHY0
	 * - IM3_IRL17 for GPHY1
	 *
	 * Due to a silicon bug IRQ lines are not really independent from
	 * each other. Sometimes the two lines are driven at the same time
	 * if only one GPHY core raises the interrupt.
	 */
	reg = phy_read(phydev, LANTIQ_MDIO_ISTAT);

	return (reg < 0) ? reg : 0;
}

static int lantiq_gphy_did_interrupt(struct phy_device *phydev)
{
	int reg;

	reg = phy_read(phydev, LANTIQ_MDIO_ISTAT);

	return reg > 0;
}

static int lantiq_gphy_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, LANTIQ_MDIO_IMASK, LANTIQ_MDIO_INIT_MASK);
	else
		err = phy_write(phydev, LANTIQ_MDIO_IMASK, 0);

	return err;
}

static struct phy_driver lantiq_gphy[] = {
	{
		.phy_id		= PHY_ID_PHY11G_1_4,
		.phy_id_mask	= 0xfffffffe,
		.name		= "Lantiq XWAY PHY11G (PEF 7071/PEF 7072) v1.4",
		.features	= (PHY_GBIT_FEATURES | SUPPORTED_Pause),
		.flags		= PHY_HAS_MAGICANEG, /*PHY_HAS_INTERRUPT,*/
		.config_init	= lantiq_gphy_config_init,
		.config_aneg	= lantiq_gphy14_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= lantiq_gphy_ack_interrupt,
		.did_interrupt	= lantiq_gphy_did_interrupt,
		.config_intr	= lantiq_gphy_config_intr,
	}, {
		.phy_id		= 0x030260D0,
		.phy_id_mask	= 0xfffffff0,
		.name		= "Lantiq XWAY VR9 GPHY 11G v1.3",
		.features	= (PHY_GBIT_FEATURES | SUPPORTED_Pause),
		.flags		= 0, /*PHY_HAS_INTERRUPT,*/
		.config_init	= lantiq_gphy_config_init,
		.config_aneg	= lantiq_gphy14_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= lantiq_gphy_ack_interrupt,
		.did_interrupt	= lantiq_gphy_did_interrupt,
		.config_intr	= lantiq_gphy_config_intr,
	}, {
		.phy_id		= 0xd565a408,
		.phy_id_mask	= 0xfffffff8,
		.name		= "Lantiq XWAY VR9 GPHY 11G v1.4",
		.features	= (PHY_GBIT_FEATURES | SUPPORTED_Pause),
		.flags		= 0, /*PHY_HAS_INTERRUPT,*/
		.config_init	= lantiq_gphy_config_init,
		.config_aneg	= lantiq_gphy14_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= lantiq_gphy_ack_interrupt,
		.did_interrupt	= lantiq_gphy_did_interrupt,
		.config_intr	= lantiq_gphy_config_intr,
	}, {
		.phy_id		= PHY_ID_PHY11G_1_5,
		.phy_id_mask	= 0xffffffff,
		.name		= "Lantiq XWAY PHY11G (PEF 7071/PEF 7072) v1.5 / v1.6",
		.features	= (PHY_GBIT_FEATURES | SUPPORTED_Pause),
		.flags		= 0, /*PHY_HAS_INTERRUPT,*/
		.config_init	= lantiq_gphy_config_init,
		.config_aneg	= lantiq_gphy14_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= lantiq_gphy_ack_interrupt,
		.did_interrupt	= lantiq_gphy_did_interrupt,
		.config_intr	= lantiq_gphy_config_intr,
	}, {
		.phy_id		= 0xd565a418,
		.phy_id_mask	= 0xfffffff8,
		.name		= "Lantiq XWAY XRX PHY22F v1.4",
		.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause),
		.flags		= 0, /*PHY_HAS_INTERRUPT,*/
		.config_init	= lantiq_gphy_config_init,
		.config_aneg	= lantiq_gphy14_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= lantiq_gphy_ack_interrupt,
		.did_interrupt	= lantiq_gphy_did_interrupt,
		.config_intr	= lantiq_gphy_config_intr,
	},
};

module_phy_driver(lantiq_gphy);

static struct mdio_device_id __maybe_unused lantiq_gphy_tbl[] = {
	{ PHY_ID_PHY11G_1_4, 0xfffffffe },
	{ 0x030260D0       , 0xfffffff0 },
	{ 0xd565a408       , 0xfffffff8 },
	{ PHY_ID_PHY11G_1_5, 0xffffffff },
	{ 0xd565a418       , 0xfffffff8 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, lantiq_gphy_tbl);

MODULE_DESCRIPTION("Lantiq PHY drivers");
MODULE_AUTHOR("Daniel Schwierzeck <daniel.schwierzeck@googlemail.com>");
MODULE_LICENSE("GPL");
