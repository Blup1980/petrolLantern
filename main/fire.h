#ifndef FIRE_H
#define FIER_H

typedef enum command_e {
    none,
    off,
    on,
    up,
    down
} command_t;


void fire_task(void *pvParameters);

    

#endif