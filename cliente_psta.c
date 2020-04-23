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

#define TAM_BLC 65536

struct requisicao{
	int porta;
	char op[16];
	char nomeRemoto[128];
};


int s = 0;

int conectarServidor(char *nomeServidor, char *portaServidor);
int listarArquivos();
int receberArquivo(char *nomeLocal);
int enviarArquivo(char *nomeLocal);
int encerrarConexao();
int requisitarOp(struct requisicao req);

void main()
{
    char comando[255];
    char *op, *parametro1, *parametro2;
    int rt;
    struct requisicao req;

    while (1){

        printf("> ");
        __fpurge(stdin);
        gets(comando);

        op = strtok(comando, " ");
        parametro1 = strtok(NULL, " ");
        parametro2 = strtok(NULL, " ");

        if (strcmp(op, "conectar") == 0){
            rt = conectarServidor(parametro1, parametro2);

            if(rt == 0){
                printf("Conectado com sucesso! Pressione Enter para continuar...\n");
                getchar();
            }
            else if(rt < 0){
                printf("Erro! Voce ja esta conectado a um servidor. Pressione Enter para continuar....\\n");
                getchar();
            } 
        }
        else if (strcmp(op, "enviar") == 0){

            strcpy(req.op, op);
            strcpy(req.nomeRemoto, parametro2);

            if(requisitarOp(req))
                enviarArquivo(parametro1);
        }
        else if (strcmp(op, "encerrar") == 0){

            strcpy(req.op, op);

            if(requisitarOp(req))
                encerrarConexao();
        }
        else if (strcmp(op, "listar") == 0){
            listarArquivos();
        }
        else if (strcmp(op, "receber") == 0){
            receberArquivo(parametro1);
        }

        system("clear");
    } 
}

int requisitarOp(struct requisicao req){

    if (send(s, &req, sizeof(req), 0) <= 0){
        perror("Send()");
        return 0;
    }

    return 1;
}

int conectarServidor(char *nomeServidor, char *portaServidor)
{
    if(s == 0){
        unsigned short port;
        struct hostent *hostnm;
        struct sockaddr_in server;

        hostnm = gethostbyname(nomeServidor);

        if (hostnm == (struct hostent *)0){
            fprintf(stderr, "Gethostbyname failed\n");
            exit(2);
        }

        port = (unsigned short)atoi(portaServidor);

        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

        if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0){
            perror("Socket()");
            exit(3);
        }

        if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0){
            perror("Connect()");
            exit(4);
        }
    }
    else{
        return -1;
    }
}

int listarArquivos(){

}

int receberArquivo(char *nomeLocal){
    
}

int enviarArquivo(char *nomeLocal){

    FILE *arquivoLocal;
    void *bloco;
    int tamanho, bLidos;

    arquivoLocal = fopen(nomeLocal,"rb");

    fseek (arquivoLocal , 0 , SEEK_END);
    tamanho = ftell(arquivoLocal);
    rewind (arquivoLocal);

    bloco = malloc(TAM_BLC);

    if (send(s, &tamanho, sizeof(int), 0) <= 0){
        perror("Send()");
        exit(1);
    }

    while(tamanho > 0 && (bLidos = fread(bloco, 1, TAM_BLC, arquivoLocal)) >= 0){

        if (send(s, bloco, bLidos, 0) <= 0){
            perror("Send()");
            exit(1);
        }

        tamanho -= bLidos;  
    }
    
    fclose(arquivoLocal);
    free(bloco);

    printf("Arquivo enviado. Precione Enter para continuar...");
    getchar();
}

int encerrarConexao(){

    close(s);
    exit(0);
}