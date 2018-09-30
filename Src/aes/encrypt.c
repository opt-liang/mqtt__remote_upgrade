
#include "aes.h"
//#include "cmac.h"
#include <string.h>
#include <stdio.h>
#include <stdio.h>
/*!
 * Encryption aBlock and sBlock
 */
 
uint8_t NETKEY[16] = { 0x00 };

const uint8_t APPKEY[16] = {0x0C, 0x7E, 0x15, 0x16, 0x57, 0xAF, 0xDE, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C}; 

static uint8_t aBlock[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };
static uint8_t sBlock[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };
uint16_t DevNonce;
uint32_t serverNonce;
/*!
 * AES computation context variable
 */
static aes_context AesContext;
                          
void PayloadEncrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *encBuffer ){
    uint16_t i;
    uint16_t bufferIndex = 0;
    uint16_t ctr = 1;
    memset( AesContext.ksch, '\0', 240 );
    aes_set_key( key, 16, &AesContext );

    while( size >= 16 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        ctr++;
        aes_encrypt( aBlock, sBlock, &AesContext );
        for( i = 0; i < 16; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
        size -= 16;
        bufferIndex += 16;
    }

    if( size > 0 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        aes_encrypt( aBlock, sBlock, &AesContext );
        for( i = 0; i < size; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
    }
}

void PayloadDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *decBuffer ){
    PayloadEncrypt( buffer, size, key, decBuffer );
}

void JoinComputeSKeys( uint8_t *macAddr, const uint8_t *serverNonce ){
    uint8_t nonce[16];
    uint8_t *pDevNonce = ( uint8_t * )&DevNonce;
    
    memset( AesContext.ksch, '\0', 240 );
    aes_set_key( APPKEY, 16, &AesContext );

    memset( nonce, 0, sizeof( nonce ) );
    nonce[0] = 0x01;
    memcpy( nonce + 1, macAddr, 6 );
    memcpy( nonce + 7, serverNonce, 3 );
    memcpy( nonce + 10, pDevNonce, 2 );
    aes_encrypt( nonce, NETKEY, &AesContext );

}













