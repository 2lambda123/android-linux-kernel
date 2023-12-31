/*
 * QLogic qlcnic NIC Driver
 * Copyright (c) 2009-2013 QLogic Corporation
 *
 * See LICENSE.qlcnic for copyright and licensing details.
 */

#include "qlcnic.h"
#include "qlcnic_hdr.h"
#include "qlcnic_83xx_hw.h"
#include "qlcnic_hw.h"

#include <net/ip.h>

#define QLC_83XX_MINIDUMP_FLASH		0x520000
#define QLC_83XX_OCM_INDEX			3
#define QLC_83XX_PCI_INDEX			0

static const u32 qlcnic_ms_read_data[] = {
	0x410000A8, 0x410000AC, 0x410000B8, 0x410000BC
};

#define QLCNIC_DUMP_WCRB	BIT_0
#define QLCNIC_DUMP_RWCRB	BIT_1
#define QLCNIC_DUMP_ANDCRB	BIT_2
#define QLCNIC_DUMP_ORCRB	BIT_3
#define QLCNIC_DUMP_POLLCRB	BIT_4
#define QLCNIC_DUMP_RD_SAVE	BIT_5
#define QLCNIC_DUMP_WRT_SAVED	BIT_6
#define QLCNIC_DUMP_MOD_SAVE_ST	BIT_7
#define QLCNIC_DUMP_SKIP	BIT_7

#define QLCNIC_DUMP_MASK_MAX	0xff

struct qlcnic_common_entry_hdr {
	u32     type;
	u32     offset;
	u32     cap_size;
	u8      mask;
	u8      rsvd[2];
	u8      flags;
} __packed;

struct __crb {
	u32	addr;
	u8	stride;
	u8	rsvd1[3];
	u32	data_size;
	u32	no_ops;
	u32	rsvd2[4];
} __packed;

struct __ctrl {
	u32	addr;
	u8	stride;
	u8	index_a;
	u16	timeout;
	u32	data_size;
	u32	no_ops;
	u8	opcode;
	u8	index_v;
	u8	shl_val;
	u8	shr_val;
	u32	val1;
	u32	val2;
	u32	val3;
} __packed;

struct __cache {
	u32	addr;
	u16	stride;
	u16	init_tag_val;
	u32	size;
	u32	no_ops;
	u32	ctrl_addr;
	u32	ctrl_val;
	u32	read_addr;
	u8	read_addr_stride;
	u8	read_addr_num;
	u8	rsvd1[2];
} __packed;

struct __ocm {
	u8	rsvd[8];
	u32	size;
	u32	no_ops;
	u8	rsvd1[8];
	u32	read_addr;
	u32	read_addr_stride;
} __packed;

struct __mem {
	u8	rsvd[24];
	u32	addr;
	u32	size;
} __packed;

struct __mux {
	u32	addr;
	u8	rsvd[4];
	u32	size;
	u32	no_ops;
	u32	val;
	u32	val_stride;
	u32	read_addr;
	u8	rsvd2[4];
} __packed;

struct __queue {
	u32	sel_addr;
	u16	stride;
	u8	rsvd[2];
	u32	size;
	u32	no_ops;
	u8	rsvd2[8];
	u32	read_addr;
	u8	read_addr_stride;
	u8	read_addr_cnt;
	u8	rsvd3[2];
} __packed;

struct __pollrd {
	u32	sel_addr;
	u32	read_addr;
	u32	sel_val;
	u16	sel_val_stride;
	u16	no_ops;
	u32	poll_wait;
	u32	poll_mask;
	u32	data_size;
	u8	rsvd[4];
} __packed;

struct __mux2 {
	u32	sel_addr1;
	u32	sel_addr2;
	u32	sel_val1;
	u32	sel_val2;
	u32	no_ops;
	u32	sel_val_mask;
	u32	read_addr;
	u8	sel_val_stride;
	u8	data_size;
	u8	rsvd[2];
} __packed;

struct __pollrdmwr {
	u32	addr1;
	u32	addr2;
	u32	val1;
	u32	val2;
	u32	poll_wait;
	u32	poll_mask;
	u32	mod_mask;
	u32	data_size;
} __packed;

struct qlcnic_dump_entry {
	struct qlcnic_common_entry_hdr hdr;
	union {
		struct __crb		crb;
		struct __cache		cache;
		struct __ocm		ocm;
		struct __mem		mem;
		struct __mux		mux;
		struct __queue		que;
		struct __ctrl		ctrl;
		struct __pollrdmwr	pollrdmwr;
		struct __mux2		mux2;
		struct __pollrd		pollrd;
	} region;
} __packed;

enum qlcnic_minidump_opcode {
	QLCNIC_DUMP_NOP		= 0,
	QLCNIC_DUMP_READ_CRB	= 1,
	QLCNIC_DUMP_READ_MUX	= 2,
	QLCNIC_DUMP_QUEUE	= 3,
	QLCNIC_DUMP_BRD_CONFIG	= 4,
	QLCNIC_DUMP_READ_OCM	= 6,
	QLCNIC_DUMP_PEG_REG	= 7,
	QLCNIC_DUMP_L1_DTAG	= 8,
	QLCNIC_DUMP_L1_ITAG	= 9,
	QLCNIC_DUMP_L1_DATA	= 11,
	QLCNIC_DUMP_L1_INST	= 12,
	QLCNIC_DUMP_L2_DTAG	= 21,
	QLCNIC_DUMP_L2_ITAG	= 22,
	QLCNIC_DUMP_L2_DATA	= 23,
	QLCNIC_DUMP_L2_INST	= 24,
	QLCNIC_DUMP_POLL_RD	= 35,
	QLCNIC_READ_MUX2	= 36,
	QLCNIC_READ_POLLRDMWR	= 37,
	QLCNIC_DUMP_READ_ROM	= 71,
	QLCNIC_DUMP_READ_MEM	= 72,
	QLCNIC_DUMP_READ_CTRL	= 98,
	QLCNIC_DUMP_TLHDR	= 99,
	QLCNIC_DUMP_RDEND	= 255
};

struct qlcnic_dump_operations {
	enum qlcnic_minidump_opcode opcode;
	u32 (*handler)(struct qlcnic_adapter *, struct qlcnic_dump_entry *,
		       __le32 *);
};

static u32 qlcnic_dump_crb(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i;
	u32 addr, data;
	struct __crb *crb = &entry->region.crb;

	addr = crb->addr;

	for (i = 0; i < crb->no_ops; i++) {
		data = qlcnic_ind_rd(adapter, addr);
		*buffer++ = cpu_to_le32(addr);
		*buffer++ = cpu_to_le32(data);
		addr += crb->stride;
	}
	return crb->no_ops * 2 * sizeof(u32);
}

static u32 qlcnic_dump_ctrl(struct qlcnic_adapter *adapter,
			    struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i, k, timeout = 0;
	u32 addr, data;
	u8 no_ops;
	struct __ctrl *ctr = &entry->region.ctrl;
	struct qlcnic_dump_template_hdr *t_hdr = adapter->ahw->fw_dump.tmpl_hdr;

	addr = ctr->addr;
	no_ops = ctr->no_ops;

	for (i = 0; i < no_ops; i++) {
		k = 0;
		for (k = 0; k < 8; k++) {
			if (!(ctr->opcode & (1 << k)))
				continue;
			switch (1 << k) {
			case QLCNIC_DUMP_WCRB:
				qlcnic_ind_wr(adapter, addr, ctr->val1);
				break;
			case QLCNIC_DUMP_RWCRB:
				data = qlcnic_ind_rd(adapter, addr);
				qlcnic_ind_wr(adapter, addr, data);
				break;
			case QLCNIC_DUMP_ANDCRB:
				data = qlcnic_ind_rd(adapter, addr);
				qlcnic_ind_wr(adapter, addr,
					      (data & ctr->val2));
				break;
			case QLCNIC_DUMP_ORCRB:
				data = qlcnic_ind_rd(adapter, addr);
				qlcnic_ind_wr(adapter, addr,
					      (data | ctr->val3));
				break;
			case QLCNIC_DUMP_POLLCRB:
				while (timeout <= ctr->timeout) {
					data = qlcnic_ind_rd(adapter, addr);
					if ((data & ctr->val2) == ctr->val1)
						break;
					usleep_range(1000, 2000);
					timeout++;
				}
				if (timeout > ctr->timeout) {
					dev_info(&adapter->pdev->dev,
					"Timed out, aborting poll CRB\n");
					return -EINVAL;
				}
				break;
			case QLCNIC_DUMP_RD_SAVE:
				if (ctr->index_a)
					addr = t_hdr->saved_state[ctr->index_a];
				data = qlcnic_ind_rd(adapter, addr);
				t_hdr->saved_state[ctr->index_v] = data;
				break;
			case QLCNIC_DUMP_WRT_SAVED:
				if (ctr->index_v)
					data = t_hdr->saved_state[ctr->index_v];
				else
					data = ctr->val1;
				if (ctr->index_a)
					addr = t_hdr->saved_state[ctr->index_a];
				qlcnic_ind_wr(adapter, addr, data);
				break;
			case QLCNIC_DUMP_MOD_SAVE_ST:
				data = t_hdr->saved_state[ctr->index_v];
				data <<= ctr->shl_val;
				data >>= ctr->shr_val;
				if (ctr->val2)
					data &= ctr->val2;
				data |= ctr->val3;
				data += ctr->val1;
				t_hdr->saved_state[ctr->index_v] = data;
				break;
			default:
				dev_info(&adapter->pdev->dev,
					 "Unknown opcode\n");
				break;
			}
		}
		addr += ctr->stride;
	}
	return 0;
}

static u32 qlcnic_dump_mux(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int loop;
	u32 val, data = 0;
	struct __mux *mux = &entry->region.mux;

	val = mux->val;
	for (loop = 0; loop < mux->no_ops; loop++) {
		qlcnic_ind_wr(adapter, mux->addr, val);
		data = qlcnic_ind_rd(adapter, mux->read_addr);
		*buffer++ = cpu_to_le32(val);
		*buffer++ = cpu_to_le32(data);
		val += mux->val_stride;
	}
	return 2 * mux->no_ops * sizeof(u32);
}

static u32 qlcnic_dump_que(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i, loop;
	u32 cnt, addr, data, que_id = 0;
	struct __queue *que = &entry->region.que;

	addr = que->read_addr;
	cnt = que->read_addr_cnt;

	for (loop = 0; loop < que->no_ops; loop++) {
		qlcnic_ind_wr(adapter, que->sel_addr, que_id);
		addr = que->read_addr;
		for (i = 0; i < cnt; i++) {
			data = qlcnic_ind_rd(adapter, addr);
			*buffer++ = cpu_to_le32(data);
			addr += que->read_addr_stride;
		}
		que_id += que->stride;
	}
	return que->no_ops * cnt * sizeof(u32);
}

static u32 qlcnic_dump_ocm(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i;
	u32 data;
	void __iomem *addr;
	struct __ocm *ocm = &entry->region.ocm;

	addr = adapter->ahw->pci_base0 + ocm->read_addr;
	for (i = 0; i < ocm->no_ops; i++) {
		data = readl(addr);
		*buffer++ = cpu_to_le32(data);
		addr += ocm->read_addr_stride;
	}
	return ocm->no_ops * sizeof(u32);
}

static u32 qlcnic_read_rom(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i, count = 0;
	u32 fl_addr, size, val, lck_val, addr;
	struct __mem *rom = &entry->region.mem;

	fl_addr = rom->addr;
	size = rom->size / 4;
lock_try:
	lck_val = QLC_SHARED_REG_RD32(adapter, QLCNIC_FLASH_LOCK);
	if (!lck_val && count < MAX_CTL_CHECK) {
		usleep_range(10000, 11000);
		count++;
		goto lock_try;
	}
	QLC_SHARED_REG_WR32(adapter, QLCNIC_FLASH_LOCK_OWNER,
			    adapter->ahw->pci_func);
	for (i = 0; i < size; i++) {
		addr = fl_addr & 0xFFFF0000;
		qlcnic_ind_wr(adapter, FLASH_ROM_WINDOW, addr);
		addr = LSW(fl_addr) + FLASH_ROM_DATA;
		val = qlcnic_ind_rd(adapter, addr);
		fl_addr += 4;
		*buffer++ = cpu_to_le32(val);
	}
	QLC_SHARED_REG_RD32(adapter, QLCNIC_FLASH_UNLOCK);
	return rom->size;
}

static u32 qlcnic_dump_l1_cache(struct qlcnic_adapter *adapter,
				struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i;
	u32 cnt, val, data, addr;
	struct __cache *l1 = &entry->region.cache;

	val = l1->init_tag_val;

	for (i = 0; i < l1->no_ops; i++) {
		qlcnic_ind_wr(adapter, l1->addr, val);
		qlcnic_ind_wr(adapter, l1->ctrl_addr, LSW(l1->ctrl_val));
		addr = l1->read_addr;
		cnt = l1->read_addr_num;
		while (cnt) {
			data = qlcnic_ind_rd(adapter, addr);
			*buffer++ = cpu_to_le32(data);
			addr += l1->read_addr_stride;
			cnt--;
		}
		val += l1->stride;
	}
	return l1->no_ops * l1->read_addr_num * sizeof(u32);
}

static u32 qlcnic_dump_l2_cache(struct qlcnic_adapter *adapter,
				struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	int i;
	u32 cnt, val, data, addr;
	u8 poll_mask, poll_to, time_out = 0;
	struct __cache *l2 = &entry->region.cache;

	val = l2->init_tag_val;
	poll_mask = LSB(MSW(l2->ctrl_val));
	poll_to = MSB(MSW(l2->ctrl_val));

	for (i = 0; i < l2->no_ops; i++) {
		qlcnic_ind_wr(adapter, l2->addr, val);
		if (LSW(l2->ctrl_val))
			qlcnic_ind_wr(adapter, l2->ctrl_addr,
				      LSW(l2->ctrl_val));
		if (!poll_mask)
			goto skip_poll;
		do {
			data = qlcnic_ind_rd(adapter, l2->ctrl_addr);
			if (!(data & poll_mask))
				break;
			usleep_range(1000, 2000);
			time_out++;
		} while (time_out <= poll_to);

		if (time_out > poll_to) {
			dev_err(&adapter->pdev->dev,
				"Timeout exceeded in %s, aborting dump\n",
				__func__);
			return -EINVAL;
		}
skip_poll:
		addr = l2->read_addr;
		cnt = l2->read_addr_num;
		while (cnt) {
			data = qlcnic_ind_rd(adapter, addr);
			*buffer++ = cpu_to_le32(data);
			addr += l2->read_addr_stride;
			cnt--;
		}
		val += l2->stride;
	}
	return l2->no_ops * l2->read_addr_num * sizeof(u32);
}

static u32 qlcnic_read_memory(struct qlcnic_adapter *adapter,
			      struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	u32 addr, data, test, ret = 0;
	int i, reg_read;
	struct __mem *mem = &entry->region.mem;

	reg_read = mem->size;
	addr = mem->addr;
	/* check for data size of multiple of 16 and 16 byte alignment */
	if ((addr & 0xf) || (reg_read%16)) {
		dev_info(&adapter->pdev->dev,
			 "Unaligned memory addr:0x%x size:0x%x\n",
			 addr, reg_read);
		return -EINVAL;
	}

	mutex_lock(&adapter->ahw->mem_lock);

	while (reg_read != 0) {
		qlcnic_ind_wr(adapter, QLCNIC_MS_ADDR_LO, addr);
		qlcnic_ind_wr(adapter, QLCNIC_MS_ADDR_HI, 0);
		qlcnic_ind_wr(adapter, QLCNIC_MS_CTRL, QLCNIC_TA_START_ENABLE);

		for (i = 0; i < MAX_CTL_CHECK; i++) {
			test = qlcnic_ind_rd(adapter, QLCNIC_MS_CTRL);
			if (!(test & TA_CTL_BUSY))
				break;
		}
		if (i == MAX_CTL_CHECK) {
			if (printk_ratelimit()) {
				dev_err(&adapter->pdev->dev,
					"failed to read through agent\n");
				ret = -EINVAL;
				goto out;
			}
		}
		for (i = 0; i < 4; i++) {
			data = qlcnic_ind_rd(adapter, qlcnic_ms_read_data[i]);
			*buffer++ = cpu_to_le32(data);
		}
		addr += 16;
		reg_read -= 16;
		ret += 16;
	}
out:
	mutex_unlock(&adapter->ahw->mem_lock);
	return mem->size;
}

static u32 qlcnic_dump_nop(struct qlcnic_adapter *adapter,
			   struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	entry->hdr.flags |= QLCNIC_DUMP_SKIP;
	return 0;
}

static int qlcnic_valid_dump_entry(struct device *dev,
				   struct qlcnic_dump_entry *entry, u32 size)
{
	int ret = 1;
	if (size != entry->hdr.cap_size) {
		dev_err(dev,
			"Invalid entry, Type:%d\tMask:%d\tSize:%dCap_size:%d\n",
			entry->hdr.type, entry->hdr.mask, size,
			entry->hdr.cap_size);
		ret = 0;
	}
	return ret;
}

static u32 qlcnic_read_pollrdmwr(struct qlcnic_adapter *adapter,
				 struct qlcnic_dump_entry *entry,
				 __le32 *buffer)
{
	struct __pollrdmwr *poll = &entry->region.pollrdmwr;
	u32 data, wait_count, poll_wait, temp;

	poll_wait = poll->poll_wait;

	qlcnic_ind_wr(adapter, poll->addr1, poll->val1);
	wait_count = 0;

	while (wait_count < poll_wait) {
		data = qlcnic_ind_rd(adapter, poll->addr1);
		if ((data & poll->poll_mask) != 0)
			break;
		wait_count++;
	}

	if (wait_count == poll_wait) {
		dev_err(&adapter->pdev->dev,
			"Timeout exceeded in %s, aborting dump\n",
			__func__);
		return 0;
	}

	data = qlcnic_ind_rd(adapter, poll->addr2) & poll->mod_mask;
	qlcnic_ind_wr(adapter, poll->addr2, data);
	qlcnic_ind_wr(adapter, poll->addr1, poll->val2);
	wait_count = 0;

	while (wait_count < poll_wait) {
		temp = qlcnic_ind_rd(adapter, poll->addr1);
		if ((temp & poll->poll_mask) != 0)
			break;
		wait_count++;
	}

	*buffer++ = cpu_to_le32(poll->addr2);
	*buffer++ = cpu_to_le32(data);

	return 2 * sizeof(u32);

}

static u32 qlcnic_read_pollrd(struct qlcnic_adapter *adapter,
			      struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	struct __pollrd *pollrd = &entry->region.pollrd;
	u32 data, wait_count, poll_wait, sel_val;
	int i;

	poll_wait = pollrd->poll_wait;
	sel_val = pollrd->sel_val;

	for (i = 0; i < pollrd->no_ops; i++) {
		qlcnic_ind_wr(adapter, pollrd->sel_addr, sel_val);
		wait_count = 0;
		while (wait_count < poll_wait) {
			data = qlcnic_ind_rd(adapter, pollrd->sel_addr);
			if ((data & pollrd->poll_mask) != 0)
				break;
			wait_count++;
		}

		if (wait_count == poll_wait) {
			dev_err(&adapter->pdev->dev,
				"Timeout exceeded in %s, aborting dump\n",
				__func__);
			return 0;
		}

		data = qlcnic_ind_rd(adapter, pollrd->read_addr);
		*buffer++ = cpu_to_le32(sel_val);
		*buffer++ = cpu_to_le32(data);
		sel_val += pollrd->sel_val_stride;
	}
	return pollrd->no_ops * (2 * sizeof(u32));
}

static u32 qlcnic_read_mux2(struct qlcnic_adapter *adapter,
			    struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	struct __mux2 *mux2 = &entry->region.mux2;
	u32 data;
	u32 t_sel_val, sel_val1, sel_val2;
	int i;

	sel_val1 = mux2->sel_val1;
	sel_val2 = mux2->sel_val2;

	for (i = 0; i < mux2->no_ops; i++) {
		qlcnic_ind_wr(adapter, mux2->sel_addr1, sel_val1);
		t_sel_val = sel_val1 & mux2->sel_val_mask;
		qlcnic_ind_wr(adapter, mux2->sel_addr2, t_sel_val);
		data = qlcnic_ind_rd(adapter, mux2->read_addr);
		*buffer++ = cpu_to_le32(t_sel_val);
		*buffer++ = cpu_to_le32(data);
		qlcnic_ind_wr(adapter, mux2->sel_addr1, sel_val2);
		t_sel_val = sel_val2 & mux2->sel_val_mask;
		qlcnic_ind_wr(adapter, mux2->sel_addr2, t_sel_val);
		data = qlcnic_ind_rd(adapter, mux2->read_addr);
		*buffer++ = cpu_to_le32(t_sel_val);
		*buffer++ = cpu_to_le32(data);
		sel_val1 += mux2->sel_val_stride;
		sel_val2 += mux2->sel_val_stride;
	}

	return mux2->no_ops * (4 * sizeof(u32));
}

static u32 qlcnic_83xx_dump_rom(struct qlcnic_adapter *adapter,
				struct qlcnic_dump_entry *entry, __le32 *buffer)
{
	u32 fl_addr, size;
	struct __mem *rom = &entry->region.mem;

	fl_addr = rom->addr;
	size = rom->size / 4;

	if (!qlcnic_83xx_lockless_flash_read32(adapter, fl_addr,
					       (u8 *)buffer, size))
		return rom->size;

	return 0;
}

static const struct qlcnic_dump_operations qlcnic_fw_dump_ops[] = {
	{QLCNIC_DUMP_NOP, qlcnic_dump_nop},
	{QLCNIC_DUMP_READ_CRB, qlcnic_dump_crb},
	{QLCNIC_DUMP_READ_MUX, qlcnic_dump_mux},
	{QLCNIC_DUMP_QUEUE, qlcnic_dump_que},
	{QLCNIC_DUMP_BRD_CONFIG, qlcnic_read_rom},
	{QLCNIC_DUMP_READ_OCM, qlcnic_dump_ocm},
	{QLCNIC_DUMP_PEG_REG, qlcnic_dump_ctrl},
	{QLCNIC_DUMP_L1_DTAG, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_ITAG, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_DATA, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_INST, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L2_DTAG, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_ITAG, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_DATA, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_INST, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_READ_ROM, qlcnic_read_rom},
	{QLCNIC_DUMP_READ_MEM, qlcnic_read_memory},
	{QLCNIC_DUMP_READ_CTRL, qlcnic_dump_ctrl},
	{QLCNIC_DUMP_TLHDR, qlcnic_dump_nop},
	{QLCNIC_DUMP_RDEND, qlcnic_dump_nop},
};

static const struct qlcnic_dump_operations qlcnic_83xx_fw_dump_ops[] = {
	{QLCNIC_DUMP_NOP, qlcnic_dump_nop},
	{QLCNIC_DUMP_READ_CRB, qlcnic_dump_crb},
	{QLCNIC_DUMP_READ_MUX, qlcnic_dump_mux},
	{QLCNIC_DUMP_QUEUE, qlcnic_dump_que},
	{QLCNIC_DUMP_BRD_CONFIG, qlcnic_83xx_dump_rom},
	{QLCNIC_DUMP_READ_OCM, qlcnic_dump_ocm},
	{QLCNIC_DUMP_PEG_REG, qlcnic_dump_ctrl},
	{QLCNIC_DUMP_L1_DTAG, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_ITAG, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_DATA, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L1_INST, qlcnic_dump_l1_cache},
	{QLCNIC_DUMP_L2_DTAG, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_ITAG, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_DATA, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_L2_INST, qlcnic_dump_l2_cache},
	{QLCNIC_DUMP_POLL_RD, qlcnic_read_pollrd},
	{QLCNIC_READ_MUX2, qlcnic_read_mux2},
	{QLCNIC_READ_POLLRDMWR, qlcnic_read_pollrdmwr},
	{QLCNIC_DUMP_READ_ROM, qlcnic_83xx_dump_rom},
	{QLCNIC_DUMP_READ_MEM, qlcnic_read_memory},
	{QLCNIC_DUMP_READ_CTRL, qlcnic_dump_ctrl},
	{QLCNIC_DUMP_TLHDR, qlcnic_dump_nop},
	{QLCNIC_DUMP_RDEND, qlcnic_dump_nop},
};

static uint32_t qlcnic_temp_checksum(uint32_t *temp_buffer, u32 temp_size)
{
	uint64_t sum = 0;
	int count = temp_size / sizeof(uint32_t);
	while (count-- > 0)
		sum += *temp_buffer++;
	while (sum >> 32)
		sum = (sum & 0xFFFFFFFF) + (sum >> 32);
	return ~sum;
}

static int qlcnic_fw_flash_get_minidump_temp(struct qlcnic_adapter *adapter,
					     u8 *buffer, u32 size)
{
	int ret = 0;

	if (qlcnic_82xx_check(adapter))
		return -EIO;

	if (qlcnic_83xx_lock_flash(adapter))
		return -EIO;

	ret = qlcnic_83xx_lockless_flash_read32(adapter,
						QLC_83XX_MINIDUMP_FLASH,
						buffer, size / sizeof(u32));

	qlcnic_83xx_unlock_flash(adapter);

	return ret;
}

static int
qlcnic_fw_flash_get_minidump_temp_size(struct qlcnic_adapter *adapter,
				       struct qlcnic_cmd_args *cmd)
{
	struct qlcnic_dump_template_hdr tmp_hdr;
	u32 size = sizeof(struct qlcnic_dump_template_hdr) / sizeof(u32);
	int ret = 0;

	if (qlcnic_82xx_check(adapter))
		return -EIO;

	if (qlcnic_83xx_lock_flash(adapter))
		return -EIO;

	ret = qlcnic_83xx_lockless_flash_read32(adapter,
						QLC_83XX_MINIDUMP_FLASH,
						(u8 *)&tmp_hdr, size);

	qlcnic_83xx_unlock_flash(adapter);

	cmd->rsp.arg[2] = tmp_hdr.size;
	cmd->rsp.arg[3] = tmp_hdr.version;

	return ret;
}

static int qlcnic_fw_get_minidump_temp_size(struct qlcnic_adapter *adapter,
					    u32 *version, u32 *temp_size,
					    u8 *use_flash_temp)
{
	int err = 0;
	struct qlcnic_cmd_args cmd;

	if (qlcnic_alloc_mbx_args(&cmd, adapter, QLCNIC_CMD_TEMP_SIZE))
		return -ENOMEM;

	err = qlcnic_issue_cmd(adapter, &cmd);
	if (err != QLCNIC_RCODE_SUCCESS) {
		if (qlcnic_fw_flash_get_minidump_temp_size(adapter, &cmd)) {
			qlcnic_free_mbx_args(&cmd);
			return -EIO;
		}
		*use_flash_temp = 1;
	}

	*temp_size = cmd.rsp.arg[2];
	*version = cmd.rsp.arg[3];
	qlcnic_free_mbx_args(&cmd);

	if (!(*temp_size))
		return -EIO;

	return 0;
}

static int __qlcnic_fw_cmd_get_minidump_temp(struct qlcnic_adapter *adapter,
					     u32 *buffer, u32 temp_size)
{
	int err = 0, i;
	void *tmp_addr;
	__le32 *tmp_buf;
	struct qlcnic_cmd_args cmd;
	dma_addr_t tmp_addr_t = 0;

	tmp_addr = dma_alloc_coherent(&adapter->pdev->dev, temp_size,
				      &tmp_addr_t, GFP_KERNEL);
	if (!tmp_addr)
		return -ENOMEM;

	if (qlcnic_alloc_mbx_args(&cmd, adapter, QLCNIC_CMD_GET_TEMP_HDR)) {
		err = -ENOMEM;
		goto free_mem;
	}

	cmd.req.arg[1] = LSD(tmp_addr_t);
	cmd.req.arg[2] = MSD(tmp_addr_t);
	cmd.req.arg[3] = temp_size;
	err = qlcnic_issue_cmd(adapter, &cmd);

	tmp_buf = tmp_addr;
	if (err == QLCNIC_RCODE_SUCCESS) {
		for (i = 0; i < temp_size / sizeof(u32); i++)
			*buffer++ = __le32_to_cpu(*tmp_buf++);
	}

	qlcnic_free_mbx_args(&cmd);

free_mem:
	dma_free_coherent(&adapter->pdev->dev, temp_size, tmp_addr, tmp_addr_t);

	return err;
}

int qlcnic_fw_cmd_get_minidump_temp(struct qlcnic_adapter *adapter)
{
	int err;
	u32 temp_size = 0;
	u32 version, csum, *tmp_buf;
	struct qlcnic_hardware_context *ahw;
	struct qlcnic_dump_template_hdr *tmpl_hdr;
	u8 use_flash_temp = 0;

	ahw = adapter->ahw;

	err = qlcnic_fw_get_minidump_temp_size(adapter, &version, &temp_size,
					       &use_flash_temp);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Can't get template size %d\n", err);
		return -EIO;
	}

	ahw->fw_dump.tmpl_hdr = vzalloc(temp_size);
	if (!ahw->fw_dump.tmpl_hdr)
		return -ENOMEM;

	tmp_buf = (u32 *)ahw->fw_dump.tmpl_hdr;
	if (use_flash_temp)
		goto flash_temp;

	err = __qlcnic_fw_cmd_get_minidump_temp(adapter, tmp_buf, temp_size);

	if (err) {
flash_temp:
		err = qlcnic_fw_flash_get_minidump_temp(adapter, (u8 *)tmp_buf,
							temp_size);

		if (err) {
			dev_err(&adapter->pdev->dev,
				"Failed to get minidump template header %d\n",
				err);
			vfree(ahw->fw_dump.tmpl_hdr);
			ahw->fw_dump.tmpl_hdr = NULL;
			return -EIO;
		}
	}

	csum = qlcnic_temp_checksum((uint32_t *)tmp_buf, temp_size);

	if (csum) {
		dev_err(&adapter->pdev->dev,
			"Template header checksum validation failed\n");
		vfree(ahw->fw_dump.tmpl_hdr);
		ahw->fw_dump.tmpl_hdr = NULL;
		return -EIO;
	}

	tmpl_hdr = ahw->fw_dump.tmpl_hdr;
	tmpl_hdr->drv_cap_mask = QLCNIC_DUMP_MASK_DEF;
	ahw->fw_dump.enable = 1;

	return 0;
}

int qlcnic_dump_fw(struct qlcnic_adapter *adapter)
{
	__le32 *buffer;
	u32 ocm_window;
	char mesg[64];
	char *msg[] = {mesg, NULL};
	int i, k, ops_cnt, ops_index, dump_size = 0;
	u32 entry_offset, dump, no_entries, buf_offset = 0;
	struct qlcnic_dump_entry *entry;
	struct qlcnic_fw_dump *fw_dump = &adapter->ahw->fw_dump;
	struct qlcnic_dump_template_hdr *tmpl_hdr = fw_dump->tmpl_hdr;
	const struct qlcnic_dump_operations *fw_dump_ops;
	struct qlcnic_hardware_context *ahw;

	ahw = adapter->ahw;

	if (!fw_dump->enable) {
		dev_info(&adapter->pdev->dev, "Dump not enabled\n");
		return -EIO;
	}

	if (fw_dump->clr) {
		dev_info(&adapter->pdev->dev,
			 "Previous dump not cleared, not capturing dump\n");
		return -EIO;
	}

	netif_info(adapter->ahw, drv, adapter->netdev, "Take FW dump\n");
	/* Calculate the size for dump data area only */
	for (i = 2, k = 1; (i & QLCNIC_DUMP_MASK_MAX); i <<= 1, k++)
		if (i & tmpl_hdr->drv_cap_mask)
			dump_size += tmpl_hdr->cap_sizes[k];
	if (!dump_size)
		return -EIO;

	fw_dump->data = vzalloc(dump_size);
	if (!fw_dump->data)
		return -ENOMEM;

	buffer = fw_dump->data;
	fw_dump->size = dump_size;
	no_entries = tmpl_hdr->num_entries;
	entry_offset = tmpl_hdr->offset;
	tmpl_hdr->sys_info[0] = QLCNIC_DRIVER_VERSION;
	tmpl_hdr->sys_info[1] = adapter->fw_version;

	if (qlcnic_82xx_check(adapter)) {
		ops_cnt = ARRAY_SIZE(qlcnic_fw_dump_ops);
		fw_dump_ops = qlcnic_fw_dump_ops;
	} else {
		ops_cnt = ARRAY_SIZE(qlcnic_83xx_fw_dump_ops);
		fw_dump_ops = qlcnic_83xx_fw_dump_ops;
		ocm_window = tmpl_hdr->ocm_wnd_reg[adapter->ahw->pci_func];
		tmpl_hdr->saved_state[QLC_83XX_OCM_INDEX] = ocm_window;
		tmpl_hdr->saved_state[QLC_83XX_PCI_INDEX] = ahw->pci_func;
	}

	for (i = 0; i < no_entries; i++) {
		entry = (void *)tmpl_hdr + entry_offset;
		if (!(entry->hdr.mask & tmpl_hdr->drv_cap_mask)) {
			entry->hdr.flags |= QLCNIC_DUMP_SKIP;
			entry_offset += entry->hdr.offset;
			continue;
		}

		/* Find the handler for this entry */
		ops_index = 0;
		while (ops_index < ops_cnt) {
			if (entry->hdr.type == fw_dump_ops[ops_index].opcode)
				break;
			ops_index++;
		}

		if (ops_index == ops_cnt) {
			dev_info(&adapter->pdev->dev,
				 "Invalid entry type %d, exiting dump\n",
				 entry->hdr.type);
			goto error;
		}

		/* Collect dump for this entry */
		dump = fw_dump_ops[ops_index].handler(adapter, entry, buffer);
		if (!qlcnic_valid_dump_entry(&adapter->pdev->dev, entry, dump))
			entry->hdr.flags |= QLCNIC_DUMP_SKIP;
		buf_offset += entry->hdr.cap_size;
		entry_offset += entry->hdr.offset;
		buffer = fw_dump->data + buf_offset;
	}
	if (dump_size != buf_offset) {
		dev_info(&adapter->pdev->dev,
			 "Captured(%d) and expected size(%d) do not match\n",
			 buf_offset, dump_size);
		goto error;
	} else {
		fw_dump->clr = 1;
		snprintf(mesg, sizeof(mesg), "FW_DUMP=%s",
			 adapter->netdev->name);
		dev_info(&adapter->pdev->dev, "%s: Dump data, %d bytes captured\n",
			 adapter->netdev->name, fw_dump->size);
		/* Send a udev event to notify availability of FW dump */
		kobject_uevent_env(&adapter->pdev->dev.kobj, KOBJ_CHANGE, msg);
		return 0;
	}
error:
	vfree(fw_dump->data);
	return -EINVAL;
}

void qlcnic_83xx_get_minidump_template(struct qlcnic_adapter *adapter)
{
	u32 prev_version, current_version;
	struct qlcnic_hardware_context *ahw = adapter->ahw;
	struct qlcnic_fw_dump *fw_dump = &ahw->fw_dump;
	struct pci_dev *pdev = adapter->pdev;

	prev_version = adapter->fw_version;
	current_version = qlcnic_83xx_get_fw_version(adapter);

	if (fw_dump->tmpl_hdr == NULL || current_version > prev_version) {
		if (fw_dump->tmpl_hdr)
			vfree(fw_dump->tmpl_hdr);
		if (!qlcnic_fw_cmd_get_minidump_temp(adapter))
			dev_info(&pdev->dev, "Supports FW dump capability\n");
	}
}
