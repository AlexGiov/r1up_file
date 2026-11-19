# ---------------------------------------------------------------------------------------
#
# \file makefile
#
# \brief Unit test makefile
#
# $Id$
#
# Copyright (c) 2013, ASK Industries GmbH, Niederwinkling.
# All Rights Reserved.
#
# ---------------------------------------------------------------------------------------

# Disable parallel execution to avoid test interference
.NOTPARALLEL:

# ---------------------------------------------------------------------------------------
# Project settings
# ---------------------------------------------------------------------------------------

# Project specific defines
PROJECT_DEFINES=

# Compile for 32 bit or 64 bit
# Attention: If you want to use GDB for debugging, you have to compile for 64 bit
CPU_WORD_SIZE=64

# List of all .c files belonging to the module under test (separated by space), always use forward slashed "/"
SRC_FILES=$(wildcard src/mut/*.c) $(wildcard src/mut/*/*.c) $(wildcard src/mut/*/*/*.c) $(wildcard src/mut/*/*/*/*.c)




# ---------------------------------------------------------------------------------------
# Environment settings
# ---------------------------------------------------------------------------------------

# MinGw will implicitely link against libkernel32.a (Windows lib). 
# The Sleep() function defined in LibOsek.c will collide with the Sleep() function defined in libkernel32.a.
# Since LibOsek is used in many units, we rename the function by preprocessor here.
PROJECT_DEFINES+= -DSleep=OsekSleep 

# Defines for the unit test, because UINT8_C() and UINT16_C() macros are handled by GCC as signed.
PROJECT_DEFINES+= -DUSE_LTYPES_PROJECT_WORKAROUNDS


# Path to DEVBIN
ifndef DEVBIN
	DEVBIN=C:/devbin
endif

# Path to some tools which may be required by the makefile
MINGWROOT=$(DEVBIN)/mingw/mingw64/8.1.0
SHELL=$(SYSTEMROOT)/System32/cmd
PYTHON=$(DEVBIN)/python/v2.7.1/python.exe
GCOV=$(MINGWROOT)/bin/gcov
CP=copy /Y
RM=rd /S /Q
DEL=del /Q
CAT=cat
SED=sed
MKDIR=mkdir
MOVE=mv
TOUCH=touch

# Path to the C compiler
CC=$(MINGWROOT)/bin/gcc

# ---------------------------------------------------------------------------------------
# Global project settings
# ---------------------------------------------------------------------------------------

UNIT_NAME=mut

# Target file for this makefile:
TARGET=$(FILE_BIN)

# Path (relative to the makefile) to store build output files
PATH_BIN=bin

# Path (relative to the makefile) to store temporary build output files
PATH_OBJ=obj

# Path (relative to the makefile) to the source files
PATH_SRC=src

# Path (relative to the makefile) to the build scripts
PATH_SCRIPT=build/scripts

# File with include arguments for the C compiler
FILE_COMP_INC=$(PATH_OBJ)/includes.inc

# File with objects for the linker
FILE_LINK_INC=$(PATH_OBJ)/objects.inc

# Extension of the output file
EXT_BIN=.exe

# Extension to be used for object files
EXT_OBJ=.obj

# Extension to be used for dependency files
EXT_DEP=.d

# Suffix of the output file
BIN_SUFFIX=_test

# Project specific define includes
PROJECT_DEFINE_INCLUDES=

# Processor specific defines
PROCESSOR_DEFINE=generic

# Compiler flags for code coverage
GCOV_FLAGS=-fprofile-arcs -ftest-coverage

# space separated list of directory names that should not be included in the list of
# possible include directories (comment line if not needed)
#EXCLUDED_INCLUDE_DIRS=

# space separated list of directory names that should not be included in the list of
# possible src directories (comment line if not needed)
EXCLUDED_SRC_DIRS=src\lib\cmocka\example src\lib\cmocka\example\chef_wrap src\lib\cmocka\build src\lib\cmocka\coverity
EXCLUDED_SRC_DIRS+=src\lib\cmocka\build\CMakeFiles\2.8.10.2\CompilerIdC src\lib\cmocka\tests



# ---------------------------------------------------------------------------------------
# Preparation phase
# ---------------------------------------------------------------------------------------

ifeq ($(findstring gcov,$(MAKECMDGOALS)),gcov)
	BIN_SUFFIX=_gcov
	PATH_OBJ:=$(PATH_OBJ)/gcov
	CFLAGS+=$(GCOV_FLAGS)
	LDFLAGS+=$(GCOV_FLAGS)
else
	BIN_SUFFIX=_test
	PATH_OBJ:=$(PATH_OBJ)/test
endif

# Name of the output file
FILE_BIN=$(PATH_BIN)/$(UNIT_NAME)$(BIN_SUFFIX)$(EXT_BIN)
FILE_EXE=$(subst /,\,$(FILE_BIN))

# The following lines call a python script to prepare the files with compiler arguments
# for include search paths and object files
ifneq ($(FILE_COMP_INC),)
	FILE_COMP_INC_TMP="$(FILE_COMP_INC).tmp"
	FILE_LINK_INC_TMP="$(FILE_LINK_INC).tmp"

	# Prepare the command and add the fixed arguments
	CMD=$(PYTHON) $(PATH_SCRIPT)/PrepareForMake.py
	CMD+= --srcdir $(PATH_SRC) --objdir $(PATH_OBJ) --inc $(FILE_COMP_INC_TMP)
	CMD+= --srcext .c .cpp --objext $(EXT_OBJ)
	CMD+= --incprefix \055I\" --incsuffix \" --objprefix \" --objsuffix \"
	CMD+= --objfilter $(EXCLUDED_SRC_DIRS)

	# Add arguments only needed for TARGET_BUILD
	ifneq ($(FILE_LINK_INC),)
		CMD+= --obj $(FILE_LINK_INC_TMP) --incfilter $(EXCLUDED_INCLUDE_DIRS)
	endif
	
	# Execute the python script
	OBJS:=$(shell $(CMD))

	# Replace path separator
	FOO:=$(shell @$(SED) 's/\\\/\//g' $(FILE_COMP_INC_TMP) > $(FILE_COMP_INC))
	FOO:=$(shell @$(SED) 's/\\\/\//g' $(FILE_LINK_INC_TMP) > $(FILE_LINK_INC))
endif


# ---------------------------------------------------------------------------------------
# Flags and settings for the C compiler, linker, etc.
# ---------------------------------------------------------------------------------------

# C compiler flags and settings
CFLAGS += -std=gnu99 -Wall $(PROJECT_DEFINES) -D$(PROCESSOR_DEFINE) -D_WIN32
CFLAGS += -g -D__DEBUG__ -DDEBUG
CFLAGS += -m$(CPU_WORD_SIZE)

ifneq ($(findstring coverity,$(MAKECMDGOALS)),coverity)
CFLAGS += -DUNIT_TESTING -DUNIT_TESTING_CMOCKA
endif

# Linker flags and settings
LDFLAGS += -g -D__DEBUG__ -DDEBUG
LDFLAGS += -m$(CPU_WORD_SIZE)

# Macros for converting the object file name in $(1) to the corresponding source file name by
# replacing $(PATH_OBJ) with $(PATH_SRC) and stripping $(EXT_OBJ)
# and vice versa
OBJ_TO_SRC=$(subst $(EXT_OBJ),,$(subst $(PATH_OBJ),$(PATH_SRC),$(1)))
SRC_TO_OBJ=$(subst $(PATH_SRC),$(PATH_OBJ),$(1))$(EXT_OBJ)



# ---------------------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------------------

# Target to show help
.PHONY: help
help:
	@echo ===============================================================================
	@echo HELP
	@echo ===============================================================================
	@echo make build:		Build $(UNIT_NAME)$(BIN_SUFFIX)$(EXT_BIN)
	@echo make test: 		Build $(UNIT_NAME)$(BIN_SUFFIX)$(EXT_BIN) and run unittest
	@echo make gcov: 		Build $(UNIT_NAME)$(BIN_SUFFIX)$(EXT_BIN) and run Unittest with gcov (C1)
	@echo make clean:		Clear all test building data
	@echo make clean_gcov: 	Clear all gcov building data
	@echo make coverity: 	Only compile module under test, do not link, do not run (Needed by coverity)
	@echo ===============================================================================

# Target to build
.PHONY: build
build: $(TARGET)

# Target to clean all objects, binaries and documentation
.PHONY: clean clean_gcov
clean clean_gcov:
	@echo ===============================================================================
	@echo Clean all ($(shell time /T)) ...
	@echo ===============================================================================
	@if exist $(PATH_OBJ) $(RM) $(subst /,\,$(PATH_OBJ)) 1>NUL 2>NUL
	@if exist $(FILE_BIN) $(DEL) $(subst /,\,$(FILE_BIN)) 1>NUL 2>NUL
	@echo ===============================================================================
	@echo Clean all done ($(shell time /T))
	@echo ===============================================================================


# ---------------------------------------------------------------------------------------
# Rules to compile source files
# ---------------------------------------------------------------------------------------

# *.c source files - dependencies are generated and pre-processed
%.c.obj: $(MAKEFILE_LIST) $(PROJECT_DEFINE_INCLUDES) | check-doubles
	@echo compiling $@
	@echo compiling $(call OBJ_TO_SRC,$@)
	@"$(CC)" $(CFLAGS) @./$(FILE_COMP_INC) -c $(call OBJ_TO_SRC,$@) -o $@ -MMD

# *.h header files - only needed in case of missing header file dependencies
%.h:
	@echo missing header file $@

# ---------------------------------------------------------------------------------------
# Rules to link the object files
# ---------------------------------------------------------------------------------------

# link all object files - delete the object files afterwards which have to be always
# rebuilt
$(FILE_BIN): $(OBJS)
	@echo linking $@
	@if not exist $(PATH_BIN) $(MKDIR) $(subst /,\,$(PATH_BIN))
	@"$(CC)" $(LDFLAGS) -o $@ @./$(FILE_LINK_INC)
	@echo ===============================================================================
	@echo Build $(FILE_BIN) done ($(shell time /T))
	@echo ===============================================================================



# ---------------------------------------------------------------------------------------
# Rules to execute tools or to print messages
# ---------------------------------------------------------------------------------------

# Rule to print start message
.PHONY: message
message:
	@echo ===============================================================================
	@echo Build $(FILE_BIN) $(MAKECMDGOALS) ($(shell time /T)) ...
	@echo ===============================================================================

# Rule to check for double header files
.PHONY: check-doubles
check-doubles: | message
	@echo checking for double header files
	@$(PYTHON) $(PATH_SCRIPT)/FindDoubleFiles.py $(PATH_SRC)
	
.PHONY: test
test: $(FILE_BIN)
	@echo ===============================================================================
	@echo Running $(FILE_BIN) ($(shell time /T)) ...
	@echo ===============================================================================
	@$(FILE_EXE)


# ---------------------------------------------------------------------------------------
# Rule to analyse gcov coverage
# ---------------------------------------------------------------------------------------

# Macro for splitting a source file into folder inside obj, and file name followed by a dot
# gcov wants it this way: "src/lib/MUT/MUT.c" -> "obj/gcov/lib/MUT MUT.c." 
SRC_TO_GCOV=$(shell echo $(subst $(PATH_SRC),$(PATH_OBJ),$(1).)|sed "s/\(.*\)\//\1 /")

.PHONY: annotate_gcov
annotate_gcov: run_gcov
	@echo ===============================================================================
	@echo Annotate gcov files ...
	@echo ===============================================================================
	$(PYTHON) build\UnitTestIt\annotate_gcov.py $(wildcard *.gcov)
	$(MOVE) *.gcov reports/


.PHONY: run_gcov
run_gcov: test
	@echo ===============================================================================
	@echo Analyzing code coverage $(FILE_BIN) ($(shell time /T)) ...
	@echo ===============================================================================
	$(foreach f, $(SRC_FILES),$(GCOV) -o $(call SRC_TO_GCOV,$(f)) -b&)
	

.PHONY: gcov
gcov:
	$(MAKE) clean_gcov
	$(MAKE) annotate_gcov

# ---------------------------------------------------------------------------------------
# Rule to compile only the files needed by coverity.
# ---------------------------------------------------------------------------------------
.PHONY: coverity
coverity: $(foreach f, $(SRC_FILES), $(call SRC_TO_OBJ, $(f)))
	@echo ===============================================================================
	@echo Make coverity done
	@echo ===============================================================================
	
	
# ---------------------------------------------------------------------------------------
# Include the dependency files
# ---------------------------------------------------------------------------------------
-include $(OBJS:$(EXT_OBJ)=$(EXT_DEP))
