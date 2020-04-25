BUNDLE = lv2brain-pan4.lv2
INSTALL_DIR = /home/anachromium/.lv2

$(BUNDLE): manifest.ttl pan4.ttl pan4.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl pan4.ttl pan4.so $(BUNDLE)

pan4.so:
	g++ -shared -fPIC -DPIC pan4.cpp `pkg-config --cflags --libs lvtk-plugin-1` -o pan4.so

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) pan4.so