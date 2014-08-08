SRC = src/$(TARGET).c \
      src/jobs.c \
      src/config.c \
      src/proc.c \
      deps/json.c
OBJ = $(SRC:.c=.o)

CFLAGS:= -Wall -Wextra -O2 -std=gnu99
INCLUDE:= -Ideps

TARGET=smonitor

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "    Linking $@..."
	@$(CC) $^ -o $@

.c.o:
	@echo "    Compiling $@..."
	@$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

clean:
	@echo "    Cleaning up $(TARGET)..."
	@rm -f ./$(TARGET)
	@rm -f $(OBJ)