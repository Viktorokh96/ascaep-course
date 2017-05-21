/*
 * usart.c
 *
 *  Created on: 08.05.2017
 *      Author: viktor96
 */
#include "usart.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

uint8_t uart_rxrd, uart_rxwr;
uint8_t uart_rx[UART_BUFSIZE];

uint8_t uart_txrd, uart_txwr;
uint8_t uart_tx[UART_BUFSIZE];

ISR(USART_RX_vect)
{
	uint8_t byte;
	uint8_t wr = (uart_rxwr+1) & UART_BUFEND;
	byte = UDR0;
	if(wr != uart_rxrd)
	{
		uart_rx[uart_rxwr] = byte;
		uart_rxwr = wr;
	}
}

/* Если произошло опустошение приемного буфера
 * а это значит что мы можем загнать байт в UDR0
 */
ISR(USART_UDRE_vect)
{
	uint8_t rd = uart_txrd;
	if(rd != uart_txwr)
	{
		UDR0 = uart_tx[rd];
		uart_txrd = (rd+1) & UART_BUFEND;
		return;
	}

	/* Запрещаем прерывание по опустошению буфера */
	unsetbit(UCSR0B, UDRIE0);
}

uint8_t USART_RX_Count()
{
	return (uart_rxwr-uart_rxrd) & UART_BUFEND;
}

void USART_Init( uint16_t bauds )
{
	/* Set baud rate */
	uint16_t baudrate = (F_CPU/16/bauds) - 1;
	UBRR0H = (uint8_t)(baudrate >> 8);
	UBRR0L = (uint8_t) baudrate;
	/* Enable receiver and transmitter */
	setmask(UCSR0B, _BV(RXCIE0)| _BV(RXEN0)| _BV(TXEN0));
	/* Set frame format: 8 data, 2 stop bit */
	setmask(UCSR0C, _BV(USBS0));
}

uint8_t USART_Recive()
{
	uint8_t rd = uart_rxrd;
	uint8_t byte;
	if(rd != uart_rxwr)
	{
		byte = uart_rx[rd];
		uart_rxrd = (rd+1) & UART_BUFEND;
		return byte;
	}

	return 0;
}

void USART_Transmit( uint8_t byte )
{
	uint8_t wr = (uart_txwr+1) & UART_BUFEND;
	if(wr != uart_txrd)
	{
		uart_tx[uart_txwr] = byte;
		uart_txwr = wr;
		/* Разрешаем прерывание по опустошению буфера */
		setbit(UCSR0B, UDRIE0);
	}
}

/* Send number in Hex format */
void USART_TransmitHexNum(uint16_t num)
{
	char num_to_hex_char[16] = {
			'0', '1', '2', '3',
			'4', '5', '6', '7',
			'8', '9', 'A', 'B',
			'C', 'D', 'E', 'F'
	};

	char msg[7] = { '0', 'x', [6] = '\0' };

	msg[2] = num_to_hex_char [(num >> 12) & 0x0F];
	msg[3] = num_to_hex_char [(num >> 8) & 0x0F];
	msg[4] = num_to_hex_char [(num >> 4) & 0x0F];
	msg[5] = num_to_hex_char [num & 0x0F];

	char *p = msg;
	for(; *p; ++p)
		USART_Transmit(*p);
}

/* Send number in decimal unsigned format */
void USART_TransmitNum(uint16_t num)
{
	char msg[6] = { [5] = '\0' };

	/* we start to write number from the end */
	char *p = &msg[5];

	uint8_t digit;

	do {
		p--;
		digit = num % 10;
		*p = '0' + digit;
	} while((num /= 10) != 0);

	/* And then - transmit it */
	for(; *p; ++p)
		USART_Transmit(*p);
}

int USART_Setup()
{
	uint8_t setupStatus = 0;

	USART_Init(UART_RATE);

	uint8_t message[] = "USART: Setup success!\n";

	USART_TransmitMsg(message);

	return setupStatus;
}

void USART_TransmitMsg(uint8_t *msg)
{
	uint8_t *p = msg;
	for(; *p; ++p)
		USART_Transmit(*p);
}

