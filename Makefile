CC      := gcc
WINDRES := windres
TARGET  := OricExplorer.exe
SRCDIR  := code

CFLAGS  := -Wall -Wextra -O2 -std=c11 -DUNICODE -D_UNICODE -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -I$(SRCDIR)
LDFLAGS := -s -municode -mwindows
LIBS    := -lcomctl32 -lcomdlg32 -lshell32 -luser32 -lgdi32

SRCS := $(SRCDIR)/main_win32.c \
        $(SRCDIR)/oric_detect.c \
        $(SRCDIR)/oric_tap.c \
        $(SRCDIR)/oric_dsk.c \
        $(SRCDIR)/oric_rom.c \
        $(SRCDIR)/oric_basic.c \
        $(SRCDIR)/oric_disasm.c \
        $(SRCDIR)/oric_screen.c \
        $(SRCDIR)/oric_export.c \
        $(SRCDIR)/oric_i18n.c

OBJS := $(SRCS:.c=.o)
RES  := $(SRCDIR)/app_res.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(RES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/oric_core.h $(SRCDIR)/oric_i18n.h $(SRCDIR)/resource.h
	$(CC) $(CFLAGS) -c $< -o $@

$(RES): $(SRCDIR)/app.rc $(SRCDIR)/resource.h $(SRCDIR)/app.manifest $(SRCDIR)/oric.ico
	$(WINDRES) -c 65001 -I$(SRCDIR) -i $< -o $@

clean:
ifeq ($(findstring cmd,$(SHELL)),cmd)
	-del /q $(subst /,\,$(OBJS) $(RES) $(TARGET)) 2>nul
else
	rm -f $(OBJS) $(RES) $(TARGET)
endif
