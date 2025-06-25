#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>

#define CRATE_CAPACITY 12
#define MAX_FRUITS 1000
#define NUM_PICKERS 3

int *fruitTree;                // Array representing fruits on the tree
int totalFruits;               // Total number of fruits
int nextFruitIndex = 0;        // Index of the next fruit to be picked

typedef struct {
    int count;                 // Number of fruits in crate
    int contents[CRATE_CAPACITY];  // Crate content
} FruitCrate;

FruitCrate currentCrate;
int crateCounter = 1;          // To count crates
bool allPicked = false;        // Signal that pickers are done

// Counting semaphores
sem_t availableSlots;          // To count empty slots in crate
sem_t readyToLoad;             // To notify loader when crate is full

// Mutexes (mutual exclusion)
pthread_mutex_t fruitAccessMutex;  // Protects access to fruit array
pthread_mutex_t crateAccessMutex;  // Protects access to crate



void* fruitPicker(void* arg) {
    int pickerId = *((int*)arg);

    while (1) {
        // Lock fruit access
        pthread_mutex_lock(&fruitAccessMutex);
        if (nextFruitIndex >= totalFruits) {
            pthread_mutex_unlock(&fruitAccessMutex);
            break;
        }
        int fruit = fruitTree[nextFruitIndex++];
        pthread_mutex_unlock(&fruitAccessMutex);

        printf("Picker %d: grabbed fruit #%d from the tree.\n", pickerId, fruit);

        // Wait for space in the crate
        sem_wait(&availableSlots);
        pthread_mutex_lock(&crateAccessMutex);

        currentCrate.contents[currentCrate.count++] = fruit;
        printf("Picker %d: dropped fruit #%d into the crate (%d of %d filled).\n",
               pickerId, fruit, currentCrate.count, CRATE_CAPACITY);

        if (currentCrate.count == CRATE_CAPACITY) {
            sem_post(&readyToLoad);
        }

        pthread_mutex_unlock(&crateAccessMutex);
        usleep(100000); // Simulate delay
    }

    free(arg);
    return NULL;
}

void* crateLoader(void* arg) {
    while (1) {
        sem_wait(&readyToLoad);
        pthread_mutex_lock(&crateAccessMutex);

        if (allPicked && currentCrate.count == 0) {
            pthread_mutex_unlock(&crateAccessMutex);
            break;
        }

        printf("Loader: Crate #%d loaded to truck contain [%d fruits]: [", crateCounter++, currentCrate.count);
        for (int i = 0; i < currentCrate.count; i++) {
            printf("%d%s", currentCrate.contents[i], (i < currentCrate.count - 1) ? ", " : "]\n");
        }

        currentCrate.count = 0;
        pthread_mutex_unlock(&crateAccessMutex);

        // Reset crate for pickers
        for (int i = 0; i < CRATE_CAPACITY; i++) {
            sem_post(&availableSlots);
        }

        if (allPicked && nextFruitIndex >= totalFruits) {
            break;
        }
    }

    printf("Loader: All crates loaded to truck!.\n");
    return NULL;
}

// Initialization of semaphores
void initialize_semaphores() {
    sem_init(&availableSlots, 0, CRATE_CAPACITY);
    sem_init(&readyToLoad, 0, 0);
    pthread_mutex_init(&fruitAccessMutex, NULL);
    pthread_mutex_init(&crateAccessMutex, NULL);
}

int main() {
    printf("Enter number of fruits on tree: ");
    scanf("%d", &totalFruits);

    if (totalFruits <= 0 || totalFruits > MAX_FRUITS) {
        printf("Invalid input. Please enter between 1 and %d fruits.\n", MAX_FRUITS);
        return 1;
    }

    fruitTree = malloc(sizeof(int) * totalFruits);
    for (int i = 0; i < totalFruits; i++) {
        fruitTree[i] = i + 1;
    }

    pthread_t pickers[NUM_PICKERS], loaderThread;
    currentCrate.count = 0;

    initialize_semaphores();

    pthread_create(&loaderThread, NULL, crateLoader, NULL);

    for (int i = 0; i < NUM_PICKERS; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&pickers[i], NULL, fruitPicker, id);
    }

    for (int i = 0; i < NUM_PICKERS; i++) {
        pthread_join(pickers[i], NULL);
    }

    allPicked = true;
    sem_post(&readyToLoad);

    pthread_join(loaderThread, NULL);
    printf("WORK DONE! All fruit has been picked and loaded for transportation.\n");


    return 0;
}
