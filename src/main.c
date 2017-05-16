#include "stdint.h"
#include "fft_tiva.h"
#include "main.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_adc.h"
#include "inc/hw_uart.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/udma.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "utils/uartstdio.h"


COMPLEX 	CentralBuffer[M][N] ;		
bool 					FlagIn[M] ;
bool 					FlagOut[M] ;
uint8_t 			Incount = 0; 
uint8_t 			Outcount = M - 3 ;

uint32_t 		ErrCount  =0  ;
extern void PF4_IntHandler(void);

void GPIOF_config(void)
{ 		volatile unsigned long delay;
	  	SYSCTL_RCGC2_R |= 0x00000020;     // 1) activate clock for Port F
  	delay = SYSCTL_RCGC2_R;           // allow time for clock to start
  	GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port F
  	GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0
 	 // only PF0 needs to be unlocked, other bits can't be locked
  	GPIO_PORTF_AMSEL_R = 0x00;        // 3) disable analog on PF
  	GPIO_PORTF_PCTL_R = 0x00000000;   // 4) PCTL GPIO on PF4-0
  	GPIO_PORTF_DIR_R = 0x0E;          // 5) PF4,PF0 in, PF3-1 out
  	GPIO_PORTF_AFSEL_R = 0x00;        // 6) disable alt funct on PF7-0
  	GPIO_PORTF_PUR_R = 0x11;          // enable pull-up on PF0 and PF4
  	GPIO_PORTF_DEN_R = 0x1F;          // 7) enable digital I/O on PF4-0
	
	GPIOIntDisable(GPIO_PORTF_BASE, GPIO_PIN_4);
	GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);
	GPIOIntRegister(GPIO_PORTF_BASE,PF4_IntHandler);
	GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_FALLING_EDGE );
	GPIOIntEnable(GPIO_PORTF_BASE , GPIO_PIN_4);
	
	//SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOF ) ;
}
void uDMA_config(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UDMA)) ;
	SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UDMA);
  IntEnable(INT_UDMAERR);
	
	uDMAControlBaseSet(ui8ControlTable);		//set base addr for control table
	//uDMAChannelAssign(UDMA_CH14_ADC0_0); 			
	
	IntEnable(INT_UDMA);
																	
	uDMAEnable();

}

void ADC_config(void)
{				//??
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	//	SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_ADC0);
				//select ADC pin PE3
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);				
			//configure ADC, sequencer SS3( 1 sample)
	SysCtlDelay( 20);
	ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_TIMER, 0);			
			//configure ADC SS3, step 0, single input channel 0

	ADCSequenceStepConfigure(ADC0_BASE, 3 , 0, ADC_CTL_CH0 | ADC_CTL_END | ADC_CTL_IE);										
	//enable uDMA for ADC SS0
	ADCSequenceDMAEnable(ADC0_BASE,3);

	ADCIntEnableEx(ADC0_BASE, ADC_INT_DMA_SS3 ) ;
	//enable ADC active during CPU sleeping

	IntEnable(INT_ADC0SS3) ;
	
	uDMAChannelAttributeDisable(UDMA_CHANNEL_ADC0,UDMA_ATTR_ALTSELECT | 
																	UDMA_ATTR_REQMASK | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_USEBURST) ;
	
	// need to be confirmed. change DMA priority every 1/25k (s), so use bust request or not
 //	uDMAChannelAttributeEnable(UDMA_CHANNEL_ADC0, UDMA_ATTR_USEBURST ) ; // use bust request 
	uDMAChannelControlSet(UDMA_CHANNEL_ADC0 | UDMA_PRI_SELECT, 
							UDMA_SIZE_32 | UDMA_SRC_INC_NONE | UDMA_DST_INC_32 | UDMA_ARB_1 );
	uDMAChannelControlSet(UDMA_CHANNEL_ADC0 | UDMA_ALT_SELECT, 
							UDMA_SIZE_32 | UDMA_SRC_INC_NONE | UDMA_DST_INC_32 | UDMA_ARB_1 );
							// hope this shit will work :((
		uDMAChannelTransferSet(UDMA_CHANNEL_ADC0 | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG,
		(void *)(ADC0_BASE + ADC_O_SSFIFO0 ) , ui32ADCBuffA,  sizeof(ui32ADCBuffA)) ;
		uDMAChannelTransferSet(UDMA_CHANNEL_ADC0 | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG,
		(void *)(ADC0_BASE + ADC_O_SSFIFO0 ), ui32ADCBuffB, sizeof(ui32ADCBuffB)) ;
		
	uDMAChannelEnable(UDMA_CHANNEL_ADC0) ;	
	//start when trigger is detected
	 ADCIntClear(ADC0_BASE, 3);
	ADCSequenceEnable(ADC0_BASE, 3);																													
	
}

void Timer_config(void)
{
	//This function use to configure timer to trigger ADC
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0 ) ;
	TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC) ;
	TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet() / FS-1 ) ;			// trigger ADC at FS (kHz)
	TimerControlTrigger(TIMER0_BASE, TIMER_A, true );
	SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);
	//IntMasterEnable() ;
	
	//TimerEnable(TIMER0_BASE, TIMER_A );		// put here or main
}

void UART_config(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);
	GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0|GPIO_PIN_1);
	
	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
		(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));			//1.2M baud
	
	UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX4_8, UART_FIFO_RX4_8);
	UARTEnable(UART0_BASE) ;
	UARTDMAEnable(UART0_BASE, UART_DMA_TX);
	
	IntEnable(INT_UART0);										//interrupt UART0
	
	uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0TX, UDMA_ATTR_ALTSELECT | 
										UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK | UDMA_ATTR_USEBURST) ;
	//uDMAChannelAttributeEnable(UDMA_CHANNEL_UART0TX, UDMA_ATTR_USEBURST );		// use bust request
	//				NEED TO CONFIRM: USEBURST OR NOT
	//
	uDMAChannelControlSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT, 
							   UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1);
	uDMAChannelControlSet(UDMA_CHANNEL_UART0TX | UDMA_ALT_SELECT , 
									UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1 ) ;
	
	uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG,
										ui8TxBuffA, (void *)(UART0_BASE + UART_O_DR),sizeof(ui8TxBuffA)) ;
	uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG,
										ui8TxBuffB, (void *)(UART0_BASE + UART_O_DR ), sizeof(ui8TxBuffB)) ;
	

	uDMAChannelEnable(UDMA_CHANNEL_UART0TX) ; //just enable transmitter, not receive anyway
}

void UART0_IntHandler(void)
{
	uint32_t ui32Status ;
	uint32_t ui32Mode ;
	uint16_t  n ;

	
	ui32Status = UARTIntStatus(UART0_BASE, true) ;
	UARTIntClear(UART0_BASE, ui32Status) ;
	
	ui32Mode =  uDMAChannelModeGet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT);
	if(ui32Mode ==UDMA_MODE_STOP)
	{
		// transfer buffer A is done ???
		//
		//		Put something here to notify CPU send data to buffer A
			booUART_A = true ; // transfer from buff A UART is done

			if(FlagOut[Outcount] == true)
				{
					for(n=0;n<N;n++)
					{
				ui8TxBuffA[n] =(uint8_t)  (CentralBuffer[Outcount][n]).real ;
					}
					Outcount++ ;
					if(Outcount==M)					//update counter
						Outcount = 0 ;
					
					FlagOut[Outcount] = false ;
					FlagIn[Outcount] = true ;
				}
				else ErrCount++ ;
		
					// configure to re-use this buffer when DMA playing with buffer B	
		uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT,
		UDMA_MODE_PINGPONG, ui8TxBuffA,(void *)( UART0_BASE + UART_O_DR ),sizeof(ui8TxBuffA)) ;
				
	}
	
	ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_UART0TX | UDMA_ALT_SELECT );
	
	if(ui32Mode == UDMA_MODE_STOP) 
	{
		//Transfer buffer B is done ??
		//
		//	Put something here to notify CPU send data to buffer B
		//
		booUART_A = false ;
		
					if(FlagOut[Outcount] == true)
				{
					for(n=0;n<N;n++)
					{
				ui8TxBuffB[n] =(uint8_t)  (CentralBuffer[Outcount][n]).real ;
					}
					Outcount++ ;
					if(Outcount==M)					//update counter
					Outcount = 0 ;
					
					FlagOut[Outcount] = false ;
					FlagIn[Outcount] = true ;
				}
				else ErrCount++ ;
		
		uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_ALT_SELECT, 
		 UDMA_MODE_PINGPONG, ui8TxBuffB, (void *)(UART0_BASE + UART_O_DR ), sizeof(ui8TxBuffB)) ;
		
	}
	
	if(!uDMAChannelIsEnabled(UDMA_CHANNEL_ADC0) )				//if DMA ADC0 is disabled?
	{
		//
		// Put something here to notify CPU or Timer know that DMA ADC0 is disabled
		// should re-configure DMA for ADC0 transfer
	}
	
	
}

void uDMA_Err_Handler(void)
{
	uint32_t ui32Status ;
	ui32Status = uDMAErrorStatusGet() ;
	if(ui32Status)
	{
		uDMAErrorStatusClear() ;
		uDMA_Err_count ++ ;
	}
}
void ADC0_IntHandler(void)
{		
	uint32_t ui32Status ;
	uint32_t ui32Mode ;
	uint16_t  n ;
	ui32Status = ADCIntStatusEx(ADC0_BASE , true);
	ADCIntClear(ADC0_BASE, ui32Status) ;				
	
	ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_ADC0 | UDMA_PRI_SELECT ) ;
	
		if(ui32Mode == UDMA_MODE_STOP )
		{	
			booADC_A = true ;
			//
			// 		get data to buffer A(ADC)  is done  => \\\copy it into circullar buffer \\\
			//																											=> Process data directly

//			if( (currentMode == fftMode) ||( currentMode == phaseMode ) )

		  if(FlagIn[Incount] == true)
			{
					for(n=0;n<N;n++)
					{
							(CentralBuffer[Incount][n]).real = (float) (ui32ADCBuffA[n])/4096 ;
					}
					FlagIn[Incount] = false ;
					FlagOut[Incount] = false ;
					Incount++;
					if(Incount==M)
							Incount = 0 ;
			}
			else ErrCount++ ;
		
			//reconfigure ADC Buff A to DMA
			uDMAChannelTransferSet(UDMA_CHANNEL_ADC0 | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG,
												(void *)( ADC0_BASE + ADC_O_SSFIFO0), ui32ADCBuffA, sizeof(ui32ADCBuffA)) ;
			
			
		}
	ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_ADC0 | UDMA_ALT_SELECT) ;
		if(ui32Mode == UDMA_MODE_STOP )
		{
			//
			// notify CPU that buffer B (ADC) is done => copy it into circullar buffer
			//
			booADC_A = false  ;
//			if((currentMode == fftMode)||(currentMode == phaseMode)) 

				if(FlagIn[Incount] == true)
				{
					for(n=0;n<N;n++)
					{
						(CentralBuffer[Incount][n]).real = (float) (ui32ADCBuffB[n])/4096 ;
					}
					FlagIn[Incount] = false ;
					FlagOut[Incount] = false ;
					Incount++ ;
					if(Incount==M)
						Incount =0 ;
				}
				else ErrCount++ ;
			
			uDMAChannelTransferSet(UDMA_CHANNEL_ADC0 | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG,
																	(void *)(ADC0_BASE + ADC_O_SSFIFO0), ui32ADCBuffB, sizeof(ui32ADCBuffB));
		}

}

extern void PF4_IntHandler( void)
{
	while(!GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0));
	switch(currentMode)
	{
		case IdleMode 			: 
		{			currentMode = fftMode; 	
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 , GPIO_PIN_1);
		}	break ;
		case fftMode		:  
		{			currentMode = osciMode  ; 
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_2);
		}	break ;
		case osciMode  :  
		{			currentMode = IdleMode   ;
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_3);
		}	break ;			
	} 
	GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);

}


int main (void)
{
	
  uint32_t 		n;
	uint8_t 			count ;
//	float fft_debug[N] ;
	SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);		//80MHz
	SysCtlPeripheralClockGating(true) ;			// enable peripherals to operate when CPU is in sleep
	
	FPULazyStackingEnable();									//enable floating point 
  FPUEnable();
  
	GPIOF_config();
	uDMA_config();
	UART_config();
	Timer_config() ;
	ADC_config() ;

	
	 twiddle_array(N, twiddle) ;
	
		for(count = 0; count<M;	count++)
		{
			FlagIn[count] = true ;
			FlagOut[count] = true ;
		}
	
		IntMasterEnable() ;
	TimerEnable(TIMER0_BASE, TIMER_A ) ; // begin
	  while(1)
  {
		while(currentMode == IdleMode)
		{
			
		}
		while(currentMode == fftMode)
		{
			for(count=0; count<M;count++)
			{
				if(FlagIn[count] == false)
				{
					FlagOut[count] = false ;
					FlagIn[count]  = false ;		//make sure that no anytask can touch this memory
					fft(CentralBuffer[count] , N , twiddle) ;
					for(n = 0; n< N ; n++)
					{
					(CentralBuffer[count][n]).real = sqrtf((CentralBuffer[count][n]).real * (CentralBuffer[count][n]).real + (CentralBuffer[count][n].imag)*(CentralBuffer[count][n]).imag) ;
					}
					FlagOut[count] = true ;
					}
			}
		}
		while(currentMode == osciMode)
		{
			for(count= 0; count<M ; count++)
			{
				if(FlagIn[count]==false)
				{
					FlagOut[count] = true ;
				}
			}
			
		}

  }

}
