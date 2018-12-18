 /****************************************************************************
 * @file     Secureic.c
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 18/04/25 11:43a $
 * @brief    Secureic sample file
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "N9H26.h"

#define SECUREIC_DBG_PRINTF    sysprintf

#define KEY_INDEX 1
void RPMC_CreateRootKey(unsigned char *u8uid, unsigned int id_len, unsigned char *rootkey);

int main()
{
    WB_UART_T uart;
    UINT32 u32ExtFreq;
    int count = 0;
    UINT8     u8UID[8];
    UINT8     u8JID[3];
    unsigned char ROOTKey[32];      /* Rootkey array */  
    unsigned char HMACKey[32];      /* HMACkey array */
    unsigned char HMACMessage[4];   /* HMAC message data, use for update HMAC key */
    unsigned char Input_tag[12];    /* Input tag data for request conte */
    unsigned char RPMCStatus;
    unsigned int RPMC_counter;
  
    u32ExtFreq = sysGetExternalClock();        /* Hz unit */    
    uart.uart_no = WB_UART_1;
    uart.uiFreq = u32ExtFreq;
    uart.uiBaudrate = 115200;
    uart.uiDataBits = WB_DATA_BITS_8;
    uart.uiStopBits = WB_STOP_BITS_1;
    uart.uiParity = WB_PARITY_NONE;
    uart.uiRxTriggerLevel = LEVEL_1_BYTE;
    sysInitializeUART(&uart);

    SECUREIC_DBG_PRINTF("SpiFlash Test...\n");

    spiFlashInit();

    if ((RPMC_ReadJEDECID(u8JID)) == -1)
    {
        sysprintf("read id error !!\n");
        return -1;
    }

    sysprintf("SPI flash jid [0x%02X_%02X_%02X]\n",u8JID[0],u8JID[1],u8JID[2]);
    
    
    if ((RPMC_ReadUID(u8UID)) == -1)
    {
        sysprintf("read id error !!\n");
        return -1;
    }

    sysprintf("SPI flash uid [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",u8UID[0], u8UID[1],u8UID[2], u8UID[3],u8UID[4], u8UID[5],u8UID[6], u8UID[7]);
  
    /* first stage, initial rootkey */
    RPMC_CreateRootKey((unsigned char *)u8UID,8, ROOTKey);    /* caculate ROOTKey with UID & ROOTKeyTag by SHA256 */

    RPMCStatus = RPMC_WrRootKey(KEY_INDEX, ROOTKey);          /* initial Rootkey, use first rootkey/counter pair */
    if(RPMCStatus == 0x80)
    {
        /* Write rootkey success */
        sysprintf("RPMC_WrRootKey Success - 0x%02X!!\n",RPMCStatus );
    }
    else
    {
        /* write rootkey fail, check datasheet for the error bit */
        sysprintf("RPMC_WrRootKey Fail - 0x%02X!!\n",RPMCStatus );
    }
    /* initial rootkey operation done     */
    /* Second stage, update HMACKey after ever power on. without update HMACkey, Gneiss would not function*/
    HMACMessage[0] = rand()%0x100;        /* Get random data for HMAC message, it can also be serial number, RTC information and so on. */
    HMACMessage[1] = rand()%0x100;
    HMACMessage[2] = rand()%0x100;
    HMACMessage[3] = rand()%0x100;

    /* Update HMAC key and get new HMACKey. 
       HMACKey is generated by SW using Rootkey and HMACMessage.
       RPMC would also generate the same HMACKey by HW 
     */
    RPMCStatus = RPMC_UpHMACkey(KEY_INDEX, ROOTKey, HMACMessage, HMACKey);     
    if(RPMCStatus == 0x80)
    {
        /* update HMACkey success */
        sysprintf("RPMC_UpHMACkey Success - 0x%02X!!\n",RPMCStatus );
    }
    else
    {
        /* write HMACkey fail, check datasheet for the error bit */
        sysprintf("RPMC_UpHMACkey Fail - 0x%02X!!\n",RPMCStatus );
    }
    
    /* Third stage, increase RPMC counter */  
    /* input tag is send in to RPMC, it could be time stamp, serial number and so on*/
    Input_tag[0] = '2';
    Input_tag[1] = '0';
    Input_tag[2] = '1';
    Input_tag[3] = '6';
    Input_tag[4] = '0';
    Input_tag[5] = '5';
    Input_tag[6] = '2';
    Input_tag[7] = '7';
    Input_tag[8] = '1';
    Input_tag[9] = '6';
    Input_tag[10] = '2';
    Input_tag[11] = '7';
    
    RPMCStatus = RPMC_IncCounter(KEY_INDEX, HMACKey, Input_tag);    
    if(RPMCStatus == 0x80)
    {
        /* increase counter success */
        sysprintf("RPMC_IncCounter Success - 0x%02X!!\n",RPMCStatus );
    }
    else
    {
        /* increase counter fail, check datasheet for the error bit */
        sysprintf("RPMC_IncCounter Fail - 0x%02X!!\n",RPMCStatus );
    }

    /* counter data in stoage in public array counter[], data is available if RPMC_IncCounter() operation successed */    
    RPMC_counter = RPMC_ReadCounterData();

    /* increase RPMC counter done*/
    sysprintf("RPMC_counter 0x%X\n",RPMC_counter);

    /* Main security operation call challenge*/
    while(1)
    {
        if(RPMC_Challenge(KEY_INDEX, HMACKey, Input_tag)!=0)
        {
            sysprintf("RPMC_Challenge Fail!!\n" );
            /* return signature miss-match */
            return 0;
        }
        else
        {
            if(count > 500)
            {
                sysprintf("\n" );
                RPMCStatus = RPMC_IncCounter(KEY_INDEX, HMACKey, Input_tag);            
                if(RPMCStatus == 0x80)
                {
                    /* increase counter success */
                    sysprintf("RPMC_IncCounter Success - 0x%02X!!\n",RPMCStatus );
                }
                else
                {
                    /* increase counter fail, check datasheet for the error bit */
                    sysprintf("RPMC_IncCounter Fail - 0x%02X!!\n",RPMCStatus );
                    while(1);
                }
                /* counter data in stoage in public array counter[], data is available if RPMC_IncCounter() operation successed */
                RPMC_counter = RPMC_ReadCounterData();
                
                /* increase RPMC counter done*/
                sysprintf("RPMC_counter 0x%X\n",RPMC_counter);
                count = 0;
            }
            else
                count++;
            sysprintf("." );
        }
    }
}

