/*
 * usart.h
 *
 *  Created on: 08.05.2017
 *      Author: viktor96
 */

#ifndef USART_H_
#define USART_H_

#define UART_RATE		9600
#define UART_BUFSIZE	256
#define UART_BUFEND		(UART_BUFSIZE-1)

#include "myutils.h"

/* Инициализация USART интерфейса */
int USART_Setup();
void USART_Init( uint16_t bauds );

/* Отправка одного байта данных */
void USART_Transmit( uint8_t data );
/* Получение одного байта данных */
uint8_t USART_Recive();
/* Получение количества принятых байтов */
uint8_t USART_RX_Count();
/* Отправка сообщения в виде текстовой строки с 0-терминатором */
void USART_TransmitMsg( uint8_t *msg );
/* Отправка текстового представления десятичного числа */
void USART_TransmitNum(uint16_t num);
/* Отправка текстового представления шестнадцатеричного числа */
void USART_TransmitHexNum(uint16_t num);

extern void USART_PrintLog(uint8_t *msg);

#endif /* USART_H_ */
