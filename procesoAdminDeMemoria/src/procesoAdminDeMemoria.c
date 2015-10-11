/*
 ============================================================================
 Name        : procesoAdminDeMemoria.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "procesoAdminDeMemoria.h"


int main(void) {
	inicializar();
	socket_swap = conectar_con_swap();
	server_socket_select(PUERTO_ESCUCHA(), procesar_mensaje_cpu);
	finalizar();
	return EXIT_SUCCESS;
}

int inicializar(){ ///////////////////
	int i;

	// ARCHIVO DE CONFIGURACION
	cfg = config_create(CONFIG_PATH);

	// ARCHIVO DE LOG
	clean_file(LOGGER_PATH);
	logger = log_create(LOGGER_PATH, "procesoAdminMem", true, LOG_LEVEL_TRACE);

	// ESTRUCTURA MEMORIA
	memoria = (t_marco**)malloc(CANTIDAD_MARCOS()*sizeof(t_marco*));

	for(i=0;i<CANTIDAD_MARCOS();i++)
		memoria[i] = (t_marco*)malloc((TAMANIO_MARCO()+1)*sizeof(char)+sizeof(int));

	// ESTRUCTURA TLB y TABLA DE PAGINAS
	TLB 	= list_create();
	paginas = list_create();

	return 0;
}

int finalizar(){  ///////////////////
	int i;

	// ARCHIVO DE CONFIGURACION
	config_destroy(cfg);

	// ARCHIVO DE LOG
	log_destroy(logger);

	// DESTRUIR LA MEMORIA
	for(i=0;i<CANTIDAD_MARCOS();i++){
		free(memoria[i]->contenido);
		free(memoria[i]);
	}
	free(memoria);

	// ESTRUCTURA TLB y TABLA DE PAGINAS
	list_destroy_and_destroy_elements(paginas,(void*)destruir_proceso);
	list_destroy(TLB);

	return 0;
}




void procesar_mensaje_cpu(int socket, t_msg* msg){
	//print_msg(msg);
	char* buff_pag  = NULL;
	char* buff_pag_esc  = NULL;
	int st,pid,nro_pagina,cant_paginas,marco,lugares;
	t_msg* resp = NULL;
	t_proceso* proceso;
	t_pagina* pagina;


//	t_pagina* pagina;

	switch (msg->header.id) {
		case MEM_INICIAR: ///////
			//param 0 cant_paginas
			//param 1 PID

			pid 			= msg->argv[0];
			cant_paginas 	= msg->argv[1];

			log_trace(logger, "Iniciar Proceso %d con %d paginas",pid,cant_paginas);
			destroy_message(msg);

			st = iniciar_proceso_CPU(pid,cant_paginas);

			switch(st){
				case 0:
					resp = argv_message(MEM_OK, 1 ,0);
					enviar_y_destroy_mensaje(socket, resp);
					log_info(logger, "El proceso %d fue inicializado correctamente",pid);
					break;
				case 1:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					enviar_y_destroy_mensaje(socket, resp);
					log_warning(logger, "No hay espacio suficiente para alocar la memoria del proceso %d",pid);
					break;
			}

			break;

		case MEM_LEER: /////////////
			//param 0 PID / 1 Pagina
			pid 		= msg->argv[0];
			nro_pagina  = msg->argv[1];
			destroy_message(msg);
			log_trace(logger, "Leer pagina %d del proceso", nro_pagina,pid);

			// BUSCO EL PROCESO
			gl_PID=pid;
			proceso = list_find(paginas,(void*)es_el_proceso_segun_PID);

			// BUSCO EL MARCO EN LA TLB Y EN LA TABLA DE PAGINAS
			marco = buscar_marco_de_pagina_en_TLB_y_tabla_paginas(pid,nro_pagina);

			// POSIBLES VALORES = >=0 (posicion en memoria) -1 (no esta en memoria) -2 (no existe la pagina)
			if(marco == -2){
				st = 0;
			}else{
				if(marco == -1){
					cant_paginas = list_count_satisfying(proceso->paginas,(void*)la_pagina_esta_cargada_en_memoria);
					marco = encontrar_marco_libre();
					if(marco == -1 /* NO HAY MAS LUGAR*/ && cant_paginas==0 /* NO HAY PAGINAS PARA SACAR*/){
						st=3;
					}else{
						buff_pag = swap_leer_pagina(pid, nro_pagina);
						if(buff_pag != NULL){
							if(cant_paginas<MAXIMO_MARCOS_POR_PROCESO()){
								agregar_pagina_en_memoria(proceso,nro_pagina,buff_pag);
								st = 2;
							}else{
								st = reemplazar_pagina_en_memoria_segun_algoritmo(proceso,nro_pagina,buff_pag);
							}
							sleep(RETARDO_MEMORIA());
						}else{
							st = 1;
						}
					}
				}
				else {
					sleep(RETARDO_MEMORIA());
					buff_pag = string_duplicate(memoria[marco]->contenido);
					st = 2;
				}
			}


			switch(st){
				case 0:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No existe la pagina %d solicitada por el proceso %d",nro_pagina,pid);
					break;
				case 1:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "La pagina %d del proceso %d no fue recibida bien del SWAP",nro_pagina,pid);
					break;
				case 2:
					//sleep(RETARDO_MEMORIA());
					resp = string_message(MEM_OK, buff_pag, 0);
					log_info(logger, "La pagina %d del proceso %d fue leida correctamente",nro_pagina,pid);
					break;
				case 3:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No hay lugar en memoria para guardar la pagina %d del proceso %d",nro_pagina,pid);
					break;
				case 4:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No se pudo guardar la informacion de la pagina desalojada del proceso %d en SWAP",pid);
					break;
			}

			enviar_y_destroy_mensaje(socket, resp);
			FREE_NULL(buff_pag);

			break;

		case MEM_ESCRIBIR:
			//param 0 PID / 1 Pagina
			buff_pag 	= string_duplicate(msg->stream);
			pid 		= msg->argv[0];
			nro_pagina 	= msg->argv[1];
			destroy_message(msg);
			log_trace(logger, "Escribir en Memoria la pagina %d del PID %d y texto: \"%s\"", nro_pagina, pid,buff_pag);

			// BUSCO EL PROCESO y la PAGINA
			gl_PID=pid;
			gl_nro_pagina=nro_pagina;
			proceso = list_find(paginas,(void*)es_el_proceso_segun_PID);
			pagina  = list_find(proceso->paginas,(void*)es_la_pagina_segun_PID_y_nro_pagina);

			// BUSCO EL MARCO EN LA TLB Y EN LA TABLA DE PAGINAS
			marco = buscar_marco_de_pagina_en_TLB_y_tabla_paginas(pid,nro_pagina);

			if(marco == -2){
				st = 0;
			}else{
				if(marco == -1){
					cant_paginas = list_count_satisfying(proceso->paginas,(void*)la_pagina_esta_cargada_en_memoria);
					marco = encontrar_marco_libre();
					if(marco == -1 /* NO HAY MAS LUGAR*/ && cant_paginas==0 /* NO HAY PAGINAS PARA SACAR*/){
						st=3;
					}else{
						// OPCIONAL
						buff_pag_esc = swap_leer_pagina(pid, nro_pagina);
						FREE_NULL(buff_pag_esc);
						// OPCIONAL

						if(buff_pag != NULL){
							if(cant_paginas<MAXIMO_MARCOS_POR_PROCESO()){
								agregar_pagina_en_memoria(proceso,nro_pagina,buff_pag);
								st = 2;
							}else{
								st = reemplazar_pagina_en_memoria_segun_algoritmo(proceso,nro_pagina,buff_pag);
							}
							pagina->modificado=1;
							sleep(RETARDO_MEMORIA());
						}else{
							st = 1;
						}
					}
				}
				else {
					strncpy(memoria[marco]->contenido,buff_pag,TAMANIO_MARCO());
					memoria[marco]->contenido[TAMANIO_MARCO()]='\0';
					pagina->modificado=1;
					sleep(RETARDO_MEMORIA());
					st = 2;
				}
			}

/*
			if(marco == -2){
				st = 0;
				log_error(logger, "No existe la pagina %d del proceso %d",nro_pagina,pid);
			}else{
				if(marco == -1){
					if(memoria->elements_count<CANTIDAD_MARCOS()){
						// OPCIONAL
							buff_pag_esc = swap_leer_pagina(pid, nro_pagina);
							FREE_NULL(buff_pag_esc);
						// OPCIONAL
						gl_PID=pid;
						gl_nro_pagina=nro_pagina;
						cant_paginas = list_count_satisfying(paginas,(void*)es_la_pagina_segun_PID_y_nro_pagina);

						if(cant_paginas<MAXIMO_MARCOS_POR_PROCESO()){
							agregar_pagina_en_memoria(pid,nro_pagina,buff_pag);
							st = 1;
						}else{
							st = reemplazar_pagina_en_memoria_segun_algoritmo(pid,nro_pagina,buff_pag);
						}

					}else{
						st=0;
						log_error(logger, "No hay lugar en memoria para guardar la pagina %d del proceso %d",nro_pagina,pid);
					}
				}
				else {
					sleep(RETARDO_MEMORIA());
					usar_pagina_en_memoria_segun_algoritmo(pid,nro_pagina,marco,flag_TLB);
					pagina_en_memoria = list_get(memoria,marco);
					free(pagina_en_memoria->contenido);
					pagina_en_memoria->contenido=buff_pag;
					st = 1;
					log_info(logger, "La pagina %d del proceso %d fue modificada exitosamente",nro_pagina,pid);
				}
				setear_flag_modificado(pid,nro_pagina);
			}

			if(st==1||st==2){
				resp = argv_message(MEM_OK, 1, 0);
			}else{
				resp = argv_message(MEM_NO_OK, 1, 0);
			}*/

			switch(st){
				case 0:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No existe la pagina %d solicitada por el proceso %d",nro_pagina,pid);
					break;
				case 1:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "La pagina %d del proceso %d no fue recibida bien del SWAP",nro_pagina,pid);
					break;
				case 2:
					//sleep(RETARDO_MEMORIA());
					resp = argv_message(MEM_OK, 1 ,0);
					log_info(logger, "La pagina %d del proceso %d fue escrita correctamente",nro_pagina,pid);
					break;
				case 3:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No hay lugar en memoria para guardar la pagina %d del proceso %d",nro_pagina,pid);
					break;
				case 4:
					resp = argv_message(MEM_NO_OK, 1 ,0);
					log_error(logger, "No se pudo guardar la informacion de la pagina desalojada del proceso %d en SWAP",pid);
					break;
			}

			enviar_y_destroy_mensaje(socket, resp);
			break;

		case MEM_FINALIZAR:
			//param 0 PID
			log_info(logger, "MEM_FINALIZAR");

			gl_PID = msg->argv[0];
			destroy_message(msg);

			// LE AVISO AL SWAP QUE LIBERE EL ESPACIO
			st = swap_finalizar(gl_PID);
			if(st !=0){
				resp = argv_message(MEM_NO_OK, 1, 0);
				enviar_y_destroy_mensaje(socket, resp);
				break;
			}

			// ELIMINO LAS ESTRUCTURAS
			list_remove_and_destroy_by_condition(paginas,(void*)es_el_proceso_segun_PID,(void*)destruir_proceso);

			// LE AVISOA LA CPU COMO TERMINO
			resp = argv_message(MEM_OK, 1, 0);
			enviar_y_destroy_mensaje(socket, resp);
			break;


		case CPU_NUEVO:
			break;

		default:
			log_warning(logger, "LA OPCION SELECCIONADA NO ESTA REGISTRADA");
			break;
	}



	//destroy_message(msg);
}

int buscar_marco_de_pagina_en_TLB_y_tabla_paginas(int pid, int nro_pagina){
	t_proceso* proceso;
	int marco;

	gl_PID=pid;
	proceso = list_find(paginas,(void*)es_el_proceso_segun_PID);

	// SE FIJA SI ESTA LA PAGINA EN LA TLB
	if(TLB_HABILITADA()){
		marco = buscar_marco_en_TLB(pid,nro_pagina);
	} else {
		marco = -1;
	}
	proceso->TLB_total++;

	// SE FIJA SI ESTA LA PAGINA EN MEMORIA
	if(marco == -1){
		sleep(RETARDO_MEMORIA());
		marco = buscar_marco_en_paginas(pid,nro_pagina);
	}else{
		proceso->TLB_hit++;
	}

	return marco;
}

int encontrar_marco_libre(){ ////////////////
	int i;
	for (i=0;i<CANTIDAD_MARCOS();i++)
		if(memoria[i]->libre)
			return i;
	// EN CASO QUE ESTE LLENA LA MEMORIA
	return -1;
}

void tasa_aciertos_TLB_total(){
	float valor;
	gl_TLB_hit=0.0;
	gl_TLB_total = 0;
	list_iterate(paginas,(void*)sumar_tasas_TLB);
	valor= (gl_TLB_hit/gl_TLB_total)*100;
	log_info(logger, "TLB - Tasa de aciertos: %f",valor);
}

void sumar_tasas_TLB(t_proceso* proceso){
	gl_TLB_hit += proceso->TLB_hit;
	gl_TLB_total += proceso->TLB_total;
}

void tasa_aciertos_TLB(t_proceso* proceso){
	float valor;

	valor= (proceso->TLB_hit/proceso->TLB_total)*100;
	log_info(logger, "Tasa de aciertos para el proceso %d: %f",proceso->PID,valor);
}


void agregar_pagina_en_memoria(t_proceso* proceso, int nro_pagina, char* contenido){ ///////////////////

	// BUSCO UN MARCO LIBRE
	int marco = encontrar_marco_libre();

	// AGREGO EL CONTENIDO A MEMORIA
	strncpy(memoria[marco]->contenido,contenido,TAMANIO_MARCO());
	memoria[marco]->contenido[TAMANIO_MARCO()]='\0';
	memoria[marco]->libre = 0;

	// BUSCO LA PAGINA EN LA TABLA DE PROCESOS
	gl_nro_pagina = nro_pagina;
	gl_PID 		  = proceso->PID;
	t_pagina* pagina = list_find(proceso->paginas,(void*)es_la_pagina_segun_PID_y_nro_pagina);

	// ACTUALIZO ATRIBUTOS
	pagina->marco=marco;
	pagina->presencia=1;
	pagina->usado=1;

	// AGREGO LA PAGINA EN LA TLB, SIEMPRE Y CUANDO ESTE HABILITADA Y YA NO EXISTA LA PAGINA
	if(TLB_HABILITADA() && !list_any_satisfy(TLB,(void*)es_la_pagina_segun_PID_y_nro_pagina)){
		// SI ESTA LLENA SACO EL ULTIMO AGREGADO
		if(TLB->elements_count==ENTRADAS_TLB()){
			list_remove(TLB,0);
		}
		list_add(TLB,pagina);
	}
}


int reemplazar_pagina_en_memoria_segun_algoritmo(t_proceso* proceso, int pagina, char* contenido){
	gl_PID		  = proceso->PID;
	int st;
	t_pagina* pag;

	// BUSCO LA PAGINA "VICTIMA" PARA QUITAR DE MEMORIA
	if(string_equals_ignore_case(ALGORITMO_REEMPLAZO(),"FIFO")){

		pag = list_remove(proceso->paginas,0);
		list_add(proceso->paginas,pag);
	}

	// ME FIJO SI ESTA MODIFICADA Y LA GUARDO EN SWAP
	if(pag->modificado)
		st = swap_escribir_pagina(proceso->PID, pag->pagina, memoria[pag->marco]->contenido);
	if(st==-1) return 4;

	// ME FIJO SI ESTA EN LA TLB y LA SACO
	gl_nro_pagina = pag->pagina;

	if(list_any_satisfy(TLB,(void*)es_la_pagina_segun_PID_y_nro_pagina))
		list_remove_by_condition(TLB,(void*)es_la_pagina_segun_PID_y_nro_pagina);

	// ACTUALIZO LA MEMORIA Y LA TABLA DE PAGINAS
	memoria[pag->marco]->libre = 1;
	pag->marco = -1;
	pag->modificado = 0;
	pag->presencia = 0;
	pag->usado = 0;

	agregar_pagina_en_memoria(proceso, pagina, contenido);

	return 2;
}



void agregar_pagina_en_TLB(int PID, int pagina, int entrada){
	t_pagina* new_pagina;
	new_pagina = crear_pagina(PID,pagina,entrada);
	if(TLB->elements_count==ENTRADAS_TLB())
		list_remove_and_destroy_element(TLB,0,(void*)destruir_pagina);
	list_add(TLB,new_pagina);
}



int buscar_marco_en_TLB(int PID,int nro_pagina){
	t_pagina* pagina;
	gl_PID=PID;
	gl_nro_pagina=nro_pagina;
	if(list_any_satisfy(TLB,(void*)es_la_pagina_segun_PID_y_nro_pagina)){
		pagina = list_find(TLB,(void*)es_la_pagina_segun_PID_y_nro_pagina);
		return pagina->marco;
	} else {
		return -1;
	}
}

int buscar_marco_en_paginas(int PID,int nro_pagina){
	t_pagina* pagina;
	gl_PID=PID;
	gl_nro_pagina=nro_pagina;
	if(list_any_satisfy(paginas,(void*)es_la_pagina_segun_PID_y_nro_pagina)){
		pagina = list_find(paginas,(void*)es_la_pagina_segun_PID_y_nro_pagina);
		return pagina->marco;
	} else {
		return -2;
	}
}

int iniciar_proceso_CPU(int pid, int paginas){ ///////////////////
	int st;

	// LE ENVIO AL PROCESO SWAP PARA QUE RESERVE EL ESPACIO
	st = swap_nuevo_proceso(pid, paginas);

	if(st==0){
		crear_estructuras_de_un_proceso(pid,paginas);
		return 0;
	}
	else{
		return 1;
	}

}

int es_la_pagina_segun_PID(t_pagina* pagina){
	return(pagina->PID==gl_PID);
}

int es_el_proceso_segun_PID(t_proceso* proceso){
	return(proceso->PID==gl_PID);
}

int es_la_pagina_segun_PID_y_nro_pagina(t_pagina* pagina){
	return(pagina->PID==gl_PID && pagina->pagina==gl_nro_pagina);
}

int la_pagina_esta_cargada_en_memoria(t_pagina* pagina){
	return(pagina->presencia);
}


void crear_estructuras_de_un_proceso(int PID,int cant_paginas){ ///////////////////
	int i;
	t_proceso* 	new_proceso 	 = (t_proceso*)malloc(sizeof(t_proceso));
	t_pagina*  	new_pagina;

	// ASOCIO EL PID Y CREO LA LISTA DE PAGINAS
	new_proceso->PID = PID;
	new_proceso->paginas = list_create();
	new_proceso->TLB_hit = 0.0;
	new_proceso->TLB_total = 0;

	// AGREGO A LA LISTA CADA PAGINA
	for(i=0;i<cant_paginas;i++){
		new_pagina = crear_pagina(PID,i,-1);
		list_add(new_proceso->paginas,new_pagina);
	}

	// AGREGO EL PROCESO A LA TABLA DE PAGINAS
	list_add(paginas,new_proceso);
}



t_pagina* crear_pagina(int PID, int pagina, int marco){
	t_pagina* new_pagina = (t_pagina*)malloc(sizeof(t_pagina));
	new_pagina->PID = PID;
	new_pagina->marco = marco;
	new_pagina->pagina = pagina;
	new_pagina->modificado = 0;
	return new_pagina;
}


void destruir_pagina(t_pagina* pagina){
	memoria[pagina->marco]->libre=1;
	free(pagina);
}

void destruir_proceso(t_proceso* proceso){
	gl_PID = proceso->PID;
	list_remove_by_condition(TLB,(void*)es_la_pagina_segun_PID);
	list_destroy_and_destroy_elements(proceso->paginas,(void*)destruir_pagina);
	free(proceso);
}

/*
void borrar_paginas_de_un_proceso_en_la_memoria(t_pagina* pagina){
	if(pagina->PID==gl_PID && pagina->entrada){
		list_remove_and_destroy_element(memoria,pagina->entrada,(void*)destruir_memoria);
	}
}

void compactar_lista_memoria(){
	t_list* aux = list_create(); //CREO LISTA COMO BKP
	list_add_all(aux,memoria);   //MUEVO TOD O LO QUE ESTA EN MEMORIA A LA AUX, ORDENANDOLO
	list_clean(memoria);		 //LIMPIO LA MEMORIA
	list_add_all(memoria,aux);	 //VUELVO A METER TOD O EN LA MEMORIA ORDENADO
}
*/

/*
void borrar_paginas_de_un_proceso_en_la_memoria(t_pagina* pagina){
	if(pagina->PID==gl_PID && pagina->entrada){
		list_remove_and_destroy_element(memoria,pagina->entrada,(void*)destruir_memoria);
	}
}

void compactar_lista_memoria(){
	t_list* aux = list_create(); //CREO LISTA COMO BKP
	list_add_all(aux,memoria);   //MUEVO TOD O LO QUE ESTA EN MEMORIA A LA AUX, ORDENANDOLO
	list_clean(memoria);		 //LIMPIO LA MEMORIA
	list_add_all(memoria,aux);	 //VUELVO A METER TOD O EN LA MEMORIA ORDENADO
}
*/
/*void eliminar_estructuras_de_un_proceso(t_proceso* proceso){
	int i;
	t_pagina_proceso pagina;

	// DEJO DISPONIBLE TODAS LAS ENTRADAS USADAS POR EL PROCESO QUE TIENE EN LA MEMORIA
	for(i=0;i<proceso->cant_paginas;i++){
		pagina = list_get(proceso->paginas,i);
		if(pagina.posicion_memoria!=-1){
				memoria[pagina.posicion_memoria].libre=1;
				memoria[pagina.posicion_memoria].modificado=0;
			}
		if(pagina.posicion_TLB!=-1){
				quitar_de_la_lista_de_prioridad_TLB(proceso->PID,i);
			}
	}

	free(proceso);
}

void quitar_de_la_lista_de_prioridad_TLB(int PID, int pagina){
	int i;
	for(i=0;i<ENTRADAS_TLB();i++){
		if(TLB[i].PID == PID && TLB[i].pagina == pagina){
			TLB[i].PID = -1;
			TLB[i].entrada = -1;
			TLB[i].orden_seleccion = 0;
			TLB[i].pagina = -1;
		} else
			TLB[i].orden_seleccion++;
	}
}

void eliminar_estructuras_de_todos_los_procesos(t_proceso* proceso){
	int i;
	t_pagina_proceso pagina;

	// DEJO DISPONIBLE TODAS LAS ENTRADAS USADAS POR EL PROCESO QUE TIENE EN LA MEMORIA
	for(i=0;i<proceso->cant_paginas;i++){
			pagina = list_get(proceso->paginas,i);
			if(pagina.posicion_memoria!=-1){
					memoria[pagina.posicion_memoria].libre=1;
					memoria[pagina.posicion_memoria].modificado=0;
				}
			if(pagina.posicion_TLB!=-1){
					quitar_de_la_lista_de_prioridad_TLB(proceso->PID,i);
				}
		}

	// LIBERO EL ESPACIO EN EL SWAP
	swap_finalizar(proceso->PID);

	free(proceso);
}

int escribir_pagina(int pid, int pagina, char* contenido){
	int estado;
	t_proceso proceso;
	int entrada;
	int pos_tlb;

	// LE MANDO AL SWAP PARA QUE GUARDE LA INFO
	estado = swap_escribir_pagina(pid, pagina, contenido);
	if(estado==-1) return estado;

	if(string_equals_ignore_case("FIFO",ALGORITMO_REEMPLAZO())){
		entrada = encontrar_pagina_libre_FIFO(pid);
	} else {
		//COMPLETAR CON EL RESTO DE LOS ALGORITMOS
		entrada = 0;
	}

	memoria[entrada].libre = 0;
	memoria[entrada].modificado = 0;
	memoria[entrada].bloque = contenido;

	entrada = encontrar_pagina_en_TLB();
	TLB[pos_tlb].PID = pid;
	TLB[pos_tlb].entrada = entrada;
	TLB[pos_tlb].pagina = pagina;
	TLB[pos_tlb].


	return 1;
}*/
