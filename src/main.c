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


COMPLEX 	CentralBuffer[N] ;		
bool 					FlagIn  = true;
uint32_t 		ErrCount_Adc  =0  ;
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
	
	IntEnable(INT_UDMA);
																	
	uDMAEnable();

}

void ADC_config(void)
{				//??
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);				
	SysCtlDelay( 20);
	ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_TIMER, 0);			
	ADCSequenceStepConfigure(ADC0_BASE, 3 , 0, ADC_CTL_CH0 | ADC_CTL_END | ADC_CTL_IE);										
	ADCSequenceDMAEnable(ADC0_BASE,3);
	ADCIntEnableEx(ADC0_BASE, ADC_INT_DMA_SS3 ) ;
	IntEnable(INT_ADC0SS3) ;
	uDMAChannelAttributeDisable(UDMA_CHANNEL_ADC3,UDMA_ATTR_ALTSELECT | 
																	UDMA_ATTR_REQMASK | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_USEBURST) ;
	uDMAChannelControlSet(UDMA_CHANNEL_ADC3 | UDMA_PRI_SELECT, 
							UDMA_SIZE_32 | UDMA_SRC_INC_NONE | UDMA_DST_INC_32 | UDMA_ARB_1 );
	uDMAChannelControlSet(UDMA_CHANNEL_ADC3 | UDMA_ALT_SELECT, 
							UDMA_SIZE_32 | UDMA_SRC_INC_NONE | UDMA_DST_INC_32 | UDMA_ARB_1 );
		uDMAChannelTransferSet(UDMA_CHANNEL_ADC3 | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG,
		(void *)(ADC0_BASE + ADC_O_SSFIFO3 ) , ui32ADCBuffA,  sizeof(ui32ADCBuffA)) ;
		uDMAChannelTransferSet(UDMA_CHANNEL_ADC3 | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG,
		(void *)(ADC0_BASE + ADC_O_SSFIFO3 ), ui32ADCBuffB, sizeof(ui32ADCBuffB)) ;
	uDMAChannelEnable(UDMA_CHANNEL_ADC3) ;	
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
}

void UART_config(void)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);
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
	
	ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_ADC3 | UDMA_PRI_SELECT ) ;
	
		if(ui32Mode == UDMA_MODE_STOP )
		{	

		  if(FlagIn == true)
			{
					for(n=0;n<N;n++)
					{
							(CentralBuffer[n]).real = (float) (ui32ADCBuffA[n])/20 ;
					}
					FlagIn = false ;
			}
			else ErrCount_Adc++ ;
		
			uDMAChannelTransferSet(UDMA_CHANNEL_ADC3 | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG,
												(void *)( ADC0_BASE + ADC_O_SSFIFO3), ui32ADCBuffA, sizeof(ui32ADCBuffA)) ;
			
		}
	ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_ADC3 | UDMA_ALT_SELECT) ;
		if(ui32Mode == UDMA_MODE_STOP )
		{

				if(FlagIn== true)
				{
					for(n=0;n<N;n++)
					{
						(CentralBuffer[n]).real = (float) (ui32ADCBuffB[n])/20 ;
					}
					FlagIn= false ;
				}
				else ErrCount_Adc++ ;
			
			uDMAChannelTransferSet(UDMA_CHANNEL_ADC3 | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG,
																	(void *)(ADC0_BASE + ADC_O_SSFIFO3), ui32ADCBuffB, sizeof(ui32ADCBuffB));
		}

}

extern void PF4_IntHandler( void)
{
	while(!GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0));
	switch(currentMode)
	{
		case fftMode		:  
		{			currentMode = osciMode  ; 
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_2);
		}	break ;
		case osciMode  :  
		{			currentMode = fftMode   ;
					GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_3);
		}	break ;			
	} 
	GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);

}


int main (void)
{
	
  uint32_t 		n;
	uint8_t 			transfer8 ;
	SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);	
	SysCtlPeripheralClockGating(true) ;			
	
	FPULazyStackingEnable();									
  FPUEnable();
  
	GPIOF_config();
	uDMA_config();
	UART_config();
	Timer_config() ;
	ADC_config() ;

	 twiddle_array(N, twiddle) ;
	
		IntMasterEnable() ;
	TimerEnable(TIMER0_BASE, TIMER_A ) ; // begin
	  while(1)
  {
		while(currentMode == fftMode)
		{
				if(FlagIn == false)
				{
					fft(CentralBuffer , N , twiddle) ;
					for(n = 0; n< N ; n++)
					{
					(CentralBuffer[n]).real = sqrtf((CentralBuffer[n]).real * (CentralBuffer[n]).real + (CentralBuffer[n].imag)*(CentralBuffer[n]).imag) ;
					}
					
					UARTCharPut(UART0_BASE, 255) ;
					for(n=0;n<N;n++)
					{
						transfer8 = (uint8_t) CentralBuffer[n].real ;
						if (transfer8 ==255)
							transfer8 = 250 ;
						UARTCharPut(UART0_BASE, transfer8) ;
					}
					FlagIn = true ;
				}
			}
		while(currentMode == osciMode)
		{
				if(FlagIn==false)
				{
					for(n=0;n<N;n++)
					{
						transfer8 = (uint8_t) CentralBuffer[n].real ;
						if (transfer8 ==255)
							transfer8 = 250 ;
						UARTCharPut(UART0_BASE, transfer8) ;
				}
				FlagIn = true ;
		}

  }
}
}
