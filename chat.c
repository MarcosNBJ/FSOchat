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
#include <ncurses.h>

// tela da lista de mensagens e tela de input
WINDOW *screen_msg, *screen_input;

// Variaveis auxiliares para guardar as posicoes de output na tela ncurses
int height, width, height_screen_msg, init_height_screen_msg;

// Variaveis para nome e fila de mensagens do usuario
char userfila[17] = "/chat-";
char username[11];

struct
{
    char de[11];     //usuario origem
    char para[11];   //usuario destino
    char corpo[501]; //corpo da mensagem

} typedef msgtp;

void intHandler(int sg)
{
    //handler do ctrl+c que manda o usuario usar o comando exit
    wprintw(screen_msg, "Para sair digite exit\n");
    wrefresh(screen_msg);
}

void list()
{

    //funcao que lista os usuarios disponiveis
    struct dirent *de;

    DIR *dr = opendir("/dev/mqueue"); //abre o diretorio

    if (dr == NULL)
    {
        wprintw(screen_msg, "Could not open current directory");
        wrefresh(screen_msg);
        return;
    }

    while ((de = readdir(dr)) != NULL)
    {

        //itera pelos elementos no diretorio
        if (strncmp("chat-", de->d_name, 5) != 0)
        {
            //caso nao seja uma fila de mensagens do nosso programa
            continue;
        }

        //printa apenas o nome do usuario
        wprintw(screen_msg, "%s\n", &de->d_name[5]);
        wrefresh(screen_msg);
    }

    closedir(dr);
}

void *threceber(void *s)
{

    //thread que recebe mensagem

    msgtp msg;
    mqd_t receber;

    //abre a fila para recebimento
    if ((receber = mq_open(userfila, O_RDWR)) < 0)
    {
        wprintw(screen_msg, "mq_open error\n");
        wrefresh(screen_msg);
        exit(1);
    }

    while (1)
    { //fica em loop esperando novas mensagens

        if ((mq_receive(receber, (char *)&msg, sizeof(msg), NULL)) < 0)
        {
            wprintw(screen_msg, "mq_receive erro\n");
            wrefresh(screen_msg);
            exit(1);
        }

        if (strcmp("all", msg.para) == 0)
        { //formato de exibição caso seja recebido um broadcast
            wprintw(screen_msg, "Broadcast de %s: %s\n", msg.de, msg.corpo);
            wrefresh(screen_msg);
        }
        else
        { //formato de exibição normal
            wprintw(screen_msg, "%s: %s\n", msg.de, msg.corpo);
            wrefresh(screen_msg);
            wrefresh(screen_input);
        }
    }

    pthread_exit(NULL);
}

void *thenviar(void *s)
{
    //thread que envia a mensagem

    int ret, try
        = 0;
    mqd_t enviar;
    msgtp msg;
    msg = *(msgtp *)s; //faz o cast do void recebido para struct msgtp

    if (strcmp(msg.para, "all") == 0)
    { //se broadcast

        struct dirent *de;
        char envfila2[17] = "/";
        DIR *dr = opendir("/dev/mqueue");

        if (dr == NULL)
        {
            wprintw(screen_msg, "Could not open current directory");
            wrefresh(screen_msg);
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
                wprintw(screen_msg, "UNKNOWN USER %s\n", &de->d_name[5]); //erro de usuario inexistente
                wrefresh(screen_msg);
                continue;
            }

            do
            {
                ret = mq_send(enviar, (void *)&msg, sizeof(msg), 0);
                try
                    ++;
            } while (ret < 0 && try < 3); //tenta enviar a mensagem 3 vezes

            if (ret < 0)
            {
                //erro retornado se não foi possível enviar mensagem
                wprintw(screen_msg, "ERRO %s:%s:%s\n", msg.de, msg.para, msg.corpo);
                wrefresh(screen_msg);
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
            wprintw(screen_msg, "UNKNOWN USER %s\n", msg.para); //erro de usuario inexistente
            wrefresh(screen_msg);
            return NULL;
        }

        do
        {
            ret = mq_send(enviar, (void *)&msg, sizeof(msg), 0);
            try
                ++;
        } while (ret < 0 && try < 3); //tenta enviar a mensagem 3 vezes

        if (ret < 0)
        {
            //erro retornado se não foi possível enviar mensagem
            wprintw(screen_msg, "ERRO %s:%s:%s\n", msg.de, msg.para, msg.corpo);
            wrefresh(screen_msg);
        }
        else
        {
            wprintw(screen_msg, "Mensagem Enviada\n");
            wrefresh(screen_msg);
        }

        mq_close(enviar);
    }

    pthread_exit(NULL);
}

int main()
{
    signal(SIGINT, intHandler); //implementa o handler para o ctrl+c
    int userflag = 0;           //variavel para ajduar na verificação do nome de usuario

    initscr();                       // inicia screen do curses.h
    getmaxyx(stdscr, height, width); // pegar altura e largura máxima da janela atual
    height_screen_msg = height - 2;
    screen_msg = newwin(height_screen_msg, width, 0, 0);       // subscreen onde eh listado as mensagens
    screen_input = newwin(1, width, height_screen_msg + 1, 0); // subscreen onde digita os comandos
    wprintw(screen_msg, "Digite o seu nome de usuario:");

    do
    {
        wscanw(screen_msg, "%s", username);
        if (strcmp(username, "all") == 0)
        {
            wprintw(screen_msg, "Voce nao pode escolher este nome de usuario\n");
            wrefresh(screen_msg);
            userflag = 1;
        }
    } while (userflag == 1); //pede para que o usuario entre com seu login, que não pode ser "all"

    strcat(userfila, username); //junta o "/chat-" com o nome do usuario pra formar o nome da fila

    mqd_t receber;
    struct mq_attr attr;

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(msgtp);
    attr.mq_flags = 0;

    mode_t pmask = umask(0000);

    //Cria e abre a fila para receber as mensagens, com os paramteros acima e permissão apenas de escrita para quem não for o dono
    if ((receber = mq_open(userfila, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        endwin();            // fechar tela da ncurse
        mq_unlink(userfila); // fechar fila de mensagens
        printf("Usuario já existe\n");
        printf("Fechando Chat\n");
        exit(1);
    }

    wprintw(screen_msg, "Fila criada, de nome %s\n", userfila);
    wrefresh(screen_msg);

    umask(pmask);

    pthread_t ids[2];

    pthread_create(&ids[0], NULL, threceber, NULL); //inicia a thread que recebe mensagens

    char oper[12]; //variavel para receber o comando
    msgtp msg;

    while (1)
    {
        wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
        wprintw(screen_input, "Comando: ");
        wrefresh(screen_input);
        wscanw(screen_input, "%s", oper);

        if (strcmp(oper, "exit") == 0)
        {                        // comando exit
            endwin();            // fechar tela da ncurse
            mq_unlink(userfila); // fechar fila de mensagens
            printf("\nFechando Chat\n");
            break;
        }
        else if (strcmp(oper, "list") == 0)
        { //comando list
            list();
        }
        else if (strcmp(oper, "enviar") == 0)
        { //comando enviar
            wprintw(screen_msg, "Digite a mensagem no formato destino:mensagem\n");
            wrefresh(screen_msg);
            wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
            wprintw(screen_input, "Mensagem: ");
            wscanw(screen_input, " %[^:]:%[^\n]", msg.para, msg.corpo);
            wrefresh(screen_input);

            strcpy(msg.de, username);
            pthread_create(&ids[1], NULL, thenviar, (void *)&msg); //cria uma thread que faz o envio da mensagem
            pthread_join(ids[1], NULL);
        }
        strcpy(oper, "");
    }

    mq_unlink(userfila);
    endwin();

    return 0;
}
