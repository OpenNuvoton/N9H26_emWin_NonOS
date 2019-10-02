#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "N9H26.h"

#if defined(__GNUC__)
__attribute__((aligned(32))) UINT8 LoadAddr[]=
{
	#include "../../VPOST/ASIC/roof_320x240_RGB565.dat"
};
#else
__align(32) UINT8 LoadAddr[]=
{
	#include "../../VPOST/ASIC/roof_320x240_RGB565.dat"
};
#endif

LCDFORMATEX lcdFormat;
extern void TransferLengthTest(void);
extern void ColorSpaceTransformTest(void);
extern void SPIFlashTest(void);
extern void SPIFlashQuadTest(void);
extern void UARTTest(void);

int main()
{
	WB_UART_T uart;
	UINT32 u32ExtFreq, u32Item;

	u32ExtFreq = sysGetExternalClock();    	/* Hz unit */
	uart.uart_no = WB_UART_1; 
	uart.uiFreq = u32ExtFreq;
	uart.uiBaudrate = 115200;
	uart.uiDataBits = WB_DATA_BITS_8;
	uart.uiStopBits = WB_STOP_BITS_1;
	uart.uiParity = WB_PARITY_NONE;
	uart.uiRxTriggerLevel = LEVEL_1_BYTE;
	sysInitializeUART(&uart);	

	sysEnableCache(CACHE_WRITE_BACK);

	sysSetLocalInterrupt(ENABLE_IRQ);

	/* init timer */		
	sysSetTimerReferenceClock (TIMER0, u32ExtFreq);	/* Hz unit */
	sysStartTimer(TIMER0, 
					100, 
					PERIODIC_MODE);

	lcdFormat.ucVASrcFormat = DRVVPOST_FRAME_RGB565;	
	vpostLCMInit(&lcdFormat, (UINT32*)LoadAddr);

	EDMA_Init();

	do
	{
		sysprintf("==================================================================\n");
		sysprintf("[1] Transfer Length and Direction Test \n");
		sysprintf("[2] Color Space Transform Test \n");			
		sysprintf("[3] PDMA+SPIFlash Test \n");	
		sysprintf("[4] PDMA+SPIFlash Quad Test \n");
		sysprintf("[5] PDMA+UART Test \n");	
		sysprintf("[6] PDMA+ADC Test \n");	
		sysprintf("==================================================================\n");

		u32Item = sysGetChar();
		
		switch(u32Item) 
		{
			case '1': 
				TransferLengthTest();			
				break;
					
			case '2':
				ColorSpaceTransformTest();					
				break;		
				
			case '3':
				SPIFlashTest();					
				break;

			case '4':
				SPIFlashQuadTest();					
				break;

			case '5':
				UARTTest();					
				break;
				
			case '6':
				sysprintf("Please refer to adc sample code \n");											
				break;
				
			case 'Q':
			case 'q': 
				u32Item = 'Q';
				sysprintf("quit edma test...\n");				
				break;	
				
			}
		
	}while(u32Item!='Q');
	
}








