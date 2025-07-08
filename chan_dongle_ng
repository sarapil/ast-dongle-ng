/*
 * chan_dongle_ng.c
 * Next-Generation Asterisk Channel Driver for USB GSM/3G/4G/5G Modems
 * Includes: Outgoing/Incoming call handling, missed call notification, auto call recording.
 */

#include "chan_dongle_ng.h"
#include <asterisk/channel.h>
#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/frame.h>
#include <asterisk/pbx.h>
#include <asterisk/app.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>

// ============================
// Voice Call State Management
// ============================

typedef struct dongle_voice_state {
    struct ast_channel *chan;
    int call_active;
    char peer_number[32];
    int is_incoming; // 1: incoming, 0: outgoing
    int missed_flag; // 1: missed and not notified
    time_t ring_time;
} dongle_voice_state;

static dongle_voice_state voice_states[MAX_DONGLES];

// ============================
// Outgoing Call Logic
// ============================

static int dongle_start_outgoing_call(dongle_device *dev, const char *number) {
    if (!dev || voice_states[dev - dongles].call_active || !dev->fd_cmd) return -1;
    char atcmd[64], resp[256];
    snprintf(atcmd, sizeof(atcmd), "ATD%s;", number);
    if (dongle_send_at(dev, atcmd, resp, sizeof(resp), 5) == 0 && strstr(resp, "OK")) {
        voice_states[dev - dongles].call_active = 1;
        voice_states[dev - dongles].is_incoming = 0;
        strncpy(voice_states[dev - dongles].peer_number, number, sizeof(voice_states[dev - dongles].peer_number));
        dongle_log("Start outgoing call: %s via %s", number, dev->name);
        return 0;
    }
    dongle_log("Failed to start outgoing call: %s via %s", number, dev->name);
    return -1;
}

// ============================
// Hangup Logic
// ============================

static int dongle_hangup_call(dongle_device *dev) {
    if (!dev || !voice_states[dev - dongles].call_active || !dev->fd_cmd) return -1;
    dongle_send_at(dev, "ATH", NULL, 0, 2); // Ignore reply
    voice_states[dev - dongles].call_active = 0;
    voice_states[dev - dongles].peer_number[0] = 0;
    voice_states[dev - dongles].is_incoming = 0;
    voice_states[dev - dongles].missed_flag = 0;
    dongle_log("Hung up call on %s", dev->name);
    return 0;
}

// ============================
// Outgoing Channel Request
// ============================

static struct ast_channel *dongle_request(const char *type, struct ast_format_cap *cap,
    const struct ast_channel *requestor, const char *dest, int *cause)
{
    // dest = dongle_name/number
    char dongle_name[64], number[64];
    if (sscanf(dest, "%63[^/]/%63s", dongle_name, number) != 2)
        return NULL;

    dongle_device *dev = dongle_find_by_name(dongle_name);
    if (!dev || voice_states[dev - dongles].call_active)
        return NULL;

    struct ast_channel *chan = ast_channel_alloc(1, AST_STATE_DOWN, "", "", number, "", "", 0, "Dongle/%s", dongle_name);
    if (!chan)
        return NULL;

    ast_format_cap_add(chan->nativeformats, ast_format_slin);
    voice_states[dev - dongles].chan = chan;
    if (dongle_start_outgoing_call(dev, number) != 0) {
        ast_channel_unref(chan);
        voice_states[dev - dongles].chan = NULL;
        return NULL;
    }

    // Open audio port if needed
    if (dev->fd_audio < 0 && strlen(dev->tty_audio) > 0)
        dev->fd_audio = open(dev->tty_audio, O_RDWR | O_NOCTTY);

    ast_channel_set_fd(chan, 0, dev->fd_audio);
    return chan;
}

// ============================
// Hangup Channel
// ============================

static int dongle_hangup(struct ast_channel *chan) {
    for (int i = 0; i < dongle_count; ++i) {
        if (voice_states[i].chan == chan) {
            dongle_hangup_call(&dongles[i]);
            voice_states[i].chan = NULL;
            break;
        }
    }
    return 0;
}

// ============================
// Audio Read/Write
// ============================

static int dongle_read(struct ast_channel *chan, void *data, int len) {
    for (int i = 0; i < dongle_count; ++i) {
        if (voice_states[i].chan == chan && voice_states[i].call_active) {
            dongle_device *dev = &dongles[i];
            if (dev->fd_audio >= 0)
                return read(dev->fd_audio, data, len);
        }
    }
    return -1;
}

static int dongle_write(struct ast_channel *chan, const void *data, int len) {
    for (int i = 0; i < dongle_count; ++i) {
        if (voice_states[i].chan == chan && voice_states[i].call_active) {
            dongle_device *dev = &dongles[i];
            if (dev->fd_audio >= 0)
                return write(dev->fd_audio, data, len);
        }
    }
    return -1;
}

// ============================
// Answer Incoming Call
// ============================

static int dongle_answer(struct ast_channel *chan) {
    for (int i = 0; i < dongle_count; ++i) {
        if (voice_states[i].chan == chan && voice_states[i].call_active && voice_states[i].is_incoming) {
            dongle_device *dev = &dongles[i];
            dongle_send_at(dev, "ATA", NULL, 0, 3);
            dongle_log("Answered incoming call on %s", dev->name);
            // Start call recording on answer
            char recfile[256];
            time_t now = time(NULL);
            snprintf(recfile, sizeof(recfile), "/var/spool/asterisk/recordings/%s_%ld.wav", dev->name, now);
            ast_log(LOG_NOTICE, "Auto-recording call on %s to %s\n", dev->name, recfile);
            // Use Asterisk built-in: record to recfile; or use ast_channel_move for external apps
            ast_app_run(chan, "MixMonitor", recfile); // Needs MixMonitor app loaded
            return 0;
        }
    }
    return -1;
}

// ============================
// Detect Incoming Call (called from dongle_reader_thread on RING/CLIP)
// ============================

void dongle_detect_incoming_call(dongle_device *dev, const char *from_number) {
    int idx = dev - dongles;
    if (voice_states[idx].call_active) return; // Already a call
    voice_states[idx].call_active = 1;
    voice_states[idx].is_incoming = 1;
    voice_states[idx].missed_flag = 1;
    voice_states[idx].ring_time = time(NULL);
    strncpy(voice_states[idx].peer_number, from_number, sizeof(voice_states[idx].peer_number));
    // Create channel and push to dialplan
    struct ast_channel *chan = ast_channel_alloc(1, AST_STATE_RING, "", "", dev->sim_number, "", "", 0, "Dongle/%s", dev->name);
    voice_states[idx].chan = chan;
    if (from_number && strlen(from_number) > 0) {
        struct ast_party_caller caller;
        memset(&caller, 0, sizeof(caller));
        ast_party_caller_set_caller(&caller, from_number, NULL, NULL, NULL);
        ast_channel_set_caller(chan, &caller, NULL);
    }
    ast_pbx_start(chan);
    dongle_log("Incoming call on %s from %s", dev->name, from_number);
}

// ============================
// Missed Call Notification Logic
// ============================

static void notify_missed_calls() {
    time_t now = time(NULL);
    for (int i = 0; i < dongle_count; ++i) {
        if (voice_states[i].is_incoming && voice_states[i].call_active &&
            voice_states[i].missed_flag && (now - voice_states[i].ring_time > 20)) { // 20s
            // No answer, treat as missed
            dongle_device *dev = &dongles[i];
            dongle_event_log("missed_call", dev->imei, dev->name, 0);
            dongle_log("Missed call on %s from %s", dev->name, voice_states[i].peer_number);
            // Optional: send SMS notification to admin
            // send_missed_call_sms(dev, voice_states[i].peer_number);
            voice_states[i].missed_flag = 0;
            dongle_hangup_call(dev);
            if (voice_states[i].chan) {
                ast_channel_hangup(voice_states[i].chan);
                voice_states[i].chan = NULL;
            }
        }
    }
}

// Call this periodically, e.g. in dongle_reader_thread or a timer thread
static void *missed_call_monitor_thread(void *arg) {
    while (1) {
        notify_missed_calls();
        sleep(5);
    }
    return NULL;
}

// ============================
// Asterisk Channel Tech Registration
// ============================

static struct ast_channel_tech dongle_tech = {
    .type = "Dongle",
    .description = "GSM/3G/4G/5G via USB dongle",
    .requester = dongle_request,
    .hangup = dongle_hangup,
    .read = dongle_read,
    .write = dongle_write,
    .answer = dongle_answer,
};

// ============================
// Module Load/Unload
// ============================

static pthread_t missed_thread;

static int load_module(void) {
    dongle_log("chan_dongle_ng loading...");
    init_db();
    dongle_load_config("/etc/asterisk/dongle_ng.conf");
    discover_modems_and_assign();
    ast_channel_register(&dongle_tech);
    load_cli();
    pthread_create(&missed_thread, NULL, missed_call_monitor_thread, NULL);
    dongle_log("chan_dongle_ng loaded.");
    return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void) {
    dongle_log("chan_dongle_ng unloading...");
    ast_channel_unregister(&dongle_tech);
    unload_cli();
    for (int i=0; i<dongle_count; ++i) {
        dongles[i].enabled = 0;
        pthread_join(dongles[i].reader_thread, NULL);
        if (dongles[i].fd_cmd >= 0) close(dongles[i].fd_cmd);
        if (dongles[i].fd_audio >= 0) close(dongles[i].fd_audio);
    }
    pthread_cancel(missed_thread);
    if (dongle_db) sqlite3_close(dongle_db);
    dongle_log("chan_dongle_ng unloaded.");
    return 0;
}

AST_MODULE_INFO_STANDARD_EXTENDED(
    .name = "chan_dongle_ng",
    .description = "Next-Gen GSM/3G/4G/5G channel driver with Voice, Missed Call, and Recording features",
    .load = load_module,
    .unload = unload_module,
    .support_level = AST_MODULE_SUPPORT_CORE,
);

// ============================
// Helper: Call Recording via MixMonitor (Optional)
// ============================

/* Example of using MixMonitor directly in dialplan:
exten => _X.,1,NoOp(Incoming GSM call)
 same => n,MixMonitor(${CHANNEL(name)}_${STRFTIME(${EPOCH},,%Y%m%d-%H%M%S)}.wav)
 same => n,Dial(SIP/100)

Or in code, see dongle_answer().
*/
