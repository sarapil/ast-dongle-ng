#include "chan_dongle_ng.h"

static char *handle_reset(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
    switch (cmd) {
    case CLI_INIT:
        e->command = "dongle_ng reset";
        e->usage = "Usage: dongle_ng reset <device>\n       Reset a dongle device";
        return NULL;
    case CLI_GENERATE:
        return NULL;
    }

    if (a->argc != 3)
        return CLI_SHOWUSAGE;

    ast_log(LOG_NOTICE, DONGLE_NG_LOG_PREFIX "reset device %s requested\n", a->argv[2]);
    /* Here we'd trigger actual reset logic */
    ast_cli(a->fd, "Dongle %s reset\n", a->argv[2]);

    return CLI_SUCCESS;
}

static struct ast_cli_entry cli_cmd[] = {
    AST_CLI_DEFINE(handle_reset, "Reset dongle"),
};

int dongle_ng_register_cli(void)
{
    return ast_cli_register_multiple(cli_cmd, ARRAY_LEN(cli_cmd));
}

void dongle_ng_unregister_cli(void)
{
    ast_cli_unregister_multiple(cli_cmd, ARRAY_LEN(cli_cmd));
}
