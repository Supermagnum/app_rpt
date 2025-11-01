# Makefile for app_rpt with gr-linux-crypto support

# Check for gr-linux-crypto
HAS_GRLINUXCRYPTO := $(shell test -d contrib/gr-linux-crypto && echo yes)

ifeq ($(HAS_GRLINUXCRYPTO),yes)
    # Build gr-linux-crypto first
    GRLINUXCRYPTO_DIR = contrib/gr-linux-crypto
    GRLINUXCRYPTO_BUILD_DIR = $(GRLINUXCRYPTO_DIR)/build
    # For shared library (default build)
    GRLINUXCRYPTO_LIB = $(GRLINUXCRYPTO_BUILD_DIR)/libgnuradio-linux-crypto.so
    # For static library (if built that way)
    GRLINUXCRYPTO_LIB_STATIC = $(GRLINUXCRYPTO_BUILD_DIR)/libgnuradio-linux-crypto.a
    
    CFLAGS += -DHAVE_GRLINUXCRYPTO
    CFLAGS += -I$(GRLINUXCRYPTO_DIR)/include
    CFLAGS += -I$(GRLINUXCRYPTO_DIR)/lib
    LDFLAGS += -L$(GRLINUXCRYPTO_BUILD_DIR) -lgnuradio-linux-crypto -lkeyutils -lcrypto
    
    # Build gr-linux-crypto as dependency
    $(GRLINUXCRYPTO_LIB):
	@echo "Building gr-linux-crypto..."
	@mkdir -p $(GRLINUXCRYPTO_BUILD_DIR)
	@cd $(GRLINUXCRYPTO_BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc) || true
	@if [ ! -f $(GRLINUXCRYPTO_LIB) ] && [ -f $(GRLINUXCRYPTO_BUILD_DIR)/libgnuradio-linux-crypto.so.* ]; then \
		echo "Shared library found with version suffix"; \
	fi
else
    $(info Building WITHOUT authentication support)
endif

.PHONY: build-gr-linux-crypto
build-gr-linux-crypto:
ifneq ($(HAS_GRLINUXCRYPTO),yes)
	$(error gr-linux-crypto submodule not found. Run: git submodule update --init)
endif
	@mkdir -p $(GRLINUXCRYPTO_BUILD_DIR)
	@cd $(GRLINUXCRYPTO_BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc)

