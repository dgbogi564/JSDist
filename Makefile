CC := gcc
CFLAGS := -fsanitize=thread -Wall -Werror -MP -MD -std=c11
CPPFLAGS := -ggdb
LIBS := -pthread -lm

SOURCEDIR := src/code
BUILDDIR := build

EXECUTABLE := compare
SOURCES:= $(wildcard $(SOURCEDIR)/*.c)
OBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
-include $(SOURCES:.c=.d)

all: dir $(BUILDDIR)/$(EXECUTABLE)

dir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(OBJECTS): $(BUILDDIR)/%.o : $(SOURCEDIR)/%.c
	$(CC) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/$(EXECUTABLE)
