CC = gcc
CFLAGS = -Wall -Wextra -fPIC -O3 -MMD -MP
LIB_NAME = libozone.a

PKGS = vulkan
DEPS := external/dryad external/kipcorn
DRYAD_BACKEND = ALSA
INC_FLAGS := -Iinclude
DEP_LIBS = 

#ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer -g

CFLAGS += ${ASAN_FLAGS}

all: $(LIB_NAME)

.PHONY: all clean sync-submodules print-pkgs

external/dryad/libdryad.a: $(shell find external/dryad -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" \))
	$(MAKE) -C external/dryad BACKEND=$(DRYAD_BACKEND)

external/kipcorn/libkipcorn.a: $(shell find external/kipcorn -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" \))
	$(MAKE) -C external/kipcorn

SPECIFIED_GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)

ifneq ($(filter all $(LIB_NAME) test,$(SPECIFIED_GOALS)),)
    $(foreach d,$(DEPS), \
        $(eval DEP_LIBS += $(d)/lib$(notdir $(d)).a) \
        $(eval PKGS += $(shell $(MAKE) -C $(d) -s print-pkgs)) \
        $(eval RAW_INCS := $(shell $(MAKE) -C $(d) -s print-incs)) \
        $(foreach i,$(RAW_INCS),$(eval INC_FLAGS += -I$(d)/$(i))) \
    )
endif

CFLAGS += $(INC_FLAGS)

ifneq ($(PKGS),)
    UNIQUE_PKGS := $(sort $(PKGS))
    CFLAGS += $(shell pkg-config --cflags $(UNIQUE_PKGS))
endif

SRCS = $(shell find modules -name "*.c")
OBJS = $(SRCS:%.c=build/%.o)

$(LIB_NAME): $(OBJS) $(DEP_LIBS)
	@echo "Archiving $(LIB_NAME)..."
	@ar rcs $@ $(OBJS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB_NAME)
	$(CC) test.c $(CFLAGS) -L. -Lexternal/dryad -Lexternal/kipcorn -lozone -ldryad -lkipcorn $(shell pkg-config --libs $(sort $(PKGS))) -o ozone_test

clean:
	rm -rf build $(LIB_NAME)
	@$(foreach d,$(DEPS), $(MAKE) -C $(d) clean;)

sync-submodules:
	git submodule update --init --recursive --remote

print-pkgs:
	@echo $(sort $(PKGS))

-include $(OBJS:.o=.d)