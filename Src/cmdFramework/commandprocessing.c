#include "commandprocessing.h"
#include "common.h"
#include "mqtt_app.h"
#include "encrypt.h"
#include "insideflash.h"
#include "crc.h"
#include "upgrade.h"
/*-------------------public variable----------------------------------*/
const uint8_t SoftwareVersion[3] = { 00, 01, 00};//software version
/*-------------------private function declare--------------------------*/
static bool isLegalCmd( uint8_t *cmd, uint16_t cmdlen );
static void handleCommand( uint8_t* cmd, uint16_t cmdlength );
/*-------------------public function----------------------------------*/

bool commandProcessing( uint8_t* cmd, uint16_t cmdlength ){  
    static uint8_t crc_error_times = 0;
	if ( isLegalCmd( cmd, cmdlength ) ){
        crc_error_times = 0;
		handleCommand( cmd, cmdlength );
		_info_("mqtt-true", cmd, cmdlength );
        return true;
	}else{
        crc_error_times ++;
        if( crc_error_times >= 2 ){
            crc_error_times = 0;
            void NetReSetState( void );
            NetReSetState();
        }
      _info_("mqtt-false", cmd, cmdlength );
        return false;
    }
}

__weak void GetSoftwareVersion( void ){
    DEBUG_INFO("software version is %d.%d\r\n", SoftwareVersion[0], SoftwareVersion[1] );
}

/*-------------------private function----------------------------------*/

static bool checkCRC16( uint8_t *cmd, uint16_t Count ){
	uint16_t crc_16 = crc16( cmd+2, Count - 2);
	if( *((uint16_t *)(cmd+Count)) == crc_16 ){
        return true;
    }
	DEBUG_INFO("Error checking--0x%04x 0x%04x\r\n", *((uint16_t *)(cmd+Count)),crc_16);
	return false;
}

//Only check the frame head and CRC16
static bool isLegalCmd( uint8_t *cmd, uint16_t cmdlen ){
    
    if( (cmd[0] == 0x7f && cmd [1] == 0xef) || (cmd[0] == 0xef && cmd [1] == 0x7f) ){
        uint16_t Count = *((uint16_t*)(cmd+2));
        if( Count != ( cmdlen - 2 ) ){
            DEBUG_INFO("isLegalCmd:cmdlen is error, real Count: %d, cmd Count: %d\r\n", cmdlen-2, Count);
            return false;
        }
        if( cmd[0] == 0x7f ){
            PayloadDecrypt( cmd + 4, Count - 2, NETKEY, cmd + 4 );
        }else{
            PayloadDecrypt( cmd + 4, Count - 2, APPKEY, cmd + 4 );
        }
        static uint8_t crcStatistics = 0;
		if( checkCRC16( cmd, Count ) == false ){
            crcStatistics ++;
            if( crcStatistics >= 2 ){
                crcStatistics = 0;
            }
            return false;
		}
        crcStatistics = 0;
		return true;
    }else{
        DEBUG_INFO("Frame head error %02x %02x\r\n",cmd[0] , cmd[1] );
        return false;
    }
}

static void handleCommand( uint8_t* cmd, uint16_t cmdlength ){
	
	uint16_t CMDOPERATION = (cmd[4] << 8) | cmd[5];
	
	if( (cmd[4] & 0x0f) < 0x0a && (cmd[4] & 0x0f) > 0x0f ){
        DEBUG_INFO("An order that is not in accordance with the rules\r\n");
		return;
    }
        
	switch( CMDOPERATION ){
        
		case UgradeInform:
			ProcessingUpgradeCmd( cmd, cmdlength );
		break;
		
		case UpgradePackage:
			ProcessUpgradePackets( cmd, cmdlength );
		break;
        
        case UpgradeStateFeedbackAck:
            WaitServerUpgradeStateAck( cmd[6] );
            DEBUG_INFO("Receive upgrade report ack\r\n");
        break;
        
		default:
            //Abnormality of report
			DEBUG_INFO("Non server command\r\n");
		break;
	}
}

