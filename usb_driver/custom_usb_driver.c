#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/usb.h>

//probe function
static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id){
	printk(KERN_INFO "###$$$ Bhargav's pen drive (%04x:%04x) plugged in\n", id->idVendor, id->idProduct);
	return 0;
}

//disconnect
static void pen_disconnect(struct usb_interface *interface){
	printk(KERN_INFO "###$$$ Bhargav's pen drive removed");
}

// usb device id
static struct usb_device_id pen_table[] = {
	//05e3:0751
	{USB_DEVICE(0x05e3, 0x0751)},
	{}// terminating entry
};
MODULE_DEVICE_TABLE(usb, pen_table);


// usb_driver
static struct usb_driver pen_driver = {
	.name = "Bhargav's custom usb driver",
	.id_table = pen_table,
	.probe = pen_probe,
	.disconnect = pen_disconnect//TODO comma ?
};

static int __init pen_init(void){
	int ret = -1;

	printk(KERN_INFO "###$$$ Bhargav's custom usb driver init function");
	printk(KERN_INFO "###$$$ registering with kernel");
	ret = usb_register(&pen_driver);
	printk(KERN_INFO "registration is complete");

	return ret;
}

static void __exit pen_exit(void){
	//unregister the device driver
	printk(KERN_INFO "###$$$ Bhargav's usb device exit function");
	usb_deregister(&pen_driver);
	printk(KERN_INFO "unregistering complete");
}

module_init(pen_init);
module_exit(pen_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bhargav");
MODULE_DESCRIPTION("Usb flash registration driver");