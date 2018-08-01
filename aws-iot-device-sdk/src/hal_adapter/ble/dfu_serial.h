/*
 * dfu_serial.h
 *
 * Created: 14/11/2017 下午 4:03:42
 *  Author: NSC
 */ 


#ifndef DFU_SERIAL_H_
#define DFU_SERIAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/* serial DFU (Device Firmware Updates) command*/
enum dfu_opcode{
    dfu_create = 0x01,
	dfu_PRN = 0x02,
	dfu_crc = 0x03,
	dfu_exe = 0x04,
	dfu_select = 0x06,
	dfu_MTU = 0x07,
	dfu_write = 0x08,
	dfu_ping = 0x09,
	dfu_resp = 0x60
};

enum dfu_erro{
	invalid_code = 0x00,
	success = 0x01,
	not_support = 0x02,
	invalid_param = 0x03,
	insufficient_resource = 0x04,
	invalid_object = 0x05,
	unsupport_type = 0x07,
	not_permit = 0x08,
	fail = 0x0A
};

struct dfu_op_resp{
	enum dfu_opcode resp_code;  //MUST be 0x60
	enum dfu_opcode req_code;
	enum dfu_erro res;
};

enum dfu_object{
	cmd_obj = 0x01,
	data_obj = 0x02
};

#define DEFAULT_PRN     0
#define RESP_VAL_OFFET  3  // sizeof( struct dfu_op_resp )

int ble_dfu(enum dfu_object obj, const uint32_t total_size, const uint8_t* data, const uint16_t size);

#ifdef __cplusplus
}
#endif
#endif /* DFU_SERIAL_H_ */
