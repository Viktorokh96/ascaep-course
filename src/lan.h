#ifndef _LAN_H
#define _LAN_H 1

#include <string.h>
#include "enc28j60.h"


/*
 * Config
 */

#define WITH_ICMP
#define MAC_ADDR		{0x00,0x13,0x33,0x01,0x25,0x48}
#define IP_ADDR			inet_addr(192, 168, 0, 140)
#define IP_SUBNET_MASK		inet_addr(255,255,255,0)
#define IP_DEFAULT_GATEWAY	inet_addr(192,168,0,1)

#define IP_PACKET_TTL		64

#define ARP_CACHE_SIZE		5

/*
 * BE conversion
 */

#define htons(a)		((((a)>>8)&0xff)|(((a)<<8)&0xff00))
#define ntohs(a)		htons(a)

#define htonl(a)		( (((a)>>24)&0xff) | (((a)>>8)&0xff00) |\
				(((a)<<8)&0xff0000) | (((a)<<24)&0xff000000) )
#define ntohl(a)		htonl(a)

#define inet_addr(a,b,c,d)	( ((uint32_t)a) | ((uint32_t)b << 8) |\
					((uint32_t)c << 16) | ((uint32_t)d << 24) )


/*
 * Ethernet
 */
 
#define ETH_TYPE_ARP		htons(0x0806)
#define ETH_TYPE_IP		htons(0x0800)

typedef struct eth_frame {
	uint8_t to_addr[6];		/* MAC-адрес адресата */
	uint8_t from_addr[6];	/* MAC-адрес отправителя */
	uint16_t type;	 /* Идентификатор протокола верхнего уровня */
	uint8_t data[]; /* Данные */
	/* CRC высчитывается автоматически контроллером enc28j60 */
} eth_frame_t;

/*
 * ARP
 */

#define ARP_HW_TYPE_ETH		htons(0x0001)
#define ARP_PROTO_TYPE_IP	htons(0x0800)

#define ARP_TYPE_REQUEST	htons(1)
#define ARP_TYPE_RESPONSE	htons(2)

/* Запись в ARP-таблице */
typedef struct arp_cache_entry {
	uint32_t ip_addr;			/* IP-адрес хоста */
	uint8_t mac_addr[];			/* MAC-адрес хоста */
} arp_cache_entry_t;

typedef struct arp_message {
	uint16_t hw_type;			/* Тип оборудования */
	uint16_t proto_type;		/* Тип протокола */
	uint8_t hw_addr_len;		/* Длина аппаратного адреса */
	uint8_t proto_addr_len;		/* Длина протокольного адреса */
	uint16_t type;				/* Код операции */
	uint8_t mac_addr_from[6];	/* Аппаратный адрес отправителя */
	uint32_t ip_addr_from;		/* IP-адрес отправителя */
	uint8_t mac_addr_to[6];		/* Аппаратный адрес адесата */
	uint32_t ip_addr_to;		/* IP-адрес адресата */
} arp_message_t;

/*
 * IP
 */

#define IP_PROTOCOL_ICMP	1
#define IP_PROTOCOL_TCP		6
#define IP_PROTOCOL_UDP		17

typedef struct ip_packet {
	uint8_t ver_head_len;	/* Версия и длина заголовка IP */
	uint8_t tos;	/* Тип обслуживания */
	uint16_t total_len;  /* Длина пакета */
	uint16_t fragment_id;	/* Идентификатор фрагмента */
	uint16_t flags_framgent_offset;	 /* Флаги и смещение фрагмента */
	uint8_t ttl;	/* Время жизни пакета */
	uint8_t protocol;	/* Идентификатор протокола верхнего уровня */
	uint16_t cksum;	 /* Контрольная сумма заголовка */
	uint32_t from_addr;	 /* IP-адрес отправителя */
	uint32_t to_addr;	/* IP-адрес получателя */
	uint8_t data[];	 /* Данные */
} ip_packet_t;


/*
 * ICMP
 */

#define ICMP_TYPE_ECHO_RQ	8
#define ICMP_TYPE_ECHO_RPLY	0

typedef struct icmp_echo_packet {
	uint8_t type;		/* Тип */
	uint8_t code; 		/* Код сообщения */
	uint16_t cksum;		/* Контрольная сумма */
	/* Далее следуют поля только для ECHO запросов */
	uint16_t id;		/* Идентификатор */
	uint16_t seq;		/* Номер последовательности */
	uint8_t data[];		/* Данные */
} icmp_echo_packet_t;


/*
 * UDP
 */

typedef struct udp_packet {
	uint16_t from_port;		/* Порт источника */
	uint16_t to_port;		/* Порт назначения */
	uint16_t len;			/* Длина UDP пакета */
	uint16_t cksum;			/* Контрольная сумма UDP заголовка */
	uint8_t data[];			/* Данные */
} udp_packet_t;


/*
 * LAN
 */

extern uint8_t net_buf[];

void lan_init();
void lan_poll();

void udp_packet(eth_frame_t *frame, uint16_t len);
uint8_t udp_send(eth_frame_t *frame, uint16_t len);
void udp_reply(eth_frame_t *frame, uint16_t len);

void set_mac_addr(uint8_t mac[6]);
void get_mac_addr(uint8_t mac[6]);
void set_ip_gateway(uint32_t gateway);
uint32_t get_ip_gateway();
void set_ip_addr(uint32_t ip);
uint32_t get_ip_addr();
void set_ip_mask(uint32_t mask);
uint32_t get_ip_mask();
uint32_t get_ip_server();
void set_ip_server(uint32_t ip);
void save_interface_settings();
void load_interface_settings();



#endif
