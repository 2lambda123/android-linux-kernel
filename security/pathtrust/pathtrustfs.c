/*
 * Copyright (C) 2015 BlackBerry Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/security.h>
#include "pathtrust_private.h"


#if defined(CONFIG_SECURITY_PATHTRUST_DEVELOP) || defined(CONFIG_SECURITY_PATHTRUST_BOOTPARAM)
#ifdef CONFIG_SECURITY_PATHTRUST_BOOTPARAM_VALUE
int pathtrust_enforce = CONFIG_SECURITY_PATHTRUST_BOOTPARAM_VALUE;
#else
int pathtrust_enforce = 1;
#endif
#endif

#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
int pathtrust_debug = 0;
#endif

#ifdef CONFIG_SECURITY_PATHTRUST_BOOTPARAM
static int __init pathtrust_setup(char *str)
{
	unsigned long enforce;
	if (!strict_strtoul(str, 0, &enforce))
		pathtrust_enforce = enforce ? 1 : 0;

	return 1;
}
__setup("pathtrust=", pathtrust_setup);
#endif



static struct dentry *pathtrust_dir;
static struct dentry *pathtrust_enforce_file;
#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
static struct dentry *pathtrust_debug_file;
#endif

/**
 * pathtrust_read_enforce - read() for /sys/kernel/security/pathtrust/enforce
 *
 * @filp: file pointer, not actually used
 * @buf: where to put the result
 * @count: maximum to send along
 * @ppos: where to start
 *
 * Buffer will contain pathtrust enabled state
 * 0 = disabled or 1 = enabled
 *
 * Returns number of bytes read or error code, as appropriate
 */
#define TMPBUFLEN	12
static ssize_t pathtrust_read_enforce(struct file *filp, char __user *buf,
			    size_t count, loff_t *ppos)
{
	char temp[TMPBUFLEN];
	ssize_t rc;

	if (*ppos != 0)
		return 0;

	snprintf(temp, TMPBUFLEN, "%d", pathtrust_enforce);
	rc = simple_read_from_buffer(buf, count, ppos, temp, strlen(temp));

	return rc;
}

#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
/**
 * pathtrust_write_enforce - write() for /sys/kernel/security/pathtrust/enforce
 * @file: file pointer, not actually used
 * @buf: where to get the data from
 * @count: bytes sent
 * @ppos: where to start
 *
 *	Input is only one char so only need a buffer of length 2
 *
 * Pathtrust enabled state value read needs to be
 * 0 = disabled, 1 = enabled
 *
 * Returns number of bytes written or error code, as appropriate
 */
static ssize_t pathtrust_write_enforce(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	char temp[2];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (count > sizeof(temp) || count == 0)
		return -EINVAL;

	if (copy_from_user(temp, buf, count) != 0)
		return -EFAULT;

	temp[1] = '\0';

	if (temp[0] == '0')
		pathtrust_enforce = 0;
	else if (temp[0] == '1')
		pathtrust_enforce = 1;
	else
		return -EINVAL;

	pr_info("toggled in '%s' (%d) mode\n", pathtrust_enforce ? "enforced" : "non-enforced", pathtrust_enforce);
	ptnl_notify_enforce(pathtrust_enforce);

	return count;
}
#else
#define pathtrust_write_enforce NULL
#endif /* CONFIG_SECURITY_PATHTRUST_DEVELOP */

static const struct file_operations pathtrust_enforce_file_ops = {
	.read		= pathtrust_read_enforce,
	.write		= pathtrust_write_enforce,
};

#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
/**
 * pathtrust_read_debug - read() for /sys/kernel/security/pathtrust/debug
 *
 * @filp: file pointer, not actually used
 * @buf: where to put the result
 * @count: maximum to send along
 * @ppos: where to start
 *
 * Buffer will contain pathtrust debug state
 * 0 = disabled or 1 = enabled
 *
 * Returns number of bytes read or error code, as appropriate
 */
static ssize_t pathtrust_read_debug(struct file *filp, char __user *buf,
			    size_t count, loff_t *ppos)
{
	char temp[TMPBUFLEN];
	ssize_t rc;

	if (*ppos != 0)
		return 0;

	snprintf(temp, TMPBUFLEN, "%d", pathtrust_debug);
	rc = simple_read_from_buffer(buf, count, ppos, temp, strlen(temp));

	return rc;
}

/**
 * pathtrust_write_debug - write() for /sys/kernel/security/pathtrust/debug
 * @file: file pointer, not actually used
 * @buf: where to get the data from
 * @count: bytes sent
 * @ppos: where to start
 *
 *	Input is only one char so only need a buffer of length 2
 *
 * Pathtrust debug state value read needs to be
 * 0 = disabled, 1 = enabled
 *
 * Returns number of bytes written or error code, as appropriate
 */
static ssize_t pathtrust_write_debug(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	char temp[2];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (count > sizeof(temp) || count == 0)
		return -EINVAL;

	if (copy_from_user(temp, buf, count) != 0)
		return -EFAULT;

	temp[1] = '\0';

	if (temp[0] == '0')
		pathtrust_debug = 0;
	else if (temp[0] == '1')
		pathtrust_debug = 1;
	else
		return -EINVAL;

	pr_info("toggled in '%s' (%d) mode\n", pathtrust_debug ? "debug" : "non-debug", pathtrust_debug);

	return count;
}

static const struct file_operations pathtrust_debug_file_ops = {
	.read		= pathtrust_read_debug,
	.write		= pathtrust_write_debug,
};
#endif

/*
 * Initialization of the sys/kernel/security/pathtrust sysfs entries
 */
int __init pathtrust_fs_init(void)
{
	pathtrust_dir = securityfs_create_dir("pathtrust", NULL);
	if (IS_ERR(pathtrust_dir)) {
		pr_err("failed to create 'pathtrust' sysfs dir");
		return -1;
	}

	pathtrust_enforce_file =
	    securityfs_create_file("enforce",
				   S_IRUSR | S_IRGRP, pathtrust_dir, NULL,
				   &pathtrust_enforce_file_ops);
	if (IS_ERR(pathtrust_enforce_file)) {
		pr_err("failed to create sysfs pathtrust 'enabled' file");
		goto out;
	}

#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
	pathtrust_debug_file =
	    securityfs_create_file("debug",
				   S_IRUSR | S_IRGRP, pathtrust_dir, NULL,
				   &pathtrust_debug_file_ops);
	if (IS_ERR(pathtrust_debug_file)) {
		pr_err("failed to create sysfs pathtrust 'debug' file");
		goto out;
	}
#endif

	return 0;
out:
	securityfs_remove(pathtrust_enforce_file);
#ifdef CONFIG_SECURITY_PATHTRUST_DEVELOP
	securityfs_remove(pathtrust_debug_file);
#endif
	securityfs_remove(pathtrust_dir);
	return -1;
}

late_initcall(pathtrust_fs_init);