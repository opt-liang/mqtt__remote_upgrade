#ifndef __SDIC__
#define __SDIC__
void PayloadEncrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *encBuffer );
void PayloadDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *decBuffer );
void JoinComputeSKeys( uint8_t *macAddr, const uint8_t *serverNonce );
void PRINT_KEY( void );

extern uint8_t NETKEY[16] ;
extern const uint8_t APPKEY[16];
extern uint16_t DevNonce;
extern uint32_t serverNonce;
#endif













