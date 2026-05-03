#include <iostream>
#include <cstdlib>
#include <ctime>
#define MAX 100

using namespace std;

int numero, Rangoizq, RangoDer;
int arrO[MAX];

// ---------------- UTIL ----------------
void copiar(int origen[], int destino[]) {
    for (int i = 0; i < numero; i++) {
        destino[i] = origen[i];
    }
}

void imprimir(int arr[]) {
    for (int i = 0; i < numero; i++) {
        cout << arr[i] << " ";
    }
    cout << endl;
}

// ---------------- BUBBLE ----------------
void bubbleSort(int arr[], long long &pasos) {
    for (int i = 0; i < numero - 1; i++) {
        for (int j = 0; j < numero - 1 - i; j++) {
            pasos++;
            if (arr[j] > arr[j + 1]) {
                swap(arr[j], arr[j + 1]);
                pasos++;
            }
        }
    }
}

// ---------------- SELECTION ----------------
void selectionSort(int arr[], long long &pasos) {
    for (int i = 0; i < numero - 1; i++) {
        int minIndex = i;
        for (int j = i + 1; j < numero; j++) {
            pasos++;
            if (arr[j] < arr[minIndex]) {
                minIndex = j;
            }
        }
        swap(arr[i], arr[minIndex]);
        pasos++;
    }
}

// ---------------- INSERTION ----------------
void insertionSort(int arr[], long long &pasos) {
    for (int i = 1; i < numero; i++) {
        int key = arr[i];
        int j = i - 1;

        while (j >= 0 && arr[j] > key) {
            pasos++;
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ---------------- QUICK ----------------
int partition(int arr[], int low, int high, long long &pasos) {
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        pasos++;
        if (arr[j] < pivot) {
            i++;
            swap(arr[i], arr[j]);
            pasos++;
        }
    }
    swap(arr[i + 1], arr[high]);
    pasos++;
    return i + 1;
}

void quickSort(int arr[], int low, int high, long long &pasos) {
    if (low < high) {
        int pi = partition(arr, low, high, pasos);
        quickSort(arr, low, pi - 1, pasos);
        quickSort(arr, pi + 1, high, pasos);
    }
}

// ---------------- MERGE ----------------
void merge(int arr[], int l, int m, int r, long long &pasos) {
    int temp[MAX];
    int i = l, j = m + 1, k = 0;

    while (i <= m && j <= r) {
        pasos++;
        if (arr[i] < arr[j]) temp[k++] = arr[i++];
        else temp[k++] = arr[j++];
    }

    while (i <= m) temp[k++] = arr[i++];
    while (j <= r) temp[k++] = arr[j++];

    for (int x = l, k = 0; x <= r; x++, k++) {
        arr[x] = temp[k];
    }
}

void mergeSort(int arr[], int l, int r, long long &pasos) {
    if (l < r) {
        int m = (l + r) / 2;
        mergeSort(arr, l, m, pasos);
        mergeSort(arr, m + 1, r, pasos);
        merge(arr, l, m, r, pasos);
    }
}

// ---------------- COUNTING ----------------
void countingSort(int arr[], long long &pasos) {
    int rango = RangoDer - Rangoizq + 1;

    int count[1000] = {0}; // ajusta si tu rango es mayor

    // contar
    for (int i = 0; i < numero; i++) {
        count[arr[i] - Rangoizq]++;
        pasos++;
    }

    // reconstruir
    int k = 0;
    for (int i = 0; i < rango; i++) {
        while (count[i] > 0) {
            arr[k++] = i + Rangoizq;
            count[i]--;
            pasos++;
        }
    }
}

// ---------------- PROBAR ----------------
void probar(void (*sortFunc)(int[], long long&), string nombre) {
    int copia[MAX];
    copiar(arrO, copia);

    long long pasos = 0;

    clock_t inicio = clock();
    sortFunc(copia, pasos);
    clock_t fin = clock();

    cout << "\n--- " << nombre << " ---\n";
    imprimir(copia);
    cout << "Pasos: " << pasos << endl;
    cout << "Tiempo: " << double(fin - inicio) / CLOCKS_PER_SEC << " s\n";
}

// ---------------- MAIN ----------------
int main() {
    cout << "Cantidad: ";
    cin >> numero;

    cout << "Rango (min max): ";
    cin >> Rangoizq >> RangoDer;

    srand(time(0));

    for (int i = 0; i < numero; i++) {
        arrO[i] = Rangoizq + rand() % (RangoDer - Rangoizq + 1);
    }

    cout << "\nOriginal:\n";
    imprimir(arrO);

    probar(bubbleSort, "Bubble");
    probar(selectionSort, "Selection");
    probar(insertionSort, "Insertion");
    probar(countingSort, "Counting");

    // QUICK
    {
        int copia[MAX];
        copiar(arrO, copia);
        long long pasos = 0;

        clock_t inicio = clock();
        quickSort(copia, 0, numero - 1, pasos);
        clock_t fin = clock();

        cout << "\n--- Quick ---\n";
        imprimir(copia);
        cout << "Pasos: " << pasos << endl;
        cout << "Tiempo: " << double(fin - inicio) / CLOCKS_PER_SEC << " s\n";
    }

    // MERGE
    {
        int copia[MAX];
        copiar(arrO, copia);
        long long pasos = 0;

        clock_t inicio = clock();
        mergeSort(copia, 0, numero - 1, pasos);
        clock_t fin = clock();

        cout << "\n--- Merge ---\n";
        imprimir(copia);
        cout << "Pasos: " << pasos << endl;
        cout << "Tiempo: " << double(fin - inicio) / CLOCKS_PER_SEC << " s\n";
    }

    return 0;
}
