/*
 * config_planif.h
 *
 *  Created on: 2/9/2015
 *      Author: utnso
 */

#ifndef CONFIG_PLANIF_H_
#define CONFIG_PLANIF_H_

#include <commons/config.h>

t_config* cfg = NULL;
char* CONFIG_PATH = "/home/utnso/Escritorio/git/tp-2015-2c-killthepony/procesoPlanificador/Debug/config.txt";

int PUERTO_ESCUCHA();

char* ALGORITMO_PLANIFICACION();

char* QUANTUM();

int TIPO_LOG();

/*
 *
 */

int PUERTO_ESCUCHA(){
	return config_get_int_value(cfg, "PUERTO_ESCUCHA");
}

char* ALGORITMO_PLANIFICACION(){
	return config_get_string_value(cfg, "ALGORITMO_PLANIFICACION");
}

char* QUANTUM(){
	return config_get_string_value(cfg, "QUANTUM");
}

int TIPO_LOG(){
	return config_get_int_value(cfg, "TIPO_LOG");
}


#endif /* CONFIG_PLANIF_H_ */
