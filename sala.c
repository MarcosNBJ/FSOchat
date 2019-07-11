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

char nomesala[20];            //nome do canal
char nomefila[26] = "canal-"; //nome da fila do canal
char membros[200][11];        //array com os membros do canal

// struct
// {
//     char sender[11];   //usuario origem
//     char receiver[11]; //usuario destino
//     char body[501];    //corpo da mensagem
// } typedef msgtp;

// struct para guardar informacoes de canais
struct
{
    char membros[200][11];
    char path_channel[26];
    int size_list_membros;
    char full_msg[523]; // atributo auxiliar para passar mensagem entre as trheads do canal
} typedef channeltp;

void *thenviarChannel(void *channel_md)
{
    /*
        thread que envia a mensagem para os membros
    */

    // char membros[200][11]; //array com os membros do canal

    channeltp *channel = (channeltp *)channel_md;

    int response_send, try_send = 0;

    mqd_t enviar;

    char string_formated[600];
    int i = 0;

    char receiver_users[500][20];
    int user_count = 0;
    char envfila[17] = "";

    for (i = 0; i < channel->size_list_membros; i++)
    {
        //percorre os membros do canal

        strcpy(envfila, "/chat-");
        strcat(envfila, channel->membros[i]);

        if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
        {
            perror("Erro ao encontrar usuario membro %s"); //erro de usuario inexistente
            continue;
        }

        do
        {
            response_send = mq_send(enviar, (void *)channel->full_msg, 523, 0);
            try_send++;
        } while (response_send < 0 && try_send < 3); //tenta enviar a mensagem 3 vezes

        if (response_send < 0)
        {
            //erro retornado se não foi possível enviar mensagem
            perror("Erro ao repassar mensagem para membro %s");
            continue;
        }
        else
        {
            // usuarios que receberam a mensagem
            strcpy(receiver_users[user_count], channel->membros[i]);
            user_count++;
        }

        mq_close(enviar);
    }

    printf("Mensagem enviada para o Channel: ");
    for (int i = 0; i < user_count; i++)
    {
        printf("%s, ", receiver_users[i]);
    }
    printf("\n");

    pthread_exit(NULL);
}

void *threceiverChannel(void *channel_md)
{
    //thread que consome a fila de mensagens do canal e prepara cada mensagem para envio
    printf("Canal ativo\n");
    char full_msg[523];
    mqd_t receber;
    pthread_t ids[2];

    channeltp *channel = (channeltp *)channel_md;

    //abre a fila para recebimento
    if ((receber = mq_open((char *)channel->path_channel, O_RDWR)) < 0)
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
        //Cria a mensagem no formato a ser enviada pros usuarios membros do canal
        strcpy(channel->full_msg, (char *)full_msg);
        pthread_create(&ids[1], NULL, thenviarChannel, (void *)channel);
    }
}

void create_channel()
{
    channeltp channel;
    channel.size_list_membros = 0; // incialmente a lista de mebros eh zero

    strcpy(channel.path_channel, "/canal-"); //nome da fila do canal
    // char membros[200][11];                   //array com os membros do canal
    strcpy(channel.membros[channel.size_list_membros++], "lays");

    char nomecanal[21];
    printf("Digite o nome do canal: ");
    scanf("%s", nomecanal);
    // execl("sala", "sala", nomecanal, NULL);

    pthread_t ids[2];

    strcat(channel.path_channel, nomecanal); //junta o "/chat-" com o nome do usuario pra formar o nome da fila

    mqd_t receber;
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 523;
    attr.mq_flags = 0;

    mode_t pmask = umask(0000);

    //Cria e abre a fila para receber as mensagens, com os paramteros acima
    if ((receber = mq_open(channel.path_channel, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        perror("Erro ao criar Canal\n");
        exit(1);
    }

    printf("Sala criada\n");

    umask(pmask);

    //inicia a thread que espera por mensagens
    pthread_create(&ids[0], NULL, threceiverChannel, (void *)&channel);
}