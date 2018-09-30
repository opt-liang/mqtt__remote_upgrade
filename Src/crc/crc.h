#ifndef __CRC__
#define __CRC__
#include<stdint.h>
uint16_t crc16(const uint8_t *buf, uint32_t len);
uint32_t Crc32_ComputeBuf( const void *buf, uint32_t bufLen );
uint32_t chip_crc( uint32_t crc32, const void *buf, uint32_t bufLen );
uint32_t chip_total_crc( uint32_t crc32 );
#endif






