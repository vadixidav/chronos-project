CC = gcc
CFLAGS = -D_GNU_SOURCE -Wall -O0
LIBS = -lrt -lchronos -lm -lpthread
LDFLAGS = -L/usr/lib

INSTALL_DIR = /usr/local/chronos
BIN_DIR = /usr/local/bin

# The directories to search in for source files for the main executable
SRCDIRS = src src/workloads
SLOPE_SRCDIRS = src/workloads src/slope
# The name to give the compiled programs
BINARY = sched_test_app
SLOPE_BINARY = find_slopes
LIBCHRONOS = /usr/lib/libchronos.so
# Extensions to use to find files to compile
SRCEXTS = .c
HDREXTS = .h
# Expand the above into full lists of source, header, and object files
HEADERS = $(foreach d,$(SRCDIRS) $(SLOPE_SRCDIRS),$(wildcard $(addprefix $(d)/*,$(HDREXTS))))
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
SLOPE_SOURCES = $(foreach d,$(SLOPE_SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
OBJS = $(addsuffix .o, $(basename $(SOURCES)))
# Find all the object files for the slope executable by the source directories, plus the {slope|workload}.o files
SLOPE_OBJS = $(addsuffix .o, $(basename $(SLOPE_SOURCES))) src/workload.o src/slope.o
# List all the backup files that are been indented
INDENT_BACKUPS = $(foreach f,$(HEADERS) $(SOURCES) $(SLOPE_SOURCES), $(wildcard $(addsuffix ~,$(f))))
# List all the dependency files
DEPENDENCY_FILES = $(foreach file,$(OBJS) $(SLOPE_OBJS),$(dir $(file)).$(notdir $(basename $(file))).d)

# Define compilation and linker commands and arguments
COMPILE = $(CC) $(CFLAGS) -c
LINK = $(CC) $(CFLAGS) $(LDFLAGS)

# List all 'phony' targets
.PHONY: all clean install indent

# Delete the default suffixes
.SUFFIXES:

# Make the executable and slope
all: $(BINARY) $(SLOPE_BINARY)

# General compilation target for all object files
%.o:%.c
	@echo '  CC     ' $@
	@$(COMPILE) -MD -o $@ $<
	@cp $*.d $(*D)/.$(*F).d; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $(*D)/.$(*F).d; \
            rm -f $*.d

-include $(DEPENDENCY_FILES)

# Generate the main executable
$(BINARY):$(OBJS)
	@if [ ! -f "$(LIBCHRONOS)" ]; then \
		echo "Error: sched_test_app depends on libchronos. Please install libchronos first and then run this command again."; \
		exit 1; \
	fi

	@echo '  LD     ' $@
	@$(LINK) $(OBJS) $(LIBS) -o $@

# Generate the slope executable
$(SLOPE_BINARY):$(SLOPE_OBJS)
	@echo '  LD     ' $@
	@$(LINK) $(SLOPE_OBJS) $(LIBS) -o $@

# Fix the indentation of all source files (requires external indent utility)
indent:
	@echo '  INDENT  ALL the things'
	@indent -linux $(SOURCES) $(SLOPE_SOURCES) $(HEADERS)

# Clean all object, dependency, and binary files
%.o-rm:
	@echo '  CLEAN   $*.o'
	@rm -f $*.o
	@rm -f $(*D)/.$(*F).d
clean: $(SLOPE_OBJS:%=%-rm) $(OBJS:%=%-rm)
	@echo '  CLEAN  ' $(BINARY)
	@rm -f $(BINARY)
	@echo '  CLEAN  ' $(SLOPE_BINARY)
	@rm -f $(SLOPE_BINARY)
	@echo '  CLEAN   indent backups'
	@rm -rf $(INDENT_BACKUPS)

# Install the executable in the right place, link to it, and calculate slopes
install: all
	@echo '  INSTALL' $(BINARY)
	@cp $(BINARY) $(INSTALL_DIR)/$(BINARY)

	@echo '  LINK   ' $(BIN_DIR)/$(BINARY) '->' $(INSTALL_DIR)/$(BINARY);
	@rm -f $(BIN_DIR)/$(BINARY);
	@ln -s $(INSTALL_DIR)/$(BINARY) $(BIN_DIR)/$(BINARY);
