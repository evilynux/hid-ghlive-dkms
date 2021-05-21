/*
 *	HID driver for Guitar Hero Live PS3, Wii U, and PS4 Guitar devices.
 *
 *	Copyright (c) 2020 Pascal Giard <pascal.giard@etsmtl.ca>
 *	Copyright (c) 2021 Daniel Nguyen <daniel.nguyen.1@ens.etsmtl.ca>
 */

#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/module.h>

#include "hid-ids.h"

MODULE_AUTHOR("Pascal Giard <pascal.giard@etsmtl.ca>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HID driver for Activision GH Live PS3, Wii U, and PS4 Guitar devices");

#define GHL_GUITAR_PS4			BIT(3)
#define GHL_GUITAR_PS3WIIU		BIT(2)
#define GHL_GUITAR_CONTROLLER	BIT(1)

#define GHL_GUITAR_POKE_INTERVAL 8 /* In seconds */

#define GHL_GUITAR_TILT_USAGE 44

/* Magic value and data taken from GHLtarUtility:
 * https://github.com/ghlre/GHLtarUtility/blob/master/PS3Guitar.cs
 * Note: The Wii U and PS3 dongles happen to share the same!
 */
static const u16 ghl_ps3wiiu_magic_value = 0x201;
static const char ghl_ps3wiiu_magic_data[] = {
	0x02, 0x08, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* PS4 magic data found through usb sniffing */
static const char ghl_ps4_magic_data[] = {
	0x30, 0x02, 0x08, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct ghlive_sc {
	struct hid_device *hdev;
	unsigned long quirks;
	int device_id;
	struct timer_list poke_timer;
	struct usb_ctrlrequest *cr;
	u8 *databuf;
};

static void ghl_magic_poke_cb_ps3wiiu(struct urb *urb)
{
	if (urb) {
		/* Free sc->cr and sc->databuf allocated in ghl_magic_poke_ps3wiiu() */
		kfree(urb->setup_packet);
		kfree(urb->transfer_buffer);
	}
}

static void ghl_magic_poke_cb_ps4(struct urb *urb)
{
	if (urb) {
		/* Free sc->databuf allocated in ghl_magic_poke_ps4() */
		kfree(urb->transfer_buffer);
	}
}

static void ghl_magic_poke_ps3wiiu(struct timer_list *t)
{
	int ret;
	struct urb *urb;
	struct ghlive_sc *sc = from_timer(sc, t, poke_timer);
	struct usb_device *usbdev = to_usb_device(sc->hdev->dev.parent->parent);
	const u16 poke_size =
		ARRAY_SIZE(ghl_ps3wiiu_magic_data);
	unsigned int pipe = usb_sndctrlpipe(usbdev, 0);

	sc->cr = kzalloc(sizeof(*sc->cr), GFP_ATOMIC);
	if (!sc->cr)
		goto resched;

	sc->databuf = kzalloc(poke_size, GFP_ATOMIC);
	if (!sc->databuf) {
		kfree(sc->cr);
		goto resched;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		kfree(sc->databuf);
		kfree(sc->cr);
		goto resched;
	}

	sc->cr->bRequestType =
		USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_OUT;
	sc->cr->bRequest = USB_REQ_SET_CONFIGURATION;
	sc->cr->wValue = cpu_to_le16(ghl_ps3wiiu_magic_value);
	sc->cr->wIndex = 0;
	sc->cr->wLength = cpu_to_le16(poke_size);
	memcpy(sc->databuf, ghl_ps3wiiu_magic_data, poke_size);

	usb_fill_control_urb(
		urb, usbdev, pipe,
		(unsigned char *) sc->cr, sc->databuf, poke_size,
		ghl_magic_poke_cb_ps3wiiu, NULL);
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret < 0) {
		kfree(sc->databuf);
		kfree(sc->cr);
	}
	usb_free_urb(urb);

resched:
	/* Reschedule for next time */
	mod_timer(&sc->poke_timer, jiffies + GHL_GUITAR_POKE_INTERVAL*HZ);
}

static void ghl_magic_poke_ps4(struct timer_list *t)
{
	int ret;
	struct urb *urb;
	struct ghlive_sc *sc = from_timer(sc, t, poke_timer);
	struct usb_device *usbdev = to_usb_device(sc->hdev->dev.parent->parent);
	const u16 poke_size =
		ARRAY_SIZE(ghl_ps4_magic_data);
	unsigned int pipe = usb_sndintpipe(usbdev, 0x02);

	sc->databuf = kzalloc(poke_size, GFP_ATOMIC);
	if (!sc->databuf) {
		kfree(sc->cr);
		goto resched;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		kfree(sc->databuf);
		goto resched;
	}

	memcpy(sc->databuf, ghl_ps4_magic_data, poke_size);

	usb_fill_int_urb(
		urb, usbdev, pipe,
		sc->databuf, poke_size,
		ghl_magic_poke_cb_ps4, NULL, 5);	/* the bInterval for the EndPoint is 5. */
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret < 0) {
		kfree(sc->databuf);
	}
	usb_free_urb(urb);

resched:
	/* Reschedule for next time */
	mod_timer(&sc->poke_timer, jiffies + GHL_GUITAR_POKE_INTERVAL*HZ);
}


static int guitar_mapping(struct hid_device *hdev, struct hid_input *hi,
			  struct hid_field *field, struct hid_usage *usage,
			  unsigned long **bit, int *max)
{
	if ((usage->hid & HID_USAGE_PAGE) == HID_UP_MSVENDOR) {
		unsigned int abs = usage->hid & HID_USAGE;

		if (abs == GHL_GUITAR_TILT_USAGE) {
			hid_map_usage_clear(hi, usage, bit, max, EV_ABS, ABS_RY);
			return 1;
		}
	}
	return 0;
}

static int ghlive_mapping(struct hid_device *hdev, struct hid_input *hi,
			      struct hid_field *field, struct hid_usage *usage,
			      unsigned long **bit, int *max)
{
	struct ghlive_sc *sc = hid_get_drvdata(hdev);

	if (sc->quirks & GHL_GUITAR_CONTROLLER)
		return guitar_mapping(hdev, hi, field, usage, bit, max);

	/* Let hid-core decide for the others */
	return 0;
}

static int ghlive_probe(struct hid_device *hdev,
			    const struct hid_device_id *id)
{
	int ret;
	struct ghlive_sc *sc;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;

	sc = devm_kzalloc(&hdev->dev, sizeof(*sc), GFP_KERNEL);
	if (sc == NULL)
		return -ENOMEM;

	sc->quirks = id->driver_data;
	hid_set_drvdata(hdev, sc);
	sc->hdev = hdev;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	if (!(hdev->claimed & HID_CLAIMED_INPUT)) {
		hid_err(hdev, "failed to claim input\n");
		hid_hw_stop(hdev);
		return -ENODEV;
	}

	if (sc->quirks & GHL_GUITAR_PS3WIIU) {
		timer_setup(&sc->poke_timer, ghl_magic_poke_ps3wiiu, 0);
	}
	else if (sc->quirks & GHL_GUITAR_PS4){
		timer_setup(&sc->poke_timer, ghl_magic_poke_ps4, 0);
	} 
	mod_timer(&sc->poke_timer,
		  jiffies + GHL_GUITAR_POKE_INTERVAL*HZ);
	return ret;
}

static void ghlive_remove(struct hid_device *hdev)
{
	struct ghlive_sc *sc = hid_get_drvdata(hdev);

	del_timer_sync(&sc->poke_timer);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id ghlive_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY_GHLIVE, USB_DEVICE_ID_SONY_PS3WIIU_GHLIVE_DONGLE),
		.driver_data = GHL_GUITAR_CONTROLLER | GHL_GUITAR_PS3WIIU},
	{ HID_USB_DEVICE(USB_VENDOR_ID_REDOCTANE_GHLIVE, USB_DEVICE_ID_REDOCTANE_PS4_GHLIVE_DONGLE),
		.driver_data = GHL_GUITAR_CONTROLLER | GHL_GUITAR_PS4 },
	{ }
};
MODULE_DEVICE_TABLE(hid, ghlive_devices);

static struct hid_driver ghlive_driver = {
	.name		= "ghlive",
	.id_table	= ghlive_devices,
	.input_mapping	= ghlive_mapping,
	.probe		= ghlive_probe,
	.remove	= ghlive_remove,
};

static int __init ghlive_init(void)
{
	dbg_hid("GHLive:%s\n", __func__);
	return hid_register_driver(&ghlive_driver);
}

static void __exit ghlive_exit(void)
{
	dbg_hid("GHLive:%s\n", __func__);

	hid_unregister_driver(&ghlive_driver);
}
module_init(ghlive_init);
module_exit(ghlive_exit);
