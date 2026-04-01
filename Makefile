# Konfiguracja kompilatora i flag
CC = gcc
CFLAGS = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS = -shared
WRAP_FLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
             -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup \
             -Wl,--wrap=strndup
LIBS = -lm
RM = rm -f

# Pliki źródłowe i nagłówkowe
SRC_LIB = ma.c memory_tests.c
OBJ_LIB = $(SRC_LIB:.c=.o)
HEADERS = ma.h memory_tests.h

# Cel domyślny
.PHONY: all
all: libma.so

# Kompilacja biblioteki współdzielonej
libma.so: $(OBJ_LIB)
	$(CC) $(LDFLAGS) $(WRAP_FLAGS) $^ -o $@ $(LIBS)

# Generowanie plików obiektowych
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Cel pomocniczy: kompilacja przykładu
test_program: ma_example.c libma.so
	$(CC) -rdynamic ma_example.c -o test_program -L. -lma -Wl,-rpath=. -I.

# Czyszczenie projektu
.PHONY: clean
clean:
	$(RM) *.o *.so test_program

# Cel testowy (opcjonalny)
.PHONY: test
test: test_program
	valgrind --leak-check=full --error-exitcode=1 ./test_program one
	valgrind --leak-check=full --error-exitcode=1 ./test_program two
	valgrind --leak-check=full --error-exitcode=1 ./test_program memory