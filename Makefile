# Makefile — Compilador IluluC
#
# Uso:
#   make          — compila o compilador
#   make test     — compila e executa exemplos
#   make clean    — remove artefatos

CC       ?= gcc
INSTALL  ?= install
PREFIX   ?= /usr
BINDIR   ?= $(PREFIX)/bin
DESTDIR  ?=
VERSION  ?= 1.0
PKGREL   ?= 1
CPPFLAGS += -D_DEFAULT_SOURCE -DILULU_VERSION=\"$(VERSION)\" -DILULU_PKGREL=\"$(PKGREL)\"
CFLAGS   ?= -std=c17 -Wall -Wextra -Wpedantic -g
LDFLAGS  ?=

SRCS = \
	error.c \
	token.c \
	lexer.c \
	ast.c \
	parser.c \
	types.c \
	symbol_table.c \
	semantic.c \
	type_checker.c \
	generator.c \
	main.c

OBJS = $(SRCS:.c=.o)

TARGET = ilulu

EXAMPLES = hello functions loops structs arrays pointers
EXAMPLE_BINS = $(addprefix examples/,$(EXAMPLES))

.PHONY: all test clean examples install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Compila um .ilc e executa
test: $(TARGET) examples
	@for ex in $(EXAMPLES); do \
		echo "=== $$ex.ilc ==="; \
		./examples/$$ex; \
	done

examples: $(TARGET)
	@for ex in $(EXAMPLES); do \
		./$(TARGET) comp examples/$$ex.ilc; \
	done

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f $(EXAMPLES)
	rm -f $(EXAMPLE_BINS)
