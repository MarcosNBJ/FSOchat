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

// constantes uteis
#define size_message 523

// Variaveis para nome e fila de mensagens do usuario
char userfila[17] = "/chat-";
char username[11];


struct
{
    char de[11];     //usuario origem
    char para[11];   //usuario destino
    char corpo[501]; //corpo da mensagem

} typedef msgtp;


void close_program(char *bye_msg)
{
    mq_unlink(userfila); // fechar fila de mensagens
    if (strlen(bye_msg) > 0)
        printf("%s", bye_msg);
    exit(0);
}


void split_format_message(char *full_msg, char *dest, char *body)
{
    // mensagem no formato usuario_destino:corpo_mensagem\n
    // usuario de destino
    char *token;
    token = strtok(full_msg, ":");
    if (token != NULL)
        strcpy(dest, token);
    else
        dest = "";
    token = strtok(NULL, ":");
    if (token != NULL)
        strcpy(body, token);
    else
        body = "";
}

void split_format_message_full(char *full_msg, char *usersrc, char *dest, char *body)
{
    // mensagem no formato usuario_origem:usuario_destino:corpo_mensagem\n
    // usuario de destino
    char *token;
    token = strtok(full_msg, ":");
    if (token != NULL)
        strcpy(usersrc, token);
    else
        usersrc = "";
    token = strtok(NULL, ":");
    if (token != NULL)
        strcpy(dest, token);
    else
        dest = "";
    token = strtok(NULL, ":");
    if (token != NULL)
        strcpy(body, token);
    else
        body = "";
}

void intHandler(int sg)
{
    //handler do ctrl+c que manda o usuario usar o comando exit
    printf("Para sair digite exit");
}

void list()
{

    //funcao que lista os usuarios disponiveis
    struct dirent *de;
    char diruser[16]="chat-";
    strcat(diruser,username);

    DIR *dr = opendir("/dev/mqueue"); //abre o diretorio

    printf("\n\t** Usuários Conectados **\n\n");

    if (dr == NULL)
    {
        printf("Could not open current directory\n");
        return;
    }

    while ((de = readdir(dr)) != NULL)
    {

        //itera pelos elementos no diretorio
        if (strncmp("chat-", de->d_name, 5) != 0 || strcmp(de->d_name,diruser)==0)
        {
            //caso nao seja uma fila de mensagens do nosso programa ou seja a fila do proprio usuario
            continue;
        }

        //printa apenas o nome do usuario
        printf("\t\t- %s\n", &de->d_name[5]);
    }
    printf("\n");
    closedir(dr);
}

void *threceber(void *s)
{

    //thread que recebe mensagem

    msgtp msg;
    char full_msg[523];
    mqd_t receber;

    //abre a fila para recebimento
    if ((receber = mq_open(userfila, O_RDWR)) < 0)
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
        
        split_format_message_full(full_msg,msg.de,msg.para,msg.corpo);

        if (strcmp("all", msg.para) == 0)
        { //formato de exibição caso seja recebido um broadcast
            sprintf(string_formated, "Broadcast << %s: %s", msg.de, msg.corpo);
            printf("%s\n",string_formated);
        }
        else
        { //formato de exibição normal
            sprintf(string_formated, "Mensagem Recebida <<< %s: %s", msg.de, msg.corpo);
            // adiciona ao log
            printf("%s\n",string_formated);
        }
    }

    pthread_exit(NULL);
}

void *thenviar(void *s)
{
    //thread que envia a mensagem

    int ret, try
        = 0;
    msgtp msg;

    char full_msg[523]="";
    strcat(full_msg,username);
    strcat(full_msg,":");
    strcat(full_msg,(char *)s);

    split_format_message((char *)s, msg.para, msg.corpo);
    strcpy(msg.de, username);
      
    

    mqd_t enviar;
    
    //msg = *(msgtp *)s; //faz o cast do void recebido para struct msgtp
    char string_formated[600];

    if (strcmp(msg.para, "all") == 0)
    { //se broadcast

        struct dirent *de;
        char envfila2[17] = "/";
        DIR *dr = opendir("/dev/mqueue");

        if (dr == NULL)
        {
            printf("Could not open current directory\n");
            return NULL;
        }

        while ((de = readdir(dr)) != NULL)
        {

            if (strncmp("chat-", de->d_name, 5) != 0)
            {
                continue; //pula filas que nao sao do nosso programa
            }

            //faz para cada fila de usuario disponivel
            strcpy(envfila2, "/");
            strcat(envfila2, de->d_name);

            if (strcmp(userfila, envfila2) == 0)
            {
                continue; //pula a fila do proprio usuario
            }

            if ((enviar = mq_open(envfila2, O_WRONLY)) < 0)
            {
                sprintf(string_formated, "UNKNOWN USER %s", &de->d_name[5]); //erro de usuario inexistente
                printf("%s\n",string_formated);
                continue;
            }

            do
            {
                ret = mq_send(enviar, (void *)full_msg, sizeof(full_msg), 0);
                try
                    ++;
            } while (ret < 0 && try < 3); //tenta enviar a mensagem 3 vezes

            if (ret < 0)
            {
                //erro retornado se não foi possível enviar mensagem
                sprintf(string_formated, "ERRO %s:%s:%s", msg.de, &de->d_name[5], msg.corpo);
                printf("%s\n",string_formated);
            }

            mq_close(enviar);
        }

        closedir(dr);
    }
    else
    { //Se mensagem normal

        char envfila[17] = "/chat-";
        strcat(envfila, msg.para); //junta o nome do destino com o "/chat-" para formar o nome da fila destino

        //abre a fila de destino
        if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
        {
            sprintf(string_formated, "UNKNOWN USER %s", msg.para); //erro de usuario inexistente
            printf("%s\n",string_formated);
            return NULL;
        }

        do
        {
            ret = mq_send(enviar, (void *)full_msg, sizeof(full_msg), 0);
            try
                ++;
        } while (ret < 0 && try < 3); //tenta enviar a mensagem 3 vezes

        if (ret < 0)
        {
            //erro retornado se não foi possível enviar mensagem
            sprintf(string_formated, "ERRO %s:%s:%s", msg.de, msg.para, msg.corpo);
            printf("%s\n",string_formated);
        }
        else
        {
            sprintf(string_formated, "Mensagem Enviada >>> %s:%s", msg.para, msg.corpo);
            printf("%s\n",string_formated);
        }

        mq_close(enviar);
    }

    pthread_exit(NULL);
}


int main()
{
    signal(SIGINT, intHandler); //implementa o handler para o ctrl+c
    int userflag = 0;           //variavel para ajduar na verificação do nome de usuario


    printf("Digite o seu nome de usuario:");

    do
    {
        scanf("%s",username);
        if (strcmp(username, "all") == 0)
        {
            printf("Voce nao pode escolher este nome de usuario, por favor digite outro\n");
            userflag = 1;
        }else if (strcmp(username, "exit") == 0){
           close_program("");
        }
        else{
           userflag = 0;
        }
    } while (userflag == 1); //pede para que o usuario entre com seu login, que não pode ser "all"

    strcat(userfila, username); //junta o "/chat-" com o nome do usuario pra formar o nome da fila

    mqd_t receber;
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(msgtp);
    attr.mq_flags = 0;

    mode_t pmask = umask(0000);

    //Cria e abre a fila para receber as mensagens, com os paramteros acima
    // e permissão apenas de escrita para quem não for o dono
    if ((receber = mq_open(userfila, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        close_program("Usuario já existe\n");
    }

    char string_formated[501];
    sprintf(string_formated, "Fila criada, de nome %s", userfila);
    printf("%s\n",string_formated);

    umask(pmask);

    pthread_t ids[2];

    pthread_create(&ids[0], NULL, threceber, NULL); //inicia a thread que recebe mensagens

    char oper[12] = ""; //variavel para receber o comando
    msgtp msg;
    
    printf("Digite o comando\n");
    while (1)
    {   

        scanf("%s",oper);

        if (strcmp(oper, "exit") == 0)
        { // comando exit
            close_program("");
        }
        else if (strcmp(oper, "list") == 0)
        { //comando list
            list();
        }
        else if (strcmp(oper, "enviar") == 0)
        { //comando enviar
            char msg_aux[501] = "";
            //print_screen_msg("Digite a mensagem no formato destino:mensagem");
            scanf("%s",msg_aux);
            // sprintf(msg.para, msg.corpo, "%[^:]:%[^\n]", msg_aux);
            //cria uma thread que faz o envio da mensagem
            pthread_create(&ids[1], NULL, thenviar, (void *)msg_aux);
            pthread_join(ids[1], NULL);
        }

        strcpy(oper, "");
    }

    mq_unlink(userfila);
    return 0;
}
