/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various existing       */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"         /* Obtains integer types */
#include "diskio.h"     /* Declarations of disk functions */
#include "fatfs_sd.h"
#include <stdio.h>
#include "integer.h"


/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    BYTE pdrv        /* Physical drive number to identify the drive */
)
{
	return SD_disk_status(pdrv);
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv                /* Physical drive number to identify the drive */
)
{
	return SD_disk_initialize(pdrv);
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    BYTE pdrv,        /* Physical drive number to identify the drive */
    BYTE *buff,        /* Data buffer to store read data */
    LBA_t sector,    /* Start sector in LBA */
    UINT count        /* Number of sectors to read */
)
{
	return SD_disk_read(pdrv, buff, sector, count);
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
    BYTE pdrv,            /* Physical drive number to identify the drive */
    const BYTE *buff,    /* Data to be written */
    LBA_t sector,        /* Start sector in LBA */
    UINT count            /* Number of sectors to write */
)
{
	return SD_disk_write(pdrv, buff, sector, count);
}

#endif


DWORD get_fattime (void)
{
    // TODO: Add RTC?
    return (40 << 25) | (1 << 21) | (1 << 16) | (0 << 11) | (0 << 5) | (0 << 0);
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
    BYTE pdrv,        /* Physical drive number (0..) */
    BYTE cmd,        /* Control code */
    void *buff        /* Buffer to send/receive control data */
)
{
	return SD_disk_ioctl(pdrv, cmd, buff);
}

