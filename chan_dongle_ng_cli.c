/*
 * إضافة أمر CLI لاختبار المودمات وتحديث جدول الخصائص المجربة تلقائياً
 */

#include "chan_dongle_ng.h"
#include <asterisk/cli.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *test_name;
    const char *desc;
    int (*test_fn)(dongle_device *dev, char *result, size_t rsz);
} modem_test_step;

// === دوال اختبارات بسيطة ===
// 1. اختبار AT
static int test_at(dongle_device *dev, char *result, size_t rsz) {
    char resp[256];
    if (dongle_send_at(dev, "AT", resp, sizeof(resp), 2) == 0 && strstr(resp, "OK")) {
        snprintf(result, rsz, "✅");
        return 0;
    }
    snprintf(result, rsz, "❌");
    return -1;
}
// 2. اختبار AT+CGSN (IMEI)
static int test_cgsn(dongle_device *dev, char *result, size_t rsz) {
    char resp[256];
    if (dongle_send_at(dev, "AT+CGSN", resp, sizeof(resp), 2) == 0 && strstr(resp, dev->imei)) {
        snprintf(result, rsz, "✅");
        return 0;
    }
    snprintf(result, rsz, "❌");
    return -1;
}
// 3. اختبار إرسال SMS (بدون إرسال فعلي)
static int test_sms(dongle_device *dev, char *result, size_t rsz) {
    // اختبار AT+CMGF=1 (نمط نصي)
    char resp[256];
    if (dongle_send_at(dev, "AT+CMGF=1", resp, sizeof(resp), 2) == 0 && strstr(resp, "OK")) {
        snprintf(result, rsz, "✅");
        return 0;
    }
    snprintf(result, rsz, "❌");
    return -1;
}
// 4. اختبار USSD
static int test_ussd(dongle_device *dev, char *result, size_t rsz) {
    char resp[256];
    if (dongle_send_at(dev, "AT+CUSD=1,\"*#06#\"", resp, sizeof(resp), 5) == 0 && strstr(resp, dev->imei)) {
        snprintf(result, rsz, "✅");
        return 0;
    }
    snprintf(result, rsz, "❌");
    return -1;
}
// 5. اختبار الصوت: هل يوجد منفذ صوت (ttyUSBx)
static int test_audio_port(dongle_device *dev, char *result, size_t rsz) {
    if (dev->fd_audio >= 0 || (dev->tty_audio && strlen(dev->tty_audio) > 0)) {
        snprintf(result, rsz, "✅");
        return 0;
    }
    snprintf(result, rsz, "❌");
    return -1;
}
// 6. اختبار مكالمة صوتية (يتطلب تدخل المستخدم)
static int test_voice_call(dongle_device *dev, char *result, size_t rsz) {
    // يجب على المستخدم تنفيذ مكالمة فعلية ومراقبة النتيجة
    snprintf(result, rsz, "⬜ (اختبار يدوي)");
    return 1;
}
// 7. اختبار التسجيل الصوتي (يتطلب تدخل المستخدم)
static int test_record(dongle_device *dev, char *result, size_t rsz) {
    snprintf(result, rsz, "⬜ (اختبار يدوي)");
    return 1;
}

// === جدول الخطوات المتسلسلة للاختبار ===
static modem_test_step modem_tests[] = {
    {"AT",          "أمر AT أساسي",             test_at},
    {"CGSN",        "قراءة IMEI",               test_cgsn},
    {"SMS",         "إرسال SMS (نظري)",         test_sms},
    {"USSD",        "USSD (استعلام *#06#)",     test_ussd},
    {"AUDIO",       "منفذ صوت رقمي",            test_audio_port},
    {"VOICECALL",   "مكالمة صوتية فعلية",       test_voice_call},
    {"RECORD",      "اختبار التسجيل الصوتي",     test_record},
};

// === CLI: dongle_ng testmodem <device_name> [step] ===
static char *cli_testmodem_cmd(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {
    if (cmd == CLI_INIT) {
        e->command = "dongle_ng testmodem";
        e->usage =
            "Usage: dongle_ng testmodem <device_name> [step]\n"
            "  اختبر خصائص مودم خطوة بخطوة (AT, IMEI, SMS, USSD, AUDIO, VOICECALL, RECORD)\n"
            "  إذا لم تحدد الخطوة، سينفذ جميع الخطوات بالتسلسل.\n";
        return NULL;
    }
    if (a->argc < 3 || a->argc > 4) return CLI_SHOWUSAGE;
    dongle_device *dev = dongle_find_by_name(a->argv[2]);
    if (!dev) {
        ast_cli(a->fd, "المودم '%s' غير موجود.\n", a->argv[2]);
        return CLI_FAILURE;
    }

    int start = 0, end = sizeof(modem_tests)/sizeof(modem_tests[0]);
    if (a->argc == 4) {
        // نفذ خطوة واحدة فقط
        for (int i = 0; i < end; ++i) {
            if (strcasecmp(a->argv[3], modem_tests[i].test_name) == 0) {
                start = i;
                end = i + 1;
                break;
            }
        }
    }

    ast_cli(a->fd, "اختبار خصائص المودم: %s\n", dev->name);
    for (int i = start; i < end; ++i) {
        char result[64];
        int st = modem_tests[i].test_fn(dev, result, sizeof(result));
        ast_cli(a->fd, " - %-9s | %-30s : %s\n", modem_tests[i].test_name, modem_tests[i].desc, result);

        // تحديث ملف الجدول (modem_voice_features_matrix.md) تلقائياً - (نموذج أولي)
        // يمكنك جعلها تكتب إلى ملف أو API لإرسال نتائج التجربة
        // مثال توضيحي (يفضل لاحقاً دعم إرسال النتائج إلى GitHub تلقائياً عبر API أو Script خارجي)
        FILE *f = fopen("modem_voice_features_matrix.md", "a");
        if (f) {
            fprintf(f, "| %-20s | %-8s | %s |\n", dev->model, modem_tests[i].test_name, result);
            fclose(f);
        }
    }
    ast_cli(a->fd, "انتهت عملية الاختبار.\n");
    return CLI_SUCCESS;
}

// أضف الأمر إلى جدول CLI
// AST_CLI_DEFINE(cli_testmodem_cmd, "اختبار خصائص المودم خطوة بخطوة")
