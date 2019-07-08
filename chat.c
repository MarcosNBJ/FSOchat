#include <dirent.h>
#include <fcntl.h>
#include <locale.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// constantes uteis
#define size_message 523

// tela da lista de mensagens e tela de input
WINDOW *screen_msg, *screen_input;

// Variaveis auxiliares para guardar as posicoes de output na tela ncurses
int height, width, height_screen_msg, init_height_screen_input;

// Variaveis para nome e fila de mensagens do usuario
char userfila[17] = "/chat-";
char username[11];

// historico de prints na tela
char message_log[5000][size_message];
// guarda tamanho atual do log
int log_msg_current_size = 0;
// guarda os ultimo indices maximo e minimo do log mostrado na tela
int last_max_index_shown = 0;
int last_min_index_shown = 0;

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

void add_message_log(char *msg)
{
    // pega
    char *pos_free = message_log[log_msg_current_size];
    strcpy(pos_free, msg);
    last_max_index_shown = log_msg_current_size;
    log_msg_current_size++;
}

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
    char string_formated[size_message + 5];
    // printa uma string na tela
    // obs: nao use "\n" na string passada, pois ira bugar o log
    sprintf(string_formated, "%d - %s", log_msg_current_size, text);
    wprintw(screen_msg, "%s\n", string_formated);
    // adiciona ao log
    add_message_log(string_formated);
    wrefresh(screen_msg);
}

char *input_text(WINDOW *screen, char *str, int max_size)
{
    // funcao para ler strings digitadas e para reconhecer keypads do teclado
    // obs: todas as string chars sao iniciadas com "", antes de chamar
    // esta funcao
    strcpy(str, "");
    int ch;
    // variaveis auxiliares
    int read_character_count = 0;
    char aux_ch[2];
    int ypos = 0, xpos = 0;

    while ((ch = wgetch(screen)) != 10)
    {
        switch (ch)
        {
        case KEY_DOWN:
            if (last_max_index_shown < log_msg_current_size && log_msg_current_size > height_screen_msg)
            {
                // rolagem para baixo na tela de mensagens
                wscrl(screen_msg, 1);
                wmove(screen_msg, height_screen_msg, 0);
                // insere novamente na tela o texto dentro do vetor
                wprintw(screen_msg, "%s", message_log[last_max_index_shown + 2]);
                last_max_index_shown++;
                wmove(screen_msg, height_screen_msg - 1, 0);
                wrefresh(screen_msg);
            }
            break;
        case KEY_UP:
            // rolagem para cima na tela de mensagens
            last_min_index_shown = last_max_index_shown - height_screen_msg + 1;
            if (last_min_index_shown >= 0)
            {
                wscrl(screen_msg, -1);
                wmove(screen_msg, 0, 0);
                // insere novamente na tela o texto dentro do vetor
                wprintw(screen_msg, message_log[last_min_index_shown]);
                wmove(screen_msg, height_screen_msg - 1, 0);
                last_max_index_shown--;
                wrefresh(screen_msg);
            }
            break;
        case 263:
            // tecla backspace pressionada, apaga um char adicionado no buffer str
            if (strlen(str) > 0)
            {
                str[strlen(str) - 1] = '\0';
                read_character_count--;
            }
            // quando cursor move uma posicao para traz, apaga o lado direito
            // da tela input passada como argumento
            wclrtoeol(screen);
            break;
        default:
            getyx(screen, ypos, xpos);
            if (read_character_count < max_size)
            {
                // concatena os caracteres que nao sao keypads down, up ou backspace
                // dentro de uma string buffer
                aux_ch[0] = ch;
                strcat(str, aux_ch);
                read_character_count++;
            }
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

void intHandler(int sg)
{
    //handler do ctrl+c que manda o usuario usar o comando exit
    print_screen_msg("Para sair digite exit");
}

void list()
{

    //funcao que lista os usuarios disponiveis
    struct dirent *de;
    char diruser[16] = "chat-";
    strcat(diruser, username);

    DIR *dr = opendir("/dev/mqueue"); //abre o diretorio

    print_screen_msg("\n\t** Usuários Conectados **\n\n");
    wrefresh(screen_msg);

    if (dr == NULL)
    {
        print_screen_msg("Could not open current directory\n");
        wrefresh(screen_msg);
        return;
    }

    while ((de = readdir(dr)) != NULL)
    {

        //itera pelos elementos no diretorio
        if (strncmp("chat-", de->d_name, 5) != 0 || strcmp(de->d_name, diruser) == 0)
        {
            //caso nao seja uma fila de mensagens do nosso programa ou seja a fila do proprio usuario
            continue;
        }

        //printa apenas o nome do usuario
        char name[11];
        sprintf(name, "\t\t- %s\n", &de->d_name[5]);
        print_screen_msg(name);
        wrefresh(screen_msg);
    }
    print_screen_msg("\n");
    wrefresh(screen_msg);
    closedir(dr);
}

void send_msg_verification(char *dest, char *body, char *index_msg_received)
{
    /*
        Funcao para enviar as mensagens de verificacao de check e confirm
    */
    // fila do usuario para mandar a mensagem de verificacao check, ou confirmacao valid/invalid
    char queue[17] = "/chat-";
    strcat(queue, dest);
    char string_formated[600];

    mqd_t open;
    int try_send = 0;
    int ret;

    // construir formato de mensagem de verificacao username:dest:<empty_msg>:index_msg:<msg_type>
    // type_msg = tipo de mensagem(msg, check, valid, invalid)
    char full_msg_check[50] = "";
    sprintf(full_msg_check, "%s:%s:%s:%s", username, dest, body, index_msg_received);

    //abre a fila do usuario
    if ((open = mq_open(queue, O_WRONLY)) < 0)
    {
        sprintf(string_formated, "UNKNOWN USER %s", dest); //erro de usuario inexistente
        print_screen_msg(string_formated);
    }

    do
    {
        ret = mq_send(open, (void *)full_msg_check, sizeof(full_msg_check), 0);
        try_send++;
    } while (ret < 0 && try_send < 3); //tenta enviar a mensagem 3 vezes

    if (ret < 0)
    {
        //erro retornado se não foi possível enviar mensagem
        sprintf(string_formated, "ERRO Assinatura %s:%s:%s", username, dest, index_msg_received);
        print_screen_msg(string_formated);
    }
    mq_close(open);
}

void check_signature(char *sender, char *body, char *index_msg_received)
{
    /*
        Funcao para verficar autenticidade de Mensagens Recebidas
    */
    sprintf(index_msg_received, "%s?", index_msg_received);
    send_msg_verification(sender, body, index_msg_received);
}

void confirm_signature(char *receiver, char *body, char *index_msg_received)
{
    /*
        Funcao para validar ou nao que foi o usuario corrente que enviou a mensagem
    */

    int aux_index = atoi(index_msg_received);
    char msg_receiver[15] = "";
    char msg_body[501] = "";
    // verificar na lista de mensagens enviadas se o destinatario eh o msm que esta pedindo a
    // confirmacao de mensagem
    split_format_message_full(messages_sent[aux_index], NULL, msg_receiver, msg_body, NULL);
    if (aux_index <= msg_index && strcmp(receiver, msg_receiver) == 0 && strcmp(body, msg_body) == 0)
        // Mensagem Confirmada
        index_msg_received[strlen(index_msg_received) - 1] = 'y';
    else
        index_msg_received[strlen(index_msg_received) - 1] = 'n';
    send_msg_verification(receiver, body, index_msg_received);
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
            char tag_msg[20] = "Mensagem Recebida";
            // Mensagem de confirmacao "y": autenticada ou "n": nao autenticada
            if (strcmp("all", msg.receiver) == 0)
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
            print_screen_msg(string_formated);
            wrefresh(screen_input);
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
    // teste de falsificacao de assinatura
    // sprintf(full_msg, "gustavo:%s:%d", (char *)dest_and_msg, msg_index);

    split_format_message((char *)dest_and_msg, msg.receiver, msg.body);
    strcpy(msg.sender, username);
    // strcpy(msg.sender, "gustavo");

    mqd_t enviar;

    char string_formated[600];

    if (strcmp(msg.receiver, "all") == 0)
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
                sprintf(string_formated, "UNKNOWN USER %s", &de->d_name[5]); //erro de usuario inexistente
                print_screen_msg(string_formated);
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
                sprintf(string_formated, "ERRO %s:%s:%s", msg.sender, &de->d_name[5], msg.body);
                print_screen_msg(string_formated);
            }

            mq_close(enviar);
        }

        closedir(dr);
    }
    else
    { //Se mensagem normal
        char envfila[17] = "/chat-";
        strcat(envfila, msg.receiver); //junta o nome do destino com o "/chat-" para formar o nome da fila destino

        //abre a fila de destino
        if ((enviar = mq_open(envfila, O_WRONLY)) < 0)
        {
            sprintf(string_formated, "UNKNOWN USER %s", msg.receiver); //erro de usuario inexistente
            print_screen_msg(string_formated);
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
            print_screen_msg(string_formated);
        }
        else
        {
            sprintf(string_formated, "Mensagem Enviada >>> %s:%s", msg.receiver, msg.body);
            print_screen_msg(string_formated);
            // adiciona mensagem enviada na lista, com destinatario e codigo
            strcpy(messages_sent[msg_index], full_msg);
            msg_index++;
        }

        mq_close(enviar);
    }
    pthread_exit(NULL);
}

void view_log()
{
    for (int i = 0; i < log_msg_current_size; i++)
    {
        wprintw(screen_msg, "%s", message_log[i]);
    }
}

int main()
{
    // setlocale(LC_ALL, "");

    WINDOW *screen_box;
    signal(SIGINT, intHandler); //implementa o handler para o ctrl+c
    int userflag = 0;           //variavel para ajduar na verificação do nome de usuario

    initscr();                       // inicia screen do curses.h
    getmaxyx(stdscr, height, width); // pegar altura e largura máxima da janela atual

    height_screen_msg = height - 7;
    screen_msg = newwin(height_screen_msg, width, 0, 0); // subscreen onde eh listado as mensagens
    init_height_screen_input = height_screen_msg + 1;
    // apenas uma subscreen para desenhar uma box sem atrapalhar a tela do input
    screen_box = newwin(6, width, init_height_screen_input, 0);
    // subscreen onde digita os comandos
    screen_input = newwin(4, width - 2, init_height_screen_input + 1, 1);

    // ativar scroll na tela de mensagens
    scrollok(screen_msg, TRUE);
    scroll(screen_msg);
    idlok(screen_msg, TRUE);

    scrollok(screen_input, TRUE);

    // ativar reconhecimento das teclas de direcao e mouse na tela de mensagens e input
    keypad(screen_msg, TRUE);
    keypad(screen_input, TRUE);
    cbreak();

    wprintw(screen_msg, "Digite o seu nome de usuario:");
    wrefresh(screen_msg);

    do
    {
        input_text(screen_msg, username, 11);
        if (strcmp(username, "all") == 0)
        {
            print_screen_msg("Voce nao pode escolher este nome de usuario, por favor digite outro");
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
    print_screen_msg(string_formated);

    umask(pmask);

    pthread_t ids[2];

    pthread_create(&ids[0], NULL, threceber, NULL); //inicia a thread que recebe mensagens

    char oper[12] = ""; //variavel para receber o comando
    msgtp msg;

    while (1)
    {
        box(screen_box, 0, 0);
        wrefresh(screen_box);
        wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
        wprintw(screen_input, "Comando: ");
        wrefresh(screen_input);
        input_text(screen_input, oper, 12);
        wclear(screen_input);
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
            print_screen_msg("Digite a mensagem no formato destino:mensagem");
            wclrtoeol(screen_input); // apaga o lado direito da linha onde estar o cursor
            wprintw(screen_input, "Mensagem: ");
            input_text(screen_input, msg_aux, size_message);
            wclear(screen_input);
            // sprintf(msg.receiver, msg.body, "%[^:]:%[^\n]", msg_aux);
            wrefresh(screen_msg);
            wrefresh(screen_input);
            //cria uma thread que faz o envio da mensagem
            pthread_create(&ids[1], NULL, thenviar, (void *)msg_aux);
            pthread_join(ids[1], NULL);
        }

        strcpy(oper, "");
    }

    mq_unlink(userfila);
    endwin();
    return 0;
}
