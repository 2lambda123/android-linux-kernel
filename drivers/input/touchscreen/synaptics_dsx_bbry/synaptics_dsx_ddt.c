/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2014 BlackBerry Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include "synaptics_dsx_core.h"
#include "synaptics_dsx_ddt.h"

#include <misc/ddt.h>
#include <misc/lw_event_types.h>


static inline int synaptics_dsx_ddt_event_send(struct synaptics_rmi4_data *rmi4_data, int etype)
{
	int ddt_etype, ret;
	struct logworthy_event_details_t details;
	char *device_name = (char *)rmi4_data->input_dev->name;

	/* convert etype to DDT event */
	switch (etype) {
	case FW_UPGRADE_EXCEEDED_RETRIES:
		ddt_etype = LW_EVENT_MTOUCH_0000059a;
		break;
	case EXCEEDED_COMM_RETRIES:
		ddt_etype = LW_EVENT_MTOUCH_0000059b;
		break;
	case UNEXPECTED_RESET:
		ddt_etype = LW_EVENT_MTOUCH_0000059c;
		break;
	case CONTROLLER_LOCKUP:
		ddt_etype = LW_EVENT_MTOUCH_000005aa;
		break;
	case MTOUCH_PROXI_NOT_READY:
		ddt_etype = LW_EVENT_MTOUCH_00002521;
	default:
		return -EINVAL;
	}

	details.d1 = 1;
	details.d2 = 2;
	details.d3 = 3;
	details.d4 = 4;
	details.creator_id = device_name;

	/* send to DDT */
	ret = ddt_send(ddt_etype, &details,
			"['logcat://main',"
			"'exec:///system/bin/dmesg']");
	return ret;
}

static char *synaptics_dsx_ddt_event_name(int etype)
{
	switch (etype) {
	case FW_UPGRADE_EXCEEDED_RETRIES:
		return "F/W UPGRADE EXCEEDED RETRIES";
	case EXCEEDED_COMM_RETRIES:
		return "EXCEEDED COMM. RETRIES";
	case UNEXPECTED_RESET:
		return "UNEXPECTED RESET";
	case CONTROLLER_LOCKUP:
		return "CONTROLLER LOCKUP";
	case MTOUCH_PROXI_NOT_READY:
		return "PROXI NOT READY";
	default:
		break;
	}
	return NULL;
}

int synaptics_dsx_ddt_send(struct synaptics_rmi4_data *rmi4_data, int etype)
{
	int ret;
	char *event_name;
	/* get event_name */
	event_name = synaptics_dsx_ddt_event_name(etype);
	if (event_name == NULL) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: invalid etype %d",
				__func__, etype);
		return -EINVAL;
	}
	/* print locally */
	dev_err(rmi4_data->pdev->dev.parent, "DDT - %s", event_name);

	/* send to DDT */
	ret = synaptics_dsx_ddt_event_send(rmi4_data, etype);

	if (ret < 0)
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to send ddt event (err=%d)\n", __func__, ret);
	return ret;
}