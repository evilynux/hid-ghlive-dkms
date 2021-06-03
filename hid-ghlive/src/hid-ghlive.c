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
	int device_id; /* What is this for? */
	struct urb *urb;
	struct timer_list poke_timer;
};

static void ghl_magic_poke_cb(struct urb *urb)
{
	struct ghlive_sc *sc = urb->context;

	if (urb->status < 0)
		hid_err(sc->hdev, "URB transfer failed : %d", urb->status);

	mod_timer(&sc->poke_timer, jiffies + GHL_GUITAR_POKE_INTERVAL*HZ);
}

static void ghl_magic_poke(struct timer_list *t)
{
	int ret;
	struct ghlive_sc *sc = from_timer(sc, t, poke_timer);

	ret = usb_submit_urb(sc->urb, GFP_ATOMIC);
	if (ret < 0)
		hid_err(sc->hdev, "usb_submit_urb failed: %d", ret);
}

static int ghl_ps3wiiu_tx(struct ghlive_sc *sc, struct usb_device *usbdev)
{
	struct usb_ctrlrequest *cr;
	u16 poke_size;
	u8 *databuf;
	unsigned int pipe;

	poke_size = ARRAY_SIZE(ghl_ps3wiiu_magic_data);
	pipe = usb_sndctrlpipe(usbdev, 0);

	cr = devm_kzalloc(&sc->hdev->dev, sizeof(*cr), GFP_ATOMIC);
	if (cr == NULL)
		return -ENOMEM;

	databuf = devm_kzalloc(&sc->hdev->dev, poke_size, GFP_ATOMIC);
	if (databuf == NULL)
		return -ENOMEM;

	cr->bRequestType =
		USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_OUT;
	cr->bRequest = USB_REQ_SET_CONFIGURATION;
	cr->wValue = cpu_to_le16(ghl_ps3wiiu_magic_value);
	cr->wIndex = 0;
	cr->wLength = cpu_to_le16(poke_size);
	memcpy(databuf, ghl_ps3wiiu_magic_data, poke_size);
	usb_fill_control_urb(
		sc->urb, usbdev, pipe,
		(unsigned char *) cr, databuf, poke_size,
		ghl_magic_poke_cb, sc);
	return 0;
}

static int ghl_ps4_tx(struct ghlive_sc *sc, struct usb_device *usbdev)
{
	int i;
	struct usb_interface *intf;
	struct usb_endpoint_descriptor *ep;
	u16 poke_size;
	u8 *databuf;
	unsigned int pipe;
	struct usb_endpoint_descriptor *ep_irq_out = NULL;

	intf = to_usb_interface(sc->hdev->dev.parent);
	if (intf->cur_altsetting->desc.bNumEndpoints != 2)
		return -ENODEV;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep = &intf->cur_altsetting->endpoint[i].desc;

		if (usb_endpoint_xfer_int(ep)) {
			if (usb_endpoint_dir_out(ep))
				ep_irq_out = ep;
		}
	}

	poke_size = ARRAY_SIZE(ghl_ps4_magic_data);
	pipe = usb_sndintpipe(usbdev, ep_irq_out->bEndpointAddress);

	databuf = devm_kzalloc(&sc->hdev->dev, poke_size, GFP_ATOMIC);
	if (databuf == NULL)
		return -ENOMEM;

	memcpy(databuf, ghl_ps4_magic_data, poke_size);

	usb_fill_int_urb(
		sc->urb, usbdev, pipe,
		databuf, poke_size,
		ghl_magic_poke_cb, sc, ep_irq_out->bInterval);
	return 0;
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
	int ret, i;
	struct ghlive_sc *sc;
	struct usb_interface *intf;
	struct usb_endpoint_descriptor *ep;
	struct usb_device *usbdev;
	u16 poke_size;
	u8 *databuf;
	unsigned int pipe, connect_mask = HID_CONNECT_DEFAULT;
	struct usb_endpoint_descriptor *ep_irq_out = NULL;

	sc = devm_kzalloc(&hdev->dev, sizeof(*sc), GFP_KERNEL);
	if (sc == NULL)
		return -ENOMEM;

	sc->quirks = id->driver_data;
	hid_set_drvdata(hdev, sc);
	sc->hdev = hdev;
	usbdev = to_usb_device(sc->hdev->dev.parent->parent);

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

	sc->urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!sc->urb)
		return -ENOMEM;

	if (sc->quirks & GHL_GUITAR_PS3WIIU)
		ret = ghl_ps3wiiu_tx(sc, usbdev);
	else if (sc->quirks & GHL_GUITAR_PS4)
		ret = ghl_ps4_tx(sc, usbdev);
	
	if (ret) {
		hid_err(hdev, "error preparing URB\n");
		return ret;
	}
	
	timer_setup(&sc->poke_timer, ghl_magic_poke, 0);
	mod_timer(&sc->poke_timer,
		  jiffies + GHL_GUITAR_POKE_INTERVAL*HZ);
	return ret;
}

static void ghlive_remove(struct hid_device *hdev)
{
	struct ghlive_sc *sc = hid_get_drvdata(hdev);

	del_timer_sync(&sc->poke_timer);
	usb_free_urb(sc->urb);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id ghlive_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_SONY_GHLIVE, USB_DEVICE_ID_SONY_PS3WIIU_GHLIVE_DONGLE),
		.driver_data = GHL_GUITAR_CONTROLLER | GHL_GUITAR_PS3WIIU },
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
