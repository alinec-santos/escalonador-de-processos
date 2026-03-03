#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void simularUsoCPU(){
    int uso = rand()%101;
    time_t agora= time(NULL);
    printf("[MONITOR_CPU] Tempo: %ld | Uso simulado da CPU: %d%%\n", agora,uso);
}

int main(){
    srand(time(NULL));
    while(1){
        simularUsoCPU();
        sleep(5);//tempo em segundos
    }
    return 0;
}