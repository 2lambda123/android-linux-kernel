/*
 * QLogic qlcnic NIC Driver
 * Copyright (c) 2009-2013 QLogic Corporation
 *
 * See LICENSE.qlcnic for copyright and licensing details.
 */

#include "qlcnic.h"
#include "qlcnic_hw.h"

int qlcnic_83xx_enable_vnic_mode(struct qlcnic_adapter *adapter, int lock)
{
	if (lock) {
		if (qlcnic_83xx_lock_driver(adapter))
			return -EBUSY;
	}
	QLCWRX(adapter->ahw, QLC_83XX_VNIC_STATE, QLCNIC_DEV_NPAR_OPER);
	if (lock)
		qlcnic_83xx_unlock_driver(adapter);

	return 0;
}

int qlcnic_83xx_disable_vnic_mode(struct qlcnic_adapter *adapter, int lock)
{
	struct qlcnic_hardware_context *ahw = adapter->ahw;

	if (lock) {
		if (qlcnic_83xx_lock_driver(adapter))
			return -EBUSY;
	}

	QLCWRX(adapter->ahw, QLC_83XX_VNIC_STATE, QLCNIC_DEV_NPAR_NON_OPER);
	ahw->idc.vnic_state = QLCNIC_DEV_NPAR_NON_OPER;

	if (lock)
		qlcnic_83xx_unlock_driver(adapter);

	return 0;
}

static int qlcnic_83xx_set_vnic_opmode(struct qlcnic_adapter *adapter)
{
	u8 id;
	int i, ret = -EBUSY;
	u32 data = QLCNIC_MGMT_FUNC;
	struct qlcnic_hardware_context *ahw = adapter->ahw;

	if (qlcnic_83xx_lock_driver(adapter))
		return ret;

	if (qlcnic_config_npars) {
		for (i = 0; i < ahw->act_pci_func; i++) {
			id = adapter->npars[i].pci_func;
			if (id == ahw->pci_func)
				continue;
			data |= qlcnic_config_npars &
				QLC_83XX_SET_FUNC_OPMODE(0x3, id);
		}
	} else {
		data = QLCRDX(adapter->ahw, QLC_83XX_DRV_OP_MODE);
		data = (data & ~QLC_83XX_SET_FUNC_OPMODE(0x3, ahw->pci_func)) |
		       QLC_83XX_SET_FUNC_OPMODE(QLCNIC_MGMT_FUNC,
						ahw->pci_func);
	}
	QLCWRX(adapter->ahw, QLC_83XX_DRV_OP_MODE, data);

	qlcnic_83xx_unlock_driver(adapter);

	return 0;
}

static void
qlcnic_83xx_config_vnic_buff_descriptors(struct qlcnic_adapter *adapter)
{
	struct qlcnic_hardware_context *ahw = adapter->ahw;

	if (ahw->port_type == QLCNIC_XGBE) {
		adapter->num_rxd = DEFAULT_RCV_DESCRIPTORS_VF;
		adapter->max_rxd = MAX_RCV_DESCRIPTORS_VF;
		adapter->num_jumbo_rxd = MAX_JUMBO_RCV_DESCRIPTORS_10G;
		adapter->max_jumbo_rxd = MAX_JUMBO_RCV_DESCRIPTORS_10G;

	} else if (ahw->port_type == QLCNIC_GBE) {
		adapter->num_rxd = DEFAULT_RCV_DESCRIPTORS_1G;
		adapter->num_jumbo_rxd = MAX_JUMBO_RCV_DESCRIPTORS_1G;
		adapter->max_jumbo_rxd = MAX_JUMBO_RCV_DESCRIPTORS_1G;
		adapter->max_rxd = MAX_RCV_DESCRIPTORS_1G;
	}
	adapter->num_txd = MAX_CMD_DESCRIPTORS;
	adapter->max_rds_rings = MAX_RDS_RINGS;
}


/**
 * qlcnic_83xx_init_mgmt_vnic
 *
 * @adapter: adapter structure
 * Management virtual NIC sets the operational mode of other vNIC's and
 * configures embedded switch (ESWITCH).
 * Returns: Success(0) or error code.
 *
 **/
static int qlcnic_83xx_init_mgmt_vnic(struct qlcnic_adapter *adapter)
{
	int err = -EIO;

	qlcnic_83xx_get_minidump_template(adapter);
	if (!(adapter->flags & QLCNIC_ADAPTER_INITIALIZED)) {
		if (qlcnic_init_pci_info(adapter))
			return err;

		if (qlcnic_83xx_set_vnic_opmode(adapter))
			return err;

		if (qlcnic_set_default_offload_settings(adapter))
			return err;
	} else {
		if (qlcnic_reset_npar_config(adapter))
			return err;
	}

	if (qlcnic_83xx_get_port_info(adapter))
		return err;

	qlcnic_83xx_config_vnic_buff_descriptors(adapter);
	adapter->ahw->msix_supported = !!qlcnic_use_msi_x;
	adapter->flags |= QLCNIC_ADAPTER_INITIALIZED;
	qlcnic_83xx_enable_vnic_mode(adapter, 1);

	dev_info(&adapter->pdev->dev, "HAL Version: %d, Management function\n",
		 adapter->ahw->fw_hal_version);

	return 0;
}

static int qlcnic_83xx_init_privileged_vnic(struct qlcnic_adapter *adapter)
{
	int err = -EIO;

	qlcnic_83xx_get_minidump_template(adapter);
	if (qlcnic_83xx_get_port_info(adapter))
		return err;

	qlcnic_83xx_config_vnic_buff_descriptors(adapter);
	adapter->ahw->msix_supported = !!qlcnic_use_msi_x;
	adapter->flags |= QLCNIC_ADAPTER_INITIALIZED;

	dev_info(&adapter->pdev->dev,
		 "HAL Version: %d, Privileged function\n",
		 adapter->ahw->fw_hal_version);
	return 0;
}

static int qlcnic_83xx_init_non_privileged_vnic(struct qlcnic_adapter *adapter)
{
	int err = -EIO;

	qlcnic_83xx_get_fw_version(adapter);
	if (qlcnic_set_eswitch_port_config(adapter))
		return err;

	if (qlcnic_83xx_get_port_info(adapter))
		return err;

	qlcnic_83xx_config_vnic_buff_descriptors(adapter);
	adapter->ahw->msix_supported = !!qlcnic_use_msi_x;
	adapter->flags |= QLCNIC_ADAPTER_INITIALIZED;

	dev_info(&adapter->pdev->dev, "HAL Version: %d, Virtual function\n",
		 adapter->ahw->fw_hal_version);

	return 0;
}

/**
 * qlcnic_83xx_vnic_opmode
 *
 * @adapter: adapter structure
 * Identify virtual NIC operational modes.
 *
 * Returns: Success(0) or error code.
 *
 **/
int qlcnic_83xx_config_vnic_opmode(struct qlcnic_adapter *adapter)
{
	u32 op_mode, priv_level;
	struct qlcnic_hardware_context *ahw = adapter->ahw;
	struct qlcnic_nic_template *nic_ops = adapter->nic_ops;

	qlcnic_get_func_no(adapter);
	op_mode = QLCRDX(adapter->ahw, QLC_83XX_DRV_OP_MODE);

	if (op_mode == QLC_83XX_DEFAULT_OPMODE)
		priv_level = QLCNIC_MGMT_FUNC;
	else
		priv_level = QLC_83XX_GET_FUNC_PRIVILEGE(op_mode,
							 ahw->pci_func);

	if (priv_level == QLCNIC_NON_PRIV_FUNC) {
		ahw->op_mode = QLCNIC_NON_PRIV_FUNC;
		ahw->idc.state_entry = qlcnic_83xx_idc_ready_state_entry;
		nic_ops->init_driver = qlcnic_83xx_init_non_privileged_vnic;
		pax_open_kernel();
		*(void **)&nic_ops->init_driver = qlcnic_83xx_init_non_privileged_vnic;
		pax_close_kernel();
	} else if (priv_level == QLCNIC_PRIV_FUNC) {
		ahw->op_mode = QLCNIC_PRIV_FUNC;
		ahw->idc.state_entry = qlcnic_83xx_idc_vnic_pf_entry;
		pax_open_kernel();
		*(void **)&nic_ops->init_driver = qlcnic_83xx_init_privileged_vnic;
		pax_close_kernel();
	} else if (priv_level == QLCNIC_MGMT_FUNC) {
		ahw->op_mode = QLCNIC_MGMT_FUNC;
		ahw->idc.state_entry = qlcnic_83xx_idc_ready_state_entry;
		pax_open_kernel();
		*(void **)&nic_ops->init_driver = qlcnic_83xx_init_mgmt_vnic;
		pax_close_kernel();
	} else {
		return -EIO;
	}

	if (ahw->capabilities & BIT_23)
		adapter->flags |= QLCNIC_ESWITCH_ENABLED;
	else
		adapter->flags &= ~QLCNIC_ESWITCH_ENABLED;

	adapter->ahw->idc.vnic_state = QLCNIC_DEV_NPAR_NON_OPER;
	adapter->ahw->idc.vnic_wait_limit = QLCNIC_DEV_NPAR_OPER_TIMEO;

	return 0;
}
