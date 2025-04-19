#include "fractal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

size_t response_serialize(response_t *response, uint8_t **buffer){
    size_t num_values = response->granularity * response->granularity;
    size_t size_values = sizeof(int) * num_values;
    size_t buffer_size = sizeof(response_t) + size_values;

    *buffer = malloc(buffer_size);
    if (*buffer == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }

    memcpy(*buffer, response, sizeof(response_t)); // Copy response to buffer 
    memcpy(*buffer + sizeof(response_t), response->values, size_values); // Append values

    return buffer_size;
}

response_t* response_deserialize(uint8_t **data){
    response_t* response = malloc(sizeof(response_t));
    if (response == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }
    
    memcpy(response, *data, sizeof(response_t)); // Copy base response (pointer is garbage)
    size_t num_values = response->granularity * response->granularity;
    size_t size_values = sizeof(int) * num_values;

    response->values = malloc(size_values); // Overwrite garbage pointer
    if (response->values == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }

    memcpy(response->values, *data + sizeof(response_t), size_values); // Add values to pointer
    return response;
}