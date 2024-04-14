#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define TMin 1
#define TMax 5

int N; // Number of tracks
int T; // Occupation time in milliseconds

sem_t binari; // Tracks semaphore
int id = 0; // ID counter
pthread_mutex_t id_lock; // ID mutex

int treni_in_arrivo = 0;
int treni_in_stazione = 0;
int treni_completati = 0;
pthread_mutex_t print; // Information mutex

// Train function
void *train(void *arg) {
    int my_id = 0;

    pthread_mutex_lock(&id_lock);
    my_id = id++;
    treni_in_arrivo++;
    pthread_mutex_unlock(&id_lock);

    // Wait until a track becomes available
    sem_wait(&binari);

    // Update train count
    pthread_mutex_lock(&print);
    treni_in_arrivo--;
    treni_in_stazione++;
    printf("Treno %d in stazione. Completati: %d, In stazione: %d, In attesa: %d\n",
           my_id, treni_completati, treni_in_stazione, treni_in_arrivo);
    pthread_mutex_unlock(&print);

    // Occupation time
    usleep(T*1000);

    // Leave the track and update the count
    pthread_mutex_lock(&print);
    treni_in_stazione--;
    treni_completati++;
    printf("Treno %d lascia la stazione. Completati: %d, In stazione: %d, In attesa: %d\n",
           my_id, treni_completati, treni_in_stazione, treni_in_arrivo);
    pthread_mutex_unlock(&print);

    sem_post(&binari);
    return NULL;
}

int main(int argc, char* argv[]) {

    N = 7;
    T = 20;

    sem_init(&binari, 0, N);
    pthread_mutex_init(&id_lock, NULL);
    pthread_mutex_init(&print, NULL);

    pthread_t tid;
    while (1) {

        // Wait for a random time between TMin and TMax
        int attesa = rand() % (TMax - TMin + 1) + TMin;
        usleep(attesa*1000);

        // Each train is a thread
        if(pthread_create(&tid, NULL, train, NULL) != 0) {
            fprintf(stderr, "Errore nella creazione del thread\n");
            return 1;
        }
        pthread_detach(tid);
    }

    sem_destroy(&binari);
    pthread_mutex_destroy(&id_lock);
    pthread_mutex_destroy(&print);
    return 0;
}
