#ifndef __UPGRADE__H__
#define __UPGRADE__H__
#include "common.h"
#include "mqtt_app.h"
#include "insideflash.h"
#include "key_value.h"
#include "crc.h"

#define UPGRADESIZE                 (100*1024)
#define UPGRADEADDR                 ADDRESS_MAPPING( 62 )
#define UPGRADEFLAG                 ADDRESS_MAPPING(9)
#define FLASHCACHE                  ADDRESS_MAPPING(119)

#define UPGRADESUCCESSFUL            0x00
#define TOTALCRCERR                  0x01
#define CURRUPGRADEPACKETCRCERR      0x02
#define CURRUPGRADEPACKETNONE        0x03

#define UPGRADE_FLAG    "UPGRADE_FLAG"
#define CURR_PACKET     "CURR_PACKET"
#define TOTAL_PACKET    "TOTAL_PACKET"
#define TOTAL_CRC       "TOTAL_CRC"
#define UPGRADE_TOPIC   "UPGRADE_TOPIC"
#define MQTT_SEND_TOPIC "UP"

#define UPGRADETIMERWAIT   100

/*-------------------public variable----------------------------------*/
extern TimerHandle_t UPGRADETIMEOUTTimer_Handle;

/*-------------------private variable----------------------------------*/
static uint8_t  upgradeIndex = 1;
static uint8_t  upgradeIndexBackup = 1;
static uint16_t currentPacketSize = 0;
static uint8_t UPGRADETOPIC[9] = { 0 };
static bool upgradeflag = false;
static bool serverackstate = false;

/*-------------------public function declare--------------------------*/
bool CheckUpgradeState( void );
void ProcessingUpgradeCmd( uint8_t *cmd, uint16_t cmdlen );
void RequestUpgradePacketTimeout( TimerHandle_t xTimer );
void CheckServerUpgradeStateAck( void );
void WaitServerUpgradeStateAck( uint8_t stat );
void ProcessUpgradePackets( const uint8_t *cmd, uint16_t size );

/*-------------------private function declare--------------------------*/
static bool EraseUpgradeBackupArea( void );
static void RequestCurrentUpgradePacketIndex( void );
static bool UpgradeStartup( const uint8_t *upgradeMsg );
static bool GetTotalUpgradePacketNumber( uint8_t* packetNum );
static bool GetCurrentUpgradePacketIndex( uint8_t* packetIndex );
static bool UpdateCurrentUpgradePacketIndex( uint8_t currentPacketIndex );
static bool IsLastUpgradePacket( void );
static void UpgradeSucceedHandleEvent( void );
static void ReportUpgradeProgress( uint8_t progress );
static void ReportUpgradeState( uint8_t err );
static void ReplyUpgradeCmd( void );

/**************************************************************************/

static bool ResetUpgradingState( void ){
    uint32_t upgrad_flag = 0;
    if( set_key_value( UPGRADE_FLAG, UINT32, (uint8_t *)&upgrad_flag ) ){
        return true;
    }
    return false;
}

static bool IsUpgradingState( void ){
    uint32_t upgrad_flag = 0;
    if( get_key_value( UPGRADE_FLAG, UINT32, (uint8_t *)&upgrad_flag ) ){
        if( upgrad_flag == 0x10086 ){
            return true;
        }
    }
    return false;
}

//when reboot, Check whether the upgrade is completed
bool CheckUpgradeState( void ){
	
	if ( IsUpgradingState() ){
        DEBUG_INFO( "restart Check Upgrade State\r\n");
		if( GetCurrentUpgradePacketIndex( &upgradeIndex ) ){
            uint32_t topic = 0;
            if( get_key_value( UPGRADE_TOPIC, STRINGS, (uint8_t *)&topic ) && topic != 0 ){
                memcpy( UPGRADETOPIC, (uint8_t *)topic, 6 );
                RequestCurrentUpgradePacketIndex();
                return true;
            }
            return false;
        }else{
            return false;
        }
	}else{
		return false;
	}
}

void ProcessingUpgradeCmd( uint8_t *cmd, uint16_t cmdlen ){
    
    xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
    if( serverackstate == true || upgradeflag == true ){
        DEBUG_INFO( "ProcessingUpgradeCmd: The upgrade has been completed, and the upgrade command is invalid\r\n" );
        return;
    }

    if( IsUpgradingState() == true ){
        extern void LanReboot( void );
        LanReboot();
        uint32_t oriTotalCRC = 0;
        get_key_value( TOTAL_CRC, UINT32, (uint8_t *)&oriTotalCRC );
        uint32_t newTotalCRC = *((uint32_t *)(cmd + 13));
        if( oriTotalCRC == newTotalCRC ){
            if( GetCurrentUpgradePacketIndex( &upgradeIndex ) ){
                xTimerStart( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
                return;
            }
        }else{
            DEBUG_INFO("Restart Upgrade\r\n");
        }
    }
    if( cmdlen == 19 && UpgradeStartup( cmd + 6 ) == false ){
        return ;
    }
    if( EraseUpgradeBackupArea() == false ){
        DEBUG_INFO("ProcessingUpgradeCmd: erase upgrade Backup failed\r\n");
        return ;
    }
    ReplyUpgradeCmd();
    RequestCurrentUpgradePacketIndex();
    serverackstate = false;
    upgradeflag = false;
}

void RequestUpgradePacketTimeout( TimerHandle_t xTimer ){
    RequestCurrentUpgradePacketIndex();
}

static bool EraseUpgradeBackupArea( void ){
    if( flash_erase( UPGRADEADDR, 50 ) == false ){//a total of 100KB
        return false;
    }
    return true;
}

static bool UpgradeStartup( const uint8_t *upgradeMsg ){
    
    //    8         4         4           4
    //   sub     allCRC  allPacketNum currentPacketNum
    
    if( upgradeMsg[6] < 40u || upgradeMsg[6] > 0xc8 ){
        DEBUG_INFO( "UpgradeStartup: upgrade para error failed, upgrade code size error, size is %d\r\n", upgradeMsg[6] );
        return false;
    }

    uint8_t topic[ 8 ] = { 0 };
    memset( topic, 0, 8 );
    
    memcpy( topic, upgradeMsg, 6 );
    uint32_t total_crc = *( uint32_t *)(upgradeMsg+7);
    uint32_t total_packet = upgradeMsg[6];
    uint32_t curr_packet = 1;
    uint32_t upgrade_flag = 0x10086;
    
    if( set_key_value( UPGRADE_TOPIC, STRINGS, topic ) && \
        set_key_value( TOTAL_CRC, UINT32, (uint8_t *)&total_crc ) &&\
        set_key_value( TOTAL_PACKET, UINT32, (uint8_t *)&total_packet ) &&\
        set_key_value( CURR_PACKET, UINT32, (uint8_t *)&curr_packet ) &&\
        set_key_value( UPGRADE_FLAG, UINT32, (uint8_t *)&upgrade_flag ) ){
//            if( flash_erase( UPGRADEFLAG , 1 ) == false ){
//                return false;
//            }
            DEBUG_INFO( "total crc is:%08x\r\n", total_crc );
            DEBUG_INFO( "total packet number is:%d\r\n", total_packet );
            DEBUG_INFO( "\r\nchange upgrade flag area\r\n");
            DEBUG_INFO( "Start to upgrade\r\n");
            memcpy( UPGRADETOPIC, topic, 6 );
            upgradeIndex = 1;
            return true;
    }
    return false;
}

static bool GetTotalUpgradePacketNumber( uint8_t* packetNum ){
    
    uint32_t total_packet = 0;
    if( get_key_value( TOTAL_PACKET, UINT32, (uint8_t *)&total_packet ) && total_packet != 0 ){
        *packetNum = ( uint8_t )total_packet;
        return true;
    }
    return false;
}

static bool GetCurrentUpgradePacketIndex( uint8_t* packetIndex ){
    
    uint32_t curr_packet = 0;
    if( get_key_value( CURR_PACKET, UINT32, (uint8_t *)&curr_packet ) && curr_packet != 0 ){
        *packetIndex = ( uint8_t )curr_packet;
        return true;
    }
    return false;
}

static bool UpdateCurrentUpgradePacketIndex( uint8_t currentPacketIndex ){

    uint32_t curr_packet = currentPacketIndex;
    if( set_key_value( CURR_PACKET, UINT32, (uint8_t *)&curr_packet ) ){
        curr_packet = 0;
        if( get_key_value( CURR_PACKET, UINT32, (uint8_t *)&curr_packet ) && curr_packet == currentPacketIndex ){
            return true;
        }
    }
    return false;
}


static bool IsLastUpgradePacket( void ){
    
    uint8_t TotalPackets = 0;
    uint8_t CurrentPacketID = 0;

    if( GetTotalUpgradePacketNumber( &TotalPackets ) == false || GetCurrentUpgradePacketIndex( &CurrentPacketID ) == false ){
        DEBUG_INFO( "Abnormal parameter error reporting\r\n");
        return false;
    }
    
    if( TotalPackets < CurrentPacketID ){
        if( TotalPackets == ( CurrentPacketID - 1 ) ){
            return true;
        }else{
            ResetUpgradingState();
            xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
            return false;
        }
    }else{
        return false;
    }
}

static bool CheckUpgradeCodeTotalCRC32( void ){
    
    uint32_t totalPacketSize = 0;
    
    uint32_t totalCRC = 0;
    
    get_key_value( TOTAL_CRC, UINT32, ( uint8_t *)&totalCRC );
    
    uint8_t TotalPackets = 0;
    if( GetTotalUpgradePacketNumber( &TotalPackets ) ){
        totalPacketSize = ( TotalPackets - 1 ) * 512 + currentPacketSize;
        if( totalPacketSize > (100*1024) ){
            DEBUG_INFO( "CheckUpgradeCodeTotalCRC32:upgrade total crc para error\r\n");
            return false;
        }
        if ( checkCRC32( (uint8_t *)UPGRADEADDR, totalPacketSize, totalCRC ) ){
            DEBUG_INFO( "CheckUpgradeCodeTotalCRC32:upgrade total crc para successful\r\n");
            return true;
        }else{
            DEBUG_INFO( "check Upgrade Code Total CRC32 failed\r\n");
        }
    }
    return false;
}

static bool PackagingCurrentUpgradeTopic( void ){
    
    for( uint8_t i = 0; i < 6; i ++ ){
        if( ( UPGRADETOPIC[i] < 0x30 ) ||\
            ( UPGRADETOPIC[i] > 0x39 && UPGRADETOPIC[i] < 0x41 ) ||\
            ( UPGRADETOPIC[i] > 0x5A && UPGRADETOPIC[i] < 0x61 ) ||\
            ( UPGRADETOPIC[i] > 0x7A ) ){
                ResetUpgradingState();//erase original area
                DEBUG_INFO( "packagingUpgrading:topic format error\r\n");
                return false;
        }
    }
    
    uint8_t CurrentPacketID = 0;
    uint8_t indexofupgradepacket[3] = { 0 };
    memset( indexofupgradepacket, 0, 3);
    
    if( GetCurrentUpgradePacketIndex( &CurrentPacketID ) == false ){
        DEBUG_INFO( "PackagingCurrentUpgradeTopic:CurrentPacketID error size:%02x\r\n", CurrentPacketID);
        return false;
    }
    sprintf( (char *)indexofupgradepacket, "%02x", CurrentPacketID );
    memcpy( UPGRADETOPIC + 6, indexofupgradepacket, 2 );
    return true;
}

static void RequestCurrentUpgradePacketIndex( void ){//Request current upgrade package from node
	
    if( upgradeflag ){
        return ;
    }
    
    if( IsLastUpgradePacket() ){
        if( CheckUpgradeCodeTotalCRC32() ){
            upgradeflag = true;
        }else{
            ReportUpgradeState( TOTALCRCERR );
			//Reply server total checkout failure
		}
        xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
        ResetUpgradingState();//erase original area
        return;
    }
    
    static uint8_t repeatIndexCount = 0;
    
    if( upgradeIndexBackup == upgradeIndex ){
        
        if( repeatIndexCount != 0 && ( repeatIndexCount % 2 ) == 0 ){
            MqttCancelSubTopic( (char *)UPGRADETOPIC );
        }
        
        repeatIndexCount ++;
        if( repeatIndexCount >= 10 ){
            repeatIndexCount = 0;
            ResetUpgradingState();
            xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
            ReportUpgradeState( CURRUPGRADEPACKETNONE );//report
            return;
        }
    }else{
        repeatIndexCount = 0;
    }
        
    upgradeIndexBackup = upgradeIndex;
    
    if( PackagingCurrentUpgradeTopic() == false ){
        ReportUpgradeState( CURRUPGRADEPACKETNONE );
        return ;
    }
	
    DEBUG_INFO( "\r\nClaim upgrade packet topic:%s\r\n", UPGRADETOPIC);
	
    MqttSubTopic( (char *)UPGRADETOPIC );
    
    xTimerStart( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );

}

void ProcessUpgradePackets( const uint8_t *cmd, uint16_t size ){ 

    if( serverackstate != true || upgradeflag != true  ){
		
		if( ( upgradeIndex ) != cmd[6] ){
            DEBUG_INFO( "Duplicate update package\r\n");
			return;
		}
        
        xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
        
        uint8_t cycleCount = 0;
        
		const uint16_t Count = *((uint16_t*)(cmd+2)) ;
        
        uint32_t crc = *((uint32_t*)(cmd+7));

        flash_rewrite:

        if( checkCRC32( (cmd+11), Count-11, crc) == true ){

            if( flash_write( (cmd+11), ( UPGRADEADDR + ( *(cmd+6) - 1 ) * 512 ) , Count-11 ) ){
                ReportUpgradeProgress( upgradeIndex );
                if( UpdateCurrentUpgradePacketIndex( ++upgradeIndex ) ){
                    currentPacketSize = Count-11;
					DEBUG_INFO( "CRC successful\r\n");
				}else{
                    ResetUpgradingState();
                    return ;
                }
            }else{
                
                DEBUG_INFO( "CRC failed\r\n");
                
                uint8_t  shifting   = ( *(cmd+6) - 1 ) % 4;
                uint32_t currenpageheading = UPGRADEADDR + ( ( *(cmd+6) - 1 ) / 4 ) * 2048 ;
                
                if( shifting != 0 ){

                    uint32_t crc = Crc32_ComputeBuf( (uint8_t*)currenpageheading, shifting * 512 );
                    writeflashcache:
                    flash_erase( FLASHCACHE, 1 );
                    flash_write( (const uint8_t *)currenpageheading, FLASHCACHE, shifting * 512 );
                    
                    if( checkCRC32( (uint8_t *)FLASHCACHE, shifting * 512, crc) == false ){
                        cycleCount ++;
                        if( cycleCount >= 5 ){
                            
							DEBUG_INFO( "Write flash cache buffer failure\r\n");
                            return ;
                        }
                        goto writeflashcache;
                    }
					writecurrenpageheading:
                    flash_erase( currenpageheading, 1 );
                    flash_write( (const uint8_t *)FLASHCACHE, currenpageheading, shifting * 512 );
                    if( checkCRC32( (uint8_t *)currenpageheading, shifting * 512, crc) == false ){
                        cycleCount ++;
                        if( cycleCount >= 5 ){
                            
							DEBUG_INFO( "Rewrite flash cache buffer to Original flash addr failure\r\n");
                            return;
                        }
                        goto writecurrenpageheading;
                    }
                }else{
					flash_erase( currenpageheading, 1 );
					cycleCount ++;
                    if( cycleCount >= 5 ){
                        
						DEBUG_INFO( "Rewrite upgrade packet failure\r\n");
                        return ;
                    }
                }
                goto flash_rewrite;
            }
        }else{
            ReportUpgradeState( CURRUPGRADEPACKETCRCERR );
            DEBUG_INFO( "CRC ERROR, Receiving an error update package\r\n" );
        }
		RequestCurrentUpgradePacketIndex();
    }else{
		xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
	}
}

static void ReplyUpgradeCmd( void ){//回复升级命令
    
    const uint8_t len = 0x09;
    
    uint8_t replyupgradecmd[ len ] = { 0x7f, 0xef, 0x00, 0x00, 0x10, 0x1F, 0x00, 0x00, 0x00 };   //successful
    
    DEBUG_INFO("Reply Upgrade Ack\r\n");
    
    MqttPublish( MQTT_SEND_TOPIC, replyupgradecmd, len, MQTT_SEND_CONFIRM );

}

//管理服务器收到应答
static void ReportUpgradeState( uint8_t err ){//报告升级状态，需要管理服务器是否接收到该命令为止，这个命令是什么时候发送？
    
    if( err ){
        extern void LanReboot( void );
        LanReboot();
    }
    const uint8_t len = 0x09;
    
    uint8_t reportupgradestatecmd[ len ] = { 0x7f, 0xef, 0x00, 0x00, 0x11, 0x1E, 0x00, 0x00, 0x00 };   //successful

    reportupgradestatecmd[6] = err;
    
    DEBUG_INFO("report Upgrade Ack\r\n");
    
    MqttPublish( MQTT_SEND_TOPIC, reportupgradestatecmd, len, MQTT_SEND_CONFIRM );
}

static void ReportUpgradeProgress( uint8_t progress ){
    
    const uint8_t len = 0x09;
    
    uint8_t totalpacket = 0;

    if( GetTotalUpgradePacketNumber( &totalpacket ) ){
        
        uint8_t reportupgradestatecmd[ len ] = { 0x7f, 0xef, 0x00, 0x00, 0x13, 0x1C, 0x00, 0x00, 0x00 };   //successful
        
        if( totalpacket == progress ){
            ReportUpgradeState( UPGRADESUCCESSFUL );
            xTimerStop( UPGRADETIMEOUTTimer_Handle, UPGRADETIMERWAIT );
            reportupgradestatecmd[ 6 ] = 0x0a;
            
        }else if ( (( uint8_t )( totalpacket * 0.2 )) == progress ){
            
            reportupgradestatecmd[ 6 ] = 2;
            
        }else if ( (( uint8_t )( totalpacket * 0.4 )) == progress ){
            
            reportupgradestatecmd[ 6 ] = 4;
            
        }else if ( (( uint8_t )( totalpacket * 0.6 )) == progress ){
            
            reportupgradestatecmd[ 6 ] = 6;
            
        }else if ( (( uint8_t )( totalpacket * 0.8 )) == progress ){
            
            reportupgradestatecmd[ 6 ] = 8;
            
        }else{
            
            return;
            
        }

        DEBUG_INFO( "Report Upgrade Progress %d%% \r\n", reportupgradestatecmd[ 6 ] * 10 );
            
        MqttPublish( MQTT_SEND_TOPIC, reportupgradestatecmd, len, MQTT_SEND_UNCONFIRM );
    }        
}

static void UpgradeSucceedHandleEvent( void ){
//	uint8_t cycleCount = 0;
//    uint8_t upgradeFlag[9] = "updata\r\n";                  //Tag upgrade success
//    uint8_t data[32] = { 0 };
    uint32_t upgrade_flag = 0x8888;
    set_key_value( "UPGRADE_SUCCESSED", UINT32, (uint8_t *)&upgrade_flag );
//rewrite:
//    memset( data, 0, 32 );
//    for( uint8_t i = 0; i < 8; i++ ){
//        data[i*4] = upgradeFlag[i];
//    }
//    
//	flash_write( data, UPGRADEFLAG  , 32 );
	
//	if( memcmp( (uint8_t *)( UPGRADEFLAG), data, 32) != 0 ){
//			cycleCount ++;
//			if( cycleCount >= 5 ){
//				DEBUG_INFO( "flash write failed\r\n");
//				return ;
//			}
//			DEBUG_INFO( "Tag upgrade failed\r\n");
//            osDelay(100);
//			goto rewrite;
//	}
	ResetUpgradingState();              //erase original area
    DEBUG_INFO( "Successful upgrading\r\n");
    osDelay(100);
    taskENTER_CRITICAL();
    HAL_NVIC_SystemReset();
    return ;
}

void WaitServerUpgradeStateAck( uint8_t stat ){
    if( stat == 0x00 ){
        if( upgradeflag == true ){
            serverackstate = true;
        }else{
            DEBUG_INFO( "Current server exceptions to send a successful upgrade command\r\n");
        }
    }else if( stat == 0x01 ){
        DEBUG_INFO("server reply upgrade failed\r\n");
    }
    DEBUG_INFO("server reply upgrade stat para :%d\r\n", stat);
}

void CheckServerUpgradeStateAck( void ){
    if( upgradeflag ){
        static uint8_t upgradeflagCount = 0;
        static uint8_t reportCount = 1;
        if( ++ upgradeflagCount > 2 ){
            if( serverackstate || reportCount >= 1 ){
                UpgradeSucceedHandleEvent();
                return ;
            }
            reportCount ++;
            upgradeflagCount = 0;
        }
    }
}


#endif
