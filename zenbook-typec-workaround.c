#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/ftrace.h>
#include <linux/platform_device.h>
#include <linux/device/bus.h>

#define UCSI_DSM_FUNC_READ 2
#define UCSI_CCI 4

struct ucsi;
struct ucsi_operations {
	int (*read_version)(struct ucsi *ucsi, u16 *version);
	int (*read_cci)(struct ucsi *ucsi, u32 *cci);
};
struct ucsi {
	u16 version;
	struct device *dev;
	void *driver_data;
	struct ucsi_operations *ops;
};
struct ucsi_acpi {
	struct device *dev;
	struct ucsi *ucsi;
	void *base;
	bool check_bogus_event;
	guid_t guid;
};
extern void *ucsi_get_drvdata(struct ucsi *ucsi);

static bool patched = false;

static int ucsi_acpi_dsm(struct ucsi_acpi *ua, int func)
{
	union acpi_object *obj;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(ua->dev), &ua->guid, 1, func,
				NULL);
	if (!obj) {
		dev_err(ua->dev, "%s: failed to evaluate _DSM %d\n",
			__func__, func);
		return -EIO;
	}

	ACPI_FREE(obj);
	return 0;
}

static int my_ucsi_acpi_read_cci(struct ucsi *ucsi, u32 *cci)
{
	struct ucsi_acpi *ua = ucsi_get_drvdata(ucsi);
	int ret;

	ret = ucsi_acpi_dsm(ua, UCSI_DSM_FUNC_READ);
	if (ret)
		return ret;

	memcpy(cci, ua->base + UCSI_CCI, sizeof(*cci));

	return 0;
}

static void notrace
zen_ftrace_cci_handler(unsigned long ip, unsigned long parent_ip,
		   struct ftrace_ops *fops, struct ftrace_regs *regs)
{
	struct pt_regs *rg = ftrace_get_regs(regs);
	rg->ip = (unsigned long)my_ucsi_acpi_read_cci;
}

static struct ftrace_ops ftrace_cci_ops __read_mostly = {
	.func = zen_ftrace_cci_handler,
	.flags = FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_IPMODIFY,
};

static int ftrace_add_cci_func(unsigned long ip)
{
	if (patched)
		return 0;

	int ret = ftrace_set_filter_ip(&ftrace_cci_ops, ip, 0, 0);
	if (ret) {
		pr_err("zenbook-typec-workaround: can't set ftrace cci filter\n");
		return ret;
	}

	ret = register_ftrace_function(&ftrace_cci_ops);
	if (ret) {
		pr_err("zenbook-typec-workaround: can't register ftrace cci handler\n");
		ftrace_set_filter_ip(&ftrace_cci_ops, ip, 1, 0);
		return ret;
	}

	patched = true;

	return 0;
}

static int ftrace_remove_cci_func(void)
{
	if (!patched)
		return 0;

	int ret = unregister_ftrace_function(&ftrace_cci_ops);
	if (ret) {
		pr_err("zenbook-typec-workaround: can't unregister ftrace cci handler\n");
		return ret;
	}

	patched = false;
	return 0;
}

static int __init zen_init_module(void)
{
	struct device *dev = bus_find_device_by_name(&platform_bus_type, NULL, "USBC000:00");
	if (!dev) {
		pr_err("zenbook-typec-workaround: bus_find_device_by_name failed\n");
		return -1;
	}

	struct ucsi_acpi *ua = dev_get_drvdata(dev);
	put_device(dev);
	if (!ua) {
		pr_err("zenbook-typec-workaround: bus_find_device_by_name failed\n");
		return -1;
	}

	ftrace_add_cci_func((unsigned long)ua->ucsi->ops->read_cci);

	return 0;
}

static void __exit zen_cleanup_module(void)
{
	ftrace_remove_cci_func();
}

module_init(zen_init_module);
module_exit(zen_cleanup_module);
MODULE_LICENSE("GPL");
