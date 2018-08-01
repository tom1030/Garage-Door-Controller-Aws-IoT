/*
 * dfu_serial.c
 *
 * Created: 14/11/2017 下午 4:03:27
 *  Author: NSC
 */ 
#include <asf.h>

#include "dfu_serial.h"
#include "ble_serial_cmd.h"

#include "timer_interface.h"

#define SLIP_BYTE_END             0300    //0xC0  /* indicates end of packet */
#define SLIP_BYTE_ESC             0333    //0xDB  /* indicates byte stuffing */
#define SLIP_BYTE_ESC_END         0334    //0xDC  /* ESC ESC_END means END data byte */
#define SLIP_BYTE_ESC_ESC         0335    //0xDD  /* ESC ESC_ESC means ESC data byte */

/** @brief Status information that is used while receiving and decoding a packet. */
typedef enum
{
  SLIP_STATE_DECODING, //!< Ready to receive the next byte.
  SLIP_STATE_ESC_RECEIVED, //!< An ESC byte has been received and the next byte must be decoded differently.
  SLIP_STATE_CLEARING_INVALID_PACKET //!< The received data is invalid and transfer must be restarted.
} slip_read_state_t;

  /** @brief Representation of a SLIP packet. */
typedef struct
{
  slip_read_state_t   state; //!< Current state of the packet (see @ref slip_read_state_t).

  uint8_t             * p_buffer; //!< Decoded data.
  uint32_t            current_index; //!< Current length of the packet that has been received.
  uint32_t            buffer_len; //!< Size of the buffer that is available.
} slip_t;

static int _slip_encode(uint8_t * p_output,  uint8_t * p_input, uint32_t input_length, uint32_t * p_output_buffer_length)
{
    if (p_output == NULL || p_input == NULL || p_output_buffer_length == NULL)
    {
        return -1;
    }

    *p_output_buffer_length = 0;
    uint32_t input_index;

    for (input_index = 0; input_index < input_length; input_index++)
    {
        switch (p_input[input_index])
        {
            case SLIP_BYTE_END:
                p_output[(*p_output_buffer_length)++] = SLIP_BYTE_ESC;
                p_output[(*p_output_buffer_length)++] = SLIP_BYTE_ESC_END;
                break;

            case SLIP_BYTE_ESC:
                p_output[(*p_output_buffer_length)++] = SLIP_BYTE_ESC;
                p_output[(*p_output_buffer_length)++] = SLIP_BYTE_ESC_ESC;
                break;

            default:
                p_output[(*p_output_buffer_length)++] = p_input[input_index];
        }
    }
    p_output[(*p_output_buffer_length)++] = SLIP_BYTE_END;

    return 0;
}

static int _slip_decode_add_byte(slip_t * p_slip, uint8_t c)
{
    if (p_slip == NULL)
    {
        return -1;
    }

    if (p_slip->current_index == p_slip->buffer_len)
    {
        return -2;// no enough memory
    }

    switch (p_slip->state)
    {
        case SLIP_STATE_DECODING:
            switch (c)
            {
                case SLIP_BYTE_END:
                    // finished reading packet
                    return 0;

                case SLIP_BYTE_ESC:
                    // wait for
                    p_slip->state = SLIP_STATE_ESC_RECEIVED;
                    break;

                default:
                    // add byte to buffer
                    p_slip->p_buffer[p_slip->current_index++] = c;
                    break;
            }
            break;

        case SLIP_STATE_ESC_RECEIVED:
            switch (c)
            {
                case SLIP_BYTE_ESC_END:
                    p_slip->p_buffer[p_slip->current_index++] = SLIP_BYTE_END;
                    p_slip->state = SLIP_STATE_DECODING;
                    break;

                case SLIP_BYTE_ESC_ESC:
                    p_slip->p_buffer[p_slip->current_index++] = SLIP_BYTE_ESC;
                    p_slip->state = SLIP_STATE_DECODING;
                    break;

                default:
                    // protocol violation
                    p_slip->state = SLIP_STATE_CLEARING_INVALID_PACKET;
                    return -1;
            }
            break;

        case SLIP_STATE_CLEARING_INVALID_PACKET:
            if (c == SLIP_BYTE_END)
            {
                p_slip->state = SLIP_STATE_DECODING;
                p_slip->current_index = 0;
            }
            break;
    }

    return -1;
}

/**@brief Function for calculating CRC-32 in blocks.
 *
 * Feed each consecutive data block into this function, along with the current value of p_crc as
 * returned by the previous call of this function. The first call of this function should pass NULL
 * as the initial value of the crc in p_crc.
 *
 * @param[in] p_data The input data block for computation.
 * @param[in] size   The size of the input data block in bytes.
 * @param[in] p_crc  The previous calculated CRC-32 value or NULL if first call.
 *
 * @return The updated CRC-32 value, based on the input supplied.
 */

static int _crc32_compute(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc)
{
    uint32_t crc;

    crc = (p_crc == NULL) ? 0xFFFFFFFF : ~(*p_crc);
    for (uint32_t i = 0; i < size; i++)
    {
        crc = crc ^ p_data[i];
        for (uint32_t j = 8; j > 0; j--)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & ((crc & 1) ? 0xFFFFFFFF : 0));
        }
    }
    return ~crc;
}

static int ble_dfu_opcode(uint8_t* op_req,uint8_t req_len,uint8_t* val_resp,uint8_t resp_len){
    uint8_t slip_input[RESP_VAL_OFFET + resp_len + 1]; //(0x60+req_opcode+result_code) + resp_value + SLIP_BYTE_END
	uint8_t slip_output[req_len * 2];
	uint32_t slip_out_len = 0;
	slip_t slip;
	uint16_t temp;
	int ret = -1;
 	enum dfu_opcode opcode;
	Timer resp_tmr;

	if((op_req == NULL) || (req_len == 0))
		return -1;

	opcode = op_req[0];
	
	_slip_encode(slip_output,op_req,req_len,&slip_out_len);
	if(STATUS_OK != usart_write_buffer_wait(&ble_uart_module, slip_output, slip_out_len)){
		return -1;
	}

	if(opcode == dfu_write)// don't need to wait response when write packet as PRN default is 0
		return 0;
	
	slip.state = SLIP_STATE_DECODING;
	slip.p_buffer = slip_input;
	slip.current_index = 0;
	slip.buffer_len = sizeof(slip_input);

	init_timer(&resp_tmr);
	countdown_ms(&resp_tmr,10); //wait 10 ms
	while(!has_timer_expired(&resp_tmr)){
		if(STATUS_OK == usart_read_wait(&ble_uart_module, &temp)){
			ret = _slip_decode_add_byte(&slip,(uint8_t)temp);
			if(ret == 0){
				struct dfu_op_resp* op_resp = (struct dfu_op_resp*)slip.p_buffer;

				if((op_resp->resp_code == dfu_resp) && (op_resp->req_code == opcode)){
					if(op_resp->res != success){
						printf("dfu error %d\r\n",op_resp->res);
						return op_resp->res;
					} else if(val_resp != NULL){
						if(slip.current_index <= resp_len + RESP_VAL_OFFET + 1)
							memcpy(val_resp,slip.p_buffer + RESP_VAL_OFFET,resp_len);
						else
							return -1;
					}
				}
				break;
			} else if(ret == -2){
				slip.state = SLIP_STATE_DECODING;
                slip.current_index = 0;
				return -2;
			}
		}
	}
	return 0;
}

int ble_dfu(enum dfu_object obj, const uint32_t total_size, const uint8_t* data, const uint16_t size){
	
	uint8_t resp_val[12]; //max size of response value is 12 for select opcode
	enum dfu_opcode opmtu = dfu_MTU;

	uint32_t max_size,offset,crc32_resp,crc32_offset;
	uint16_t mtu;
	
	static uint32_t crc32 = 0x00000000;
	static uint32_t packet_size = 0;
	uint16_t cnt = 0;
	
	if(((obj != cmd_obj) && (obj != data_obj)) || 
		(data == NULL) || (total_size == 0) || (size == 0))
		return -1;

	/* get MTU */
	if(0 != ble_dfu_opcode(&opmtu,1,resp_val,2))
		return -1;
	mtu = resp_val[0] | (resp_val[1] << 8);
	mtu = mtu / 2 - 2;
	
	uint8_t op_req[1 + mtu];   //opcode + data;
	
	/* check last object*/
	cnt = 0;
	op_req[cnt++] = dfu_select;
	op_req[cnt++] = obj; //command object
	if(0 != ble_dfu_opcode(op_req,cnt,resp_val,12)){
		return -1;
	}
	cnt = 0;
	//LSB
	max_size = resp_val[cnt++] | (resp_val[cnt++] << 8) |
		(resp_val[cnt++] << 16) | (resp_val[cnt++] << 24);
		
	offset = resp_val[cnt++] | (resp_val[cnt++] << 8) |
		(resp_val[cnt++] << 16) | (resp_val[cnt++] << 24);
		
	crc32_resp = resp_val[cnt++] | (resp_val[cnt++] << 8) |
		(resp_val[cnt++] << 16) | (resp_val[cnt++] << 24);

	crc32_offset = crc32;
	crc32 = _crc32_compute(data,size,&crc32);
	
	/* check if already transfer total size of valid init or data packet*/
	if((offset != total_size) || (crc32_resp != crc32)){
		
		if((offset % packet_size == 0)
			|| (offset > total_size) 
			|| (crc32_resp != crc32_offset)){
			packet_size = 0;
		}
		if(packet_size == 0){
			if(total_size < max_size)
				packet_size = total_size;
			else {
				if(total_size - offset > max_size)
					packet_size = max_size;
				else
					packet_size = total_size - offset;
			}

			/* create new object and transfers packet*/
			cnt = 0;
			op_req[cnt++] = dfu_create;
			op_req[cnt++] = obj; //command object
			//packet size,LSB
			op_req[cnt++] = packet_size & 0xFF;
			op_req[cnt++] = (packet_size >> 8) & 0xFF;
			op_req[cnt++] = (packet_size >> 16) & 0xFF;
			op_req[cnt++] = (packet_size >> 24) & 0xFF;
			
			if(0 != ble_dfu_opcode(op_req,cnt,NULL,0)){
				return -1;
			}
		}
		/* transfer data in unit of mtu size*/
		uint16_t size_write = 0;
		while(size_write < size){
			
			cnt = 0;
			op_req[cnt++] = dfu_write;

			if(size - size_write >= mtu){
				memcpy(&op_req[cnt],data + size_write,mtu);
				cnt += mtu;
				size_write += mtu;
			} else {
				memcpy(&op_req[cnt],data + size_write,size - size_write);
				cnt += size - size_write;
				size_write += size - size_write;
			}
			if(0 != ble_dfu_opcode(op_req,cnt,NULL,0))
				return -1;
		}
		
		/* calculate CRC*/
		cnt = 0;
		op_req[cnt++] = dfu_crc;
		if(0 != ble_dfu_opcode(op_req,cnt,resp_val,8))
			return -1;

		cnt = 0;
		offset = resp_val[cnt++] | (resp_val[cnt++] << 8) |
			(resp_val[cnt++] << 16) | (resp_val[cnt++] << 24);
		
		crc32_resp = resp_val[cnt++] | (resp_val[cnt++] << 8) |
			(resp_val[cnt++] << 16) | (resp_val[cnt++] << 24);
		

		if(((offset % packet_size != 0) && (offset != total_size)) 
			|| (crc32_resp != crc32)){
			return 0;
		}
	}

	/* execute command*/
	cnt = 0;
	op_req[cnt++] = dfu_exe;
	if(0 != ble_dfu_opcode(op_req,cnt,NULL,0))
		return -1;
	if(offset == total_size)
		crc32 = 0x00000000;
	
	return 0;
}

