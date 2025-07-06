#ifndef _CHAN_DONGLE_NG_H_
#define _CHAN_DONGLE_NG_H_

#include <asterisk.h>
#include <asterisk/module.h>
#include <asterisk/channel.h>
#include <asterisk/logger.h>
#include <asterisk/cli.h>
#include <asterisk/config.h>
#include <asterisk/lock.h>
#include <asterisk/time.h>

#define DONGLE_NG_LOG_PREFIX "[dongle_ng] "

struct dongle_ng_device {
    char imei[32];
    char name[64];
    char tty[256];
    AST_RWLOCK_DEFINE(lock);
};

int dongle_ng_register_cli(void);
void dongle_ng_unregister_cli(void);

#endif /* _CHAN_DONGLE_NG_H_ */
