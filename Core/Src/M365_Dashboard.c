/*
 * M365_Dashboard.c
 *
 *  Created on: Nov 27, 2021
 *      Author: stancecoke
 */


#include "main.h"
#include "config.h"
#include "stm32f1xx_hal.h"
#include "print.h"
#include "M365_Dashboard.h"
enum { STATE_LOST, STATE_START_DETECTED, STATE_LENGTH_DETECTED };

UART_HandleTypeDef huart3;
static uint8_t ui8_rx_buffer[64];
static uint8_t ui8_dashboardmessage[64];
static uint8_t	ui8_tx_buffer[] = {0x55, 0xAA, 0x06, 0x21, 0x64, 0x00, 0x01, 0xF0, 0x00, 0x00, 0x00, 0x00};
static uint8_t ui8_oldpointerposition=64;
static uint8_t ui8_recentpointerposition=0;
static uint8_t ui8_messagestartpos=255;
static uint8_t ui8_messagelength=0;
static uint8_t ui8_state= STATE_LOST;



void M365Dashboard_init(UART_HandleTypeDef huart1) {
//        CLEAR_BIT(huart3.Instance->CR3, USART_CR3_EIE);
	if (HAL_UART_Receive_DMA(&huart1, (uint8_t*) ui8_rx_buffer, sizeof(ui8_rx_buffer)) != HAL_OK) {
		Error_Handler();
	}
}

void search_DashboardMessage(MotorState_t *MS, MotorParams_t *MP, UART_HandleTypeDef huart1){



	ui8_recentpointerposition = sizeof(ui8_rx_buffer) - (DMA1_Channel5->CNDTR); //Pointer of UART1RX DMA Channel
		if (ui8_recentpointerposition<ui8_oldpointerposition){
			ui8_oldpointerposition=ui8_recentpointerposition-1;
			ui8_state=STATE_LOST;
		}
		while(ui8_oldpointerposition!=ui8_recentpointerposition){

			switch (ui8_state) {
			case STATE_LOST: { //if no message start is detected yet, search for start pattern 0x55 0xAA
				if(ui8_rx_buffer[ui8_oldpointerposition]==0xAA&&ui8_rx_buffer[ui8_oldpointerposition-1]==0x55){
					ui8_messagestartpos=ui8_oldpointerposition-1;
					ui8_state=STATE_START_DETECTED;
				}
			}
				break;

			case STATE_START_DETECTED: { //read the lenght of the message
				if(ui8_oldpointerposition==ui8_messagestartpos+2){
					ui8_messagelength=ui8_rx_buffer[ui8_oldpointerposition]+6;
					ui8_state=STATE_LENGTH_DETECTED;
				}
			}
				break;
			case STATE_LENGTH_DETECTED: { //read whole message and call processing
				if(ui8_oldpointerposition==ui8_messagestartpos+ui8_messagelength-1){
					memcpy(ui8_dashboardmessage,ui8_rx_buffer+ui8_messagestartpos,ui8_messagelength);
					process_DashboardMessage( MS,  MP, (uint8_t*)&ui8_dashboardmessage,ui8_messagelength,huart1);
					ui8_state=STATE_LOST;
				  	   CLEAR_BIT(DMA1_Channel5->CCR, DMA_CCR_EN);
				  	   DMA1_Channel5->CNDTR=sizeof(ui8_rx_buffer);
				  	   SET_BIT(DMA1_Channel5->CCR, DMA_CCR_EN);


				}
			}
				break;
			} //end switch

			ui8_oldpointerposition=(ui8_oldpointerposition+1)% sizeof(ui8_rx_buffer);
		}// end of while

}

void process_DashboardMessage(MotorState_t *MS, MotorParams_t *MP, uint8_t *message, uint8_t length, UART_HandleTypeDef huart1 ){
	//while(HAL_UART_GetState(&huart1)!=HAL_UART_STATE_READY){}
	//HAL_Delay(2); // bad style, but wait for characters coming in, if message is longer than expected
	if(!checkCRC(message, length)){
	//	55 AA 06 21 64 00 00 00 00 00 74 FF
		switch (message[4]) {

		case 0x64: {

			addCRC((uint8_t*)ui8_tx_buffer, ui8_tx_buffer[2]+6);
			HAL_HalfDuplex_EnableTransmitter(&huart1);
			HAL_UART_Transmit_DMA(&huart1, (uint8_t*)ui8_tx_buffer, sizeof(ui8_tx_buffer));
			}

		case 0x65: {

			MS->i_q_setpoint = map(message[7],THROTTLEOFFSET,THROTTLEMAX,0,PH_CURRENT_MAX);
			}

		default: {
		//	MS->i_q_setpoint = 0; // stop motor for safety reason
			}
		}//end switch


	}

}

void addCRC(uint8_t * message, uint8_t size){
    unsigned long cksm = 0;
    for(int i = 2; i < size - 2; i++) cksm += message[i];
    cksm ^= 0xFFFF;
    message[size - 2] = (uint8_t)(cksm&0xFF);
    message[size - 1] = (uint8_t)((cksm&0xFF00) >> 8);
}

int16_t checkCRC(uint8_t * message, uint8_t size){
    unsigned long cksm = 0;
    for(int i = 2; i < size - 2; i++) cksm += message[i];
    cksm ^= 0xFFFF;
    return cksm-(message[size - 2]+(message[size - 1]<<8));
}