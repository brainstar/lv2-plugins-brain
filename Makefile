BUNDLE = lv2brain-pan.lv2
INSTALL_DIR = /home/anachromium/.lv2

CC = clang++
CFLAGS = -fdenormal-fp-math=positive-zero -g -Wall -shared -fPIC -DPIC 

all: $(BUNDLE)

$(BUNDLE): manifest.ttl pan4.ttl pan4.so pan5.ttl pan5.so pan9.ttl pan9.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl pan4.ttl pan4.so pan5.ttl pan5.so pan9.ttl pan9.so $(BUNDLE)

pan4.so:
	$(CC) $(CFLAGS) pan4.cpp `pkg-config --cflags --libs lvtk-2` -o pan4.so

pan5.so:
	$(CC) $(CFLAGS) pan5.cpp `pkg-config --cflags --libs lvtk-2` -o pan5.so

pan9.so:
	$(CC) $(CFLAGS) pan9.cpp `pkg-config --cflags --libs lvtk-2` -o pan9.so

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) pan4.so pan5.so pan9.so