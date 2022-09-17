/*
 * pci_skel.c - Basic PCI device driver setup.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>

static struct pci_device_id ids[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AA_3),
	},
	{
		0,
	}
};
MODULE_DEVICE_TABLE(pci, ids);

static unsigned char skel_get_revision(struct pci_dev *dev)
{
	u8 revision;

	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	return revision;
}

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	/*
     * Do probing type stuff here. Like calling request_region();
	 */
	int ret = pci_enable_device(dev);
	printk(KERN_INFO "pci_skel: pci_enable_device() status is %d\n", ret);

	if (skel_get_revision(dev) == 0x42)
		return -ENODEV;

	return 0;
}

static void remove(struct pci_dev *dev)
{
	/*
     * Clean up any allocated resources and stuff here. Like call
     * release_region()
	 */
}

static struct pci_driver pci_driver = {
	.name = "pci_skel",
	.id_table = ids,
	.probe = probe,
	.remove = remove,
};

static int __init pci_skel_init(void)
{
	return pci_register_driver(&pci_driver);
}

static void __exit pci_skel_exit(void)
{
	pci_unregister_driver(&pci_driver);
}

MODULE_LICENSE("GPL");

module_init(pci_skel_init);
module_exit(pci_skel_exit);
