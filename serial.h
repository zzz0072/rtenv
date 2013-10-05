#ifndef SERIAL_H_20131005
#define SERIAL_H_20131005
#include "stm32f10x.h"

void serialout(USART_TypeDef* uart, unsigned int intr);
void serialin(USART_TypeDef* uart, unsigned int intr);
void rs232_xmit_msg_task();
#endif
