#include <avr/eeprom.h>

#include "lan.h"
#include "usart.h"

#define SETTINGS_ADDR  ((uint8_t *) 0x0100)


typedef struct ifsettings {
	uint8_t mac_addr[6];
	uint32_t ip_addr;
	uint32_t ip_mask;
	uint32_t ip_gateway;
	uint32_t ip_server;
} if_settings;

if_settings interface_settings;

uint8_t net_buf[ENC28J60_MAXFRAME];

uint8_t arp_cache_wr;
/* ARP-таблица (кэш) */
arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

void eth_reply(eth_frame_t *frame, uint16_t len);
void ip_reply(eth_frame_t *frame, uint16_t len);
uint16_t ip_cksum(uint32_t sum, uint8_t *buf, uint16_t len);


uint8_t *arp_resolve(uint32_t node_ip_addr);
uint8_t ip_send(eth_frame_t *frame, uint16_t len);
void ip_reply(eth_frame_t *frame, uint16_t len);
void eth_send(eth_frame_t *frame, uint16_t len);
uint16_t ip_cksum(uint32_t sum, uint8_t *buf, uint16_t len);

void set_mac_addr(uint8_t mac[6])
{
	memcpy(interface_settings.mac_addr, mac, 6);
}

void get_mac_addr(uint8_t mac[6])
{
	memcpy(mac, interface_settings.mac_addr, 6);
}

void set_ip_gateway(uint32_t gateway)
{
	interface_settings.ip_gateway = gateway;
}

uint32_t get_ip_gateway()
{
	return interface_settings.ip_gateway;
}

void set_ip_addr(uint32_t ip)
{
	interface_settings.ip_addr = ip;
}

uint32_t get_ip_addr()
{
	return interface_settings.ip_addr;
}

void set_ip_mask(uint32_t mask)
{
	interface_settings.ip_mask = mask;
}

uint32_t get_ip_mask()
{
	return interface_settings.ip_mask;
}

uint32_t get_ip_server()
{
	return interface_settings.ip_server;
}

void set_ip_server(uint32_t ip)
{
	interface_settings.ip_server = ip;
}

void save_interface_settings()
{
	eeprom_busy_wait();

	eeprom_write_block(&interface_settings, SETTINGS_ADDR, sizeof(if_settings));
}

void load_interface_settings()
{
	eeprom_busy_wait();

	eeprom_read_block(&interface_settings, SETTINGS_ADDR, sizeof(if_settings));
}


/*
 * UDP
 */

/*
 * Отправка UDP-сегмента
 * Должны быть установлены следующие поля:
 * 		ip.to_addr - адрес получателя
 * 		udp.from_port - порт отправителя
 * 		udp.to_port - порт получателя
 * len - длина поля данных сегмента
 * Если MAC-адрес узла или шлюза ещё не определён, функция возвращает 0
 */
uint8_t udp_send(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void*)(frame->data);
	udp_packet_t *udp = (void*)(ip->data);

	len += sizeof(udp_packet_t);

	ip->protocol = IP_PROTOCOL_UDP;
	ip->from_addr = get_ip_addr();

	udp->len = htons(len);
	udp->cksum = 0;
	udp->cksum = ip_cksum(len + IP_PROTOCOL_UDP,
		(uint8_t*)udp-8, len+8);

	return ip_send(frame, len);
}

void udp_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void*)(frame->data);
	udp_packet_t *udp = (void*)(ip->data);

	USART_PrintLog((uint8_t *)"\nUDP filter started!");

	if(len >= sizeof(udp_packet_t))
	{
		udp_packet(frame, ntohs(udp->len) - 
			sizeof(udp_packet_t));
	}
}

void udp_reply(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void*)(frame->data);
	udp_packet_t *udp = (void*)(ip->data);
	uint16_t temp;

	USART_PrintLog((uint8_t *)"\nUDP reply!");

	len += sizeof(udp_packet_t);

	temp = udp->from_port;
	udp->from_port = udp->to_port;
	udp->to_port = temp;

	udp->len = htons(len);

	udp->cksum = 0;
	udp->cksum = ip_cksum(len + IP_PROTOCOL_UDP, 
		(uint8_t*)udp-8, len+8);

	ip_reply(frame, len);
}


/*
 * ICMP
 */

#ifdef WITH_ICMP

void icmp_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *packet = (void*)frame->data;
	icmp_echo_packet_t *icmp = (void*)packet->data;

	USART_PrintLog((uint8_t *)"\nICMP filter started!");

	if(len >= sizeof(icmp_echo_packet_t) )
	{
		if(icmp->type == ICMP_TYPE_ECHO_RQ)
		{
			icmp->type = ICMP_TYPE_ECHO_RPLY;
			icmp->cksum += 8; // update cksum
			USART_PrintLog((uint8_t *)"\nICMP ECHO reply!");
			ip_reply(frame, len);
		}
	}
}

#endif


/*
 * IP
 */

uint16_t ip_cksum(uint32_t sum, uint8_t *buf, size_t len)
{
	while(len >= 2)
	{
		sum += ((uint16_t)*buf << 8) | *(buf+1);
		buf += 2;
		len -= 2;
	}

	if(len)
		sum += (uint16_t)*buf << 8;

	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~htons((uint16_t)sum);
}

/* Отправка IP-пакета
 * Должны быть установлены следующие поля:
 * 		ip.to_addr - адрес получателя
 * 		ip.protocol - код протокола верхнего уровня
 * len - длина поля данных пакета
 * Если MAC-адрес узла/шлюза ещеё не определен - вернет 0.
 */
uint8_t ip_send(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *ip = (void*)(frame->data);
	uint32_t route_ip;
	uint8_t *mac_addr_to;

	// Если адрес находится в локальной сети - прописывем его напрямую
	if( ((ip->to_addr ^ get_ip_addr()) & get_ip_mask()) == 0 )
		route_ip = ip->to_addr;
	else
		route_ip = get_ip_gateway();

	// Разрешение MAC-адреса
	if(!(mac_addr_to = arp_resolve(route_ip)))
		return 0;

	// send packet
	len += sizeof(ip_packet_t);

	memcpy(frame->to_addr, mac_addr_to, 6);
	frame->type = ETH_TYPE_IP;

	ip->ver_head_len = 0x45;
	ip->tos = 0;
	ip->total_len = htons(len);
	ip->fragment_id = 0;
	ip->flags_framgent_offset = 0;
	ip->ttl = IP_PACKET_TTL;
	ip->cksum = 0;
	ip->from_addr = get_ip_addr();
	ip->cksum = ip_cksum(0, (void*)ip, sizeof(ip_packet_t));

	eth_send(frame, len);
	return 1;
}

void ip_reply(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *packet = (void*)(frame->data);

	USART_PrintLog((uint8_t *)"\nIP reply!");

	packet->total_len = htons(len + sizeof(ip_packet_t));
	packet->fragment_id = 0;
	packet->flags_framgent_offset = 0;
	packet->ttl = IP_PACKET_TTL;
	packet->cksum = 0;
	packet->to_addr = packet->from_addr;
	packet->from_addr = get_ip_addr();
	packet->cksum = ip_cksum(0, (void*)packet, sizeof(ip_packet_t));

	eth_reply((void*)frame, len + sizeof(ip_packet_t));
}

void ip_filter(eth_frame_t *frame, uint16_t len)
{
	ip_packet_t *packet = (void*)(frame->data);
	
	USART_PrintLog((uint8_t *)"\nIP filter started!");

	//if(len >= sizeof(ip_packet_t))
	//{
		if( (packet->ver_head_len == 0x45) &&
			(packet->to_addr == get_ip_addr()) )
		{
			len = ntohs(packet->total_len) - 
				sizeof(ip_packet_t);

			USART_PrintLog((uint8_t *)"\nIP datagram for me!");

			switch(packet->protocol)
			{
#ifdef WITH_ICMP
			case IP_PROTOCOL_ICMP:
				icmp_filter(frame, len);
				break;
#endif
			case IP_PROTOCOL_UDP:
				udp_filter(frame, len);
				break;
			}
		}
	//}
}


/*
 * ARP
 */

/* Поиск в кэше ARP */
uint8_t *arp_search_cache(uint32_t node_ip_addr)
{
	uint8_t i;
	for(i = 0; i < ARP_CACHE_SIZE; ++i)
	{
		if(arp_cache[i].ip_addr == node_ip_addr)
			return arp_cache[i].mac_addr;
	}

	return 0;
}

/* Разрешение MAC-адреса
 * Возвращает 0 если разрешение адреса ещё в процессе
 * (например не оказалось в кэше)
 */
uint8_t *arp_resolve(uint32_t node_ip_addr)
{
	uint8_t mac_addr[6];
	get_mac_addr(mac_addr);

	eth_frame_t *frame = (void*)net_buf;
	arp_message_t *msg = (void*)(frame->data);
	uint8_t *mac;

	// Ищем адрес в кэше
	if((mac = arp_search_cache(node_ip_addr)))
		return mac;

	// Посылаем запрос, если адрес не кэширован
	memset(frame->to_addr, 0xff, 6);  // Широковещательный адрес
	frame->type = ETH_TYPE_ARP;  // Запрос ARP

	msg->hw_type = ARP_HW_TYPE_ETH;
	msg->proto_type = ARP_PROTO_TYPE_IP;
	msg->hw_addr_len = 6;
	msg->proto_addr_len = 4;
	msg->type = ARP_TYPE_REQUEST;
	memcpy(msg->mac_addr_from, mac_addr, 6);
	msg->ip_addr_from = get_ip_addr();
	memset(msg->mac_addr_to, 0x00, 6);
	msg->ip_addr_to = node_ip_addr;

	eth_send(frame, sizeof(arp_message_t));
	return 0;
}

void arp_filter(eth_frame_t *frame, uint16_t len)
{
	uint8_t mac[6];
	get_mac_addr(mac);
	uint32_t ip_addr = get_ip_addr();

	arp_message_t *msg = (void*)(frame->data);

	USART_PrintLog((uint8_t *)"\nArp filter started!");

	if(len >= sizeof(arp_message_t))
		{

			if( (msg->hw_type == ARP_HW_TYPE_ETH) &&
				(msg->proto_type == ARP_PROTO_TYPE_IP) &&
				(msg->ip_addr_to == ip_addr) )
			{
				//USART_PrintLog((uint8_t *)"\nArp SWITCH!");
				switch(msg->type)
				{
				// Если к нам послали запрос - отвечаем
				case ARP_TYPE_REQUEST:
					msg->type = ARP_TYPE_RESPONSE;
					memcpy(msg->mac_addr_to, msg->mac_addr_from, 6);
					memcpy(msg->mac_addr_from, mac, 6);
					msg->ip_addr_to = msg->ip_addr_from;
					msg->ip_addr_from = get_ip_addr();
					eth_reply(frame, sizeof(arp_message_t));
					break;
				// ARP-ответ => добавляем узел в кэш
				case ARP_TYPE_RESPONSE:
					if(!arp_search_cache(msg->ip_addr_from))
					{
						arp_cache[arp_cache_wr].ip_addr = msg->ip_addr_from;
						memcpy(arp_cache[arp_cache_wr].mac_addr, msg->mac_addr_from, 6);
						arp_cache_wr++;
						if(arp_cache_wr == ARP_CACHE_SIZE)
							arp_cache_wr = 0;
					}
					break;
				}
			}
		}
}


/*
 * Ethernet
 */

/*
 * Отправка Ethernet-фрейма
   Должны быть установлены следующие поля:
   	   - frame.to_addr - MAC-адрес получателя
   	   - frame.type - протокол
   len - длина поля данных фрейма
 */
void eth_send(eth_frame_t *frame, uint16_t len)
{
	uint8_t mac[6];
	get_mac_addr(mac);

    memcpy(frame->from_addr, mac, 6);
    enc28j60_send_packet((void*)frame, len +
        sizeof(eth_frame_t));
}

 
void eth_reply(eth_frame_t *frame, uint16_t len)
{
	uint8_t mac[6];
	get_mac_addr(mac);

	USART_PrintLog((uint8_t *)"\nEth_reply!");
	memcpy(frame->to_addr, frame->from_addr, 6);
	memcpy(frame->from_addr, mac, 6);
	enc28j60_send_packet((void*)frame, len + 
		sizeof(eth_frame_t));
}

void eth_filter(eth_frame_t *frame, uint16_t len)
{
	if(len >= sizeof(eth_frame_t))
	{
		switch(frame->type)
		{
		case ETH_TYPE_ARP:
			arp_filter(frame, len - sizeof(eth_frame_t));
			break;
		case ETH_TYPE_IP:
			ip_filter(frame, len - sizeof(eth_frame_t));
			break;
		}
	}
}


/*
 * LAN
 */

void lan_init()
{
	uint8_t mac[6];
	get_mac_addr(mac);
	enc28j60_init(mac);
}

void lan_poll()
{
	uint16_t len;
	eth_frame_t *frame = (void*)net_buf;
	
	while((len = enc28j60_recv_packet(net_buf, sizeof(net_buf))))
		eth_filter(frame, len);
}
