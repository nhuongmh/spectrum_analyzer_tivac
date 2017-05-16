#include <stdint.h>
#include <stdbool.h>

#define BUFF_IN_SIZE  				128
#define UART_TxBUFF				128


COMPLEX samples[N] ;
COMPLEX twiddle[N] ;
uint32_t			uDMA_Err_count = 0 ;

uint8_t ui8ControlTable[1024] __attribute__ ((aligned(1024)));


static uint32_t ui32ADCBuffA[BUFF_IN_SIZE] ;
static uint32_t ui32ADCBuffB[BUFF_IN_SIZE] ;

static uint8_t 	ui8TxBuffA[UART_TxBUFF] ;
static uint8_t 	ui8TxBuffB[UART_TxBUFF] ;

void vFFT_task(void );
void vOsci_task(void );
void vIdle_task(void );

bool  booADC_A = true ;				// this varible used to indicate whether or not Buff A is full
bool 	booUART_A = true ;			// the same with ADC
bool  ProcessFlag = false ;
uint32_t 	errorprocess  = 0 ;


typedef enum MODE
{
	osciMode,
	fftMode ,
	IdleMode 
}	mainMode ;

mainMode currentMode = fftMode ;
//int8_t currentMode = IdleMode ;

