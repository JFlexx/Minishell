
#include <stddef.h>			/* NULL */
#include <stdio.h>			/* setbuf, printf */
#include <stdlib.h>
#include <unistd.h>			/* execvp */
#include <sys/wait.h>		/* wait para el padre */
#include <sys/stat.h> 		/* O_rdonly...... */
#include <fcntl.h>			/* O_rdonly....... */
#include <stdbool.h>		/* Booleanos */
#include <string.h>			/* para el error de strcmp */
#include <sys/times.h>		/* times */

extern int obtain_order();		/* See parser.y for description */

int main(void) {
	char ***argvv = NULL;
	int argvc;
	char **argv = NULL;
	int argc;
	char *filev[3] = { NULL, NULL, NULL };
	int bg;
	int ret;


	/**** variables creada por mi ****/
	int fd_in, fd_out,fd_error; //descriptor in,out y error, cuando se realice Open:)
	//int dupError; //para el control de errores al usar dup
	int valor_out;
	int valor_in;
	int valor_error;// guardar el valor de descriptores antes cambiarlos y poder restaurarlos
	int bgpid;// cuando exista background, realizaremos getppid() y mostraremos con el formato 
	int pidTimes;// para el fork de time()

	clock_t tInicio;//variable para time
	clock_t tFinal;//variable para time
	struct tms guardaTime, guardaTime2;//variable para time
	long tickRelojSegundo= sysconf(_SC_CLK_TCK);
	clock_t tiempoUsuario;//recogera el tiempo del usuario
	clock_t tUsuarioMili;// recogera el tiempo del usuario en milisegundos
	clock_t tiempoSistema;// recogera el tiempo del sistema
	clock_t tsistemaMili;// recogera el tiempo del sistema en milisegundos
	clock_t tiempoReal;//recogera el tiempo real
	clock_t tRealMili;//recogera el tiempo real en milisegundos
	/*********************************/

	/**------------------------> Empezando con control de seniales**/
	//no deben morir por las seniales  (SIGINT, SIGQUIT) 
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);


	//no tocar
	setbuf(stdout, NULL);			/* Unbuffered */
	setbuf(stdin, NULL);


	while (1) {
		fprintf(stderr, "%s", "msh> ");				/* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		if (ret == 0) break;						/* EOF */
		if (ret == -1) continue;					/* Syntax error */
		argvc = ret - 1;							/* Line */
		if (argvc == 0) continue;					/* Empty line */
		//ejemplos ls -l --> argvv[0]= ls -l ; argv[0]= argvv[0]--> argv[0]=ls, argv[1]=-l
		
		/* entendiendo redireccionamiento */	
		// if (filev[0]) printf("< %s\n", filev[0]);/* IN ejemplos cat < .aux.txt */
		// if (filev[1]) printf("> %s\n", filev[1]);/* OUT ls > .aux.txt */
		// if (filev[2]) printf(">& %s\n", filev[2]);/* ERR */
		// if (bg) printf("&\n");

		int numeroMandatos;//para guardar el número de mandatos
		numeroMandatos= argvc;//mandatos totales= argvc y  pipes= argvc-1
		
		bool mandatoInterno;//para controlar cuando haya un mandatoInterno
		mandatoInterno= false;
		


		//------> redireccion de entrada
		if(filev[0])
		{// siguiendo el código de las transparencias de clase
			fd_in= open(filev[0], O_RDONLY);//abriendolo para lectura

			if(fd_in<0){perror("Error en open");continue;}//solucionado: quitar exit ya que es un error de usuario, si no pongo "continue"
															// se sale y se ejecuta el remove en las pruebas borrando el fichero cuando no debería

			valor_in= dup(0);//guardo el descriptor de entrada para despues restaurar
			close(0);//cierro el descriptor de entrada que es el 0
			dup(fd_in);// copio el descriptor en el 0
			close(fd_in);//cierro, ya no lo necesito
			dup2(fd_in,0);
			close(fd_in);
		}

		//--> redireccion de salida 
		if (filev[1])
		{
			
			fd_out= open(filev[1], O_CREAT | O_TRUNC | O_WRONLY, 0666);//Si el fichero no existe se crea, si existe se trunca , modo de creacion 0666.

			if(fd_out<0){perror("Error en open"); continue;}//no hacemos exit antes errores provocados por usuarios

			valor_out= dup(1);
			close(1);//cerramos el descriptor 1
			dup(fd_out);// redireccionamos a la salida el fd
			close(fd_out);//cerramos por se acaso
			dup2(fd_out,1);
			close(fd_out);
		}
		
		//--> redireccion de error
		if(filev[2])
		{
			//igual que en redireccion de salida según el enunciado
			fd_error= open(filev[2], O_CREAT| O_TRUNC | O_WRONLY, 0666);

			if(fd_error<0){perror("Error en  open"); continue;}

			valor_error= dup(2);
			close(2);
			dup(fd_error);
			close(fd_error);
		}

		
		/************************************************************ Empezando con mandatos Internos ****************************************/
		// printf("%d\n",argc);
		//--> Mandato Interno cd: Cambia el directorio por defecto. Si aparece Directorio
		// 	  debe cambiar al mismo. Si no aparece, cambia al directorio especificado en la variable
		//	  de entorno HOME. Presenta (por la salida est´andar) como resultado el camino absoluto
		//	  al directorio actual de trabajo (man getcwd) con el formato: "%s\n".
		// --> empezando cd 
		// --> empezando umask
		// --> empezando time
		// --> empezando read... sin tiempo
		if (strcmp(argvv[0][0], "cd")==0 || strcmp(argvv[0][0], "umask")==0 || strcmp(argvv[0][0], "time")==0   /* || strcmp(argvv[0][0], "read")==0 */){mandatoInterno = true;}
		// printf("mand %d\n", mandatoInterno);
		if(mandatoInterno) {
			// printf("interno %d\n", mandatoInterno);
			argv= argvv[0];
			for (argc = 0; argv[argc]; argc++){// se necesita controlar el número de argumentos
				
				if (strcmp(argvv[0][0], "cd")==0)
				{
					// con argc representaremos el número de argumentos
					// printf("argc= %d\n",argc);
					//si hay solo un argumento argc = 0, entonces...
					int maxlen= 512;
					char buffCd[maxlen];
					

					if(argv[1])// significa que se especifica directorio
					{
						//-- >debemos cambiar al [Directorio] especificado
						//chdir (const char * ruta);
						// printf("con directorio\n");
						int errorChdir;
						errorChdir= chdir(argv[argc +1]);
						if(errorChdir < 0){// Para la prueba donde se debe notificar el error
							perror("No existe el directorio especificado\n");
							break;
						}

						//char *_getcwd( char *buffer, int maxlen)
						getcwd(buffCd, maxlen);
						// printf("Demasiados arguementos cd [Directorio]\n");// segun las pruebas no hace falta controlar esto


					}

					else if(!argv[1]){// significa que no se espefica directorio
						// printf("sin directorio\n");
						//sse necesita notifica en caso de error
						chdir(getenv("HOME"));
						getcwd(buffCd, maxlen);

					}
					printf("%s\n", buffCd);
					break;


				}

				/**
				 * @brief Resultado el valor de la actual mascara con el formato:
				 * "%o\n". Ademas, si aparece Valor (dado en octal, man strtol), cambia la mascara
				 * a dicho valor indicado.
				 * 
				 */
				if(strcmp(argvv[0][0], "umask")==0)
				{
					// printf("entro aqui");
					if(argv[2])// si hay un segundo argumento, error
					{
						perror("Supera el nº de argumentos\n");
						break;//salgo 
					}
					
					//Si con tiene umask [Valor]
					else if(argv[1])
					{//cuando tiene valor

						//long int strtol(const char *str, char **endptr, int base) 
						// es necesario que el argumento valor sea octal, se hace uso de strtol como se indica en la guia
						unsigned int mascara= strtol(argv[argc +1], NULL, 8);
						if(mascara <1){
							perror("Error argumento erroneo\n");
							break;
						}
						umask(mascara);
						printf("%o\n", mascara);
						break;
						

					}

					//cuando no tiene valor, es decir, sin argumentos
					else if(!argv[1])
					{

						//obtenemos la antigua máscara
						mode_t antiguaMascara;
						//https://www.ibm.com/docs/en/zos/2.2.0?topic=functions-umask-set-retrieve-file-creation-mask en esta doc 
						//explica como obtener la mascara actual en el "ejemplo CELEBU01" de dicha doc
						antiguaMascara= umask(0666);//todos los permisos creo, que con 0660 basta
						printf("%o\n", antiguaMascara);
						umask(antiguaMascara);//establezco la mascara actual
						break;

					}

				}//umask
				
				/**
				 * @brief NOTAIMPORTANTE: Es la única manera que recuerda y busqué en su momento documentacion para relizar esta parte de time
				 * 				no dispongo ya del fichero copiado de la convocatoria ordinaria.	
				 * 				Con esto quiero decir que en caso de que la metodología sea igual que la anterior práctica es porque 
				 * 				fue la única forma que entendí para realizat time(),además de que el año pasado no existía este mandato
				 * 
				 */
				if (strcmp(argvv[0][0], "time")==0)
				{
					// printf("Entro por mandato time\n");

					pidTimes= fork();
					tInicio= times(&guardaTime);

					if (pidTimes < 0)
					{
						perror("Error en fork()");
						exit(1);
					}
					else if (pidTimes == 0)
					{// estamos en hijo
						//ejecutamos mandato times
						if(execvp(argv[argc+1], &argv[argc+1]) < 0)
						{
							perror("Error en execvp");
							exit(1);
						}

					}
					else
					{// estamos en padre
						//realizamos el siguiente timea para obtener el tiempo final cuando se termine de ejecutar el mandato
						tFinal = times(&guardaTime2);
						wait(NULL);

						tiempoUsuario = ((guardaTime2.tms_utime - guardaTime.tms_utime) + (guardaTime2.tms_cutime - guardaTime.tms_cutime)) / tickRelojSegundo;
						tUsuarioMili = tiempoUsuario * 1000000;// paso a milisegundos 

						tiempoSistema = ((guardaTime2.tms_stime - guardaTime.tms_stime) + (guardaTime2.tms_cstime - guardaTime.tms_cstime)) /tickRelojSegundo;
						tsistemaMili = tiempoSistema * 1000000;// paso a milisegundos

						tiempoReal = (0- 0) /tickRelojSegundo;tRealMili = tiempoReal * 1000000;
						printf("%d.%03du %d.%03ds %d.%03dr\n", (int)tiempoUsuario, (int)tUsuarioMili, (int)tiempoSistema, (int)tsistemaMili, (int)tiempoReal, (int)tRealMili);
					
						break;

					}
					

				}//time
				
				/** Sin terminar **/
				// if (strcmp(argvv[0][0], "read")==0){
					// printf("entro aqui");
					// if(!argv[1])// error cuando no se le pasa nombre de variables 
					// {
					// 	perror("Ingrese el nombre de variable\n");
					// 	break;//salgo 
					// }
					// char * delimitadores= " \t";//tabulador
					// char line[512];
					// char * lecturaLinea= strtok(linea, delimitadores);
					// while()
					// {// while para recorrer la lectura de la linea
					// 	break;
					// }

				// }

			}

			// printf("Probando si entra aqui con mandato: %s\n", argvv[0][0]);
		}//-------->Fin mandatos internos


		/******************************************************************* un mandato o varios mandatos *********************************************/
		// printf("ddas\n");
		/** -----------------> Empezando con background **/
		if(!bg && !mandatoInterno) {// si no hay mandatos en segundos plano y no se trata de mandatos internos
			//las volvemos a dejar por defecto
			// printf("Entro aqui\n");
			//-------------------------------> cuando existe un solo mandato
			if(numeroMandatos ==1){
				// printf("un solo mandato\n");

				pid_t pid= fork();
				if(pid < 0){
					perror("Error en fork()");
					exit(1);
				}

				else if(pid ==0){//estamos en hijo
					signal(SIGINT, SIG_DFL);//señales por defecto
					signal(SIGQUIT, SIG_DFL);
					if ((execvp(argvv[0][0], argvv[0])) < 0)
					{
						perror("Error en execvp()");
					}
				}

				else{//estamos en padre 
					signal(SIGINT, SIG_IGN);//ignora si le llegan estas señales
					signal(SIGQUIT, SIG_IGN);

					wait(NULL);
					
				}

			}//un solo mandato

			//-----------------------------> cuando existen varios mandatos: numeroMandatos > 1
			else{
				pid_t pidMandatos;
				int guardaLectura;
				int fdPipe[2];

				guardaLectura =0;
				
				pidMandatos= fork();
				
				if(pidMandatos < 0)
				{// lanzo error del fork
					perror("Error en fork de multiples mandatos\n");
					exit(1);
				}

				if (pidMandatos == 0) 
				{//estamos en hijo Pipe

					/*******************************************/
					/**En las pruebas no se tiene en cuenta la 
					 * muerte por las seniales SIGINT y SIGQUIT para varios mandatos
					 * por tanto, usaré lo mismo que si fuera un solo mandato, señal poor defecto
					/*******************************************/
					signal(SIGINT, SIG_DFL);
					signal(SIGQUIT, SIG_DFL);


					//printf("argvc %d\n", argvc);
					/**********************************************
					 * https://www.youtube.com/watch?v=8LdQ09Ep9RY*
					 * Para los pipes se ha seguido la explicación
					 * de este video, donde realiza la comunicacion
					 * con el ultimo pipe a partir del minuto 9:22 de dicho video
					 * Donde se explica la estructura de los pipes
					 * ********************************************/
					for(argvc = 1; (argv = argvv[argvc]); argvc++)
					{//bucle proporcionado en el código de apoyo 

						//printf("Entro al bucle\n");
						if(pipe(fdPipe) <0)// tantos pipes como (mandatos -1  ) existan
						{//controlo error del pipe
							perror("Error en pipe\n");
							exit(1);
						}

						pid_t mandatosPid;
						mandatosPid = fork();
						if(mandatosPid<0){
							perror("Error en fork() de pipes\n");
							exit(1);
						}

						if (mandatosPid  == 0) 
						{// estamos en el hijo

							// printf("fdPipe[0]=%d\n", fdPipe[0]);
							// printf("in=%d\n", guardaLectura);
							if (guardaLectura!= 0)
							{//entrará apatir del segundo mandatos para comunicar los procesos y leer por el STDIN
								// printf("entro\n");
								dup2(guardaLectura, STDIN_FILENO);// Leemos del STDIN
								close(guardaLectura);// ya no se necesita
							}

							// printf("fdPipe[1]= %d\n", fdPipe[1]);
							if (fdPipe[1] != 0) 
							{//entrará por primera vez para escribir y enviarlos por la salida STDOUT
								// printf("entro aqui\n");
								dup2(fdPipe[1], STDOUT_FILENO);// Escribimos del STADOUT
								close(fdPipe[1]);//cerramos porque ya no lo necesitamos
							}

							execvp(argvv[argvc-1][0],argvv[argvc-1]);
						}
					
						close(fdPipe[1]);
						guardaLectura = fdPipe[0];// guardamos el descriptor de lectura

					}//fin bucle que recorre los pipes

					if (guardaLectura != 0)
					{
						dup2(guardaLectura, STDIN_FILENO);// leemos del STDIN el último mandato 
					}


					// printf("Valor de argvc: argvc: %d\n", argvc);
					if(execvp(argvv[argvc-1][0], argvv[argvc-1]) == -1)
					{// realizamos el execvp del último mandato
						perror("Error en execvp");
						exit(1);
					}
					
				}
				signal(SIGINT, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
				//estamos en padre; como ha terminado el bucle solo esperar al último mandato
				wait(NULL);// con esto nos aseguramos que solo esperamos al último proceso
	


			}//varios mandatos

		}//if de mandatos o solo un mandato



		/************************************************************************************ Background ********************************************************************/
		/**
		 * @brief -Un mandato o secuencia terminado en ‘&’ supone la ejecucion asıncrona en segundo plano del mismo, 
		 * - Minishell no queda bloqueado 
		 * - En cambio, actualiza el valor de la variable bgpid con el fomato "[%d]\n".
		 */
		//----------------------------> empezando con background
		if (bg && !mandatoInterno)// cuando hay mandatos en segundo plano sin ser internos
		{//realizo lo mismo que cuando habia un solo mandato, pero bloqueando las seniales
			

			pid_t pid= fork();
			if(pid < 0)
			{
				perror("Error en fork()");
				exit(1);
			}

			else if(pid ==0)
			{//Estamos en hijo
				//no deben morir por las seniales  (SIGINT, SIGQUIT) 
				signal(SIGINT, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
				pid_t pid2= fork();
				if(pid2 <0)
				{//control de errores
					perror("Error en fork()");
					exit(1);
				}

				if(pid2 ==0)
				{//Estamos en padre hijo
					bgpid= getppid();
					printf("[%d]\n", bgpid);

					if ((execvp(argvv[0][0], argvv[0])) < 0)
					{
						perror("Error en execvp()");
						exit(1);
					}

				}
				else
				{//Estamos en el padre
					//las volvemos a dejar por defecto
					signal(SIGINT, SIG_DFL);
					signal(SIGQUIT, SIG_DFL);
					//no es necesario esperar
					exit(0);
					/*****************************NOESPERAMOS *****************************/
				}
			}

			else
			{//Estamos en padre 
				//esperamos al bg
				wait(NULL);
			}

		}

		//---------------------> restaura entrada		
		if(filev[0])
		{
			//printf("restaurando entrada");
			//cerramos el decriptor del fichero 
			
			dup2(valor_in, 0);//restauro gracias a la variable que guardo el valor de entrada
			close(valor_in);//cierro sino da error de descriptores abiertos IMPORTANTE en ./freefds


		}

		//---------------------> restaura salida	
		if(filev[1])
		{
			//printf("restaurando salida");
			//cerramos el decriptor del fichero 
			
			dup2(valor_out, 1);//restauro gracias a la variable anterior
			close(valor_out);
			
		}	
		
		//---------------------> restaura error	
		if(filev[2])
		{
			
			dup2(valor_error, 2);
			close(valor_error);
		}


	}// while
	exit(0);
	return 0;
}
