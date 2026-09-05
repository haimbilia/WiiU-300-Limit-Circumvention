#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wups/share/wups_rules

WUT_ROOT := $(DEVKITPRO)/wut
WUMS_ROOT := $(DEVKITPRO)/wums

ifeq ($(PROBE),load-only)
TARGET   := WiiUMenuTitleLimitProbeLoadOnly
BUILD    := build-load-only
SOURCES  := probes/load-only
DATA     := data
INCLUDES := probes/load-only
else
TARGET   := WiiUMenuTitleLimit
BUILD    := build
SOURCES  := src src/utils src/patches
DATA     := data
INCLUDES := src
endif

CFLAGS   := -g -Wall -Wextra -Werror -O2 -ffunction-sections $(MACHDEP)
CFLAGS   += $(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__
CXXFLAGS := $(CFLAGS) -std=c++20
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map) $(WUPSSPECS)

ifeq ($(DEBUG),1)
CXXFLAGS += -DDEBUG
CFLAGS += -DDEBUG
endif

ifeq ($(PATCH),1)
CXXFLAGS += -DTITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH=1
CFLAGS += -DTITLE_LIMIT_ENABLE_BEHAVIORAL_PATCH=1
endif

ifeq ($(PROBE),load-only)
LIBS    := -lwups -lwut
else
LIBS    := -lfunctionpatcher -lwups -lwut
endif
LIBDIRS := $(PORTLIBS) $(WUPS_ROOT) $(WUMS_ROOT) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifneq ($(strip $(CPPFILES)),)
export LD := $(CXX)
else
export LD := $(CC)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE    := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                     -I$(CURDIR)/$(BUILD)
export LIBPATHS   := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf $(TARGET).map

else

.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).wps

$(OUTPUT).wps: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
