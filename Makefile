CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

SRCS = codegen.c lexer.c parser.c semantic_analyzer.c
OBJS = $(SRCS:.c=.o)
TARGET = codegen

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) temp temp.c temp.exe
