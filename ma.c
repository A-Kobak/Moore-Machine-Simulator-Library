// autor: Aeksandra Kobak
// Plik zawiera implementację biblioteki, która pozwala tworzyć i zarządzać automatami.


#include "ma.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

// Struktura, za pomocą której jest przechowywany adres podłączonego automatu
// (lub NULL jeżeli nic nie jest podłączone) i podłączony indeks w connections.
typedef struct {
    moore_t *source_automaton;  // Wskaźnik na źródłowy automat
    size_t source_output_index; // Indeks wyjścia w źródłowym automacie
} connection_t;

// Struktura listy, aby przechowywać adresy automatów, do których
// jest podłaczony fragment wyjścia automatu.
typedef struct connections_list {
    moore_t *connected_automaton; // Podłączony automat
    struct connections_list *next;
} connections_list;

// Struktura automatu.
struct moore {
    size_t n;                   // Liczba wejść
    size_t m;                   // Liczba wyjść
    size_t s;                   // Liczba bitów stanu
    transition_function_t t;    // Funkcja przejścia
    output_function_t y;        // Funkcja wyjścia
    uint64_t *current_state;    // Aktualny stan
    uint64_t *next_state;       // Następny stan (obliczany w kroku)
    uint64_t *output;           // Aktualne wyjście
    connection_t *connections;  // Tablica połączeń wejść
    uint64_t *external_input;   // Zewnętrzne wejścia (ustawiane ma_set_input)
    connections_list *list;      // Lista połączeń wyjść
    uint64_t *input_buffer;     // Bufor wejściowy używany w kroku
};

// Funkcja tworząca nowy automat.
moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
    output_function_t y, uint64_t const *q) {
        // Sprawdzenie czy wartości argumentów są poprawne
        if (m == 0 || s == 0 || t == NULL || y == NULL || q == NULL) {
            errno = EINVAL;
            return NULL;
        }

        // Alokacja pamięci dla automatu i obsługa błędów
        moore_t *a = malloc(sizeof(moore_t));
        if (!a) {
            errno = ENOMEM; 
            return NULL;
        }

        // Alokacje pamięci i  obsługa błędów oraz ustawienie watrości
        // dla kolejnych składowych automatu
        a->n = n;
        a->m = m;
        a->s = s;
        a->t = t;
        a->y = y;
        a->list = NULL;

        size_t state_size = (s + 63) / 64;
        a->current_state = calloc(state_size, sizeof(uint64_t));
        if (!a->current_state) {
            free(a);
            errno = ENOMEM;
            return NULL;
        }

        a->next_state = calloc(state_size, sizeof(uint64_t));
        if (!a->next_state) {
            free(a->current_state);
            free(a);
            errno = ENOMEM;
            return NULL;
        }
        memcpy(a->current_state, q, state_size * sizeof(uint64_t));

        size_t output_size = (m + 63) / 64;
        a->output = calloc(output_size, sizeof(uint64_t));
        if (!a->output) {
            free(a->current_state);
            free(a->next_state);
            free(a);
            errno = ENOMEM;
            return NULL;
        }
        y(a->output, a->current_state, m, s);

        if (n > 0) {
            a->connections = calloc(n, sizeof(connection_t));
            if (!a->connections) {
                free(a->current_state);
                free(a->next_state);
                free(a->output);
                free(a);
                errno = ENOMEM;
                return NULL;
            }

            size_t input_size = (n + 63) / 64;
            a->external_input = calloc(input_size, sizeof(uint64_t));
            if (!a->external_input) {
                free(a->connections);
                free(a->current_state);
                free(a->next_state);
                free(a->output);
                free(a);
                errno = ENOMEM;
                return NULL;
            }

            a->input_buffer = calloc(input_size, sizeof(uint64_t));
            if (!a->input_buffer) {
                free(a->connections);
                free(a->external_input);
                free(a->current_state);
                free(a->next_state);
                free(a->output);
                free(a);
                errno = ENOMEM;
                return NULL;
            }
        } else {
            a->external_input = NULL;
            a->connections = NULL;
            a->input_buffer = NULL;
        }

        return a;
}

// Funkcja wyjść będąca identycznością, używana przy tworzeniu automatu
// za pomocą funkcji ma_create_simple, 
void identity(uint64_t *output, const uint64_t *state, size_t m, size_t s) {
    size_t min = (m < s) ? m : s;       // Liczba bitów do skopiowania
    size_t full_words = min / 64;        // Pełne słowa (64 bity)
    for (size_t i = 0; i < full_words; i++) {
        output[i] = state[i];            // Kopiuj pełne słowa
    }
    if (min % 64 != 0) {                // Ostatnie niepełne słowo
        uint64_t mask = (1ULL << (min % 64)) - 1; // Maska dla pozostałych bitów
        output[full_words] = state[full_words] & mask; // Maskuj bity
    }
}

// Funckja tworząca nowy automat, w którym liczba wyjść jest równa liczbie bitów
// stanu, funkcja wyjść jest identycznością, a stan początkowy jest wyzerowany
moore_t *ma_create_simple(size_t n, size_t m, transition_function_t t) {
    // Sprawdzenie czy wartości argumentów są poprawne
    if (m == 0 || !t) {
        errno = EINVAL;
        return NULL;
    }

    // Utworzenie wskaźnika na ciąg bitów reprezentujący początkowy stan automatu
    uint64_t *q = calloc((m + 63)/64, sizeof(uint64_t));
    if (!q) {
        errno = ENOMEM;
        return NULL;
    }

    // Utworzenia automatu poprzez wywołanie funckji ma_create_full
    moore_t *a = ma_create_full(n, m, m, t, identity, q);
    free(q);
    return a;
}

// Funkcja zwraca wskaźnik, pod którym przechowywany jest ciąg bitów
// zawierający wartości sygnałów na wyjściu automatu.
uint64_t const * ma_get_output(moore_t const *a){
    if (!a) {
        errno = EINVAL;
        return NULL;
    }
    return a->output;
}

// Funkcja usuwa wskazany automat i zwalnia całą używaną przez niego pamięć.
// Przed tym rozłącza wszystkie połączenia automatu.
void ma_delete(moore_t *a) {
    if (!a) return;

    // Rozłącz wszystkie WEJŚCIA (gdzie 'a' jest odbiorcą)
    for (size_t i = 0; i < a->n; i++) {
        moore_t *source = a->connections[i].source_automaton;
        if (source) {
            // Usuń 'a' z listy źródłowego automatu
            connections_list **current = &source->list;
            while (*current) {
                if ((*current)->connected_automaton == a) {
                    connections_list *to_remove = *current;
                    *current = to_remove->next;
                    free(to_remove);
                    break;
                }
                current = &(*current)->next;
            }
        }
    }

    // Rozłącz wszystkie WYJŚCIA (gdzie 'a' jest źródłem)
    connections_list *current = a->list;
    while (current) {
        moore_t *target = current->connected_automaton;
        if (target) {
        for (size_t i = 0; i < target->n; i++) {
            if (target->connections[i].source_automaton == a){
                target->connections[i].source_automaton = NULL;
            }
        }
        }
        connections_list *next = current->next;
        free(current);
        current = next;
    }

    // Zwolnij pamięć
    free(a->connections);
    free(a->external_input);
    free(a->current_state);
    free(a->next_state);
    free(a->output);
    free(a->input_buffer);
    free(a);
}

// Funkcja podłącza kolejne num sygnałów wejściowych automatu a_in do sygnałów
// wyjściowych automatu a_out, począwszy od sygnałów o numerach odpowiednio in
// i out, ewentualnie odłączając wejścia od innych wyjść, jeśli były podłączone.
int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num) {
    // Sprawdzenie poprawności argumentów
    if (!a_in || !a_out || num == 0 || in + num > a_in->n || out + num > a_out->m) {
        errno = EINVAL;
        return -1;
    }

    // Rozłącz istniejące połączenia przed dodaniem nowych
    if (ma_disconnect(a_in, in, num) == -1) {
        return -1; // errno ustawione przez ma_disconet w razie błędu
    }

    // Sprawdzenie czy pomiędzy automatami jest już jakieś połączenie,
    // jeżeli jest, to w a_out->list będzie o tym informacja,
    // więc nie będzie trzeba jej dodawać.
    connections_list *current = a_out->list;
    while (current && current->connected_automaton != a_in) {
        current = current->next;
    }
    int isInformation = 0;
    if (current && current->connected_automaton == a_in) isInformation = 1;
    connections_list *new_node = NULL;

    // Jeżeli przeszliśmy całą tablice i nie znaleźliśmy w niej wartości a_in
    if (isInformation == 0) {
        // Zaalokuj pamięć na wpis do listy a_out
        new_node = malloc(sizeof(connections_list));
        if (!new_node) {
            errno = ENOMEM;
            return -1;
        }
        // Dodaj wpis na początek listy a_out
        new_node->connected_automaton = a_in;
        new_node->next = a_out->list;
        a_out->list = new_node;
    }

    for (size_t i = 0; i < num; i++) {
        // Aktualizuj połączenie w tablicy a_in->connection
        a_in->connections[in + i].source_automaton = a_out;
        a_in->connections[in + i].source_output_index = out + i;
    }
    return 0;
}

// Funkcja odłącza kolejne num sygnałów wejściowych
// automatu a_in od sygnałów wyjściowych, począwszy od wejścia o numerze in. 
int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    // Sprawdzenie poprawności argumentów
    if (!a_in || num == 0 || in + num > a_in->n) {
        errno = EINVAL;
        return -1;
    }

    // Dla każdego sygnału wejściowego sprawdzamy czy jest on połączony
    // z jakimś automatem, jeżeli tak to odłączamy go ustawiając
    // wskaźnik w connection na NULL i sprawdzamy czy należy usunąć
    // informację z listy automatu którego wyjście było podłączone.
    for (size_t i = 0; i < num; i++) {
        moore_t *a_out = a_in->connections[in + i].source_automaton;
        if (!a_out) continue;
        a_in->connections[in + i].source_automaton = NULL;
        // Usuń wpis z listy a_out jeżeli rozłączane jest ostatnie istniejące
        // połaczenie między tymi automatami
        int has_other_connections = 0;
        for (size_t j = 0; j < a_in->n; j++) {
            if (a_in->connections[j].source_automaton == a_out){
                has_other_connections = 1;
                break;
            }
        }
        // Usuwanie
        if (has_other_connections == 0) {
            connections_list **current = &a_out->list;
            while (*current) {
                if ((*current)->connected_automaton == a_in) {
                    connections_list *to_remove = *current;
                    *current = to_remove->next; 
                    free(to_remove);
                    break; 
                }
                current = &(*current)->next;
            }
        }
    }   

    return 0;
}

// Funkcja ustawia wartości sygnałów na niepodłączonych wejściach automatu,
// tablica input jest zapisywana w a->external_input.
int ma_set_input(moore_t *a, uint64_t const *input) {
    if (!a || !input || a->n == 0) {
        errno = EINVAL;
        return -1;
    }

    size_t input_size = (a->n + 63) / 64;
    memcpy(a->external_input, input, input_size * sizeof(uint64_t));
    return 0;
}

// Funkcja ustawia stan automatu.
int ma_set_state(moore_t *a, uint64_t const *state) {
    if (!a || !state) {
        errno = EINVAL;
        return -1;
    }

    size_t state_size = (a->s + 63) / 64;
    memcpy(a->current_state, state, state_size * sizeof(uint64_t));
    // Aktualizacja stanu wyjścia
    a->y(a->output, a->current_state, a->m, a->s);
    return 0;
}

// Funkcja pomocnicza używana w ma_step, ustawia konkretny bit
// w tablicy 64-bitowych liczb 
static void set_bit(uint64_t *array, size_t index, int value) {
    size_t word = index / 64;
    size_t bit = index % 64;
    if (value) {
        array[word] |= (1ULL << bit);
    } else {
        array[word] &= ~(1ULL << bit);
    }
}

// Funkcja pomocnicza używana w ma_step,
// odczytuje wartość bitu z tablicy
static int get_bit(const uint64_t *array, size_t index) {
    size_t word = index / 64;
    size_t bit = index % 64;
    return (array[word] >> bit) & 1;
}

//Funkcja wykonuje jeden krok obliczeń podanych automatów.
int ma_step(moore_t *at[], size_t num) {
    // Sprawdzenie poprawności argumentów
    if (!at || num == 0) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++) {
        if (!at[i]) {
            errno = EINVAL;
            return -1;
        }
    }

    for (size_t i = 0; i < num; i++) {
        moore_t *a = at[i];
        if (a->n > 0) {
            uint64_t *input = a->input_buffer;
            // Budowanie wejścia z połączeń i zewnętrznych wejść
            for (size_t j = 0; j < a->n; j++) {
                const connection_t *conn = &a->connections[j];
                int bit;
                if (conn->source_automaton) {
                    bit = get_bit(conn->source_automaton->output, conn->source_output_index);
                } 
                else {
                    bit = get_bit(a->external_input, j);
                }
                set_bit(input, j, bit);
            }

            // Obliczanie następnego stanu
            a->t(a->next_state, input, a->current_state, a->n, a->s);
        } else {
            a->t(a->next_state, NULL, a->current_state, 0, a->s);
        }
    }

    // Zaktualizuj stany i wyjścia wszystkich automatów
    for (size_t i = 0; i < num; i++) {
        moore_t *a = at[i];
        uint64_t *temp;
        // Zamień wyjście
        a->y(a->output, a->next_state, a->m, a->s);
        // Zamień stan
        temp = a->current_state;
        a->current_state = a->next_state;
        a->next_state = temp;
        
    }
    return 0;
}