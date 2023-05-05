#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include <stdint.h>

#include "diskio.h"
#include "fatfs_sd.h"

#include "cy_retarget_io.h"

#define TRUE  1
#define FALSE 0

#define SD_CS_PIN CYBSP_D5

#define __SD_CS_OUT()      cyhal_gpio_init(SD_CS_PIN, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false)
#define __SD_CS_CLR()      cyhal_gpio_write(SD_CS_PIN, false)
#define __SD_CS_SET()      cyhal_gpio_write(SD_CS_PIN, true)

static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType;
static uint8_t PowerFlag = 0;


extern cyhal_spi_t mSPI;


/* SPI Chip Select */
static void SELECT(void)
{
  __SD_CS_CLR();
}

/* SPI Chip Deselect */
static void DESELECT(void)
{
  __SD_CS_SET();
}

/* SPI data transmission */
static void SPI_TxByte(BYTE data)
{
  cyhal_spi_send(&mSPI, data);
}

/* SPI return type function for sending and receiving data */
static uint8_t SPI_RxByte(void)
{
  uint8_t data;
  data = 0;
  cyhal_spi_recv(&mSPI, &data);
  //printf("%d\r\n\n", data);
  return data;
}

/* SPI pointer functions for sending and receiving data */
static void SPI_RxBytePtr(uint8_t *buff) 
{
  *buff = SPI_RxByte();
  //buff = SPI_RxByte();
  //printf("buff = %d\r\n\n", *buff);
}


/* SD card ready standby */
static uint8_t wait_not_busy(unsigned int timeout){

	unsigned int tmr;
	for(tmr = timeout*10; tmr; tmr--){
		uint8_t d =  SPI_RxByte();
		if(d == 0xFF) return true;
		cyhal_system_delay_us(100);
	}
	return false;

}


/* power on */
static void SD_PowerOn(void) 
{
  uint8_t cmd_arg[6];
  uint32_t Count = 0x1FFF;
  
  DESELECT();
  
  for(int i = 0; i < 10; i++)
  {
    SPI_TxByte(0xFF);
  }
  
  /* SPI Chips Select */
  SELECT();
  
  /* Initial GO_IDLE_STATE state transition */
  cmd_arg[0] = (CMD0 | 0x40);
  cmd_arg[1] = 0;
  cmd_arg[2] = 0;
  cmd_arg[3] = 0;
  cmd_arg[4] = 0;
  cmd_arg[5] = 0x95;
  
  /* send command */
  for (int i = 0; i < 6; i++)
  {
    SPI_TxByte(cmd_arg[i]);
  }
  
  /* waiting for response */
  while ((SPI_RxByte() != 0x01) && Count)
  {
    Count--;
  }
  
  DESELECT();
  SPI_TxByte(0XFF);
  
  PowerFlag = 1;
}

/* power off */
static void SD_PowerOff(void) 
{
  PowerFlag = 0;
}

/* Check power status */
static uint8_t SD_CheckPower(void) 
{
  /*  0=off, 1=on */
  return PowerFlag;
}


static BYTE SD_ReadyWait (void)
{
	BYTE d;
	UINT tmr;

	for(tmr = 5000; tmr; tmr--){
		d = SPI_RxByte();
		if(d == 0xFF) break;
		cyhal_system_delay_us(100);
    }
	//printf("d = %x\r\n\n", d);
	return (d == 0xFF) ? 1 : 0;
}



/* data packet reception */
static bool SD_RxDataBlock(BYTE *buff, UINT btr) 
{
  uint8_t token;
  
  printf("from rx dtat block\r\n\n");

  /* waiting for response till 100ms */
  UINT tmr;
  
  for(tmr = 1000; tmr ; tmr--){
    token = SPI_RxByte(); 
    //printf("token = %x\r\n\n", token);
    if(token == 0xFE) break;
    cyhal_system_delay_us(100); 
  }
  
  /* Error handling when receiving tokens other than 0xFE */
  if(token != 0xFE)
    return FALSE;
  
  /* Receive data into buffer */
  do 
  {
	//printf("from inside do blockk\r\n\n");
    SPI_RxBytePtr(buff++);
    SPI_RxBytePtr(buff++);
  } while(btr -= 2);
  
  SPI_RxByte(); /* Ignore CRC */
  //printf("token = %x\r\n\n", token);
  SPI_RxByte();
  //printf("token = %x\r\n\n", token);
  
  return TRUE;
}


/* data transfer packet */
#if _READONLY == 0

static bool SD_TxDataBlock(const BYTE *buff, BYTE token)
{
  uint8_t resp = 0, wc;
  uint8_t i = 0;

  printf("from tx dtat block\r\n\n");
  /* Waiting for SD card ready */
  if (SD_ReadyWait() != 0xFF)
    return FALSE;
  
  /* token transfer */
  SPI_TxByte(token);      
  
  /* If it is a data token */
  if (token != 0xFD) 
  { 
    wc = 0;
    
    /* 512 bytes data transmission */
    do 
    { 
      SPI_TxByte(*buff++);
      SPI_TxByte(*buff++);
    } while (--wc);
    
    SPI_RxByte();       /* CRC 무시 */
    SPI_RxByte();
    
    /* Receive date response */        
    while (i <= 64) 
    {			
      resp = SPI_RxByte();
      
      /* Error response handling */
      if ((resp & 0x1F) == 0x05) 
        break;
      
      i++;
    }
    
    /* Clear the SPI receive buffer */
    while (SPI_RxByte() == 0);
  }
  
  if ((resp & 0x1F) == 0x05)
    return TRUE;
  else
    return FALSE;
}
#endif /* _READONLY */



/* Send CMD Packet */
static BYTE SD_SendCmd(BYTE cmd, DWORD arg) 
{
  uint8_t crc, res;
  
  SELECT();

  /* SD card standby */
  wait_not_busy(300);

  SPI_TxByte(cmd | 0x40);
  
  int8_t s;
  for(s = 24; s >= 0; s -= 8){
	  SPI_TxByte(arg >> s);
  }
  
  /* CRC preparation per instruction */
  crc = 0xFF;
  if (cmd == CMD0)
    crc = 0x95; /* CRC for CMD0(0) */
  
  if (cmd == CMD8)
    crc = 0x87; /* CRC for CMD8(0x1AA) */
  
  /* CRC send */
  SPI_TxByte(crc);
  
  uint8_t i;
  for(i = 0; ((res = SPI_RxByte()) & 0x80) && i != 0xFF; i++);
  
  //printf("Res from sd send command = %d\r\n\n", res);
  return res;
}


uint8_t SD_cardAcmd(uint8_t cmd, uint32_t arg) {
	  SD_SendCmd(CMD55, 0);
      return SD_SendCmd(cmd, arg);
}
/*-----------------------------------------------------------------------
  Global functions used in fatfs
   Used in the user_diskio.c file
-----------------------------------------------------------------------*/

/* SD card initialization */
DSTATUS SD_disk_initialize(BYTE drv) 
{
  /* Supports only one type of drive */
  if(drv)
	 return STA_NOINIT;

  uint8_t n, type, ocr[4];
  UINT tmr;
  uint8_t status;
  
  uint32_t arg;
  DESELECT();

  uint8_t i;
  for(i = 0; i < 10; i++){
	  SPI_TxByte(0xFF);
  }
  
  SELECT();
  
  for(tmr = SD_INIT_TIMEOUT*10; tmr; tmr--){
	  if((status = SD_SendCmd(CMD0, 0)) == R1_IDLE_STATE){
  			break;
  		}
  		cyhal_system_delay_us(100);
  	}
  
  // check SD version
  if ((SD_SendCmd(CMD8, 0x1AA) & R1_ILLEGAL_COMMAND)) {
  	   type = SD_CARD_TYPE_SD1;
  	   printf("SD_CARD_TYPE_SD1\r\n\n");
  }
  else {
  	   // only need last byte of r7 response
  	   for (i = 0; i < 4; i++) {
  	      status = SPI_RxByte();
  	   }
  	   if (status != 0xAA) {
  	      //error(SD_CARD_ERROR_CMD8);
  	      goto fail;
  	   }
  	   type = SD_CARD_TYPE_SD2;
  	   printf("SD_CARD_TYPE_SD2\r\n\n");
  }
  
  arg = type == SD_CARD_TYPE_SD2 ? 0X40000000 : 0;
  //printf("Arguments = %x\r\n\n", arg);
  
  SD_SendCmd(CMD55, 0);
  status = SD_SendCmd(ACMD41, arg);
  SD_SendCmd(CMD55, 0);
  status = SD_SendCmd(ACMD41, arg);
  //printf("command acmd41 = %d\r\n\n", status);

  for(tmr = SD_INIT_TIMEOUT*10; tmr; tmr--){
	  if((status = SD_cardAcmd(ACMD41, arg)) == R1_READY_STATE){
  			break;
  		}
  	  cyhal_system_delay_us(100);
  }
  printf("SD_CARD Status = %d\r\n\n", status);
  if(tmr <= 0){
	    printf("SD_CARD Status = not ready\r\n\n");
  		//error(SD_CARD_ERROR_ACMD41);
  		goto fail;
  }

  // if SD2 read OCR register to check for SDHC card
  if (type == SD_CARD_TYPE_SD2) {
  	   if (SD_SendCmd(CMD58, 0)) {
  	      //error(SD_CARD_ERROR_CMD58);
  	      goto fail;
  	   }
  	   if ((SPI_RxByte() & 0xC0) == 0xC0) {
  	      type = SD_CARD_TYPE_SDHC;
  	   }
  	    // discard rest of ocr - contains allowed voltage range
  	    for (i = 0; i < 3; i++) {
  	    	SPI_RxByte();
  	    }
  	}


  CardType = type;
  if (type) {			/* OK */
  		Stat &= ~STA_NOINIT;
  		printf("clock high\r\n\n");
  } else {			/* Failed */
  		Stat = STA_NOINIT;
  		printf("failed\r\n\n");
  }
  
  DESELECT();
  return Stat;

fail:
	DESELECT();
  	return false;
}


//------------------------------------------------------------------------------
/** Wait for start block token */
static uint8_t waitStartBlock(void) {
  uint32_t tmr;
  uint8_t status;
  for(tmr = SD_READ_TIMEOUT*10; tmr; tmr--){
	  if((status = SPI_RxByte()) != 0xFF) break;
	  cyhal_system_delay_us(100);
  }
  if(tmr <= 0){
	  //error(SD_CARD_ERROR_READ_TIMEOUT);
	  goto fail;
  }

  if (status != DATA_START_BLOCK) {
    //error(SD_CARD_ERROR_READ);
    goto fail;
  }
  return true;

fail:
  DESELECT();
  return false;
}



uint8_t readRegister(uint8_t cmd, void* buf){
	uint8_t* dst = (uint8_t*)(buf);

	if (SD_SendCmd(cmd, 0)) {
	    //error(SD_CARD_ERROR_READ_REG);
	    goto fail;
	  }
	  if (!waitStartBlock()) {
	    goto fail;
	  }
	  // transfer data
	  uint16_t i;
	  for (i = 0; i < 16; i++) {
	    dst[i] = SPI_RxByte();
	  }
	  SPI_RxByte();;  // get first crc byte
	  SPI_RxByte();;  // get second crc byte
	  DESELECT();
	  return true;

	fail:
	  DESELECT();
	  return false;

}


uint8_t readCSD(csd2_t* csd) {
      return readRegister(CMD9, csd);
}

uint32_t cardSize(void) {
  csd2_t csd;
  if (!readCSD(&csd)) {
    return 0;
  }
  if(csd.csd_ver == 1) {
	    uint32_t c_size = ((uint32_t)csd.c_size_high << 16)
	                      | (csd.c_size_mid << 8) | csd.c_size_low;
	    return (c_size + 1) << 10;
  }
	else {
    //error(SD_CARD_ERROR_BAD_CSD);
    return 0;
  }
}


/* Check disk status */
DSTATUS SD_disk_status(BYTE drv) 
{
  printf("from disk status block\r\n\n");
  if (drv)
    return STA_NOINIT; 
  
  return Stat;
}


/* sector read */
DRESULT SD_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) 
{
  printf("from disk read block\r\n\n");
  if (pdrv || !count)
    return RES_PARERR;
  
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;
  

  if (!(CardType & 4))
    sector *= 512;
  
  SELECT();

  if (count == 1) 
  { 
	//printf("inside count 1\r\n\n");
    if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512))
      count = 0;
    //printf("inside count 0\r\n\n");
  } 
  else 
  { 

    if (SD_SendCmd(CMD18, sector) == 0) 
    {       
      do {
        if (!SD_RxDataBlock(buff, 512))
          break;
        
        buff += 512;
      } while (--count);
      

      SD_SendCmd(CMD12, 0); 
    }
  }
  
  DESELECT();
  SPI_RxByte();
  
  //printf(" count = %d\r\n\n", count);
  return count ? RES_ERROR : RES_OK;
}


#if _READONLY == 0
DRESULT SD_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) 
{
	printf("from disk write block\r\n\n");
  if (pdrv || !count)
    return RES_PARERR;
  
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;
  
  if (Stat & STA_PROTECT)
    return RES_WRPRT;
  
  if (!(CardType & 4))
    sector *= 512;
  
  SELECT();
  
  if (count == 1) 
  { 

    if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE))
      count = 0;
  } 
  else 
  { 

    if (CardType & 2) 
    {
      SD_SendCmd(CMD55, 0);
      SD_SendCmd(CMD23, count);
    }
    
    if (SD_SendCmd(CMD25, sector) == 0) 
    {       
      do {
        if(!SD_TxDataBlock(buff, 0xFC))
          break;
        
        buff += 512;
      } while (--count);
      
      if(!SD_TxDataBlock(0, 0xFD))
      {        
        count = 1;
      }
    }
  }
  
  DESELECT();
  SPI_RxByte();
  
  return count ? RES_ERROR : RES_OK;

}
#endif

/* other functions */
DRESULT SD_disk_ioctl(BYTE drv, BYTE ctrl, void *buff) 
{
	printf("from ioct block\r\n\n");
  DRESULT res;
  BYTE n, csd[16], *ptr = buff;
  WORD csize;
  
  if (drv)
    return RES_PARERR;
  
  res = RES_ERROR;
  
  if (ctrl == CTRL_POWER) 
  {
    switch (*ptr) 
    {
    case 0:
      if (SD_CheckPower())
        SD_PowerOff();          /* Power Off */
      res = RES_OK;
      break;
    case 1:
      SD_PowerOn();             /* Power On */
      res = RES_OK;
      break;
    case 2:
      *(ptr + 1) = (BYTE) SD_CheckPower();
      res = RES_OK;             /* Power Check */
      break;
    default:
      res = RES_PARERR;
    }
  } 
  else 
  {
    if (Stat & STA_NOINIT)
      return RES_NOTRDY;
    
    SELECT();
    
    switch (ctrl) 
    {
    case GET_SECTOR_COUNT: 
      /* The number of sectors in the SD card (DWORD) */
      if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16)) 
      {
        if ((csd[0] >> 6) == 1) 
        { 
          /* SDC ver 2.00 */
          csize = csd[9] + ((WORD) csd[8] << 8) + 1;
          *(DWORD*) buff = (DWORD) csize << 10;
        } 
        else 
        { 
          /* MMC or SDC ver 1.XX */
          n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
          csize = (csd[8] >> 6) + ((WORD) csd[7] << 2) + ((WORD) (csd[6] & 3) << 10) + 1;
          *(DWORD*) buff = (DWORD) csize << (n - 9);
        }
        
        res = RES_OK;
      }
      break;
      
    case GET_SECTOR_SIZE: 
      /* unit size of sector (WORD) */
      *(WORD*) buff = 512;
      res = RES_OK;
      break;
      
    case CTRL_SYNC: 
      /* sync write */
      if (SD_ReadyWait() == 0xFF)
        res = RES_OK;
      break;
      
    case MMC_GET_CSD: 
      /* CSD receive information (16 bytes) */
      if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(ptr, 16))
        res = RES_OK;
      break;
      
    case MMC_GET_CID: 
      /* CID receive information (16 bytes) */
      if (SD_SendCmd(CMD10, 0) == 0 && SD_RxDataBlock(ptr, 16))
        res = RES_OK;
      break;
      
    case MMC_GET_OCR: 
      /* OCR receive information (4 bytes) */
      if (SD_SendCmd(CMD58, 0) == 0) 
      {         
        for (n = 0; n < 4; n++)
        {
          *ptr++ = SPI_RxByte();
        }
        
        res = RES_OK;
      }     
      
    default:
      res = RES_PARERR;
    }
    
    DESELECT();
    SPI_RxByte();
  }
  
  return res;
}
