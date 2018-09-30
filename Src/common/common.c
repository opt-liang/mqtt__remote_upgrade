#include <stdint.h>
#include <string.h>
#include "cmsis_os.h"
#include <stdio.h>
#include "common.h"
#include "stm32f1xx_hal.h"
#include "crc.h"

#define ITM_Port8(n)    (*((volatile unsigned char *)(0xE0000000+4*n)))
#define ITM_Port16(n)   (*((volatile unsigned short*)(0xE0000000+4*n)))
#define ITM_Port32(n)   (*((volatile unsigned long *)(0xE0000000+4*n)))

#define DEMCR           (*((volatile unsigned long *)(0xE000EDFC)))
#define TRCENA          0x01000000

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f) {
  if (DEMCR & TRCENA) {
    while (ITM_Port32(0) == 0);
    ITM_Port8(0) = ch;
  }
  return(ch);
}

bool checkCRC32( const uint8_t *sour,uint32_t size, uint32_t desCRC ){
    
    uint32_t CURRENTCRC = Crc32_ComputeBuf( sour, size );
    
    static uint32_t globalCRC32 = 0;
    if( globalCRC32 != CURRENTCRC ){
        globalCRC32 = CURRENTCRC;
        DEBUG_INFO( "ori crc32 is %08x -- cacl crc32 is %08x\r\n", desCRC ,CURRENTCRC);
    }
    
    if( CURRENTCRC == desCRC ){
		return true;
	}else{
		return false;
	}
}

void _info_( char *flag, uint8_t *buff, uint16_t len ){
    printf("%s[%d]:", flag, len );
    for( uint16_t i = 0; i < len; i ++ ){
        printf("%02x ", buff[i] );
    }
    printf("\r\n");
}

char* memstr(const char* full_data, const char* substr, int full_data_len){  
    if (full_data == NULL || full_data_len <= 0 || substr == NULL) {  
        return NULL;
    }
  
    if (*substr == '\0') {
        return NULL;
    }  
  
    int sublen = strlen(substr);
  
    int i;
    char* cur = (char *)full_data;  
    int last_possible = full_data_len - sublen + 1;
    for (i = 0; i < last_possible && last_possible <= 2048u ; i++) {
        if (*cur == *substr) {
            if (memcmp(cur, substr, sublen) == 0) {
                return cur;
            }
        }
        cur++;
    }
    
    return NULL;
}

uint16_t LIB_HexTo2Asc(uint8_t hex)
{
	uint16_t dat;
	if(((hex&0xF0)>>4)<=9)  dat=((((hex&0xF0)>>4)+0x30)<<8);
		else dat=((((hex&0xF0)>>4)+0x41-10)<<8);
	if((hex&0x0F)<=9)	dat+=((hex&0x0F)+0x30);
		else dat+=((hex&0x0F)+0x41-10);	
	return dat;
}

uint16_t LIB_BigLittleEndian_16BitConvert(uint16_t tdata)           
{
  return ((tdata>>8) + (tdata<<8));
}

void LIB_nByteHexTo2Asc(char *asc, uint8_t *hex, uint16_t len)
{
  uint16_t i;

  for(i = 0; i<len; i++)
  {
    *((uint16_t *)(asc + 2*i)) = LIB_BigLittleEndian_16BitConvert(LIB_HexTo2Asc(hex[i]));
  }
}

void SetPayloadLength( uint8_t *cmd, uint16_t len ){
    len -= 2;
	uint8_t  *plen = (uint8_t *)&len;
	cmd[2] = plen[0];
	cmd[3] = plen[1];
}

void SetPayloadCRC( uint8_t *cmd, uint16_t size ){
    size -= 4;
    uint16_t crc16_t = crc16( cmd+2, size );
    uint8_t *pcrc16_t = (uint8_t *)&crc16_t;
    cmd[size + 2] = pcrc16_t[0];
    cmd[size + 3] = pcrc16_t[1];
}

bool islegalIP( int a,int b, int c, int d ){
    if (a < 0 && a > 255 )
            return false;
    if (b < 0 && b > 255 )  
         return false;
    if (c < 0 && c > 255 )  
            return false;  
    if (d < 0 && d > 255 )  
            return false;
    return true;
}

uint32_t transformatIP( const char *IP ){
    int a, b, c, d;
    uint32_t ip_parameter = 0;
    if( sscanf( (const char*)IP,"%d.%d.%d.%d",&a,&b,&c,&d) == 0x04 ){
        if( islegalIP( a, b, c, d  ) == false ){
            return ip_parameter;
        }
        ip_parameter = ( (a) & 0x000000ff ) | ( (b<<8) & 0x0000ff00 ) | ( (c<<16) & 0x00ff0000 ) | ( (d<<24) & 0xff000000 );
    }
    return ip_parameter;
}


bool isAck( void ){
    #include "cycle_queue.h"
    extern SeqCQueue seqCQueue;
    int16_t waitCount = 0;
    while( QueueNotEmpty( &seqCQueue ) == 0 && waitCount < 2000 ){
        waitCount ++;
        osDelay(1);
    }
    return ( QueueNotEmpty( &seqCQueue ) == 1 ) ;
}

static uint8_t IrqNestLevel = 0;
void BoardDisableIrq( void )
{
    __disable_irq( );
    IrqNestLevel++;
}

void BoardEnableIrq( void )
{
    IrqNestLevel--;
    if( IrqNestLevel == 0 )
    {
        __enable_irq( );
    }
}



void delay_us(uint32_t nTimer){
	uint32_t i=0;
	for(i=0;i<nTimer;i++){
		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
	}
}

void delay_ms( uint32_t nTimer ){
	uint32_t i = 1000 * nTimer;
	delay_us( i );
}



