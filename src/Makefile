.PHONY: default mtstates
default: mtstates

BUILD_DATE  := $(shell date "+%Y-%m-%dT%H:%M:%S")

LNX_GCC_RUN := gcc -shared -fPIC -O2  -Werror=return-type
WIN_GCC_RUN := gcc -shared -fPIC -O2
MAC_GCC_RUN := MACOSX_DEPLOYMENT_TARGET=10.8 gcc -O2 -bundle -undefined dynamic_lookup -all_load

LNX_COPTS   := -g
WIN_COPTS   := -I/mingw64/include/lua5.1 
MAC_COPTS   := -I/usr/local/opt/lua/include/lua5.3 

LNX_LOPTS   := -lpthread
WIN_LOPTS   := -lkernel32
MAC_LOPTS   := -lpthread

LNX_SO_EXT  := so
WIN_SO_EXT  := dll
MAC_SO_EXT  := so

GCC_RUN     :=
SO_EXT      :=
COPTS       :=
LOPTS       :=

# platforms: LNX, WIN, MAC
# (may be set in sandbox.mk)

PLATFORM    := LNX
LUA_VERSION := 5.4

-include sandbox.mk

GCC_RUN       := $(or $(GCC_RUN),       $($(PLATFORM)_GCC_RUN))
SO_EXT        := $(or $(SO_EXT),        $($(PLATFORM)_SO_EXT))
COPTS         := $(or $(COPTS),         $($(PLATFORM)_COPTS))
LOPTS         := $(or $(LOPTS),         $($(PLATFORM)_LOPTS))

mtstates:
	@mkdir -p build/lua$(LUA_VERSION)/
	$(GCC_RUN) $(COPTS) \
	    -D MTSTATES_VERSION=Makefile"-$(BUILD_DATE)" \
	    main.c         state.c        error.c      util.c   \
	    notify_capi_impl.c receiver_capi_impl.c \
	    async_util.c   mtstates_compat.c  \
	    $(LOPTS) \
	    -o build/lua$(LUA_VERSION)/mtstates.$(SO_EXT)
	    

