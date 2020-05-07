#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio_ext.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>

#define TAM_BLC 65536
#define h_addr h_addr_list[0]

struct requisicao{
	int porta;
	char op[16];
	char nomeRemoto[128];
};

int socketControle = 0;
int socketDados = 0;
int socketD = 0;

int conectarServidor(char *nomeServidor, char *portaServidor);
int listarArquivos();
int receberArquivo(char *nomeLocal);
int enviarArquivo(char *nomeLocal);
int criarSocket(struct sockaddr_in *cliente, struct sockaddr_in *servidor);
int encerrarConexao(int s);
int requisitarOp(struct requisicao req);
int aceitarConexao(struct sockaddr_in cliente, struct sockaddr_in servidor);
void encerraCliente();
int verificaExistencia();

void main()
{
    char comando[255];
    char *op, *parametro1 = NULL, *parametro2 = NULL;
    struct requisicao req;
    struct sockaddr_in cliente; 
    struct sockaddr_in servidor;
    int rt = 0, liberar = 0, portaDados = 0;

    signal(SIGINT,encerraCliente);

    portaDados = criarSocket(&cliente, &servidor);

    while (1){

        printf("> ");
        __fpurge(stdin);
        //gets(comando);
        fgets(comando, sizeof(comando), stdin);
        comando[strlen(comando)-1] = '\0';

        op = strtok(comando, " ");
        parametro1 = strtok(NULL, " ");
        parametro2 = strtok(NULL, " ");

        if(parametro1 != NULL && parametro2 == NULL){
            parametro2 = malloc(strlen(parametro1)+1);
            strcpy(parametro2, parametro1);
            liberar = 1;
        }

        if (op && parametro1 && parametro2 && strcmp(op, "conectar") == 0){

            if(socketControle != 0){
                printf("ERRO: Voce já está conectado a um servidor. Pressione Enter para continuar....\n");
                getchar();
            }
            else if(conectarServidor(parametro1, parametro2)){
                printf("Conectado com sucesso! Pressione Enter para continuar...\n");
                getchar();
            }
            else{
                socketControle = 0;
                printf("ERRO: Não foi possível se conectar ao servidor. Pressione Enter para continuar....\n");
                getchar();
            } 
        }
        else if (op && parametro1 && strcmp(op, "enviar") == 0){
            strcpy(req.op, op);
            strcpy(req.nomeRemoto, parametro2);
            req.porta = portaDados;

            if(fopen(parametro1,"rb") && requisitarOp(req)){     
                socketDados = aceitarConexao(cliente, servidor);
                if(enviarArquivo(parametro1)){             
                    printf("Arquivo enviado. Pressione Enter para continuar...");
                    getchar();
                }
                else{
                    printf("ERRO: O arquivo não pode ser enviado. Pressione Enter para continuar...");
                    getchar();
                }
            }
            else{
                printf("ERRO: Não foi possivel enviar a requisição. Pressione Enter para continuar...");
                getchar();
            }

            encerrarConexao(socketDados);
        }
        else if (op && strcmp(op, "listar") == 0){
            strcpy(req.op, op);
            req.porta = portaDados;
            if(requisitarOp(req)){
                socketDados = aceitarConexao(cliente, servidor);
                if(listarArquivos()){
                    printf("Lista de arquivos recebida. Pressione Enter para continuar...");
                    getchar();
                }
                else{
                    printf("ERRO: Não foi possivel listar os arquivos. Pressione Enter para continuar...");
                    getchar();
                }
            }
            else{
                printf("ERRO: Não foi possivel enviar a requisição. Pressione Enter para continuar...");
                getchar();
            }

            encerrarConexao(socketDados);
        }
        else if (op && parametro1 && strcmp(op, "receber") == 0){
            strcpy(req.op, op);
            strcpy(req.nomeRemoto, parametro1);
            req.porta = portaDados;

            if(requisitarOp(req)){
                socketDados = aceitarConexao(cliente, servidor);
                recv(socketDados, &rt, sizeof(rt), 0);
                if(rt && receberArquivo(parametro2)){
                    printf("Arquivo recebido. Pressione Enter para continuar...");
                    getchar();
                }
                else{
                    printf("ERRO: O arquivo não pode ser baixado. Pressione Enter para continuar...");
                    getchar();
                }
            }
            else{
                printf("ERRO: Não foi possivel enviar a requisição. Pressione Enter para continuar...");
                getchar();
            }

            encerrarConexao(socketDados);
        }
        else if (op && strcmp(op, "encerrar") == 0){
            strcpy(req.op, op);

            if(requisitarOp(req)){
                if(encerrarConexao(socketControle)){
                    printf("Conexão com o servidor encerrada. Pressione Enter para continuar...");
                    socketControle = 0;
                    getchar();
                }
                else{
                    printf("ERRO: Voce não está conectado a um servidor. Pressione Enter para continuar...");
                    getchar();
                }
            }
            else{
                printf("ERRO: Não foi possivel enviar a requisição. Pressione Enter para continuar...");
                getchar();
            }          
        }
        else{
            printf("ERRO: Operação inválida. Pressione Enter para continuar...");
            getchar();
        }

        if(liberar){
            free(parametro2);
            liberar = 0;

            op = NULL;
            parametro1 = NULL;
            parametro2 = NULL;
        }

        system("clear");
    }
}

int requisitarOp(struct requisicao req){

    if (send(socketControle, &req, sizeof(req), 0) <= 0){
        return 0;
    }

    return 1;
}

int conectarServidor(char *nomeServidor, char *portaServidor)
{
    if(socketControle == 0){
        unsigned short port;
        struct hostent *hostnm;
        struct sockaddr_in server;

        hostnm = gethostbyname(nomeServidor);

        if (hostnm == (struct hostent *)0){
            return 0;
        }

        port = (unsigned short)atoi(portaServidor);

        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

        if ((socketControle = socket(PF_INET, SOCK_STREAM, 0)) < 0){
            return 0;
        }

        if (connect(socketControle, (struct sockaddr *)&server, sizeof(server)) < 0){
            return 0;
        }

        return 1;
    }
    else{
        return 0;
    }
}

int listarArquivos(){

    int bRecebidos;
    char listaArquivos[512];

    if ((bRecebidos = recv(socketDados, listaArquivos, sizeof(listaArquivos), 0)) <= 0){
        return 0;
    }

    printf("\n -- LISTA DE ARQUIVOS NO SERVIDOR --\n\n");
    printf("%s\n", listaArquivos);

    return 1;
}

int receberArquivo(char *nomeLocal){
    
    void *bloco;
    FILE *arquivoLocal;
    int bRecebidos = 0, tamanho;
    
    arquivoLocal = fopen(nomeLocal,"wb");
    
    bloco = malloc(TAM_BLC);
   
    if (recv(socketDados, &tamanho, sizeof(tamanho), 0) <= 0){
        return 0;
    }
    
    do{
        if ((bRecebidos = recv(socketDados, bloco, TAM_BLC, 0)) <= 0){
            return 0;
        }

        tamanho -= bRecebidos;

    }while(fwrite(bloco, 1, bRecebidos, arquivoLocal) >= 0 && tamanho > 0);

    fclose(arquivoLocal);
    free(bloco);

    return 1;
}

int enviarArquivo(char *nomeLocal){

    FILE *arquivoLocal;
    void *bloco;
    int tamanho, bLidos;

    if((arquivoLocal = fopen(nomeLocal,"rb")) <= 0){
        return 0;
    }

    fseek (arquivoLocal , 0 , SEEK_END);
    tamanho = ftell(arquivoLocal);
    rewind (arquivoLocal);

    bloco = malloc(TAM_BLC);

    if (send(socketDados, &tamanho, sizeof(int), 0) <= 0){
        return 0;
    }

    while(tamanho > 0 && (bLidos = fread(bloco, 1, TAM_BLC, arquivoLocal)) >= 0){

        if (send(socketDados, bloco, bLidos, 0) <= 0){
            return 0;
        }

        tamanho -= bLidos;  
    }
    
    fclose(arquivoLocal);
    free(bloco);

    return 1;
}

int criarSocket(struct sockaddr_in *cliente, struct sockaddr_in *servidor){

    unsigned short port; 
    struct sockaddr_in server;                    
    int namelen;

    if ((socketD = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        return 0;
    }

    server.sin_family = AF_INET;   
    server.sin_port   = 0;      
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketD, (struct sockaddr *) &server, sizeof(server)) < 0){
        return 0;
    }

    namelen = sizeof(server);
    if (getsockname(socketD, (struct sockaddr *) &server, &namelen) < 0){
        return 0;
    }

    if (listen(socketD, 1) != 0){
        return 0;
    }

    *servidor = server;

    return ntohs(server.sin_port);
}

int aceitarConexao(struct sockaddr_in cliente, struct sockaddr_in servidor){

    int ns;
    int namelen = sizeof(servidor);

    if ((ns = accept(socketD, (struct sockaddr *) &cliente, (socklen_t *) &namelen)) == -1){
        return 0;
    }

    return ns;
}

int encerrarConexao(int s){

    if(s != 0 && close(s) < 0){
        return 0;
    }

    return 1;
}

void encerraCliente(){

    struct requisicao req;
    strcpy(req.op, "encerrar");
    requisitarOp(req);

    encerrarConexao(socketControle);
    encerrarConexao(socketDados);
    encerrarConexao(socketD);

    system("clear");
    exit(1);
}