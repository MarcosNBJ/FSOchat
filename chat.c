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

char *input_text(WINDOW *screen, char *str)
{
    // funcao para ler strings digitadas e para reconhecer keypads do teclado
    // obs: todas as string chars sao iniciadas com "", antes de chamar
    // esta funcao
    strcpy(str, "");
    char aux_ch[2];
    int ch;
    while ((ch = wgetch(screen)) != 10)
    {
        switch (ch)
        {
        case KEY_DOWN:
            wscrl(screen, 1);
            break;
        case KEY_UP:
            wscrl(screen, -1);
            break;
        case 263:
            // basckspace
            if (strlen(str) > 0)
                str[strlen(str) - 1] = '\0';
            wclrtoeol(screen);
            break;
        default:
            // concatena os caracteres que nao sao keypads down, up ou backspace
            aux_ch[0] = ch;
            strcat(str, aux_ch);
            break;
        }
    }
    return str;
}

void split_format_message(char *full_msg, char *dest, char *body)
{
    // mensagem no formato usuario_destino:corpo_mensagem\n
    // usuario de destino
    char *token;
    token = strtok(full_msg, ":");
    strcpy(dest, token);
    token = strtok(NULL, ":");
    strcpy(body, token);
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

    // ativar reconhecimento das teclas de direcao e mouse na tela de mensagens e input
    keypad(screen_msg, TRUE);
    keypad(screen_input, TRUE);
    cbreak();

    print_screen_msg("Digite o seu nome de usuario:");

    do
    {
        input_text(screen_msg, username);
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

    char oper[12] = ""; //variavel para receber o comando
    msgtp msg;

    while (1)
    {
        wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
        wprintw(screen_input, "Comando: ");
        wrefresh(screen_input);
        input_text(screen_input, oper);
        wprintw(screen_msg, "comando: %s\n", oper);
        wrefresh(screen_msg);

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
            print_screen_msg("Digite a mensagem no formato destino:mensagem\n");
            wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
            wprintw(screen_input, "Mensagem: ");
            input_text(screen_input, msg_aux);
            // sprintf(msg.para, msg.corpo, "%[^:]:%[^\n]", msg_aux);
            split_format_message(msg_aux, msg.para, msg.corpo);
            wrefresh(screen_msg);
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
