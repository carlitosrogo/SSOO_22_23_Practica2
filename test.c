#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include "parser.h"
#include <string.h>
#include <stdbool.h>

#define BUF 1024

typedef struct{
	char promp[BUF];
	int *pids;
}tBG;



// MAIN
int main(void){

	char buf[BUF];		//buffer line
	char *ruta;		//string para getenv
	char cwd[1024];		//string para getcwd
	tline *line;
	int i, j, l, fdin, fderr, fdout, nump, estado, largo;	//contadores, variabels para for, uso de numeracion variada
	
	int *comandos;
	tBG *backgraund;		//creamos un array de estructuras para que vaya guardando nuestros datos
	bool acabado, correcto;		//nos dice si un programa de backgraund ha acabado del todo o falta por ejecutarse algun programa (acabado=true; no acabdo = false)
	
	/* DB BACKGRAUND */
	//cremos un array para guardar el pid o los pids de los procesos que están en backgraund, al igual que su promp
	//se creara nuevo cada vez que se inicie la minishell, memoria incial = 4 procesos
	backgraund = malloc(4*sizeof(backgraund));	//cremos un array dinámico que en un principio tendrá espacio para 4 procesos en segundo plano	
	nump = 0;	//inicializar numero de procesos actuales en bg
	
	signal(SIGINT,SIG_IGN); //La minishell ignora el Ctrl+C

	/* PROGRAMA */
	printf("msh> ");
	while (fgets(buf, BUF, stdin)){

		signal(SIGINT,SIG_IGN); //La minishell ignora el Ctrl+C
		
		line = tokenize(buf);
		
		largo = strlen(buf);
		buf[largo-1] = ' ';
		
		/* BACKGRAUND */
		//Comprobamos que queden hijos (procesos) activos o no
		for(i=nump-1; i>=0; i--){		//recorremos los hijos de abajo arriba, esto ayuda que se si no hay ningun proceso en bg => i=nump-1=-1<0
			acabado = true;		//el proceso en un principio está acabado hasta que se demuestre lo contrario
			j=0;
			
			//comprobar que todos los procesos de esa ejecuación han acabado									
			while(backgraund[i].pids[j] != '\0'){	
				estado = waitpid(backgraund[i].pids[j], NULL, WNOHANG);	//comprobamos si su estado se ha actualzado
				if(estado != -1){					//en el caso de que su estado sea -1, significa que han acabado
					acabado = false;				//si se demuestra lo contrario, el procesa no puede estar acabado
				} 
				j++;
			}
			
			//en el caso de que todos los procesos de esa ejecucion hayan finalizados, se espera a que mueran y se ignoran en el array
			if(acabado == true){					//en el caso de que acabdo sea true, significa que han acabado
				printf("El trabajo %s ha acabado.\n",backgraund[i].promp);	//mostrasmos el proceso que ya ha acabado
				
				while(backgraund[i].pids[j] != '\0'){		//hacemos que el padre espere un momento a que los hijos mueran para que no se quede zombi una vez finalizado
					waitpid(backgraund[i].pids[j], 0, 0);			
				}
				
				for (l = i; l < nump - 1; l++) {			//recolocamos el array una posición cada casilla hacia arriba para ordenarlo
                    			backgraund[l] = backgraund[l+1];		
                		}
                		nump--;
			}
		}
		
		
		/* NO VÁLIDO */
		if(line == NULL || strcmp(buf,"\n") == 0){
			continue;
		}
		
		
		
		/* COMANDOS CREADOS */
		//SIGINT (CTRL+C)
		
		//JOBS
		else if(strcmp(line->commands[0].argv[0],"jobs") == 0){
		
			if(nump == 0){
				printf("No hay procesos en backgraund ejecutandose.\n");
			} 
			else{
				for(i=0; i<nump; i++){				//recorremos las casillas del array en función de los hijos que se hayan creado en backgraund
					printf("[%d]+ Running    %s \n",i+1,backgraund[i].promp);
				}
			}
		}
		
		//FG
		else if(strcmp(line->commands[0].argv[0],"fg") == 0){
			//si el comando fg no tiene argumentos (numero)
			if(line->commands[0].argv[1] == NULL){
				//si no hay procesos en el backgraund
				if(nump == 0){	
					printf("No hay procesos en backgraund ejecutandose.\n");
				}
				//si hay procesos en el backgraund
				else{
					printf("El trabajo %s esta en foregraund.\n",backgraund[nump-1].promp);
					j=0;
					
					while(backgraund[nump-1].pids[j] != '\0'){		//hacemos que el padre espere un momento a que los hijos acaben
						waitpid(backgraund[nump-1].pids[j], 0, 0);			
						j++;
					}
                			nump--;
				}
			}
			//si el comando fg tiene argumentos (numero)
			else{
				int pb = atoi(line->commands[0].argv[1]);	//obtenemso el número del proceso que queremos pasar a foregraund
				
				//si hay demasiado comandos con el fg
				if(line->commands[0].argc>2){
					printf("Demasiados argumentos para el fg.\n");
				}
				
				//si no hay procesos en el backgraund
				else if(nump == 0){
					printf("No hay procesos en backgraund ejecutandose.\n");
				}
				
				//si hay procesos en el backgraund y el numero es uno de los procesos
				else if(pb > 0 || pb < nump+2){
					printf("El trabajo %s esta en foregraund.\n",backgraund[pb-1].promp);
					j=0;
					
					while(backgraund[pb-1].pids[j] != '\0'){		//hacemos que el padre espere un momento a que los hijos acaben
						waitpid(backgraund[pb-1].pids[j], 0, 0);			
						j++;
					}
                			
                			for (l = pb-1; l < nump - 1; l++) {			//recolocamos el array una posición cada casilla hacia arriba para ordenarlo
                    				backgraund[l] = backgraund[l+1];		
                			}	
                			nump--;
				}
				
				//si el numero no está en los procesos 
				else{
					printf("Argumendo del fg mal introducido: %s.\n",line->commands[0].argv[1]);
				}
			}
		}
		
			
		//EXIT
		else if(strcmp(line->commands[0].argv[0],"exit") == 0){
			for(i=0; i<nump; i++){
				j=0;
				while(backgraund[i].pids[j] != '\0'){		//hacemos que el padre espere un momento a que los hijos acaben
					waitpid(backgraund[i].pids[j], 0, 0);			
					j++;
				}
			}
			
			exit(0);
		}
		
		//CD
		else if(strcmp(line->commands[0].argv[0],"cd") == 0){
			//solo comando cd
			if(line->commands[0].argc == 1){
				ruta = "HOME";		   //obtener path del HOME
				//la ruta existe
				if(chdir(getenv(ruta)) == 0){
					printf("Ruta actual: %s\n",getcwd(cwd,sizeof(cwd)));	//mostramos la ubicación actual
				}
				//la ruta no existe
				else{
					printf("Ruta inexistente.\n");
				}
				
			}
			//comando cd con ruta
			else if(line->commands[0].argc == 2){
				ruta = line->commands[0].argv[1];
				//la ruta existe
				if(chdir(ruta) == 0){							//cambiamos de ruta, en caso de que no de error
					printf("Ruta actual: %s\n",getcwd(cwd,sizeof(cwd)));		//mostramos la ruta actual a la que se ha cambiado
				}
				//la ruta no existe
				else{									//sino se muestra error por pantalla
					printf("Ruta inexistente.\n");
				}
									
			}
			//comando cd con errores en la ruta
			else{
				printf("Ruta indicada erronea.\n");
			}
		}
		



		/* FOREGRAUND Y BACKGRAUND***/
		else{	
			int p[line->ncommands - 1][2];
			pid_t pid;

			////* UN COMANDO FOREGRAUND Y BACKGRAUND *////
			if (line->ncommands == 1){

				// si el mandato no exite
				if (line->commands[0].filename == NULL){
					printf("mandato %s incorrecto: No se encuentra el mandato\n",line->commands[0].argv[0]);
				}
				
				// si el mandato exite
				else{
				
					//* UN COMANDO BACKGRAUND *//
					if(line->background){
					
						pid = fork();
						
						//error hijo
						if (pid < 0){	
							printf("Error con la creación del hijo\n");
						}
						
						//hijo
						if (pid == 0){	
							
							signal(SIGINT,SIG_DFL); //El proceso hijo vuelve a poder ser sensible a la señal Ctrl+C
						
							// sacamos la solución por un fichero
							if (line->redirect_input != NULL){
								fdin = open(line->redirect_input, O_RDONLY, 777);

								// en el caso de que el fichero exista
								if (fdin > 0){
									dup2(fdin, 0);
								}
								// en el caso de que el fichero no exista
								else{
									fprintf(stderr, "%s error: el fichero no existe.\n", line->redirect_input);
								}
							}
							// sacamos la solución por un fichero
							if (line->redirect_output != NULL){
								fdout = open(line->redirect_output, O_CREAT | O_WRONLY, 777);
								dup2(fdout, 1);
							}

							// sacamos la solución por un fichero error
							else if (line->redirect_error != NULL){
								fderr = open(line->redirect_error, O_CREAT | O_WRONLY, 777);
								dup2(fderr, 2);
							}
							
							execv(line->commands[0].filename, line->commands[0].argv);
						
						}
						
						//padre
						else{	
						
							signal(SIGINT,SIG_IGN); //El padre ignora el Ctrl+C
						
							//en el caso de que la memeoria del backgraund esté llena, aumentamos su espacio
							if(nump > 4){
								backgraund = realloc(backgraund, (nump+1)*sizeof(backgraund));
							}
						
							//metemos en este caso el unico pid y promp del exec en nuestro array
							backgraund[nump].pids = (int*) malloc(1*sizeof(int));	//damos espacio para un pid en este caso
							backgraund[nump].pids[0] = pid;				//metemos al array el pid del hijo
							strcpy(backgraund[nump].promp,buf);	//metemos al array lo que hay en buffer
							
							nump++;
						}
					} //fin un comando backgraund
				
				
					//* UN COMANDO FOREGRAUND *//
					else{
						pid = fork();
						
						/* proceso hijo */
						if (pid < 0){	//error hijo
							printf("Error con la creación del hijo\n");
						}
						else if (pid == 0){
						
							signal(SIGINT,SIG_DFL); //El proceso hijo vuelve a poder ser sensible a la señal Ctrl+C

							// sacamos la solución por un fichero
							if (line->redirect_input != NULL){
								printf("redirección de entrada: %s\n", line->redirect_input);
								fdin = open(line->redirect_input, O_RDONLY, 777);

								// en el caso de que el fichero exista
								if (fdin > 0){
									dup2(fdin, 0);
								}
								// en el caso de que el fichero no exista
								else{
									fprintf(stderr, "%s error: el fichero no existe.\n", line->redirect_input);
								}
							}
							// sacamos la solución por un fichero
							if (line->redirect_output != NULL){
								printf("redirección de salida: %s\n", line->redirect_output);
								fdout = open(line->redirect_output, O_CREAT | O_WRONLY, 777);
								dup2(fdout, 1);
							}

							// sacamos la solución por un fichero error
							else if (line->redirect_error != NULL){
								printf("redirección de salida error: %s\n", line->redirect_error);
								fderr = open(line->redirect_error, O_CREAT | O_WRONLY, 777);
								dup2(fderr, 2);
							}

							execv(line->commands[0].filename, line->commands[0].argv);
							exit(0);
						}//fin hijo

						/* proceso padre */
						else{
						
							signal(SIGINT,SIG_IGN); //El padre ignora el Ctrl+C
							
							waitpid(pid, 0, 0);
						}
						
					}//fin mandato foregraund
				}//fin si el mandato exite
			}//fin un comando



			////* MAS DE UN COMANDO FOREGRAUND Y BACKGRAUND *////
			else if (line->ncommands > 1){
				
				// Inicializar pipes
				for (l = 0; l < line->ncommands - 1; l++){
					pipe(p[l]);
				}
				
				
				
				//* MAS DE UN COMANDO BACKGRAUND *//
				if(line->background){
														
					correcto = true;		// se llevará el comando a backgraund hasta que se demuestre lo contrario
					comandos = (int*) malloc(line->ncommands*sizeof(int));	//damos espacio para cada pid de cada hijo
					
					// Creacion de los hijos y utilizacion de pipes
					for (i = 0; i < line->ncommands; i++){
						
						// si el mandato no exite
						if (line->commands[i].filename == NULL){
							printf("mandato %s incorrecto: No se encuentra el mandato\n",line->commands[i].argv[0]);
							correcto = false;	//como se demuestra lo contrario, el comando no se llevará a backgraund
						}
						
						// si el mandato exite
						else{

							pid = fork(); // creamos un hijo por cada comando

							/* error al crear al hijo */
							if (pid < 0){
								printf("Error con la creación del hijo\n");
							}

							/* proceso hijo */
							else if (pid == 0){
							
								signal(SIGINT,SIG_DFL); //El proceso hijo vuelve a poder ser sensible a la señal Ctrl+C

								// Primer elemento commands
								if (i == 0){
									dup2(p[i][1], 1); // enviamos por el pipe 1

									// sacamos la solución del pipe por un fichero
									if (line->redirect_input != NULL){ 
										printf("redirección de entrada: %s\n", line->redirect_input);
										fdin = open(line->redirect_input, O_RDONLY, 777);

										// en el caso de que el fichero exista
										if (fdin > 0){									
											dup2(fdin, 0);
										}
										// en el caso de que el fichero no exista
										else{
											fprintf(stderr, "%s error: el fichero no existe.\n", line->redirect_input);
											correcto = false;
										}
									}
								}

								// Ultimo elemento commands
								if (i == line->ncommands - 1){
									dup2(p[i - 1][0], 0); // recibimos por la entrada in la salida del pipe

									// sacamos la solución del pipe por un fichero
									if (line->redirect_output != NULL){
										printf("redirección de salida: %s\n", line->redirect_output);
										fdout = open(line->redirect_output, O_CREAT | O_WRONLY, 777);
										dup2(fdout, 1);
									}

									// sacamos la solución del pipe por un fichero error
									else if (line->redirect_error != NULL){
										printf("redirección de salida error: %s\n", line->redirect_error);
										fderr = open(line->redirect_error, O_CREAT | O_WRONLY, 777);
										dup2(fderr, 2);
									}
								}

								// Elemento intermedio commands
								else{
									dup2(p[i - 1][0], 0);
									dup2(p[i][1], 1);
								}

								// crerramos todos los pipes del hijo que va a hacer el exec
								for (j = 0; j < line->ncommands - 1; j++){
									close(p[j][0]);
									close(p[j][1]);
								}

								execv(line->commands[i].filename, line->commands[i].argv);
							} // fin hijos

							/* proceso padre */
							else{
							
								signal(SIGINT,SIG_IGN); //El padre ignora el Ctrl+C
							
								// cerramos primer hijo
								if (i == 0){
									close(p[i][1]);
								}
								// cerramos ultimo hijo
								else if (i == line->ncommands - 1){
									close(p[i][0]);
								}
								// cerramos intermedio hijo
								else{
									close(p[i - 1][0]);
									close(p[i][1]);
								}
							
								
								//metemos el pid 
								comandos[i] = pid;				//metemos al array el pid del hijo
									
							}
						} //fin si el mandato exite
					} // fin for hijos y padre
					
					if(correcto == true){
						//en el caso de que la memeoria del backgraund esté llena, aumentamos su espacio
						if(nump > 4){
							backgraund = realloc(backgraund, (nump+1)*sizeof(backgraund));
						}
						
						strcpy(backgraund[nump].promp, buf);
						backgraund[nump].pids = comandos;
						nump++;	
					}			
				}  //fin mas de un comando foregraund
				
				
				
				
				//* MAS DE UN COMANDO FOREGRAUND *//				
				else{
					// Creacion de los hijos y utilizacion de pipes
					for (i = 0; i < line->ncommands; i++){

						// si el mandato no exite
						if (line->commands[i].filename == NULL){
							printf("mandato %s incorrecto: No se encuentra el mandato\n",line->commands[i].argv[0]);
						}
						// si el mandato exite
						else{

							pid = fork(); // creamos un hijo por cada comando

							/* error al crear al hijo */
							if (pid < 0){
								printf("Error con la creación del hijo\n");
							}

							/* proceso hijo */
							else if (pid == 0){
							
								signal(SIGINT,SIG_DFL); //El proceso hijo vuelve a poder ser sensible a la señal Ctrl+C

								// Primer elemento commands
								if (i == 0){
									dup2(p[i][1], 1); // enviamos por el pipe 1

									// sacamos la solución del pipe por un fichero
									if (line->redirect_input != NULL){ 
										printf("redirección de entrada: %s\n", line->redirect_input);
										fdin = open(line->redirect_input, O_RDONLY, 777);

										// en el caso de que el fichero exista
										if (fdin > 0){									
											dup2(fdin, 0);
										}
										// en el caso de que el fichero no exista
										else{
											fprintf(stderr, "%s error: el fichero no existe.\n", line->redirect_input);
										}
									}
								}

								// Ultimo elemento commands
								if (i == line->ncommands - 1){
									dup2(p[i - 1][0], 0); // recibimos por la entrada in la salida del pipe

									// sacamos la solución del pipe por un fichero
									if (line->redirect_output != NULL){
										printf("redirección de salida: %s\n", line->redirect_output);
										fdout = open(line->redirect_output, O_CREAT | O_WRONLY, 777);
										dup2(fdout, 1);
									}

									// sacamos la solución del pipe por un fichero error
									else if (line->redirect_error != NULL){
										printf("redirección de salida error: %s\n", line->redirect_error);
										fderr = open(line->redirect_error, O_CREAT | O_WRONLY, 777);
										dup2(fderr, 2);
									}
								}

								// Elemento intermedio commands
								else{
									dup2(p[i - 1][0], 0);
									dup2(p[i][1], 1);
								}

								// crerramos todos los pipes del hijo que va a hacer el exec
								for (j = 0; j < line->ncommands - 1; j++){
									close(p[j][0]);
									close(p[j][1]);
								}

								execv(line->commands[i].filename, line->commands[i].argv);
							} // fin hijos

							/* proceso padre */
							else{
							
								signal(SIGINT,SIG_IGN); //El padre ignora el Ctrl+C
							
								// cerramos primer hijo
								if (i == 0){
									close(p[i][1]);
								}
								// cerramos ultimo hijo
								else if (i == line->ncommands - 1){
									close(p[i][0]);
								}
								// cerramos intermedio hijo
								else{
									close(p[i - 1][0]);
									close(p[i][1]);
								}
							}

							// esperamos para que los hijos no se queden zombis
							waitpid(pid, 0, 0);
						} //fin si el mandato exite
					} // fin for hijos y padre
				}// fin mas de un comando foregraund
			} // fin mas de un comando backgraund y foregraund
		}// fin solo comandos
		printf("msh> ");	
	}
	
	free(backgraund);
	return 0;
}
