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
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#define TAM_BLC 65536
#define PASTA "Arquivos/"

struct parametro{
	int ns;
	struct sockaddr_in cliente;
};

struct requisicao{
	int porta;
	char op[16];
	char nomeRemoto[128];
};

void *atenderCliente(void *parametros);
int encerrarConexao();
int listarArquivos(struct parametro clienteInfo);
int enviarArquivo();
int receberArquivo(struct parametro clienteInfo, char *nomeRemoto);

void main(){
    
    //unsigned short port;                   
    struct sockaddr_in client; 
    struct sockaddr_in server; 
    int s, ns, namelen, tp;
    pthread_t thread;
    struct parametro parametros;

    //port = (unsigned short)7777;

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0){
	  perror("Socket()");
	  exit(2);
    }

    server.sin_family = AF_INET;   
    server.sin_port   = 0;//htons(port);       
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("Bind()");
        exit(3);
    }

    namelen = sizeof(server);
    if (getsockname(s, (struct sockaddr *) &server, &namelen) < 0){
        perror("getsockname()");
        exit(1);
    }

    if (listen(s, 1) != 0){
        perror("Listen()");
        exit(4);
    }

    printf("Servidor aberto na porta %d.\n", ntohs(server.sin_port));

    while(1){

        if ((ns = accept(s, (struct sockaddr *) &client, (socklen_t *) &namelen)) == -1){
            perror("Accept()");
            exit(5);
        }
 
        parametros.ns = ns;
        parametros.cliente = client;

        tp = pthread_create(&thread, NULL, &atenderCliente, (void*)&parametros);

        if (tp) {
            printf("ERRO: impossivel criar um thread\n");
            exit(-1);
        }

        printf("Thread atendente criada\n");	

        pthread_detach(thread);
    };
    
    close(s);
}

void *atenderCliente(void *parametros){

    struct requisicao req;
    int ns = ((struct parametro*)parametros)-> ns;

    while(1){

        if (recv(ns, &req, sizeof(req), 0) <= 0){
            perror("Recv()");
            exit(6);
        }

        if (strcmp(req.op, "enviar") == 0){
            receberArquivo(*((struct parametro*)parametros), req.nomeRemoto);
        }
        else if (strcmp(req.op, "encerrar") == 0){
            break;            
        }
        else if (strcmp(req.op, "listar") == 0){
            listarArquivos(*((struct parametro*)parametros));
        }
        else if (strcmp(req.op, "receber") == 0){
            enviarArquivo(*((struct parametro*)parametros), req.nomeRemoto);
        }
    }

    printf("Thread encerrada.\n");
    pthread_exit(NULL);
}

int listarArquivos(struct parametro clienteInfo){

    char chamada[128] = "ls ";
    char listaArquivos[512];
    FILE *shell = NULL;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);
    strcat(chamada, PASTA);

    shell = popen(chamada,"r");
	fread(listaArquivos,sizeof(char),512,shell);
    pclose(shell);

    if (send(clienteInfo.ns, listaArquivos, sizeof(listaArquivos), 0) <= 0){
        perror("Send()");
        return 0;
    }
}

int enviarArquivo(struct parametro clienteInfo, char *nomeRemoto){
    
    FILE *arquivoRemoto;
    void *bloco;
    int tamanho, bLidos;
    char *caminhoArquivo;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);

    caminhoArquivo = (char*)malloc(sizeof(char)*128);
    strcat(caminhoArquivo, PASTA);
    strcat(caminhoArquivo, nomeRemoto);
    arquivoRemoto = fopen(caminhoArquivo,"rb");

    fseek (arquivoRemoto , 0 , SEEK_END);
    tamanho = ftell(arquivoRemoto);
    rewind (arquivoRemoto);

    bloco = malloc(TAM_BLC);
    
    if (send(clienteInfo.ns, &tamanho, sizeof(tamanho), 0) <= 0){
        perror("Send()");
        exit(1);
    }
    
    while(tamanho > 0 && (bLidos = fread(bloco, 1, TAM_BLC, arquivoRemoto)) >= 0){

        if (send(clienteInfo.ns, bloco, bLidos, 0) <= 0){
            perror("Send()");
            exit(1);
        }

        tamanho -= bLidos;  
    }
    
    fclose(arquivoRemoto);
    free(bloco);
    free(caminhoArquivo);
}

int receberArquivo(struct parametro clienteInfo, char *nomeRemoto){

    void *bloco;
    FILE *arquivoRemoto;
    int bRecebidos = 0, tamanho;
    char *caminhoArquivo;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);

    caminhoArquivo = (char*)malloc(sizeof(char)*128);
    strcat(caminhoArquivo, PASTA);
    strcat(caminhoArquivo, nomeRemoto);
    arquivoRemoto = fopen(caminhoArquivo,"wb");

    bloco = malloc(TAM_BLC);

    if (recv(clienteInfo.ns, &tamanho, sizeof(tamanho), 0) <= 0){
        perror("Recv()");
        exit(6);
    }

    do{
        if ((bRecebidos = recv(clienteInfo.ns, bloco, TAM_BLC, 0)) <= 0){
            perror("Recv()");
            exit(6);
        }

        tamanho -= bRecebidos;

    }while(fwrite(bloco, 1, bRecebidos, arquivoRemoto) >= 0 && tamanho > 0);

    fclose(arquivoRemoto);
    free(bloco);
    free(caminhoArquivo);

    printf("Recebido o arquivo do endereco IP %s da porta %d\n", inet_ntoa(clienteInfo.cliente.sin_addr), ntohs(clienteInfo.cliente.sin_port));
}