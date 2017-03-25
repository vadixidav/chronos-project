#BUILD_32_ON_64:=YES#Uncomment this to build 32-bit versions on a 64-bit system

CC=g++
CFLAGS=-Wall
INSTALL_DIR:=/usr/local/chronos
INCLUDE_DIR:=/usr/local/include/chronos
BIN_DIR:=/usr/local/bin
LIB_DIR:=/usr/lib

LIBCHRONOS_BASE:=libchronos.so
LIBCHRONOS_VERSION:=3
LIBCHRONOS:=$(LIBCHRONOS_BASE).$(LIBCHRONOS_VERSION)

CLEAR_SCHEDSTATS:=clear_schedstats

ifdef BUILD_32_ON_64
	INSTALL_DIR:=$(INSTALL_DIR)/32
	LIB_DIR:=$(LIB_DIR)32
	CFLAGS += -m32
endif

LIBOBJS = chronos.o chronos_utils.o chronos_aborts.o
DEPENDENCY_FILES = $(foreach file,$(OBJS), $(dir $(file)).$(notdir $(basename $(file))).d)

# Define compilation and linker commands and arguments
COMPILE = $(CC) $(CFLAGS) -fPIC -c
LINK = $(CC) $(CFLAGS) $(LDFLAGS)

# List all 'phony' targets
.PHONY: all clean install indent

all: $(LIBCHRONOS) $(CLEAR_SCHEDSTATS)

# General compilation target for all object files
%.o:%.c
	@echo '  CC     ' $@
	@$(COMPILE) -MD -o $@ $<
	@cp $*.d $(*D)/.$(*F).d; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $(*D)/.$(*F).d; \
		rm -f $*.d

-include $(DEPENDENCY_FILES)

$(LIBCHRONOS): $(LIBOBJS)
	@echo '  LD     ' $@
	@$(CC) $(CFLAGS) -shared -Wl,-soname,$(LIBCHRONOS) -o $(LIBCHRONOS) $(LIBOBJS)

$(CLEAR_SCHEDSTATS): clear_schedstats.c
	@echo '  LD     ' $(CLEAR_SCHEDSTATS)
	@$(CC) $(CFLAGS) clear_schedstats.c -o $(CLEAR_SCHEDSTATS) $(LIBCHRONOS) -lrt

# Clean all object, dependency, and binary files
%.o-rm:
	@echo '  CLEAN   $*.o'
	@rm -f $*.o
	@rm -f $(*D)/.$(*F).d
clean: $(LIBOBJS:%=%-rm) clear_schedstats.o-rm
	@echo '  CLEAN  ' $(LIBCHRONOS)
	@rm -rf $(LIBCHRONOS)
	@rm -rf $(LIBCHRONOS_BASE)*
	@echo '  CLEAN  ' $(CLEAR_SCHEDSTATS)
	@rm -f $(CLEAR_SCHEDSTATS)

install: preinstall
	@echo '  INSTALL *.h'
	@cp *.h $(INCLUDE_DIR)

	@echo '  INSTALL' $(LIBCHRONOS)
	@cp $(LIBCHRONOS) $(INSTALL_DIR)/$(LIBCHRONOS)

	@echo '  LINK   ' $(LIB_DIR)/$(LIBCHRONOS) '->' $(INSTALL_DIR)/$(LIBCHRONOS)
	@rm -f $(LIB_DIR)/$(LIBCHRONOS)
	@ln -s $(INSTALL_DIR)/$(LIBCHRONOS) $(LIB_DIR)/$(LIBCHRONOS)

	@echo '  LINK   ' $(LIB_DIR)/$(LIBCHRONOS_BASE) '->' $(LIB_DIR)/$(LIBCHRONOS)
	@rm -f $(LIB_DIR)/$(LIBCHRONOS_BASE)
	@ln -s $(LIB_DIR)/$(LIBCHRONOS) $(LIB_DIR)/$(LIBCHRONOS_BASE)

	@echo '  INSTALL' $(CLEAR_SCHEDSTATS)
	@cp $(CLEAR_SCHEDSTATS) $(INSTALL_DIR)/$(CLEAR_SCHEDSTATS)

	@echo '  LINK   ' $(BIN_DIR)/$(CLEAR_SCHEDSTATS) '->' $(INSTALL_DIR)/$(CLEAR_SCHEDSTATS);
	@rm -f $(BIN_DIR)/$(CLEAR_SCHEDSTATS);
	@ln -s $(INSTALL_DIR)/$(CLEAR_SCHEDSTATS) $(BIN_DIR)/$(CLEAR_SCHEDSTATS);

preinstall: all
	@if [ '$(shell /usr/bin/id -u)' -ne 0 ]; then \
		echo "Please run this command as root (or sudo)."; \
		exit 1; \
	fi

	@if ! test -d $(INCLUDE_DIR); then \
		mkdir -p $(INCLUDE_DIR); \
		echo '  MKDIR  ' $(INCLUDE_DIR); \
	fi

	@if ! test -d $(INSTALL_DIR)/slope; then \
		mkdir -p $(INSTALL_DIR)/slope; \
		echo '  MKDIR  ' $(INSTALL_DIR)/slope; \
	fi
