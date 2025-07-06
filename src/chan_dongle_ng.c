#include "chan_dongle_ng.h"

static int load_module(void);
static int unload_module(void);

static struct ast_channel *dongle_request(const char *type, struct ast_format_cap *cap,
                                          const struct ast_channel *requestor, const char *dest, int *cause);

static struct ast_channel_tech dongle_ng_tech = {
    .type = "Dongle_NG",
    .description = "Minimal dongle_ng channel",
    .requester = dongle_request,
};

static int load_module(void)
{
    int res;
    res = ast_channel_register(&dongle_ng_tech);
    if (res) {
        ast_log(LOG_ERROR, DONGLE_NG_LOG_PREFIX "failed to register channel\n");
        return AST_MODULE_LOAD_DECLINE;
    }
    if (dongle_ng_register_cli()) {
        ast_channel_unregister(&dongle_ng_tech);
        return AST_MODULE_LOAD_DECLINE;
    }
    ast_log(LOG_NOTICE, DONGLE_NG_LOG_PREFIX "module loaded\n");
    return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
    dongle_ng_unregister_cli();
    ast_channel_unregister(&dongle_ng_tech);
    ast_log(LOG_NOTICE, DONGLE_NG_LOG_PREFIX "module unloaded\n");
    return 0;
}

static struct ast_module_info my_mod = {
    .self = __FILE__,
    .description = "dongle_ng channel",
    .load = load_module,
    .unload = unload_module,
};

AST_MODULE_INFO_STANDARD_EXTENDED(my_mod);

static struct ast_channel *dongle_request(const char *type, struct ast_format_cap *cap,
                                          const struct ast_channel *requestor, const char *dest, int *cause)
{
    struct ast_channel *chan;
    chan = ast_channel_alloc(1, AST_STATE_DOWN, 0, 0, "dongle-ng", dest, NULL, NULL, requestor ? requestor->linkedid : NULL, 0, "Dongle/%s", dest);
    if (!chan)
        return NULL;

    ast_format_cap_append(cap, ast_format_ulaw, 0);
    ast_channel_set_writeformat(chan, ast_format_ulaw);
    ast_channel_set_rawwriteformat(chan, ast_format_ulaw);
    ast_channel_set_readformat(chan, ast_format_ulaw);
    ast_channel_set_rawreadformat(chan, ast_format_ulaw);

    return chan;
}
