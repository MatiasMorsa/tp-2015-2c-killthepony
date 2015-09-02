/*
 * procesoSwap.h
 *
 *  Created on: 30/8/2015
 *      Author: utnso
 */

#ifndef PROCESOSWAP_H_
#define PROCESOSWAP_H_

#include <util.h>

#include <commons/config.h>
#include <commons/log.h>

char* CONFIG_PATH = "config.txt";
char* LOGGER_PATH = "log.txt";

t_log* logger;
t_config* config;

int PUERTO_ESCUCHA();
int iniciar_server();

int inicializar();
int finalizar();

void procesar_mensaje(int socket, t_msg* msg);


#endif /* PROCESOSWAP_H_ */