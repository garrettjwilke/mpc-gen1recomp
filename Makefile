# MPC Gen1Recomp (Pokemon Red, Blue, Yellow) for Akai MPC Sample.
# `make` cross-builds build/mpc-gen1recomp (aarch64-linux-musl static binary),
# then packs build/autorun/ and build/mpc-gen1recomp.zip.
# `make test` builds and runs the host unit test suite.
# `make deploy` uploads the zip to the MPC via SSH / mpc-boot-thingy.

WARN = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
HOSTCC = cc
HOSTCXX = c++
ZIGCC = zig cc -target aarch64-linux-musl
ZIGCXX = zig c++ -target aarch64-linux-musl

UI = mpc-ui-toolkit/src
BUILD = build
APP = $(BUILD)/mpc-gen1recomp

INCLUDES = -I src -I $(UI)

DEFINES = -DENABLE_SOUND=1 -DENABLE_LCD=1 -DMINIGB_APU_AUDIO_FORMAT_S16SYS=1 -DAUDIO_SAMPLE_RATE=44100

HOST_CFLAGS = $(WARN) -O2 -g $(INCLUDES) $(DEFINES)
DEV_CFLAGS  = $(WARN) -O2 $(INCLUDES) $(DEFINES)

SRC_APP = src/input_map.c src/audio_engine.c src/ui_overlay.c src/config.c src/cJSON.c src/rom_manager.c src/g1_runner.c src/minigb_apu.c
UI_COMMON_SRC = $(UI)/ui.c $(UI)/widgets.c $(UI)/filebrowser.c $(UI)/input.c $(UI)/usb_sd.c
UI_DEVICE_SRC = $(UI)/app.c $(UI)/screen.c $(UI)/surface.c $(UI_COMMON_SRC)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  HOST_LDFLAGS = -lm -pthread
else
  HOST_LDFLAGS = -lm -pthread -ldl
endif

PACK = mpc-ui-toolkit/tools/pack-autorun.sh
ZIP = $(BUILD)/mpc-gen1recomp.zip

.PHONY: all test deploy clean

all: $(ZIP)

# -----------------------------------------------------------------------------
# Host Unit & Integration Tests
# -----------------------------------------------------------------------------
$(BUILD)/host/test-runner: $(SRC_APP) $(UI_COMMON_SRC) test/test_runner.c | $(BUILD)/host
	$(HOSTCC) $(HOST_CFLAGS) -o $@ \
	  test/test_runner.c $(SRC_APP) $(UI_COMMON_SRC) \
	  $(HOST_LDFLAGS)

test: $(BUILD)/host/test-runner
	./$(BUILD)/host/test-runner

# -----------------------------------------------------------------------------
# Device Cross-Compilation (aarch64-linux-musl)
# -----------------------------------------------------------------------------
$(APP): src/main.c $(SRC_APP) $(UI_DEVICE_SRC) | $(BUILD)
	mkdir -p $(BUILD)/device/obj
	for s in src/main.c $(SRC_APP) $(UI_DEVICE_SRC); do \
	  obj=$(BUILD)/device/obj/$$(basename $$s .c).o; \
	  $(ZIGCC) $(DEV_CFLAGS) -c -o $$obj $$s; \
	done
	$(ZIGCC) -static -O2 -s -o $@ $(BUILD)/device/obj/*.o -lm -pthread

$(BUILD) $(BUILD)/host:
	mkdir -p $@

$(ZIP): $(APP) $(PACK)
	$(PACK) --name mpc-gen1recomp --bin $(APP) --out $(BUILD)

deploy: $(ZIP)
	../mpc-boot-thingy/setup.sh --ssh --add $(ZIP) --title "mpc-gen1recomp"

clean:
	rm -rf build

