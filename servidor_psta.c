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
#include <sys/syscall.h>
#include <signal.h>

#define PORT 7000
#define TAM_BLC 65536
#define PASTA "Arquivos/"
#define h_addr h_addr_list[0]

struct parametro{
	int socketControle;
	struct sockaddr_in cliente;
};

struct requisicao{
	int porta;
	char op[16];
	char nomeRemoto[128];
};

void *atenderCliente(void *parametros);
int listarArquivos(struct parametro clienteInfo, int socketDados);
int enviarArquivo(struct parametro clienteInfo, char *nomeRemoto, int socketDados);
int receberArquivo(struct parametro clienteInfo, char *nomeRemoto, int socketDados);
int encerrarConexao(int s);
int conectarCliente(struct sockaddr_in cliente, int portaCliente);
int verificaExistencia(int socketDados, char *nomeArquivo);
void encerraServidor();

int socketControle = 0;
int socketAux = 0;

void main(){
                    
    struct sockaddr_in client; 
    struct sockaddr_in server; 
    int namelen, tp;
    pthread_t thread;
    struct parametro parametros;
    
    signal(SIGINT, encerraServidor);

    if ((socketControle = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        exit(0);
    }

    server.sin_family = AF_INET;   
    server.sin_port   = htons(PORT);      
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketControle, (struct sockaddr *)&server, sizeof(server)) < 0){
        exit(0);
    }

    if (listen(socketControle, 1) != 0){
        exit(0);
    }

    printf("Servidor aberto na porta %d.\n", ntohs(server.sin_port));

    while(1){

        if ((socketAux = accept(socketControle, (struct sockaddr *) &client, (socklen_t *) &namelen)) == -1){
            printf("ERRO: impossivel aceitar conexão.\n");
            exit(0);
        }
 
        parametros.socketControle = socketAux;
        parametros.cliente = client;

        tp = pthread_create(&thread, NULL, &atenderCliente, (void*)&parametros);

        if (tp) {
            printf("ERRO: impossivel criar thread.\n");
            exit(0);
        }

        printf("Thread atendente criada.\n");	
        pthread_detach(thread);
    };
    
    close(socketControle);
}

void *atenderCliente(void *parametros){

    struct requisicao req;
    int socketControle = ((struct parametro*)parametros)-> socketControle;
    int socketDados = 0;

    while(1){

        if (recv(socketControle, &req, sizeof(req), 0) <= 0){
            return 0;
        }

        if (strcmp(req.op, "enviar") == 0){
            socketDados = conectarCliente(((struct parametro*)parametros)-> cliente, req.porta);
            if(receberArquivo(*((struct parametro*)parametros), req.nomeRemoto, socketDados)){
                printf("Recebido arquivo do endereco IP %s porta %d\n", inet_ntoa(((struct parametro*)parametros)-> cliente.sin_addr), req.porta);
            };
            encerrarConexao(socketDados);
        }
        else if (strcmp(req.op, "listar") == 0){
            socketDados = conectarCliente(((struct parametro*)parametros)-> cliente, req.porta);
            if(listarArquivos(*((struct parametro*)parametros), socketDados)){
                printf("Enviada lista de arquivos para o IP %s porta %d\n", inet_ntoa(((struct parametro*)parametros)-> cliente.sin_addr), req.porta);
            }
            encerrarConexao(socketDados);
        }
        else if (strcmp(req.op, "receber") == 0){
            socketDados = conectarCliente(((struct parametro*)parametros)-> cliente, req.porta);
            if(verificaExistencia(socketDados, req.nomeRemoto) && enviarArquivo(*((struct parametro*)parametros), req.nomeRemoto, socketDados)){
                printf("Enviado arquivo para o IP %s porta %d\n", inet_ntoa(((struct parametro*)parametros)-> cliente.sin_addr), req.porta);
            }
            encerrarConexao(socketDados);
        }
        else if (strcmp(req.op, "encerrar") == 0){
            printf("O IP %s desconectou.\n", inet_ntoa(((struct parametro*)parametros)-> cliente.sin_addr));
            break;            
        }
    }

    printf("Thread atendente encerrada.\n");
    pthread_exit(NULL);
}

int verificaExistencia(int socketDados, char *nomeArquivo){

    int rt;
    char *caminhoArquivo;

    caminhoArquivo = (char*)malloc(sizeof(char)*128);
    bzero(caminhoArquivo, sizeof(caminhoArquivo));
    strcat(caminhoArquivo, PASTA);
    strcat(caminhoArquivo, nomeArquivo);

    if(fopen(caminhoArquivo, "rb") <= 0){
        rt = 0;
    }
    else{
        rt = 1;
    }

    send(socketDados, &rt, sizeof(rt), 0);
    free(caminhoArquivo);

    return rt;
}

int listarArquivos(struct parametro clienteInfo, int socketDados){

    char chamada[128] = "ls ";
    char listaArquivos[512];
    FILE *shell = NULL;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);
    strcat(chamada, PASTA);

    shell = popen(chamada,"r");
    bzero(listaArquivos, sizeof(listaArquivos));
	fread(listaArquivos,sizeof(char),512,shell);
    pclose(shell);


    if (send(socketDados, listaArquivos, sizeof(listaArquivos), 0) <= 0){
        return 0;
    }
}

int enviarArquivo(struct parametro clienteInfo, char *nomeRemoto, int socketDados){
    
    FILE *arquivoRemoto;
    void *bloco;
    int tamanho, bLidos;
    char *caminhoArquivo;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);

    caminhoArquivo = (char*)malloc(sizeof(char)*128);
    bzero(caminhoArquivo, sizeof(caminhoArquivo));
    strcat(caminhoArquivo, PASTA);
    strcat(caminhoArquivo, nomeRemoto);
    if((arquivoRemoto = fopen(caminhoArquivo,"rb")) <= 0){
        free(caminhoArquivo);
        return 0;
    }

    fseek (arquivoRemoto , 0 , SEEK_END);
    tamanho = ftell(arquivoRemoto);
    rewind (arquivoRemoto);

    bloco = malloc(TAM_BLC);
    
    if (send(socketDados, &tamanho, sizeof(tamanho), 0) <= 0){
        return 0;
    }
    
    while(tamanho > 0 && (bLidos = fread(bloco, 1, TAM_BLC, arquivoRemoto)) >= 0){

        if (send(socketDados, bloco, bLidos, 0) <= 0){
            return 0;
        }

        tamanho -= bLidos;  
    }
    
    fclose(arquivoRemoto);
    free(bloco);
    free(caminhoArquivo);

    return 1;
}

int receberArquivo(struct parametro clienteInfo, char *nomeRemoto, int socketDados){

    void *bloco;
    FILE *arquivoRemoto;
    int bRecebidos = 0, tamanho;
    char *caminhoArquivo;

    mkdir(PASTA, S_IRWXU | S_IRWXG | S_IRWXO);

    caminhoArquivo = (char*)malloc(sizeof(char)*128);
    bzero(caminhoArquivo, sizeof(caminhoArquivo));
    strcat(caminhoArquivo, PASTA);
    strcat(caminhoArquivo, nomeRemoto);
    arquivoRemoto = fopen(caminhoArquivo,"wb");

    bloco = malloc(TAM_BLC);

    if (recv(socketDados, &tamanho, sizeof(tamanho), 0) <= 0){
        return 0;
    }

    do{
        if ((bRecebidos = recv(socketDados, bloco, TAM_BLC, 0)) <= 0){
            return 0;
        }

        tamanho -= bRecebidos;

    }while(fwrite(bloco, 1, bRecebidos, arquivoRemoto) >= 0 && tamanho > 0);

    fclose(arquivoRemoto);
    free(bloco);
    free(caminhoArquivo);

    return 1;
}

int conectarCliente(struct sockaddr_in cliente, int portaCliente){

    int s = 0, count = 0, rt = 0;
    unsigned short port;
    struct hostent *hostnm;
    struct sockaddr_in server;

    hostnm = gethostbyname(inet_ntoa(cliente.sin_addr));

    if (hostnm == (struct hostent *)0){
        return 0;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)portaCliente);
    server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        return 0;
    }

    while((rt = connect(s, (struct sockaddr *)&server, sizeof(server))) < 0 && count < 250){
        count++;
    }

    if(rt < 0){
        return 0;
    }

    return s;
}

int encerrarConexao(int s){
    close(s);
}

void encerraServidor(){
    encerrarConexao(socketControle);
    encerrarConexao(socketAux);

    system("clear");
    exit(1);
}