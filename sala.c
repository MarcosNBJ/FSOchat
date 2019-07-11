#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>



char nomesala[20]; //nome do canal
char nomefila[26]="canal-"; //nome da fila do canal
char membros[200][11]; //array com os membros do canal

struct
{
    char sender[11];   //usuario origem
    char receiver[11]; //usuario destino
    char body[501];    //corpo da mensagem
} typedef msgtp;


void split_format_message_full(char *full_msg, char *sender, char *dest,
                               char *body, char *index_msg)
{
    /*
        Funcao para cortar as mensagens por partes, atraves dos ":"
    */
    // mensagem no formato usuario_origem:usuario_destino:corpo_mensagem\n
    // usuario de destino
    char *token;
    token = strtok(full_msg, ":");
    if (token != NULL && sender != NULL)
        strcpy(sender, token);
    else
        sender = "";
    token = strtok(NULL, ":");
    if (token != NULL && dest != NULL)
        strcpy(dest, token);
    else
        dest = "";
    token = strtok(NULL, ":");
    if (token != NULL && body != NULL)
        strcpy(body, token);
    else
        body = "";
    token = strtok(NULL, ":");
    if (token != NULL && index_msg != NULL)
    {
        strcpy(index_msg, token);
    }
    else
        index_msg = "";
}


void *thenviar(void *dest_and_msg)
{
    /*
        thread que envia a mensagem
    */

    int response_send, try_send = 0;
    msgtp msg;

    // Formato de Mensagem username:dest:<msg_content>:index:<msg_type>
    char full_msg[523] = "";
    sprintf(full_msg, "%s:%s:%d", nomefila, (char *)dest_and_msg, msg_index);

    split_format_message((char *)dest_and_msg, msg.receiver, msg.body);
    strcpy(msg.sender, nomefila);

    mqd_t enviar;

    char string_formated[600];
    int i=0;

    char envfila2[17] = "/";

    for(i=0;i<200;i++){
        //percorre os membros do canal

        strcpy(envfila2, "/");
        strcat(envfila2, membros[i]);

        if ((enviar = mq_open(envfila2, O_WRONLY)) < 0)
        {
            continue;
        }

        do
        {
            response_send = mq_send(enviar, (void *)full_msg, sizeof(full_msg), 0);
            try_send++;
        } while (response_send < 0 && try_send < 3); //tenta enviar a mensagem 3 vezes

        if (response_send < 0)
        {
            //erro retornado se não foi possível enviar mensagem
	        continue;
        }

        mq_close(enviar);
    }


    pthread_exit(NULL);
}

void *threceber(void *s)
{

    //thread que consome a fila de mensagens do canal e prepara cada mensagem para envio

    msgtp msg;
    char full_msg[523];
    mqd_t receber;
    pthread_t ids[2];
    char msg_cmd[501] = "#";//significa que é um canal

    //abre a fila para recebimento
    if ((receber = mq_open(nomefila, O_RDWR)) < 0)
    {
        perror("mq_open error\n");
        exit(1);
    }

    char string_formated[600];
    while (1)
    { //fica em loop esperando novas mensagens

        if ((mq_receive(receber, (char *)full_msg, sizeof(full_msg), NULL)) < 0)
        {
            perror("mq_receive erro\n");
            exit(1);
        }

        char index_msg[10] = "";
        // char msg_type[10];
        split_format_message_full(full_msg, msg.sender, msg.receiver, msg.body, index_msg);

        // Verifica se é uma mensagem normal:msg ou de verificar assinatura
        if (index_msg[strlen(index_msg) - 1] == '?')
        {
            // É uma mensagem de verificacao '?', entao apenas
            // verifique e envie a resposta 'y' ou 'n'
            confirm_signature(msg.sender, msg.body, index_msg);
        }
        else if (index_msg[strlen(index_msg) - 1] == 'y' || index_msg[strlen(index_msg) - 1] == 'n')
        {   
            //Cria a mensagem no formato a ser enviada pros usuarios membros do canal
            strcat(msg_cmd,nomesala);
            strcat(msg_cmd,":");
            strcat(msg_cmd,msg.sender);
            strcat(msg_cmd,":");
            strcat(msg_cmd,msg.body)

            pthread_create(&ids[1], NULL, thenviar, (void *)msg_cmd);
        }
        else
        {
            // É uma mensagem comum, entao cheque a autenticidade da mensagem
            // enviando uma mensagem de check "?"
            check_signature(msg.sender, msg.body, index_msg);
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv){

    
    strcpy(nomesala, argv[1]);
    pthread_t ids[2];


    strcat(nomefila, nomesala); //junta o "/chat-" com o nome do usuario pra formar o nome da fila

    mqd_t receber;
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(msgtp);
    attr.mq_flags = 0;

    mode_t pmask = umask(0000);

    //Cria e abre a fila para receber as mensagens, com os paramteros acima
    if ((receber = mq_open(nomefila, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        exit();
    }

    umask(pmask);
    
    //inicia a thread que espera por mensagens
    pthread_create(&ids[0], NULL, threceber, NULL);

   

}