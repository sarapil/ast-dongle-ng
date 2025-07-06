MODULE=chan_dongle_ng.so
SRCS=src/chan_dongle_ng.c src/chan_dongle_ng_cli.c
CFLAGS+=-fPIC -shared -Wall -I/usr/include/asterisk

$(MODULE): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(MODULE)

.PHONY: clean
