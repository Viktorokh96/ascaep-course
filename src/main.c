#include "myutils.h"
#include "enc28j60.h"
#include "usart.h"
#include "lan.h"

#include <avr/io.h>
#include <stdlib.h>

#define BUFF_SIZE 576
#define false 0
#define true 1

uint8_t enable_log = false;

void USART_PrintLog(uint8_t *msg)
{
	if (enable_log && msg)
		USART_TransmitMsg(msg);
}

/* Обработчик получаемых UDP-сегментов */
void udp_packet(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void *) (frame->data);
	udp_packet_t *udp = (void *) (ip->data);
	uint8_t *data = udp->data;
	uint8_t i , count;

	if (enable_log) {
		USART_Transmit('\n');
		/* Отправляем данные в USART */
		for (i = 0; i < len; ++i)
			USART_Transmit(data[i]);
	}

	/* Возвращаем компьютеру то, что приходило
	 * с момента последнего обмена
	 */
	count = USART_RX_Count();
	if (count)
	{
		for (i = 0; i < count; ++i)
			data[i] = USART_Recive();
		udp_reply(frame,count);
	}
}

uint8_t send_udp_test(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port)
{
	eth_frame_t *frame = (void *) net_buf;
	ip_packet_t *ip = (void *) (frame->data);
	udp_packet_t *udp = (void *) (ip->data);
	uint8_t *data = (void *) (udp->data);
	uint8_t msg[] = "Hello! I'm MCU! How is you?\n";

	ip->to_addr = dst_ip;
	udp->to_port = dst_port;
	udp->from_port = src_port;

	memcpy(data, msg, sizeof(msg));

	return udp_send(frame, sizeof(msg));
}

void USART_PrintNetAddr(uint32_t addr)
{
	USART_TransmitNum((addr >> 24) & 0xff);
	USART_Transmit('.');
	USART_TransmitNum((addr >> 16) & 0xff);
	USART_Transmit('.');
	USART_TransmitNum((addr >> 8) & 0xff);
	USART_Transmit('.');
	USART_TransmitNum((addr) & 0xff);
}

void show_settings()
{
	uint8_t i, mac_addr[6];
	get_mac_addr(mac_addr);

	USART_TransmitMsg("\nMAC = ");

	for (i = 0; i < 6; i = i+2)
	{
		USART_TransmitHexNum((uint16_t)((mac_addr[i] << 8) |
				(mac_addr[i+1])));
		USART_Transmit(' ');
	}

	uint32_t tmp = ntohl(get_ip_addr());
	USART_TransmitMsg("\nIP = ");
	USART_PrintNetAddr(tmp);

	tmp = ntohl(get_ip_mask());
	USART_TransmitMsg("\nSubnet mask = ");
	USART_PrintNetAddr(tmp);

	tmp = ntohl(get_ip_gateway());
	USART_TransmitMsg("\nDefault gateway = ");
	USART_PrintNetAddr(tmp);

	tmp = ntohl(get_ip_server());
	USART_TransmitMsg("\nServer IP = ");
	USART_PrintNetAddr(tmp);

	USART_Transmit('\n');
}

void command_mode()
{
	uint8_t ch;

	uint8_t *helpmsg = "\ns - send test udp packet to server\n";

	do {
		USART_TransmitMsg("\ntap 'q' to quit\n");
		USART_TransmitMsg(helpmsg);
		do{
			ch = USART_Recive();
			if (ch == 's') {
				send_udp_test(get_ip_server(), htons(53), htons(80));
				USART_TransmitMsg("\nTest packet was sent\n");
			}
			/* В командном режиме прослушка пакетов не останавливается */
			lan_poll();
		} while (ch == 0);
	} while (ch != 'q');

}


void configure_interface()
{
	uint8_t ch, i;
	uint8_t a, b, c, d;
	uint8_t addr_str[16];
	uint8_t mac_addr[6] = MAC_ADDR;
	uint8_t *p,
			*helpmsg = "\nchange:\ni - IP\nm - mask\nd - default\ns - server IP\n";
	uint32_t tmp;

	enum to_configure { IP = 1, MASK, DEFAULT, SERVER };
	uint8_t toconf;

	do {
		show_settings();
		USART_TransmitMsg("\ntap 'q' to quit\n");
		USART_TransmitMsg(helpmsg);
		i = 0; toconf = 0; p = addr_str;
		do {
			ch = USART_Recive();
			if (ch == 'i')
				toconf = IP;
			else if (ch == 'm')
				toconf = MASK;
			else if (ch == 'd')
				toconf = DEFAULT;
			else if (ch == 's')
				toconf = SERVER;
			else if (ch == 'q')
				return;
			else if (ch == 'h')
			USART_TransmitMsg(helpmsg);
		} while (!toconf);

		USART_TransmitMsg("Enter address in x.x.x.xs format (s - stop):\n");

		while (ch != 'q'){
			ch = USART_Recive();

			if ((i == 16) || (ch == 's')) break;
			if ('\b' == ch) i = (0 == i) ? 0 : i-1;
			if ((ch >= '0' && ch <= '9') || ch == '.')
				addr_str[i++] = (ch == '.') ? '\0' : ch;
		};

		addr_str[i] = '\0';

		if (ch != 'q')
		{
			a = (uint8_t) (atoi((char *) p) & 0xff);
			for(; *p; ++p); ++p;
			b = (uint8_t) (atoi((char *) p) & 0xff);
			for(; *p; ++p); ++p;
			c = (uint8_t) (atoi((char *) p) & 0xff);
			for(; *p; ++p); ++p;
			d = (uint8_t) (atoi((char *) p) & 0xff);

			tmp = inet_addr(a, b, c, d);

			if (toconf == IP)
				set_ip_addr(tmp);
			else if (toconf == MASK)
				set_ip_mask(tmp);
			else if (toconf == DEFAULT)
				set_ip_gateway(tmp);
			else if (toconf == SERVER)
				set_ip_server(tmp);

			set_mac_addr(mac_addr);
			save_interface_settings();
			USART_TransmitMsg("\nConfiguration successfully seted\n");
		}
	} while('q' != ch);
}

void show_intro()
{
	USART_TransmitMsg("\nTo start work:\n");
    USART_TransmitMsg("s - view interface settings\n");
    USART_TransmitMsg("c - configure interface settings\n");
    USART_TransmitMsg("d - enter to command mode\n");
    USART_TransmitMsg("l - enable/disable logger\n");
    USART_TransmitMsg("h - help\n");
    USART_TransmitMsg("%> ");
}

int main(void)
{
	enum program_state { POLLING, CONFIGURE, VIEW, CMD};
	uint8_t count, c, state = POLLING;
	uint8_t *polling_msg = "\nSTATE: Polling\n";

    USART_Setup();
    sei();
    load_interface_settings();
    lan_init();

    USART_TransmitMsg("End of initialization enc28j60\n");
    USART_TransmitMsg(polling_msg);

    show_intro();

    while(1)
    {
    	/* Проверяем, пришло ли сообщение извне */
    	count = USART_RX_Count();
    	if (count)
    	{
    		c = USART_Recive();
    		switch(c)
    		{
    			case 's':
        			USART_TransmitMsg("\nSTATE: View\n");
    				state = VIEW;
				break;

    			case 'd':
    				USART_TransmitMsg("\nSTATE: Command mode\n");
    				state = CMD;
    			break;

    			case 'c':
        			USART_TransmitMsg("\nSTATE: Configure\n");
					state = CONFIGURE;
				break;

    			case 'l':
    				enable_log = !enable_log;
    				USART_TransmitMsg(enable_log ?
    						"\nLOG: enabled\n":
    						"\nLOG: disabled\n");
    			break;

    			case 'h':
    				show_intro();
				break;
    		}
    	}

    	switch(state)
    	{
    		case POLLING:
    			lan_poll();
    		break;

    		case CONFIGURE:
    			configure_interface();
    			USART_TransmitMsg(polling_msg);
    			state = POLLING;
    		break;

    		case CMD:
    			command_mode();
    			USART_TransmitMsg(polling_msg);
    			state = POLLING;
    		break;

    		case VIEW:
    			show_settings();
    			USART_TransmitMsg(polling_msg);
    			state = POLLING;
    		break;
    	}
    }
}
