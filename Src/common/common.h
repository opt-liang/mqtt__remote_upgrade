#ifndef __COMMOM__
#define __COMMOM__
#include <stdint.h>
#include <stdbool.h>
typedef enum {
    TUBE_TEN,
    TUBE_SINGLE
}TUBE_PORT;

#define DEBUG_INFO(fmt,args...) 	printf(fmt, ##args)

extern char* memstr(const char* full_data, const char* substr, int full_data_len);
#define findstring(x,y,z)   memstr(x,y,z)
void LIB_nByteHexTo2Asc(char *asc, uint8_t *hex, uint16_t len);
bool islegalIP( int a,int b, int c, int d );
uint32_t transformatIP( const char *IP );
bool isAck( void );
void BoardDisableIrq( void );
void BoardEnableIrq( void );
void delay_ms( uint32_t nTimer );

void _info_( char *flag, uint8_t *buff, uint16_t len );
bool checkCRC32( const uint8_t *sour,uint32_t size, uint32_t desCRC );
void SetPayloadLength( uint8_t *cmd, uint16_t len );
void SetPayloadCRC( uint8_t *cmd, uint16_t size );
#endif


