CC ?= cc
CFLAGS ?= -std=c99 -O0 -g -Wall -Wextra -Werror
CPPFLAGS ?=
LDFLAGS ?=
OBJECTS = bus.o controller.o main.o
.PHONY: all run clean
all: controller
controller: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
run: controller
	./controller
clean:
	$(RM) controller controller_demo $(OBJECTS) $(OBJECTS:.o=.d)
-include $(OBJECTS:.o=.d)
