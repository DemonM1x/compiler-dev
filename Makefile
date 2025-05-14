CC = gcc
CFLAGS = -Wall -g -Wno-unused-function
LOCAL_INCLUDES = -I.

# Flex and Bison commands for Windows
FLEX = win_flex
BISON = win_bison
FLEX_FLAGS = 
BISON_FLAGS = -d

# Source and object files
SRCS = main.c ast.c risc_generator.c ast_visualizer.c error_handler.c parser/parser.tab.c lexer/lex.yy.c
OBJS = $(SRCS:.c=.o)
TARGET = compiler.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(LOCAL_INCLUDES) -c $< -o $@

# Flex and Bison rules
lexer/lex.yy.c: lexer/lexer.l
	$(FLEX) $(FLEX_FLAGS) -o $@ $<

parser/parser.tab.c parser/parser.tab.h: parser/parser.y
	$(BISON) $(BISON_FLAGS) -o parser/parser.tab.c $<

# Add dependencies to ensure flex and bison files are generated
main.o: parser/parser.tab.h error_handler.h
parser/parser.tab.o: parser/parser.tab.c
lexer/lex.yy.o: lexer/lex.yy.c
risc_generator.o: risc_generator.c risc_generator.h ast.h error_handler.h
ast_visualizer.o: ast_visualizer.c ast_visualizer.h ast.h
error_handler.o: error_handler.c error_handler.h

clean:
	-rm -f $(OBJS) $(TARGET) 