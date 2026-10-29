CC ?= cc
CFLAGS ?= -std=c99 -O0 -g -Wall -Wextra -Werror
CPPFLAGS ?=
LDFLAGS ?=

# Independent build directories ensure changing LOG rebuilds the right variant.
LOG ?= flow
ifneq ($(words $(LOG)),1)
$(error LOG must contain exactly one mode: flow, ports, both or none)
endif
ifeq ($(filter $(LOG),flow ports both none),)
$(error Invalid LOG=$(LOG). Use flow, ports, both or none)
endif
FLOW_ENABLED := $(if $(filter flow both,$(LOG)),1,0)
PORTS_ENABLED := $(if $(filter ports both,$(LOG)),1,0)
LOG_FLAGS = -DNPU_LOG_FLOW=$(FLOW_ENABLED) -DNPU_LOG_PORTS=$(PORTS_ENABLED)
BUILD_DIR := build/$(LOG)
TARGET := $(BUILD_DIR)/tiny_npu
SOURCES := logging.c bus.c controller.c sram.c fetcher.c mac.c activation.c scale_clamp.c fifo.c npu.c main.c
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/%.o)
LOG_FILE ?= $(LOG).log

.PHONY: all run log clean help
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(LOG_FLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
run: $(TARGET)
	./$(TARGET)
log: $(TARGET)
	./$(TARGET) > "$(LOG_FILE)"
	@echo "Saved $(LOG_FILE) (LOG=$(LOG))"
clean:
	$(RM) -r build
help:
	@echo 'make run LOG=flow   # pipeline events (default)'
	@echo 'make run LOG=ports  # all block ports every cycle'
	@echo 'make run LOG=both   # both views'
	@echo 'make run LOG=none   # silent on success; errors still go to stderr'
	@echo 'make log LOG=flow   # save flow.log; replace LOG for another view'
-include $(OBJECTS:.o=.d)
