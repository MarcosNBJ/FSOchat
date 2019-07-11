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

// constantes uteis
#define size_message 523

// Variaveis para nome e fila de mensagens do usuario
char userfila[17] = "/chat-";
char username[11];

// lista de mensagens enviadas
char messages_sent[5000][size_message];
// guarda tamanho atual da lista de mensagens enviadas
int msg_index = 0;

struct
{
    char sender[11];   //usuario origem
    char receiver[11]; //usuario destino
    char body[501];    //corpo da mensagem
} typedef msgtp;

void close_program(char *bye_msg)
{
    mq_unlink(userfila); // fechar fila de mensagens
    mq_unlink("/canal-teste");
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
    {
        strcpy(dest, token);
    }
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

void intHandler(int sg)
{
    //handler do ctrl+c que manda o usuario usar o comando exit
    printf("Para sair digite exit");
}

void list()
{

    //funcao que lista os usuarios disponiveis
    struct dirent *de;
    char diruser[16] = "chat-";
    strcat(diruser, username);

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
        if ((strncmp("chat-", de->d_name, 5) != 0 && strncmp("canal-", de->d_name, 5) != 0) || strcmp(de->d_name, diruser) == 0)
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

void send_msg_verification(char *receiver, char *body, char *index_msg_received, int is_broadcast)
{
    /*
        Funcao para enviar as mensagens de verificacao de check e confirm
    */
    // fila do usuario para mandar a mensagem de verificacao check, ou confirmacao valid/invalid
    char queue[17] = "/chat-";
    strcat(queue, receiver);
    char string_formated[600];

    mqd_t open;
    int try_send = 0;
    int ret;

    // construir formato de mensagem de verificacao username:receiver:<empty_msg>:index_msg:<msg_type>
    // type_msg = tipo de mensagem(msg, check, valid, invalid)
    char full_msg_check[50] = "";
    char dest[20] = "";
    strcpy(dest, (is_broadcast == 1) ? "broadcast" : receiver);
    sprintf(full_msg_check, "%s:%s:%s:%s", username, dest, body, index_msg_received);

    //abre a fila do usuario
    if ((open = mq_open(queue, O_WRONLY)) < 0)
    {
        sprintf(string_formated, "UNKNOWN USER %s", receiver); //erro de usuario inexistente
        printf("%s\n", string_formated);
    }

    do
    {
        ret = mq_send(open, (void *)full_msg_check, sizeof(full_msg_check), 0);
        try_send++;
    } while (ret < 0 && try_send < 3); //tenta enviar a mensagem 3 vezes

    if (ret < 0)
    {
        //erro retornado se não foi possível enviar mensagem
        sprintf(string_formated, "ERRO Assinatura %s:%s:%s", username, receiver, index_msg_received);
        printf("%s\n", string_formated);
    }
    mq_close(open);
}

void check_signature(char *receiver, char *body, char *index_msg_received)
{
    /*
        Funcao para verficar autenticidade de Mensagens Recebidas
    */
    sprintf(index_msg_received, "%s?", index_msg_received);
    send_msg_verification(receiver, body, index_msg_received, 0);
}

void confirm_signature(char *receiver, char *body, char *index_msg_received)
{
    /*
        Funcao para validar ou nao que foi o usuario corrente que enviou a mensagem
    */

    int aux_index = atoi(index_msg_received);
    char msg_receiver[15] = "";
    char msg_body[501] = "";
    split_format_message_full(messages_sent[aux_index], NULL, msg_receiver, msg_body, NULL);
    // verificar na lista de mensagens enviadas se o destinatario eh o msm que esta pedindo a
    // confirmacao de mensagem ou verifica se foi um broadcast
    int is_broadcast = strcmp(msg_receiver, "all");
    int receiver_valid = strcmp(msg_receiver, receiver) && is_broadcast;
    // int receiver_valid = strcmp(msg_receiver, receiver);
    if (aux_index <= msg_index && strcmp(msg_body, body) == 0 && receiver_valid == 0)
        // Mensagem Confirmada
        index_msg_received[strlen(index_msg_received) - 1] = 'y';
    else
        index_msg_received[strlen(index_msg_received) - 1] = 'n';
    send_msg_verification(receiver, body, index_msg_received, !is_broadcast);
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

        char index_msg[10] = "";
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
            char tag_msg[20] = "Mensagem Recebida";
            // Mensagem de confirmacao "y": autenticada ou "n": nao autenticada
            if (strcmp("broadcast", msg.receiver) == 0)
            { //caso seja recebido um broadcast
                strcpy(tag_msg, "BroadCast");
            }

            char result[20] = "";
            // É uma mensagem com a resposta se a mensagem é valida ou nao
            if (index_msg[strlen(index_msg) - 1] == 'y')
                strcat(result, "Autenticada");
            else
                strcat(result, "Nao Autenticada");

            sprintf(string_formated, "%s <<< %s:%s\t(%s)", tag_msg, msg.sender, msg.body, result);
            // adiciona ao log
            printf("%s\n", string_formated);
        }
        else
        {
            // É uma mensagem comum, entao cheque a autenticidade da mensagem
            // enviando uma mensagem de check "?"
            check_signature(msg.sender, msg.body, index_msg);
        }
        memset(&msg, 0, sizeof(msgtp));
    }

    pthread_exit(NULL);
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
    sprintf(full_msg, "%s:%s:%d", username, (char *)dest_and_msg, msg_index);

    split_format_message((char *)dest_and_msg, msg.receiver, msg.body);
    strcpy(msg.sender, username);

    // teste de falsificacao de assinatura
    // sprintf(full_msg, "gustavo:%s:%d", (char *)dest_and_msg, msg_index);
    // strcpy(msg.sender, "gustavo");

    mqd_t enviar;
    char envfila[17] = "";

    char string_formated[600];

    if (strcmp(msg.receiver, "all") == 0)
    { //se broadcast

        struct dirent *de;
        DIR *dr = opendir("/dev/mqueue");
        char receiver_users[500][20];
        int user_count = 0;

        if (dr == NULL)
        {
            printf("%s\n", "Could not open current directory");
            return NULL;
        }

        while ((de = readdir(dr)) != NULL)
        {

            if (strncmp("chat-", de->d_name, 5) != 0)
            {
                continue; //pula filas que nao sao /chat ou nao sao do nosso programa
            }

            //faz para cada fila de usuario disponivel
            strcpy(envfila, "/");
            strcat(envfila, de->d_name);

            if (strcmp(userfila, envfila) == 0)
            {
                continue; //pula a fila do proprio usuario
            }

            if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
            {
                sprintf(string_formated, "UNKNOWN USER %s", &de->d_name[5]); //erro de usuario inexistente
                printf("%s\n", string_formated);
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
                sprintf(string_formated, "ERRO %s:%s", &de->d_name[5], msg.body);
                printf("%s\n", string_formated);
            }
            else
            {
                // usuarios que receberam a mensagem
                strcpy(receiver_users[user_count], &de->d_name[5]);
                user_count++;
            }

            mq_close(enviar);
        }
        sprintf(string_formated, "BroadCast Enviado >>> ");
        for (int i = 0; i < user_count; i++)
        {
            strcat(string_formated, receiver_users[i]);
            strcat(string_formated, ", ");
        }
        printf("%s\n", string_formated);

        // adiciona mensagem enviada na lista, com destinatario e codigo
        strcpy(messages_sent[msg_index], full_msg);
        msg_index++;

        closedir(dr);
    }
    else
    {
        char *aux_msg = (char *)dest_and_msg;
        // se for uma mensagem direta para um usuario ou mensagem para um canal
        if (aux_msg[0] == '#')
        {
            char *token;
            strcpy(envfila, "/canal-");
            token = strtok(msg.receiver, "#");
            strcpy(msg.receiver, token);
        }
        else
            strcpy(envfila, "/chat-");

        strcat(envfila, msg.receiver); //junta o nome do destino com o "/chat-" para formar o nome da fila destino

        //abre a fila de destino
        if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
        {
            sprintf(string_formated, "UNKNOWN USER %s", msg.receiver); //erro de usuario inexistente
            printf("%s\n", string_formated);
            return NULL;
        }

        do
        {
            response_send = mq_send(enviar, (void *)full_msg, sizeof(full_msg), 0);
            try_send++;
        } while (response_send < 0 && try_send < 3); //tenta enviar a mensagem 3 vezes

        if (response_send < 0)
        {
            //erro retornado se não foi possível enviar mensagem
            sprintf(string_formated, "ERRO %s:%s:%s", msg.sender, msg.receiver, msg.body);
            printf("%s\n", string_formated);
        }
        else
        {
            sprintf(string_formated, "Mensagem Enviada >>> %s:%s", msg.receiver, msg.body);
            printf("%s\n", string_formated);
            // adiciona mensagem enviada na lista, com destinatario e codigo
            strcpy(messages_sent[msg_index], full_msg);
            msg_index++;
        }

        mq_close(enviar);
    }
    pthread_exit(NULL);
}

void *thenviarChannel(void *full_msg)
{
    /*
        thread que envia a mensagem para os membros
    */

    char membros[200][11]; //array com os membros do canal
    int size_list_membros = 1;
    // strcpy(membros[0], "lucas");
    strcpy(membros[0], "lays");

    int response_send, try_send = 0;

    mqd_t enviar;

    char string_formated[600];
    int i = 0;

    char receiver_users[500][20];
    int user_count = 0;
    char envfila[17] = "";

    for (i = 0; i < size_list_membros; i++)
    {
        //percorre os membros do canal

        strcpy(envfila, "/chat-");
        strcat(envfila, membros[i]);

        if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
        {
            perror("Erro ao encontrar usuario membro %s"); //erro de usuario inexistente
            continue;
        }

        do
        {
            strcpy(full_msg, "lucas:teste:Pelo Channel:0");
            response_send = mq_send(enviar, (void *)full_msg, sizeof(full_msg), 0);
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
            strcpy(receiver_users[user_count], membros[i]);
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

void *threceiverChannel(void *filaCanal)
{
    //thread que consome a fila de mensagens do canal e prepara cada mensagem para envio
    printf("Canal ativo\n");
    msgtp msg;
    char full_msg[523];
    mqd_t receber;
    pthread_t ids[2];

    //abre a fila para recebimento
    if ((receber = mq_open((char *)filaCanal, O_RDWR)) < 0)
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
        pthread_create(&ids[1], NULL, thenviarChannel, (void *)full_msg);
    }
}

void create_channel()
{
    char filaCanal[26] = "/canal-"; //nome da fila do canal
    char membros[200][11];          //array com os membros do canal

    char nomecanal[21];
    printf("Digite o nome do canal: ");
    scanf("%s", nomecanal);
    // execl("sala", "sala", nomecanal, NULL);

    pthread_t ids[2];

    strcat(filaCanal, nomecanal); //junta o "/chat-" com o nome do usuario pra formar o nome da fila

    mqd_t receber;
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(msgtp);
    attr.mq_flags = 0;

    mode_t pmask = umask(0000);

    //Cria e abre a fila para receber as mensagens, com os paramteros acima
    if ((receber = mq_open(filaCanal, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        perror("Erro ao criar Canal\n");
        exit(1);
    }

    printf("Sala criada\n");

    umask(pmask);

    //inicia a thread que espera por mensagens
    pthread_create(&ids[0], NULL, threceiverChannel, (void *)filaCanal);
}

int main()
{
    signal(SIGINT, intHandler); //implementa o handler para o ctrl+c
    int userflag = 0;           //variavel para ajduar na verificação do nome de usuario

    printf("Digite o seu nome de usuario:");

    do
    {
        scanf("%s", username);
        if (strcmp(username, "all") == 0)
        {
            printf("Voce nao pode escolher este nome de usuario, por favor digite outro\n");
            userflag = 1;
        }
        else if (strcmp(username, "exit") == 0)
        {
            close_program("");
        }
        else
        {
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
    printf("%s\n", string_formated);

    umask(pmask);

    pthread_t ids[2];

    pthread_create(&ids[0], NULL, threceber, NULL); //inicia a thread que recebe mensagens

    char msg_cmd[501] = ""; //variavel para receber o comando ou mensagem
    msgtp msg;

    // regex_t reg;
    // if (regcomp(&reg, "a[[:alnum:]]", REG_EXTENDED | REG_NOSUB) != 0)
    // {
    //     fprintf(stderr, "erro regcomp\n");
    //     exit(1);
    // }

    printf("Digite o comando\n");
    while (1)
    {

        scanf(" %[^\n]s", msg_cmd);

        if (strcmp(msg_cmd, "exit") == 0)
        { // comando exit
            close_program("");
        }
        else if (strcmp(msg_cmd, "list") == 0)
        { //comando list
            list();
        }
        else if (strcmp(msg_cmd, "canal") == 0)
        { //comando canal, para criar um novo canal
            create_channel();
        }
        else
        { //comando enviar
            //cria uma thread que faz o envio da mensagem
            pthread_create(&ids[1], NULL, thenviar, (void *)msg_cmd);
            pthread_join(ids[1], NULL);
        }

        strcpy(msg_cmd, "");
    }

    mq_unlink(userfila);
    return 0;
}
