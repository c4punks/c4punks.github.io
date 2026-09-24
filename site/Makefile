# The C 4 Punk Developers organisation site.
#
#   make            build site.wasm
#   make run        serve it on http://localhost:8080 under wasmtime
#   make export     render the static site into the repository root
#
CWIST_ROOT ?= /home/yjlee/cwist
WASI_SDK   ?= $(HOME)/toolchains/wasi-sdk-25.0-x86_64-linux
WASMTIME   ?= $(HOME)/toolchains/wasmtime-v25.0.2-x86_64-linux/wasmtime
ARCHIVE     = $(CWIST_ROOT)/libcwist_wasip2_wasm32-wasip2.a
OUT_DIR    ?= ..

TEMPLATES = $(wildcard templates/*.html) templates/app.css
SOURCES   = main.c content.h projects.h guides.h templates.h

CFLAGS = --target=wasm32-wasip2 -std=c17 -O2 -Wall -Wextra -fvisibility=hidden \
	-D_WASI_EMULATED_GETPID \
	-I. -I$(CWIST_ROOT)/include -I$(CWIST_ROOT)/lib \
	-I$(CWIST_ROOT)/lib/cjson \
	-I$(CWIST_ROOT)/lib/boringssl/include \
	-I$(CWIST_ROOT)/lib/libttak/include \
	-I$(CWIST_ROOT)/lib/sqlite3

LDFLAGS = -lwasi-emulated-pthread -lwasi-emulated-getpid \
	-Wl,--gc-sections -Wl,--allow-undefined \
	-Wl,-z,stack-size=4194304

.PHONY: all run export clean

all: site.wasm

# Templates are embedded so the WASI build needs no filesystem at run time.
templates.h: $(TEMPLATES) tools/embed.sh
	./tools/embed.sh $@ $(TEMPLATES)

site.wasm: main.c content.h projects.h guides.h templates.h Makefile
	$(WASI_SDK)/bin/clang $(CFLAGS) -o $@ main.c $(ARCHIVE) $(LDFLAGS)

run: site.wasm
	$(WASMTIME) run -S preview2=y -S tcp=y -S inherit-network=y site.wasm

export: site.wasm
	$(WASMTIME) run -S preview2=y --dir $(OUT_DIR)::/out site.wasm --export /out

clean:
	rm -f site.wasm templates.h
