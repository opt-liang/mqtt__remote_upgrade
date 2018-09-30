#ifndef __COMMAND__PROCESSING__
#define __COMMAND__PROCESSING__
#include <stdint.h>
#include <stdbool.h>

enum ServerDealCommand{
    /* upgrade_operation */
	UgradeInform                           = 0x1F10,
	UpgradeStateFeedbackAck                = 0x1E11, 
	UpgradePackage                         = 0x1D12,
};

bool commandProcessing( uint8_t* cmd, uint16_t cmdlength );

#endif






