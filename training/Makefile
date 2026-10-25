CC     = gcc
CFLAGS = -Wall -Wextra -O2
SRC    = main.c dnn_train.c train_data.c value.c

nn: $(SRC) value.h dnn_train.h train_data.h
	$(CC) $(CFLAGS) -o nn $(SRC) -lm

clean:
	rm -f nn
