CC = gcc

CFLAGS = -Wall -Wextra -O3 -DOZONE_ENABLE_VK_VALIDATION=1
NAME = libozone.a

INC_FLAGS = -Imodules

DRYAD_BACKEND ?= ALSA
KIPCORN_OPENGL ?= 0

PKGS = vulkan

SRCS := $(shell find modules -name '*.c')
OBJS := $(SRCS:%.c=build/%.o)

ifneq ($(PKGS),)
INC_FLAGS += $(shell pkg-config --cflags $(PKGS))
endif

INC_FLAGS += $(shell $(MAKE) -s -C ../dryad print-incs)
INC_FLAGS += $(shell $(MAKE) -s -C ../kipcorn print-incs)

all: $(NAME)

.PHONY: all clean deps sync-submodules print-libs print-incs

../dryad/libdryad.a:
	$(MAKE) -C ../dryad BACKEND=$(DRYAD_BACKEND)

../kipcorn/libkipcorn.a:
	$(MAKE) -C ../kipcorn KIPCORN_OPENGL=$(KIPCORN_OPENGL)

deps: ../dryad/libdryad.a ../kipcorn/libkipcorn.a

$(NAME): deps $(OBJS)
	ar rcs $@ $(OBJS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(NAME)
	$(MAKE) -C ../dryad clean
	$(MAKE) -C ../kipcorn clean

sync-submodules:
	git submodule update --init --recursive --remote

print-libs:
	@echo $(abspath libozone.a) \
	     $(abspath ../dryad/libdryad.a) \
	     $(abspath ../kipcorn/libkipcorn.a) \
	     $(shell pkg-config --libs $(PKGS))
	@$(MAKE) -s -C ../dryad print-libs
	@$(MAKE) -s -C ../kipcorn print-libs

print-incs:
	@for dir in $(INC_FLAGS); do \
	    case $$dir in \
	        -I*) echo -n "-I$(abspath $${dir#-I}) " ;; \
	        *) echo -n "$$dir " ;; \
	    esac; \
	done
	@echo