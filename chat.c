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
int height, width, height_screen_msg, init_height_screen_input;

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
    endwin();            // fechar tela da ncurse
    mq_unlink(userfila); // fechar fila de mensagens
    if (strlen(bye_msg) > 0)
        printf("%s", bye_msg);
    printf("Fechando Chat\n");
    exit(0);
}

void print_screen_msg(char *text)
{
    wprintw(screen_msg, text);
    wrefresh(screen_msg);
}

void *wait_keypad()
{
    int ch;
    while ((ch = wgetch(screen_msg)) != KEY_F(1))
    {
        switch (ch)
        {
        case KEY_DOWN:
            wscrl(screen_msg, 1);
            break;
        case KEY_UP:
            wscrl(screen_msg, -1);
            break;
        default:
            // se nao for nenhuma das duas teclas, empurra o char lido
            // devolta para o fluxo de entrada, para que o wscanw da
            // tela de input consiga ler o valor
            print_screen_msg("teste\n");
            ungetc(ch, stdin);
            break;
        }
        wrefresh(screen_msg);
    }
}

void intHandler(int sg)
{
    //handler do ctrl+c que manda o usuario usar o comando exit
    print_screen_msg("Para sair digite exit\n");
}

void list()
{

    //funcao que lista os usuarios disponiveis
    struct dirent *de;

    DIR *dr = opendir("/dev/mqueue"); //abre o diretorio

    if (dr == NULL)
    {
        print_screen_msg("Could not open current directory\n");
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
        char string_formated[50];

        sprintf(string_formated, "%s\n", &de->d_name[5]);
        print_screen_msg(string_formated);
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
        perror("mq_open error\n");
        exit(1);
    }

    char string_formated[600];
    while (1)
    { //fica em loop esperando novas mensagens

        if ((mq_receive(receber, (char *)&msg, sizeof(msg), NULL)) < 0)
        {
            perror("mq_receive erro\n");
            exit(1);
        }

        if (strcmp("all", msg.para) == 0)
        { //formato de exibição caso seja recebido um broadcast
            sprintf(string_formated, "Broadcast de %s: %s\n", msg.de, msg.corpo);
            print_screen_msg(string_formated);
        }
        else
        { //formato de exibição normal
            sprintf(string_formated, "%s: %s\n", msg.de, msg.corpo);
            print_screen_msg(string_formated);
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
    char string_formated[600];

    if (strcmp(msg.para, "all") == 0)
    { //se broadcast

        struct dirent *de;
        char envfila2[17] = "/";
        DIR *dr = opendir("/dev/mqueue");

        if (dr == NULL)
        {
            print_screen_msg("Could not open current directory");
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
                sprintf(string_formated, "UNKNOWN USER %s\n", &de->d_name[5]); //erro de usuario inexistente
                print_screen_msg(string_formated);
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
                sprintf(string_formated, "ERRO %s:%s:%s\n", msg.de, msg.para, msg.corpo);
                print_screen_msg(string_formated);
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
            sprintf(string_formated, "UNKNOWN USER %s\n", msg.para); //erro de usuario inexistente
            print_screen_msg(string_formated);
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
            sprintf(string_formated, "ERRO %s:%s:%s\n", msg.de, msg.para, msg.corpo);
            print_screen_msg(string_formated);
        }
        else
        {
            print_screen_msg("Mensagem Enviada\n");
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
    screen_msg = newwin(height_screen_msg, width, 0, 0); // subscreen onde eh listado as mensagens
    init_height_screen_input = height_screen_msg + 1;
    screen_input = newwin(1, width, init_height_screen_input, 0); // subscreen onde digita os comandos

    // ativar scroll na tela de mensagens
    scrollok(screen_msg, TRUE);
    scroll(screen_msg);
    idlok(screen_msg, TRUE);

    // ativar reconhecimento das teclas de direcao e mouse
    keypad(screen_msg, TRUE);
    cbreak();

    pthread_t thread_keypad;

    pthread_create(&thread_keypad, NULL, wait_keypad, NULL);

    print_screen_msg("Digite o seu nome de usuario:");

    do
    {
        wscanw(screen_msg, "%s", username);
        if (strcmp(username, "all") == 0)
        {
            print_screen_msg("Voce nao pode escolher este nome de usuario\n");
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

    //Cria e abre a fila para receber as mensagens, com os paramteros acima
    // e permissão apenas de escrita para quem não for o dono
    if ((receber = mq_open(userfila, O_RDWR | O_CREAT | O_EXCL, 0622, &attr)) < 0)
    {
        close_program("Usuario já existe\n");
    }

    char string_formated[501];
    sprintf(string_formated, "Fila criada, de nome %s\n", userfila);
    print_screen_msg(string_formated);

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
        { // comando exit
            close_program("");
        }
        else if (strcmp(oper, "list") == 0)
        { //comando list
            list();
        }
        else if (strcmp(oper, "enviar") == 0)
        { //comando enviar
            print_screen_msg("Digite a mensagem no formato destino:mensagem\n");
            wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
            wprintw(screen_input, "Mensagem: ");
            wscanw(screen_input, " %[^:]:%[^\n]", msg.para, msg.corpo);
            wrefresh(screen_input);

            strcpy(msg.de, username);
            //cria uma thread que faz o envio da mensagem
            pthread_create(&ids[1], NULL, thenviar, (void *)&msg);
            pthread_join(ids[1], NULL);
        }
        strcpy(oper, "");
    }

    mq_unlink(userfila);
    endwin();
    return 0;
}
