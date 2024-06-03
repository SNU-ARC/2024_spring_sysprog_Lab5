#--------------------------------------------------------------------------------------------------
# Network Lab                             Spring 2024                           System Programming
#
# Makefile
#
# GNU make documentation: https://www.gnu.org/software/make/manual/make.html
#
# Typically, the only thing you need to modify in this Makefile is the list of source files.
#

#--- variable declarations

# directories
SRC_DIR=src
OBJ_DIR=obj
DEP_DIR=.deps

# C compiler and compilation flags
CC=gcc
CFLAGS=-Wall -Wno-stringop-truncation -O2 -pthread
# CFLAGS=-Wall -Wno-stringop-truncation -O2 -g -pthread
DEPFLAGS=-MMD -MP -MT $@ -MF $(DEP_DIR)/$*.d

# make sure SOURCES includes ALL source files required to compile the project
SOURCES=mcdonalds.c burger.c client.c net.c
HDT_SOURCES=burger.c burger.h client.c mcdonalds.c net.c net.h
TARGET=mcdonalds client
COMMON=$(OBJ_DIR)/net.o $(OBJ_DIR)/burger.o

# derived variables
OBJECTS=$(SOURCES:.c=$(OBJ_DIR)/%.o)
DEPS=$(SOURCES:.c=$(DEP_DIR)/%.d)

#--- rules
.PHONY: doc

all: mcdonalds client

mcdonalds: $(OBJ_DIR)/mcdonalds.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

client: $(OBJ_DIR)/client.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEP_DIR) $(OBJ_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ -c $<

$(DEP_DIR):
	@mkdir -p $(DEP_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

-include $(DEPS)

doc: $(wildcard $(SRC_DIR/*))
	doxygen doc/Doxyfile

clean:
	rm -rf $(OBJ_DIR) $(DEP_DIR)

mrproper: clean
	rm -rf $(TARGET) doc/html
