#include <asf.h>

#include "periphs.h"

static PortGroup * clk_port_base;
static uint32_t clk_pin_mask;
static PortGroup * data_port_base;
static uint32_t data_pin_mask;


typedef void (*fp) (void);

#define clk_high() clk_port_base->OUTSET.reg = clk_pin_mask
#define clk_low() clk_port_base->OUTCLR.reg = clk_pin_mask

#define isDATA_Ch() (data_port_base->IN.reg & data_pin_mask)

// NOTE:  all timing need fine tune by scope later !!!
#define nop()   __ASM volatile ("nop")

void __attribute__((optimize("O0"))) send_0 (void){
	clk_high();
	nop();nop();nop();
	clk_low();
	nop();nop();nop();

	clk_high();
	nop();nop();nop();
	clk_low();
	nop();nop();nop();
	
	for(int i=0;i<25;i++); // 10 -> 2.3us
}

void __attribute__((optimize("O0"))) send_1 (void) {
    clk_high();
	nop();nop();nop();
    clk_low();
	nop();nop();nop();
	
    clk_high();
	nop();nop();nop();
    clk_low();
	nop();nop();nop();

	clk_high();
	nop();nop();nop();
    clk_low();
	nop();nop();nop();

	clk_high();
	nop();nop();nop();
    clk_low();
	nop();nop();nop();

	for(int i=0;i<25;i++); // 10 -> 2.3us
}

int __attribute__((optimize("O0"))) read_ch_status (void) {
    int status = 0;
    int cnt;
    int is_no_respone = 0;

    // === 1st "1" symbol === //
    cnt = 0;
    clk_high();
    if(isDATA_Ch()) {
        cnt++;
    }else{
        is_no_respone = 1;
    }
    clk_low();
    if(isDATA_Ch()) is_no_respone = 1;

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    status |= cnt;
    status <<= 8;
	
	for(int i=0;i<25;i++); // 10 -> 2.3us

    // === 2nd "1" symbol === //
    cnt = 0;
    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    status |= cnt;
    status <<= 8;
	for(int i=0;i<25;i++); // 10 -> 2.3us
	

    // === 3rd "1" symbol === //
    cnt = 0;
    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low();

    clk_high(); 
    if(isDATA_Ch()) cnt++;
    clk_low(); 

    status |= cnt;
	for(int i=0;i<25;i++); // 10 -> 2.3us

    // ============ //
    if(is_no_respone == 1) {
         status = -1;
    }
    return status;
}

uint32_t __attribute__((optimize("O0"))) send_ts_cmd (uint32_t ch, uint32_t cmd) {
    uint32_t status = 0;
    uint32_t ch_addr;

    switch(ch) {
    	case CH_NUMBER_A: {
    		ch_addr = CH_NUMBER_A_ADDR;
    		break;
    	}
    	case CH_NUMBER_B: {
    		ch_addr = CH_NUMBER_B_ADDR;
    		break;
    	}
    	case CH_NUMBER_C: {
    		ch_addr = CH_NUMBER_C_ADDR;
    		break;
    	}
    	case CH_NUMBER_D: {
    		ch_addr = CH_NUMBER_D_ADDR;
    		break;
    	}
    	/*
    	case CH_NUMBER_E: {
    		ch_addr = CH_NUMBER_E_ADDR;
    		break;
    	}
    	case CH_NUMBER_F: {
    		ch_addr = CH_NUMBER_F_ADDR;
    		break;
    	}
    	case CH_NUMBER_G: {
    		ch_addr = CH_NUMBER_G_ADDR;
    		break;
    	}
    	case CH_NUMBER_H: {
    		ch_addr = CH_NUMBER_H_ADDR;
    		break;
    	}
    	*/
    	default:
    		ch_addr = 0;
    }
	
    //==================================
    // R
    clk_low();
	for(int i=0;i<80;i++); // 10 -> 2.3us

    // 110
    send_1();
    send_1();
    send_0();

    // ch address
    switch(ch_addr) {
        case 0: {
            send_0(); send_0(); send_0(); break;
        }
        case 1: {
            send_0(); send_0(); send_1(); break;
        }
        case 2: {
            send_0(); send_1(); send_0(); break;
        }
        case 3: {
            send_0(); send_1(); send_1(); break;
        }
        case 4: {
            send_1(); send_0(); send_0(); break;
        }
        case 5: {
            send_1(); send_0(); send_1(); break;
        }
        case 6: {
            send_1(); send_1(); send_0(); break;
        }
        case 7: {
            send_1(); send_1(); send_1(); break;
        }
    }

    // cmd
    switch(cmd) {
        case TS_CMD_OFF_IMME: {
            send_0(); send_0(); send_0(); send_1();
            break;
        }
        case TS_CMD_OFF_ZEROX: {
            send_0(); send_0(); send_1(); send_0();
            break;
        }
        case TS_CMD_ON_IMME: {
            send_0(); send_0(); send_1(); send_1();
            break;
        }
        case TS_CMD_ON_ZEROX: {
            send_0(); send_1(); send_0(); send_0();
            break;
        }
        case TS_CMD_ON_IMME_DI: {
            send_0(); send_1(); send_0(); send_1();
            break;
        }
        case TS_CMD_ON_ZEROX_DI: {
            send_0(); send_1(); send_1(); send_0();
            break;
        }
        case TS_CMD_HEART_BEAT: {
            send_0(); send_1(); send_1(); send_1();
            break;
        }
        case TS_CMD_SEND_PWR_8P: {
            send_1(); send_0(); send_0(); send_1();
            break;
        }
        case TS_CMD_SEND_PWR_16P: {
            send_1(); send_0(); send_1(); send_0();
            break;
        }
        case TS_CMD_SEND_PWR_32P: {
            send_1(); send_0(); send_1(); send_1();
            break;
        }
        case TS_CMD_SEND_PWR_64P: {
            send_1(); send_1(); send_0(); send_0();
            break;
        }
        case TS_CMD_POLL_STATE: {
            send_1(); send_1(); send_1(); send_1();
            break;
        }
    }

    // 1
    send_1();

    // status
    if((ch == CH_NUMBER_A) || (ch == CH_NUMBER_B) 
		|| (ch == CH_NUMBER_C) || (ch == CH_NUMBER_D)) {
        status = read_ch_status(); 
    }else {
        status = 0;
        send_1(), send_1(), send_1(); // only has a, b, c physical on board, so other channels just send 3 "1" symbols, not read
    }

    // 1
    send_1();

    clk_high();        // CLK idle is high
    //==================================

    /// decode status
    if(status == -1) 
        return;

    #define encode_status(d2, d1, d0) (d2<<16 | d1<<8 | d0)
    switch(status) {
        case encode_status(2,2,2) : {
            //             0,0,0 = 0
            status = 0;
            break;
        }
        case encode_status(2,2,4) : {
            //             0,0,1 = 1
            status = 1;
            break;
        }
        case encode_status(2,4,2) : {
            //             0,1,0 = 2
            status = 2;
            break;
        }
        case encode_status(2,4,4) : {
            //             0,1,1 = 3
            status = 3;
            break;
        }
        case encode_status(4,2,2) : {
            //             1,0,0 = 4
            status = 4;
            break;
        }
        case encode_status(4,2,4) : {
            //             1,0,1 = 5
            status = 5;
            break;
        }
        case encode_status(4,4,2) : {
            //             1,1,0 = 6
            status = 6;
            break;
        }
        case encode_status(4,4,4) : {
            //             1,1,1 = 7
            status = 7;
            break;
        }
        default: {
            status = 0;
        }
    }
	return status;
}

void pull_status_ch (uint32_t ch) {
    send_ts_cmd(ch, TS_CMD_POLL_STATE);
}

void solid_relay_init(void){
	struct port_config pin_conf;
	uint32_t ts_state;
	
	port_get_config_defaults(&pin_conf);

	/* Configure solid relay CLK as outputs, turn them off */
	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(SR_CLK_PIN, &pin_conf);
	port_pin_set_output_level(SR_CLK_PIN, false);

	/* Configure solid relay DATA as inputs*/
	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_NONE;
	port_pin_set_config(SR_DATA_PIN, &pin_conf);

	clk_port_base = port_get_group_from_gpio_pin(SR_CLK_PIN);
	clk_pin_mask = (1UL << (SR_CLK_PIN % 32));
	
	data_port_base = port_get_group_from_gpio_pin(SR_DATA_PIN);
	data_pin_mask = (1UL << (SR_DATA_PIN % 32));

#if 0
	while(1){
		
		if(BUTTON_0_ACTIVE == port_pin_get_input_level(BUTTON_0_PIN)){
			ts_state = send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_POLL_STATE);
			if(ts_state == TS_STATUS_VAL_OFF)
				send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_ON_IMME_DI);
			else if(ts_state == TS_STATUS_VAL_ON_DITHER)
				send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_OFF_IMME);
		}
		send_ts_cmd(CH_NUMBER_A_ADDR,TS_CMD_ON_IMME_DI);
		ts_state = send_ts_cmd(CH_NUMBER_A_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_B_ADDR,TS_CMD_ON_IMME_DI);
		ts_state = send_ts_cmd(CH_NUMBER_B_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_C_ADDR,TS_CMD_ON_IMME_DI);
		ts_state = send_ts_cmd(CH_NUMBER_C_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_ON_IMME_DI);
		ts_state = send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_POLL_STATE);
		delay_s(5);
		
		send_ts_cmd(CH_NUMBER_A_ADDR,TS_CMD_OFF_IMME);
		ts_state = send_ts_cmd(CH_NUMBER_A_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_B_ADDR,TS_CMD_OFF_IMME);
		ts_state = send_ts_cmd(CH_NUMBER_B_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_C_ADDR,TS_CMD_OFF_IMME);
		ts_state = send_ts_cmd(CH_NUMBER_C_ADDR,TS_CMD_POLL_STATE);
		send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_OFF_IMME);
		ts_state = send_ts_cmd(CH_NUMBER_D_ADDR,TS_CMD_POLL_STATE);
		delay_s(60);
		
	}
#endif
}

struct tcc_module lamp_tcc_instance;
static void configure_lamp_tcc(void)
{
	struct tcc_config config_tcc;

	tcc_get_config_defaults(&config_tcc, LAMP_ON_PWM4CTRL_MODULE);
	
	config_tcc.counter.period = system_cpu_clock_get_hz() / 2000; // 2K 
	config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_SINGLE_SLOPE_PWM;
	config_tcc.compare.match[LAMP_ON_PWM4CTRL_CHANNEL] = config_tcc.counter.period / 2;  //50% duty cycle
	config_tcc.pins.enable_wave_out_pin[LAMP_ON_PWM4CTRL_OUTPUT] = true;
	config_tcc.pins.wave_out_pin[LAMP_ON_PWM4CTRL_OUTPUT] = LAMP_ON_PWM4CTRL_PIN;
	config_tcc.pins.wave_out_pin_mux[LAMP_ON_PWM4CTRL_OUTPUT] = LAMP_ON_PWM4CTRL_PINMUX;

	tcc_init(&lamp_tcc_instance, LAMP_ON_PWM4CTRL_MODULE, &config_tcc);
	tcc_disable(&lamp_tcc_instance);
}

void lamp_onoff(bool onoff){
	if(onoff)
		tcc_enable(&lamp_tcc_instance);
	else
		tcc_disable(&lamp_tcc_instance);
}

void lamp_init(void){
	struct port_config pin_conf;
	uint32_t ts_state;
	
	port_get_config_defaults(&pin_conf);

	/* Configure lamp sns as inputs*/
	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_NONE;
	port_pin_set_config(LAMP_SNS_INPUT, &pin_conf);

	configure_lamp_tcc();
}

struct tcc_module buzz_tcc_instance;
static void configure_buzz_tcc(void)
{
	struct tcc_config config_tcc;

	tcc_get_config_defaults(&config_tcc, BUZZ_ON_PWM4CTRL_MODULE);

	config_tcc.counter.period = system_cpu_clock_get_hz() / 2800; // 2.8 kHz
	config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_SINGLE_SLOPE_PWM;
	config_tcc.compare.match[BUZZ_ON_PWM4CTRL_CHANNEL] = config_tcc.counter.period / 2;  //50% duty cycle
	config_tcc.pins.enable_wave_out_pin[BUZZ_ON_PWM4CTRL_OUTPUT] = true;
	config_tcc.pins.wave_out_pin[BUZZ_ON_PWM4CTRL_OUTPUT] = BUZZ_ON_PWM4CTRL_PIN;
	config_tcc.pins.wave_out_pin_mux[BUZZ_ON_PWM4CTRL_OUTPUT] = BUZZ_ON_PWM4CTRL_PINMUX;

	tcc_init(&buzz_tcc_instance, BUZZ_ON_PWM4CTRL_MODULE, &config_tcc);
	tcc_disable(&buzz_tcc_instance);
}

void buzz_onoff(bool onoff){
	if(onoff)
		tcc_enable(&buzz_tcc_instance);
	else
		tcc_disable(&buzz_tcc_instance);
}

void buzz_init(void){
	struct port_config pin_conf;
	uint32_t ts_state;
	
	port_get_config_defaults(&pin_conf);

	/* Configure lamp sns as inputs*/
	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_NONE;
	port_pin_set_config(BUZZ_SENSOR_INPUT, &pin_conf);

	configure_buzz_tcc();
}

