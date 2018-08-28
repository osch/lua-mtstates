#####################################################################################
#
# Use this Makefile for building under linux.
#
# This Makefile will configure everything auomatically and uses 
# cmake for compiling and linking.
#
#####################################################################################
#
# Variables
#
# These variables can be overriden by make invocation, e.g: make VERBOSE=1 CMAKE_DEFS="..."
# Overriding is also possible in the included file sandbox.mk

PROJ_DIR   = $(shell pwd)
BUILD_DIR  = $(PROJ_DIR)/build

MAKE       = make
MAKE_FLAGS = --no-print-directory

CMAKE       = cmake
CMAKE_FLAGS = 

C_FLAGS             = -Wall -pthread -DMTSTATES_VERSION=scm-`date +"%Y-%m-%d-%H%M%S"`

SHARED_LINKER_FLAGS = -pthread

VERBOSE = 

PKG_CONFIG           = pkg-config

# set LUA_PKG_CONFIG_NAME to the name of the used lua package
# suitable for the pk-config command, e.g. lua51, lua52, lua53 .
LUA_PKG_CONFIG_NAME  = lua

# define CHECK_PKG_CONFIG = true in sandbox.mk if package-config is not available
define CHECK_PKG_CONFIG
    $(PKG_CONFIG) --print-errors $(LUA_PKG_CONFIG_NAME)
endef

define COMPILE_FLAGS_CMAKE_DEFS
    -D LUAMTSTATES_COMPILE_FLAGS="`$(PKG_CONFIG) --cflags $(LUA_PKG_CONFIG_NAME)`" \
    -D CMAKE_C_FLAGS="$(C_FLAGS)"
endef

define  LINK_FLAGS_CMAKE_DEFS
    -D LUAMTSTATES_LINK_FLAGS="`$(PKG_CONFIG) --libs $(LUA_PKG_CONFIG_NAME)`" \
    -D CMAKE_SHARED_LINKER_FLAGS="$(SHARED_LINKER_FLAGS)"
endef

define ADDITIONAL_CMAKE_DEFS
endef

#####################################################################################

-include $(PROJ_DIR)/sandbox.mk

.PHONY: default init build clean
default: build

init: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile: $(PROJ_DIR)/Makefile \
                       $(PROJ_DIR)/CMakeLists.txt \
                       $(PROJ_DIR)/sandbox.mk
	@mkdir -p $(BUILD_DIR) && \
	cd $(BUILD_DIR) && \
	$(CHECK_PKG_CONFIG) && \
	$(CMAKE) $(CMAKE_FLAGS) $(COMPILE_FLAGS_CMAKE_DEFS) \
	                        $(LINK_FLAGS_CMAKE_DEFS) \
	                        $(ADDITIONAL_CMAKE_DEFS) \
	                        $(PROJ_DIR)

$(PROJ_DIR)/sandbox.mk:
	@ echo "**** initializing sandbox.mk for local build definitions..." && \
	  echo "# Here you can override variables from the Makefile" > "$@"

build: init
	@cd $(BUILD_DIR) && \
	$(MAKE) $(MAKE_FLAGS) VERBOSE=$(VERBOSE) && \
	ln -sf libluamtstates.so mtstates.so

clean:
	rm -rf $(BUILD_DIR)
	find "$(PROJ_DIR)" -name "*.o" -o -name "*.so"  |xargs rm -f

