#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>
#include <iomanip>
#include <cmath>

using namespace std;

// Zmienne globalne do zliczania fizycznych operacji dyskowych
int global_disk_reads = 0;
int global_disk_writes = 0;

// Stale konfiguracyjne
const int B_FACTOR = 100;     // Liczba rekordow na jedna strone dyskowa (blok)
const int N_BUFFERS = 10;     // Liczba stron mieszczacych sie jednoczesnie w RAM
const int T = 5;              // Liczba tasm wejsciowych oraz wyjsciowych (lacznie 2*T tasm)

// ---------------------------------------------------------
// 1. STRUKTURA POJEDYNCZEGO REKORDU
// ---------------------------------------------------------
struct Record {
    char pesel[12];

    bool operator<(const Record& other) const {
        int wynik_porownania = strcmp(pesel, other.pesel);
        if (wynik_porownania < 0) {
            return true;
        }
        return false;
    }

    bool operator>=(const Record& other) const {
        int wynik_porownania = strcmp(pesel, other.pesel);
        if (wynik_porownania >= 0) {
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------
// 2. KOPIEC MINIMUM (Do rownoczesnego scalania serii z T tasm)
// ---------------------------------------------------------
struct HeapNode {
    Record rec;
    int tape_index;
};

class MinHeap {
private:
    HeapNode arr[T];
    int current_size;

    void siftDown(int idx) {
        int smallest = idx;
        int left = (2 * idx) + 1;
        int right = (2 * idx) + 2;

        if (left < current_size) {
            if (arr[left].rec < arr[smallest].rec) {
                smallest = left;
            }
        }

        if (right < current_size) {
            if (arr[right].rec < arr[smallest].rec) {
                smallest = right;
            }
        }

        if (smallest != idx) {
            swap(arr[idx], arr[smallest]);
            siftDown(smallest);
        }
    }

    void siftUp(int idx) {
        int parent = (idx - 1) / 2;

        if (idx > 0) {
            if (arr[idx].rec < arr[parent].rec) {
                swap(arr[idx], arr[parent]);
                siftUp(parent);
            }
        }
    }

public:
    MinHeap() {
        current_size = 0;
    }

    void push(Record r, int tape_idx) {
        arr[current_size].rec = r;
        arr[current_size].tape_index = tape_idx;
        current_size = current_size + 1;

        siftUp(current_size - 1);
    }

    HeapNode pop() {
        HeapNode root = arr[0];

        arr[0] = arr[current_size - 1];
        current_size = current_size - 1;

        siftDown(0);
        return root;
    }

    bool isEmpty() {
        if (current_size == 0) {
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------
// 3. WARSTWA DYSKOWA I STRONICOWANIE (Klasa Tape)
// ---------------------------------------------------------
class Tape {
private:
    fstream file;
    string filename;
    Record buffer[B_FACTOR];
    int buffer_pos;
    int buffer_count;
    bool is_write_mode;

public:
    Tape(string name) {
        filename = name;
        buffer_pos = 0;
        buffer_count = 0;
        is_write_mode = false;
    }

    void openWrite() {
        file.open(filename, ios::out | ios::binary | ios::trunc);
        buffer_pos = 0;
        is_write_mode = true;
    }

    void openRead() {
        file.open(filename, ios::in | ios::binary);
        buffer_pos = 0;
        buffer_count = 0;
        is_write_mode = false;
    }

    void close() {
        if (is_write_mode) {
            flush();
        }
        if (file.is_open()) {
            file.close();
        }
    }

    void writeRecord(Record r) {
        buffer[buffer_pos] = r;
        buffer_pos = buffer_pos + 1;

        // Gdy bufor osiagnie rozmiar strony, zrzucamy go na dysk
        if (buffer_pos == B_FACTOR) {
            int bajty_do_zapisu = B_FACTOR * sizeof(Record);
            file.write((char*)buffer, bajty_do_zapisu);

            global_disk_writes = global_disk_writes + 1;
            buffer_pos = 0;
        }
    }

    bool readRecord(Record& r) {
        // Jesli wyczerpalismy biezacy bufor, czytamy kolejna strone z dysku
        if (buffer_pos >= buffer_count) {
            int bajty_do_odczytu = B_FACTOR * sizeof(Record);
            file.read((char*)buffer, bajty_do_odczytu);

            int przeczytane_bajty = file.gcount();
            buffer_count = przeczytane_bajty / sizeof(Record);

            if (buffer_count == 0) {
                return false;
            }

            global_disk_reads = global_disk_reads + 1;
            buffer_pos = 0;
        }

        r = buffer[buffer_pos];
        buffer_pos = buffer_pos + 1;
        return true;
    }

    void flush() {
        if (is_write_mode) {
            if (buffer_pos > 0) {
                int bajty_do_zapisu = buffer_pos * sizeof(Record);
                file.write((char*)buffer, bajty_do_zapisu);

                global_disk_writes = global_disk_writes + 1;
                buffer_pos = 0;
            }
        }
    }
};

// ---------------------------------------------------------
// 4. FUNKCJE POMOCNICZE I GENERATOR PESEL
// ---------------------------------------------------------
Record generateRandomPesel() {
    Record r;

    // 1. Losujemy rok i wybieramy stulecie
    int rok_dwucyfrowy = rand() % 100;
    int czy_xxi_wiek = rand() % 2; // 0 = XX wiek (1900-1999), 1 = XXI wiek (2000-2099)

    int pelny_rok = 1900 + rok_dwucyfrowy;
    if (czy_xxi_wiek == 1) {
        pelny_rok = 2000 + rok_dwucyfrowy;
    }

    // 2. Dokladne sprawdzenie przestepnosci roku kalendarzowego
    bool czy_przestepny = false;
    if (pelny_rok % 4 == 0) {
        if (pelny_rok % 100 != 0) {
            czy_przestepny = true;
        } else {
            if (pelny_rok % 400 == 0) {
                czy_przestepny = true;
            }
        }
    }

    // 3. Miesiac kalendarzowy (1-12)
    int miesiac_kalendarzowy = (rand() % 12) + 1;

    // 4. Dokladna liczba dni w zaleznosci od miesiaca i przestepnosci
    int max_dni = 31;

    if (miesiac_kalendarzowy == 2) {
        if (czy_przestepny) {
            max_dni = 29;
        } else {
            max_dni = 28;
        }
    } else {
        if (miesiac_kalendarzowy == 4 || 
            miesiac_kalendarzowy == 6 || 
            miesiac_kalendarzowy == 9 || 
            miesiac_kalendarzowy == 11) {
            max_dni = 30;
        } else {
            max_dni = 31;
        }
    }

    // 5. Losowanie dnia
    int dzien = (rand() % max_dni) + 1;

    // 6. Kodowanie stulecia w PESEL (dla XXI wieku dodajemy 20)
    int miesiac_pesel = miesiac_kalendarzowy;
    if (czy_xxi_wiek == 1) {
        miesiac_pesel = miesiac_pesel + 20;
    }

    // 7. Czterocyfrowa seria
    int seria = rand() % 10000;

    // 8. Rozbicie wartosci na czyste cyfry
    int cyfry[11];

    cyfry[0] = rok_dwucyfrowy / 10;
    cyfry[1] = rok_dwucyfrowy % 10;

    cyfry[2] = miesiac_pesel / 10;
    cyfry[3] = miesiac_pesel % 10;

    cyfry[4] = dzien / 10;
    cyfry[5] = dzien % 10;

    cyfry[6] = (seria / 1000) % 10;
    cyfry[7] = (seria / 100) % 10;
    cyfry[8] = (seria / 10) % 10;
    cyfry[9] = seria % 10;

    // 9. Obliczenie sumy kontrolnej
    int wagi[10] = {1, 3, 7, 9, 1, 3, 7, 9, 1, 3};
    int suma = 0;

    for (int i = 0; i < 10; i++) {
        int iloczyn = cyfry[i] * wagi[i];
        suma = suma + iloczyn;
    }

    int reszta = suma % 10;
    int cyfra_kontrolna = (10 - reszta) % 10;
    cyfry[10] = cyfra_kontrolna;

    // 10. Przepisanie do rekordu
    for (int i = 0; i < 11; i++) {
        r.pesel[i] = '0' + cyfry[i];
    }
    r.pesel[11] = '\0';

    return r;
}

void cleanupTapes(Tape* tapes[], int count) {
    for (int i = 0; i < count; i++) {
        if (tapes[i] != nullptr) {
            tapes[i]->close();
            delete tapes[i];
            tapes[i] = nullptr;
        }
    }
}

void printFile(string file_name) {
    Tape tape(file_name);
    tape.openRead();

    Record r;
    int counter = 0;

    while (tape.readRecord(r)) {
        cout << r.pesel << " ";
        counter = counter + 1;

        if (counter % 5 == 0) {
            cout << endl;
        }
    }

    cout << endl;
    tape.close();
}

void generateRandomFile(string file_name, int num_records) {
    Tape tape(file_name);
    tape.openWrite();

    for (int i = 0; i < num_records; i++) {
        Record r = generateRandomPesel();
        tape.writeRecord(r);
    }

    tape.close();
}

void loadFromTextFile(string txt_filename, string bin_filename) {
    ifstream txt(txt_filename);
    if (!txt.is_open()) {
        cout << "Blad otwarcia pliku " << txt_filename << endl;
        return;
    }

    Tape tape(bin_filename);
    tape.openWrite();

    Record r;
    while (txt >> r.pesel) {
        tape.writeRecord(r);
    }

    txt.close();
    tape.close();
    cout << "Zaladowano dane z pliku tekstowego do bazy dyskowej." << endl;
}

// ---------------------------------------------------------
// 5. ETAP 1: TWORZENIE POCZATKOWYCH SERII (WIELKIE BUFORY)
// ---------------------------------------------------------
int createInitialRuns(string input_file_name) {
    Tape input_tape(input_file_name);
    input_tape.openRead();

    Tape* out_tapes[T];
    for (int i = 0; i < T; i++) {
        string tape_name = "tape_" + to_string(i) + ".bin";
        out_tapes[i] = new Tape(tape_name);
        out_tapes[i]->openWrite();
    }

    int max_ram = B_FACTOR * N_BUFFERS;
    Record* ram_memory = new Record[max_ram];

    int runs_created = 0;
    int current_tape_idx = 0;
    Record current_record;

    while (true) {
        int current_ram_size = 0;

        // Pobieramy z pliku wejsciowego tyle rekordow, ile miesci RAM
        while (current_ram_size < max_ram) {
            bool czy_odczytano = input_tape.readRecord(current_record);
            if (!czy_odczytano) {
                break;
            }

            ram_memory[current_ram_size] = current_record;
            current_ram_size = current_ram_size + 1;
        }

        // Jesli nic nie wczytano, caly plik zostal podzielony na serie
        if (current_ram_size == 0) {
            break;
        }

        // Sortowanie w pamieci operacyjnej (wewnetrzne)
        sort(ram_memory, ram_memory + current_ram_size);

        // Zapis posortowanej serii na kolejna tasme wyjsciowa
        for (int i = 0; i < current_ram_size; i++) {
            out_tapes[current_tape_idx]->writeRecord(ram_memory[i]);
        }

        runs_created = runs_created + 1;
        current_tape_idx = (current_tape_idx + 1) % T;
    }

    delete[] ram_memory;
    input_tape.close();
    cleanupTapes(out_tapes, T);

    return runs_created;
}

// ---------------------------------------------------------
// 6. ETAP 2: SCALANIE SERII (MERGE RUNS)
// ---------------------------------------------------------
int mergeRuns(bool show_phases, int& final_tape_idx) {
    int current_phase = 0;
    int in_start = 0;
    int out_start = T;

    while (true) {
        Tape* in_tapes[T];
        Tape* out_tapes[T];

        for (int i = 0; i < T; i++) {
            string in_name = "tape_" + to_string(in_start + i) + ".bin";
            in_tapes[i] = new Tape(in_name);
            in_tapes[i]->openRead();

            string out_name = "tape_" + to_string(out_start + i) + ".bin";
            out_tapes[i] = new Tape(out_name);
            out_tapes[i]->openWrite();
        }

        bool tape_active[T];
        Record current_val[T];
        bool has_data = false;

        // Inicjalizacja: wczytujemy pierwszy rekord z kazdej tasmy
        for (int i = 0; i < T; i++) {
            bool read_success = in_tapes[i]->readRecord(current_val[i]);
            if (read_success) {
                tape_active[i] = true;
                has_data = true;
            } else {
                tape_active[i] = false;
            }
        }

        // Jesli zadna tasma nie ma danych, konczymy proces scalania
        if (!has_data) {
            cleanupTapes(in_tapes, T);
            cleanupTapes(out_tapes, T);
            break;
        }

        int runs_in_this_phase = 0;
        int current_out = 0;

        while (has_data) {
            runs_in_this_phase = runs_in_this_phase + 1;
            MinHeap heap;

            // Wkladamy do kopca biezace rekordy ze wszystkich aktywnych tasm
            for (int i = 0; i < T; i++) {
                if (tape_active[i]) {
                    heap.push(current_val[i], i);
                }
            }

            // Scalanie pojedynczej serii za pomoca kopca
            while (!heap.isEmpty()) {
                HeapNode min_node = heap.pop();
                out_tapes[current_out]->writeRecord(min_node.rec);

                int idx = min_node.tape_index;
                Record next_rec;
                bool read_success = in_tapes[idx]->readRecord(next_rec);

                if (read_success) {
                    if (next_rec >= min_node.rec) {
                        // Kolejny element nalezy do tej samej serii (jest wiekszy badz rowny)
                        heap.push(next_rec, idx);
                    } else {
                        // Poczatek nowej serii - zatrzymujemy rekord na kolejna serie
                        current_val[idx] = next_rec;
                        tape_active[idx] = true;
                    }
                } else {
                    // Koniec danych na tej tasmie
                    tape_active[idx] = false;
                }
            }

            // Przelaczamy zapis kolejnej serii na nastepna tasme wyjsciowa
            current_out = (current_out + 1) % T;

            // Sprawdzamy, czy zostaly jeszcze jakies serie na tasmach wejsciowych
            has_data = false;
            for (int i = 0; i < T; i++) {
                if (tape_active[i]) {
                    has_data = true;
                }
            }
        }

        cleanupTapes(in_tapes, T);
        cleanupTapes(out_tapes, T);

        current_phase = current_phase + 1;

        if (show_phases) {
            cout << "--- Wynik po fazie " << current_phase << " ---" << endl;
            for (int i = 0; i < T; i++) {
                string name = "tape_" + to_string(out_start + i) + ".bin";
                cout << "Tasma: " << name << endl;
                printFile(name);
            }
        }

        // Jesli w biezacej fazie powstala tylko 1 seria lub mniej, plik jest w pelni posortowany
        if (runs_in_this_phase <= 1) {
            final_tape_idx = out_start;
            break;
        }

        // Zamiana rol: tasmy wyjsciowe staja sie wejsciowymi w kolejnej fazie
        swap(in_start, out_start);
    }

    return current_phase;
}

// ---------------------------------------------------------
// 7. EKSPERYMENT POROWNAWCZY (PRAKTYKA VS TEORIA)
// ---------------------------------------------------------
void runExperiment() {
    int test_sizes[] = {1000, 5000, 10000, 50000, 100000};

    cout << "\n=== ROZPOCZYNAM EKSPERYMENT ===" << endl;
    cout << left 
         << setw(10) << "N" 
         << setw(10) << "Fazy(P)" << setw(10) << "Fazy(T)" 
         << setw(12) << "Odc(P)"  << setw(12) << "Odc(T)" 
         << setw(12) << "Zap(P)"  << setw(12) << "Zap(T)" << endl;
    cout << string(78, '-') << endl;

    for (int i = 0; i < 5; i++) {
        int N = test_sizes[i];
        srand(12345);

        global_disk_reads = 0;
        global_disk_writes = 0;

        generateRandomFile("plik_wejsciowy.bin", N);
        createInitialRuns("plik_wejsciowy.bin");

        int final_tape = 0;
        int total_phases = mergeRuns(false, final_tape);

        // Teoretyczne wartosci wykladnicze:
        // Liczba stron pliku S_N = ceil(N / b)
        int pages = ceil((double)N / B_FACTOR);

        // Liczba poczatkowych serii r = ceil(N / (n * b))
        int initial_runs = ceil((double)N / (B_FACTOR * N_BUFFERS));

        int theo_phases = 0;
        if (initial_runs > 1) {
            theo_phases = ceil(log(initial_runs) / log(T));
        }

        int theo_reads = pages * (1 + theo_phases);
        int theo_writes = pages * (1 + theo_phases);

        cout << left 
             << setw(10) << N 
             << setw(10) << total_phases << setw(10) << theo_phases 
             << setw(12) << global_disk_reads << setw(12) << theo_reads 
             << setw(12) << global_disk_writes << setw(12) << theo_writes << endl;
    }

    cout << "======================================\n" << endl;
}

// ---------------------------------------------------------
// 8. MENU GLOWNE
// ---------------------------------------------------------
int main() {
    string input_filename = "plik_wejsciowy.bin";
    int choice = 0;
    srand(time(NULL));

    while (true) {
        cout << "\n=== MENU PROJEKTU SBD ===" << endl;
        cout << "1. Generuj losowe rekordy PESEL" << endl;
        cout << "2. Wpisz rekordy recznie (z klawiatury)" << endl;
        cout << "3. Wczytaj rekordy z pliku tekstowego (.txt)" << endl;
        cout << "4. Wyswietl plik wejsciowy (niesortowany)" << endl;
        cout << "5. Uruchom algorytm sortowania (Wielkie Bufory)" << endl;
        cout << "6. Przeprowadz automatyczny eksperyment (5 zestawow)" << endl;
        cout << "7. Wyjscie" << endl;
        cout << "Wybor: ";
        cin >> choice;

        if (choice == 1) {
            int amount = 0;
            cout << "Ile rekordow wygenerowac? (np. 5000): ";
            cin >> amount;
            generateRandomFile(input_filename, amount);
            cout << "Wygenerowano!" << endl;
        } 
        else if (choice == 2) {
            int amount = 0;
            cout << "Ile rekordow chcesz wpisac recznie? ";
            cin >> amount;

            Tape tape(input_filename);
            tape.openWrite();

            cout << "Podawaj 11-znakowe numery PESEL (oddzielone spacja/enterem):" << endl;
            for (int i = 0; i < amount; i++) {
                Record r;
                cin >> ws;
                cin.getline(r.pesel, 12);
                r.pesel[11] = '\0';
                tape.writeRecord(r);
            }

            tape.close();
            cout << "Zapisano do pliku dyskowego!" << endl;
        } 
        else if (choice == 3) {
            string txt_name;
            cout << "Podaj nazwe pliku tekstowego (np. test.txt): ";
            cin >> txt_name;
            loadFromTextFile(txt_name, input_filename);
        } 
        else if (choice == 4) {
            cout << "--- ZAWARTOSC " << input_filename << " ---" << endl;
            printFile(input_filename);
        } 
        else if (choice == 5) {
            global_disk_reads = 0;
            global_disk_writes = 0;

            int initial_runs = createInitialRuns(input_filename);
            cout << "Etap 1: Utworzono " << initial_runs << " serii (poczatkowy rozrzut)." << endl;

            cout << "Czy pokazywac podglad tasm po kazdej fazie? (1 - Tak, 0 - Nie): ";
            bool show = false;
            cin >> show;

            int final_tape = 0;
            int total_phases = mergeRuns(show, final_tape);

            cout << "\n=== PODSUMOWANIE SORTOWANIA ===" << endl;
            cout << "Plik wynikowy znajduje sie w: tape_" << final_tape << ".bin" << endl;
            cout << "Calkowita liczba faz scalania: " << total_phases << endl;
            cout << "Fizyczne odczyty dyskowe: " << global_disk_reads << endl;
            cout << "Fizyczne zapisy dyskowe:  " << global_disk_writes << endl;
        } 
        else if (choice == 6) {
            runExperiment();
        } 
        else if (choice == 7) {
            break;
        }
    }

    return 0;
}