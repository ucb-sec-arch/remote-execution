#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x60d90935, "module_layout" },
	{ 0x3ebab4e4, "kobject_put" },
	{ 0xc6177942, "kmalloc_caches" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0xd2a941d4, "sg_init_table" },
	{ 0xe7810810, "sysfs_remove_bin_file" },
	{ 0x9eb34ea7, "mem_map" },
	{ 0x2d37342e, "cpu_online_mask" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xfa8c44d9, "cpu_info" },
	{ 0x47c7b0d2, "cpu_number" },
	{ 0xb0d013a0, "kobject_create_and_add" },
	{ 0x2bc95bd4, "memset" },
	{ 0x88941a06, "_raw_spin_unlock_irqrestore" },
	{ 0x70d1f8f3, "strncat" },
	{ 0x50eedeb8, "printk" },
	{ 0x5152e605, "memcmp" },
	{ 0xb4390f9a, "mcount" },
	{ 0x3af98f9e, "ioremap_nocache" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x579fbcd2, "cpu_possible_mask" },
	{ 0x86a4889a, "kmalloc_order_trace" },
	{ 0xbfa46a74, "pv_cpu_ops" },
	{ 0xe69f138, "crypto_destroy_tfm" },
	{ 0x2e20f1a6, "sysfs_create_file" },
	{ 0xba236513, "kmem_cache_alloc_trace" },
	{ 0x7ecb001b, "__per_cpu_offset" },
	{ 0x587c70d8, "_raw_spin_lock_irqsave" },
	{ 0xc03c3dcc, "kernel_kobj" },
	{ 0x37a0cba, "kfree" },
	{ 0x2e60bace, "memcpy" },
	{ 0x94bb7b01, "pv_mmu_ops" },
	{ 0xedc03953, "iounmap" },
	{ 0xde4ac87a, "sysfs_create_bin_file" },
	{ 0xb6aaac7c, "sysfs_remove_file" },
	{ 0x74c134b9, "__sw_hweight32" },
	{ 0x701d0ebd, "snprintf" },
	{ 0x202953e4, "apic" },
	{ 0x4ecac20b, "crypto_alloc_base" },
	{ 0x6228c21f, "smp_call_function_single" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0E62FA5CC0D02F043D37051");
