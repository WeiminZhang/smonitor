CFLAGS:= -Wall -Wextra -O2 -std=gnu99

ifeq ($(DESTINATION), ATOM)
	INCLUDE:= -I. -I$(PROJECT_TOP)/src/include
else
	INCLUDE:= -I. -I$(PROJECT_TOP)/src/include -I/usr/include/x86_64-linux-gnu
endif

SPLINT_CONFIG:= -f $(PROJECT_SETTINGS)/misra.splint -f ./exceptions.splint $(INCLUDE) -I/usr/include/x86_64-linux-gnu

TARGET:=smonitor

OBJS = \
	jobs.o \
	json.o \
	$(TARGET).o


.PHONY:atom
atom:
	@echo "  Building $(TARGET)..."
	@make -s clean
	@make -s $(TARGET) DESTINATION=ATOM

.PHONY:release
release:
	@echo "  Building $(TARGET)..."
	@make -s clean
	@make -s $(TARGET) DESTINATION=DIALOG

.c.o:
	@echo "    Compiling $@..."
	@$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(TARGET): $(OBJS)
	@echo "    Linking $@..."
	$(CC) $^ -o $@

.PHONY: analyze
analyze:
	@echo "  Splinting $(TARGET)..."
	@splint $(SPLINT_CONFIG) $< $(PRODUCT).c

.PHONY: debug
debug:
	@make -s clean
	@make -s get_shared_sources
	@make -s analyze
	@make -s $(TARGET)

clean:
	@echo "    Cleaning up $(TARGET)..."
	@rm -f ./$(TARGET)
	@rm -f ./*.o
