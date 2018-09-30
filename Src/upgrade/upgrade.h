#ifndef __UPGRADE__
#define __UPGRADE__

extern TimerHandle_t UPGRADETIMEOUTTimer_Handle;

bool CheckUpgradeState( void );
void RequestUpgradePacketTimeout( TimerHandle_t xTimer );
void ProcessingUpgradeCmd( uint8_t *cmd, uint16_t cmdlen );
void ProcessUpgradePackets( const uint8_t *cmd, uint16_t size );
void WaitServerUpgradeStateAck( uint8_t stat );
bool IsUpgradingState( void );

#endif
