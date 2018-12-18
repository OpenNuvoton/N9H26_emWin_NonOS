/****************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 18/04/25 11:43a $
 * @brief    SpiLoader_gzip source file
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "N9H26.h"
#include "spiloader.h"
#include "tag.h"

#define LOAD_IMAGE    0
#define CHECK_HEADER_ONLY    1

ERRCODE DrvSPU_Open(void);
VOID spuDacPLLEnable (void);
VOID spuDacPrechargeEnable (void);

UINT8 image_buffer[512];
unsigned char *imagebuf;
unsigned int *pImageList;


int do_bootm (UINT32 u32SrcAddress,UINT32 u32DestAddress, UINT32 u32Mode);

extern void lcmFill2Dark(unsigned char*);
extern void initVPostShowLogo(void);
extern void AudioChanelControl(void);
extern void backLightEnable(void);
int kfd, mfd;
LCDFORMATEX lcdInfo;

#ifdef __Security__
#define KEY_INDEX 1
void RPMC_CreateRootKey(unsigned char *u8uid, unsigned int id_len, unsigned char *rootkey);
#endif

UINT32 u32TimerChannel = 0;
void Timer0_300msCallback(void)
{
#ifdef __BACKLIGHT__
    backLightEnable();
#endif
    sysClearTimerEvent(TIMER0, u32TimerChannel);
}

#define __DDR2__
#define E_CLKSKEW   0x0088ff00

void initClock(void)
{
    UINT32 u32ExtFreq;
    UINT32 reg_tmp;

    u32ExtFreq = sysGetExternalClock();     /* Hz unit */

    if(u32ExtFreq==12000000)
    {
        outp32(REG_SDREF, 0x805A);
    }
    else
    {
        outp32(REG_SDREF, 0x80C0);
    }

#ifdef __UPLL_240__
    /* support UPLL 240MHz, MPLL 360MHz, APLL 432MHz */
    outp32(REG_CKDQSDS, E_CLKSKEW);
    #ifdef __DDR2__
        outp32(REG_SDTIME, 0x2ABF394A);     /* REG_SDTIME for N9H26 360MHz SDRAM clock */
        outp32(REG_SDMR, 0x00000432);
        outp32(REG_MISC_SSEL, 0x00000155);  /* set MISC_SSEL to Reduced Strength to improve EMI */
    #endif

    /* initial DRAM clock BEFORE inital system clock since we change it from low (216MHz by IBR) to high (360MHz) */
    sysSetDramClock(eSYS_MPLL, 360000000, 360000000);   /* change from 216MHz (IBR) to 360MHz */

    /* initial system clock */
    sysSetSystemClock(eSYS_UPLL,
                    240000000,      /* Specified the APLL/UPLL clock, unit Hz */
                    240000000);     /* Specified the system clock, unit Hz */
    sysSetCPUClock (240000000);     /* Unit Hz */
    sysSetAPBClock ( 60000000);     /* Unit Hz */

    /* set APLL to 432MHz to support TVout (need 27MHz) */
    sysSetPllClock(eSYS_APLL, 432000000);
#endif  /* __UPLL_240__ */

#ifdef __UPLL_264__
    /* support UPLL 264MHz, MPLL 396MHz, APLL 432MHz */
    outp32(REG_CKDQSDS, E_CLKSKEW);
    #ifdef __DDR2__
        outp32(REG_SDTIME, 0x332F5A4B);     /* REG_SDTIME for N9H26 396MHz SDRAM clock */
        outp32(REG_SDMR, 0x00000432);
        outp32(REG_MISC_SSEL, 0x00000155);  /* set MISC_SSEL to Reduced Strength to improve EMI */
    #endif

    /* initial DRAM clock BEFORE inital system clock since we change it from low (216MHz by IBR) to high (396MHz) */
    sysSetDramClock(eSYS_MPLL, 396000000, 396000000);   /* change from 216MHz (IBR) to 396MHz */

    /* initial system clock */
    sysSetSystemClock(eSYS_UPLL,
                    264000000,      /* Specified the APLL/UPLL clock, unit Hz */
                    264000000);     /* Specified the system clock, unit Hz */
    sysSetCPUClock (264000000);     /* Unit Hz */
    sysSetAPBClock ( 66000000);     /* Unit Hz */

    /* set APLL to 432MHz to support TVout (need 27MHz) */
    sysSetPllClock(eSYS_APLL, 432000000);
#endif  /* __UPLL_264__ */

    /* always set HCLK234 to 0 */
    reg_tmp = inp32(REG_CLKDIV4) | CHG_APB;     /* MUST set CHG_APB to HIGH when configure CLKDIV4 */
    outp32(REG_CLKDIV4, reg_tmp & (~HCLK234_N));
    
#ifdef __UPLL_NOT_SET__
    sysprintf("Spi Loader DONOT set anything and follow IBR setting !!\n");
    sysprintf("  REG_SDTIME = 0x%08X\n", inp32(REG_SDTIME));
#endif  /* __UPLL_NOT_SET__  */
}

#ifndef __No_LCM__
static UINT32 bIsInitVpost=FALSE;
void initVPostShowLogo(void)
{
    if(bIsInitVpost==FALSE)
    {
        bIsInitVpost = TRUE;
        //lcdInfo.ucVASrcFormat = DRVVPOST_FRAME_YCBYCR;
        lcdInfo.ucVASrcFormat = DRVVPOST_FRAME_RGB565;
        lcdInfo.nScreenWidth = PANEL_WIDTH;
        lcdInfo.nScreenHeight = PANEL_HEIGHT;
        vpostLCMInit(&lcdInfo, (UINT32*)FB_ADDR);
        //backLightEnable();
    }
}
#endif
void init(void)
{
    WB_UART_T  uart;
    UINT32  u32ExtFreq;
    UINT32 u32Cke = inp32(REG_AHBCLK);

    /* Reset SIC engine to fix USB update kernel and mvoie file */
    outp32(REG_AHBCLK, u32Cke  | (SIC_CKE | NAND_CKE | SD_CKE));
    outp32(REG_AHBIPRST, inp32(REG_AHBIPRST )|SIC_RST );
    outp32(REG_AHBIPRST, 0);
    outp32(REG_AHBCLK,u32Cke);

    sysDisableCache();
    sysFlushCache(I_D_CACHE);
    sysEnableCache(CACHE_WRITE_BACK);
    u32ExtFreq = sysGetExternalClock();          /* KHz unit */

    /* enable UART */
    sysUartPort(1);
    uart.uiFreq = u32ExtFreq;                    /* Hz unit */
    uart.uiBaudrate = 115200;
    uart.uart_no = WB_UART_1;
    uart.uiDataBits = WB_DATA_BITS_8;
    uart.uiStopBits = WB_STOP_BITS_1;
    uart.uiParity = WB_PARITY_NONE;
    uart.uiRxTriggerLevel = LEVEL_1_BYTE;
    sysInitializeUART(&uart);
    sysprintf("SPI Loader gzip start (%s).\n", DATE_CODE);
#ifdef __UPLL_240__
    sysprintf("SPI Loader gzip start 240MHz (%s)\n", DATE_CODE);
#elif defined __UPLL_264__
    sysprintf("SPI Loader gzip start 264MHz (%s)\n", DATE_CODE);
#else
    sysprintf("SPI Loader gzip start (%s)\n", DATE_CODE);
#endif

}

/*-----------------------------------------------------------------------------
 * For RTC feature
 *---------------------------------------------------------------------------*/
#define RTC_DELAY       500000

void RTC_Check(void)
{
    UINT32 volatile i;

    i =0;
    while((inp32(REG_FLAG) & RTC_REG_FLAG) != RTC_REG_FLAG)
    {
        i++;
        if(i > RTC_DELAY)
        {
            //sysprintf("Time out\n");
            break;
        }
    }
}


/* The following codes are added to support Linux tag list [2007/03/21] */
/* currently, only compressed romfs's size and physical address are supported! */
int  TAG_create(unsigned int addr, unsigned int size)
{
    static struct tag *tlist;
    
    tlist = (struct tag *) 0x100; /* will destroy BIB_ShowInfo() */
    
    /* tag-list start */
//    sysprintf("tlist->hdr.tag = ATAG_CORE;\n");
    tlist->hdr.tag = ATAG_CORE;
    tlist->hdr.size = tag_size (tag_core);
    tlist = tag_next (tlist);

    /* tag-list node */
    tlist->hdr.tag = ATAG_INITRD2;
    tlist->hdr.size = tag_size (tag_initrd);
    tlist->u.initrd.start = addr;  /* romfs starting address  */
    tlist->u.initrd.size = size;   /* romfs size  */
    tlist = tag_next (tlist);

    /* tag-list node */
//     tlist->hdr.tag = ATAG_MACADDR;
//     tlist->hdr.size = tag_size (tag_macaddr);
//     memcpy(&tlist->u.macaddr.mac[0], &_HostMAC[0], 6);
//     
//     uprintf("===>%02x %02x %02x %02x %02x %02x\n", tlist->u.macaddr.mac[0],
//                                                    tlist->u.macaddr.mac[1],
//                                                    tlist->u.macaddr.mac[2],
//                                                    tlist->u.macaddr.mac[3],
//                                                    tlist->u.macaddr.mac[4],
//                                                    tlist->u.macaddr.mac[5],
//                                                    tlist->u.macaddr.mac[6]);
                                               
    tlist = tag_next (tlist);

     /* tag-list end */
    tlist->hdr.tag = ATAG_NONE;
    tlist->hdr.size = 0;

    return 0;
}

volatile int tag_flag = 0, tagaddr,tagsize;

int main(void)
{
    unsigned int startBlock;
    unsigned int fileLen;
    unsigned int executeAddr;
    
#ifdef __Security__
    UINT8     u8UID[8];
    unsigned char ROOTKey[32];     /* Rootkey array */
    unsigned char HMACKey[32];     /* HMACkey array */
    unsigned char HMACMessage[4];  /* HMAC message data, use for update HMAC key */
    unsigned char Input_tag[12];   /* Input tag data for request conte */
    unsigned char RPMCStatus;
#endif
    int count, i;
    void    (*fw_func)(void);
    
    outp32(0xFF000000, 0);

    init();
    initClock();

    DrvSPU_Open();
    spuDacPLLEnable();
    spuDacPrechargeEnable();

#ifndef __No_LCM__
    initVPostShowLogo();
#else
    /* Turn on HCLK4_CKE clock control */
    outpw(REG_AHBCLK, inpw(REG_AHBCLK) | HCLK4_CKE);
#endif

#ifdef __No_RTC__
    sysprintf("* Not Config RTC\n");
    outp32(REG_APBCLK, inp32(REG_APBCLK) & ~RTC_CKE);   /* disable RTC clock to save power */
#else
    #ifdef __RTC_HW_PWOFF__
    sysprintf("Enable HW Power Off\n");
    /* RTC H/W Power Off Function Configuration */
    RTC_Check();    /* waiting for RTC regiesters ready for access */
    outp32(PWRON, (inp32(PWRON) & ~PCLR_TIME) | 0x60005);   /* Press Power Key during 6 sec to Power off (0x'6'0005) */
    RTC_Check();
    outp32(RIIR, 0x4);
    RTC_Check();
    outp32(REG_APBCLK, inp32(REG_APBCLK) & ~RTC_CKE);   /* disable RTC clock to save power */
    #else
    /* RTC H/W Power Off Function Configuration */
    RTC_Check();    /* waiting for RTC regiesters ready for access */
    outp32(PWRON, (inp32(PWRON) & ~PCLR_TIME) & ~0x4);   /* Press Power Key during 6 sec to Power off (0x'6'0005) */
    RTC_Check();
    outp32(RIIR, 0x4);
    RTC_Check();
    sysprintf("Disable HW Power Off - 0x%X\n",inp32(PWRON));
    outp32(REG_APBCLK, inp32(REG_APBCLK) & ~RTC_CKE);   /* disable RTC clock to save power */
    #endif
#endif
    /* 2013/9/26, enable External RESET Debounce feature */
    /* with debounce counter 0x0FFF (4096 * 83.3ns = 341.1968us) */
    outp32(REG_EXTRST_DEBOUNCE, inp32(REG_EXTRST_DEBOUNCE) & (~EXTRST_DEBOUNCE));   /* MUST disable debounce before set counter to 0 */
    outp32(REG_DEBOUNCE_CNTR, inp32(REG_DEBOUNCE_CNTR) & (~DEBOUNCE_CNTR));
    outp32(REG_DEBOUNCE_CNTR, inp32(REG_DEBOUNCE_CNTR) | 0x0FFF);
    outp32(REG_EXTRST_DEBOUNCE, inp32(REG_EXTRST_DEBOUNCE) | EXTRST_DEBOUNCE);      /* enable debounce after set counter */
    
    /* 2013/9/26, suspend USBH to save power. MUST enable clock first, and then suspend it. */
    outp32(REG_AHBCLK, inp32(REG_AHBCLK) | HCLK3_CKE);
    outp32(REG_AHBCLK2, inp32(REG_AHBCLK2) | OHCI_CKE | H20PHY_CKE);        /* enable USB Host clock */
    outp32(REG_USBPCR0, inp32(REG_USBPCR0) & (~BIT8));                      /* suspend USB Host PHY 0 */
    for (i=0; i<2000; i++);     /* MUST wait suspend be completed before disable USB Host clock. */
    outp32(REG_AHBCLK2, inp32(REG_AHBCLK2) & (~(OHCI_CKE | H20PHY_CKE)));   /* disable USB Host clock */

    /* 2013/10/1, suspend USBD to save power. MUST enable clock first, and then suspend it. */
    outp32(REG_AHBCLK, inp32(REG_AHBCLK) | HCLK3_CKE);
    outp32(REG_AHBCLK, inp32(REG_AHBCLK) | USBD_CKE);           /* enable USB Device clock */
    outp32(PHY_CTL, inp32(PHY_CTL) & (~Phy_suspend));           /* suspend USB Device PHY */
    for (i=0; i<2000; i++);     /* wait suspend be completed before disable USB Device clock. */
    outp32(REG_AHBCLK, inp32(REG_AHBCLK) & (~USBD_CKE));        /* disable USB Device clock */

    imagebuf = (UINT8 *)((UINT32)image_buffer | 0x80000000);
    pImageList=((unsigned int *)(((unsigned int)image_buffer)|0x80000000));

    /* Initial DMAC and NAND interface */
    SPI_OpenSPI();
#ifdef __Security__
    if ((RPMC_ReadUID(u8UID)) == -1)
    {
        sysprintf("read id error !!\n");
        return -1;
    }

    sysprintf("SPI flash uid [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",u8UID[0], u8UID[1],u8UID[2], u8UID[3],u8UID[4], u8UID[5],u8UID[6], u8UID[7]);
  
    /* first stage, initial rootkey */
    RPMC_CreateRootKey((unsigned char *)u8UID,8, ROOTKey);    /* caculate ROOTKey with UID & ROOTKeyTag by SHA256 */

    /* Second stage, update HMACKey after ever power on. without update HMACkey, Gneiss would not function */
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
    /* input tag is send in to RPMC, it could be time stamp, serial number and so on */
    for(i= 0; i<12;i++)
        Input_tag[i] = u8UID[i%8];
    
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
            
    if(RPMC_Challenge(KEY_INDEX, HMACKey, Input_tag)!=0)
    {
        sysprintf("RPMC_Challenge Fail!!\n" );
        /* return signature miss-match */
        while(1);
    }
    else
        sysprintf("RPMC_Challenge Pass!!\n" );
#endif        
    memset(imagebuf, 0, 1024);
    sysprintf("Load Image ");
    /* read image information */
#ifndef __OTP_4BIT__
    SPIReadFast(0, 63*1024, 1024, (UINT32*)imagebuf);  /* offset, len, address */
#else
    outpw(REG_GPEFUN1, (inpw(REG_GPEFUN1)  & ~(MF_GPE8 |MF_GPE9)) | 0x44);

    JEDEC_Probe();
    spiFlashFastReadQuads(63*1024, 1024, (UINT32*)imagebuf);  /* offset, len, address */
#endif

    if (((*(pImageList+0)) == 0xAA554257) && ((*(pImageList+3)) == 0x63594257))
    {
        count = *(pImageList+1);

        pImageList=((unsigned int*)(((unsigned int)image_buffer)|0x80000000));
        startBlock = fileLen = executeAddr = 0;
        
        /* load logo first */
        pImageList = pImageList+4;
        for (i=0; i<count; i++)
        {
            if (((*(pImageList) >> 16) & 0xffff) == 4)    /* logo */
            {
                startBlock = *(pImageList + 1) & 0xffff;
                executeAddr = *(pImageList + 2);
                fileLen = *(pImageList + 3);
#ifndef __OTP_4BIT__
                SPIReadFast(0, startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#else
                spiFlashFastReadQuads(startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#endif
                break;
            }
            /* pointer to next image */
            pImageList = pImageList+12;
        }

        pImageList=((unsigned int*)(((unsigned int)image_buffer)|0x80000000));
        startBlock = fileLen = executeAddr = 0;

        /* load romfs file */
        pImageList = pImageList+4;
        for (i=0; i<count; i++)
        {
            if (((*(pImageList) >> 16) & 0xffff) == 2)    /* RomFS */
            {
                startBlock = *(pImageList + 1) & 0xffff;
                executeAddr = *(pImageList + 2);
                fileLen = *(pImageList + 3);
#ifndef __OTP_4BIT__
                SPIReadFast(0, startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#else
                spiFlashFastReadQuads(startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#endif
                tag_flag = 1;
                tagaddr = executeAddr;
                tagsize = fileLen;

                break;
            }
            /* pointer to next image */
            pImageList = pImageList+12;
        }

        pImageList=((unsigned int*)(((unsigned int)image_buffer)|0x80000000));
        startBlock = fileLen = executeAddr = 0;

        /* load execution file */
        pImageList = pImageList+4;
        for (i=0; i<count; i++)
        {
            if (((*(pImageList) >> 16) & 0xffff) == 1)    /* execute */
            {
                UINT32 u32Result;
                startBlock = *(pImageList + 1) & 0xffff;
                executeAddr = *(pImageList + 2);
                fileLen = *(pImageList + 3);
                
#ifndef __OTP_4BIT__
                SPIReadFast(0, startBlock * 0x10000, 64, (UINT32*)IMAGE_BUFFER);
#else
                spiFlashFastReadQuads(startBlock * 0x10000, 64, (UINT32*)IMAGE_BUFFER);
#endif                
                
                u32Result = do_bootm(IMAGE_BUFFER, 0, CHECK_HEADER_ONLY);
                        
                if(u32Result)        /* Not compressed */
                {

#ifndef __OTP_4BIT__
                    SPIReadFast(0, startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#else
                    spiFlashFastReadQuads(startBlock * 0x10000, fileLen, (UINT32*)executeAddr);
#endif
                }
                else                /* compressed */
                {
#ifndef __OTP_4BIT__
                    SPIReadFast(0, startBlock * 0x10000, fileLen, (UINT32*)IMAGE_BUFFER);
#else
                    spiFlashFastReadQuads(startBlock * 0x10000, fileLen, (UINT32*)IMAGE_BUFFER);
#endif
                    do_bootm(IMAGE_BUFFER, executeAddr, LOAD_IMAGE);
                }
                sysSetGlobalInterrupt(DISABLE_ALL_INTERRUPTS);
                sysSetLocalInterrupt(DISABLE_FIQ_IRQ);
//                  Invalid and disable cache
                sysDisableCache();
                sysInvalidCache();
//                 memcpy(0x0, kbuf, CP_SIZE);

                if(tag_flag)
                {
                    sysprintf("Create Tag - Address 0x%08X, Size 0x%08X\n",tagaddr,tagsize );
                    TAG_create(tagaddr,tagsize);
                }
                
                /* JUMP to kernel */
                sysprintf("Jump to kernel\n\n");

                
                //lcmFill2Dark((char *)(FB_ADDR | 0x80000000));
                outp32(REG_AHBIPRST, JPG_RST | SIC_RST |UDC_RST | EDMA_RST);
                outp32(REG_AHBIPRST, 0);
                outp32(REG_APBIPRST, UART1RST | UART0RST | TMR1RST | TMR0RST );
                outp32(REG_APBIPRST, 0);
                sysFlushCache(I_D_CACHE);
                
                    
                fw_func = (void(*)(void))(executeAddr);
                fw_func();
                break;
            }
            /* pointer to next image */
            pImageList = pImageList+12;
        }
    }

    return(0); /* avoid compilation warning */
}
