/*
 * copyright (c) Espressif System 2010
 *
 * This file is part of Espressif IOT Software Development Kit.
 * Copyright (C) 2013 - 2016, Espressif Systems
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

typedef enum {
    SPI_FLASH_RESULT_OK,
    SPI_FLASH_RESULT_ERR,
    SPI_FLASH_RESULT_TIMEOUT
} SpiFlashOpResult;

typedef struct{
	uint32	deviceId;
	uint32	chip_size;    // chip size in byte
	uint32	block_size;
	uint32  sector_size;
	uint32  page_size;
	uint32  status_mask;
} SpiFlashChip;

#define SPI_FLASH_SEC_SIZE      4096

uint32 spi_flash_get_id(void);
SpiFlashOpResult spi_flash_erase_sector(uint16 sec);
SpiFlashOpResult spi_flash_write(uint32 des_addr, uint32 *src_addr, uint32 size);
SpiFlashOpResult spi_flash_read(uint32 src_addr, uint32 *des_addr, uint32 size);

#endif
