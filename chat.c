#include<stdio.h>
#include<stdlib.h>
#include<mqueue.h>
#include<unistd.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<signal.h>
#include<fcntl.h>
#include <dirent.h> 

char userfila[17]="/chat-";
char username[11];

struct
{
  char de[11]; //usuario origem
  char para[11];//usuario destino
  char corpo[501];//corpo da mensagem
  
}typedef msgtp;

void intHandler(int sg) {
     //handler do ctrl+c que manda o usuario usar o comando exit
    printf("Para sair digite exit\n");
}


void list(){
    
    //funcao que lista os usuarios disponiveis
    struct dirent *de; 
    
    DIR *dr = opendir("/dev/mqueue"); //abre o diretorio
  
    if (dr == NULL)  
    { 
        printf("Could not open current directory" ); 
        return;
    } 

    while ((de = readdir(dr)) != NULL){ 
             
          //itera pelos elementos no diretorio
          if(strncmp("chat-",de->d_name,5)!=0){
                continue; //caso nao seja uma fila de mensagens do nosso programa
          }

            printf("%s\n", &de->d_name[5]);//printa apenas o nome do usuario
    } 
  
    closedir(dr);   

}


void * threceber ( void * s){   
  
  //thread que recebe mensagem

  msgtp msg;
  mqd_t receber;

  //abre a fila para recebimento
  if((receber = mq_open(userfila, O_RDWR))<0){
     perror("mq_open error");
     exit(1);
  }

  while(1){ //fica em loop esperando novas mensagens

    if((mq_receive(receber, (char*) &msg, sizeof(msg), NULL)) < 0){
      perror("mq_receive erro");
      exit(1);
    }

    if(strcmp("all",msg.para)==0){ //formato de exibição caso seja recebido um broadcast
        printf("Broadcast de %s: %s\n",msg.de,msg.corpo);
    }
    else{//formato de exibição normal
        printf("%s: %s\n",msg.de,msg.corpo);
    }

  }

  pthread_exit(NULL) ;
   
}

void *thenviar (void *s){
  //thread que envia a mensagem 

  int ret, try=0;
  mqd_t enviar;
  msgtp msg;
  msg = *(msgtp*)s;//faz o cast do void recebido para struct msgtp
  
  if(strcmp(msg.para,"all")==0){//se broadcast
       
      struct dirent *de; 
      char envfila2[17]="/";        
      DIR *dr = opendir("/dev/mqueue"); 
      
      if (dr == NULL)  
      { 
            printf("Could not open current directory" ); 
            return;
      } 

      while ((de = readdir(dr)) != NULL){

              if(strncmp("chat-",de->d_name,5)!=0){
                continue;//pula filas que nao sao do nosso programa
              }

              //faz para cada fila de usuario disponivel
              strcpy(envfila2,"/");
              strcat(envfila2, de->d_name);
 
              if(strcmp(userfila,envfila2)==0){
                continue; //pula a fila do proprio usuario
              }


              if((enviar = mq_open(envfila2, O_WRONLY))<0){
                 printf("UNKNOWN USER %s\n", &de->d_name[5]);//erro de usuario inexistente
                 continue;
              }
              
              do{
                ret=mq_send(enviar, (void*) &msg, sizeof(msg), 0);
                try++;
              }while(ret<0 && try<3); //tenta enviar a mensagem 3 vezes

              if(ret<0){
                printf("ERRO %s:%s:%s\n", msg.de,msg.para,msg.corpo); //erro retornado se não foi possível enviar mensagem
                
              }
              
             mq_close(enviar);              
      } 
      
      closedir(dr);  
  }
  else{ //Se mensagem normal
  
        char envfila[17]="/chat-";
        strcat(envfila, msg.para); //junta o nome do destino com o "/chat-" para formar o nome da fila destino
        
        //abre a fila de destino
        if((enviar = mq_open(envfila, O_WRONLY))<0){
           printf("UNKNOWN USER %s\n", msg.para);//erro de usuario inexistente
           return;
        }
        
        do{
          ret=mq_send(enviar, (void*) &msg, sizeof(msg), 0);
          try++;
        }while(ret<0 && try<3); //tenta enviar a mensagem 3 vezes

        if(ret<0){
          printf("ERRO %s:%s:%s\n", msg.de,msg.para,msg.corpo); //erro retornado se não foi possível enviar mensagem
       
        }
        
       mq_close(enviar);
 }

 pthread_exit(NULL);
}


int main(){

 signal(SIGINT, intHandler); //implementa o handler para o ctrl+c
 int userflag = 0; //variavel para ajduar na verificação do nome de usuario

 printf("Digite o seu nome de usuario:");

 do{
  scanf("%s", username);
  if(strcmp(username,"all")==0){
    printf("Voce nao pode escolher este nome de usuario\n");
    userflag = 1;
  }
 }
 while(userflag==1); //pede para que o usuario entre com seu login, que não pode ser "all"

 strcat(userfila, username);//junta o "/chat-" com o nome do usuario pra formar o nome da fila

 printf("Fila criada, de nome %s\n", userfila);

 mqd_t receber;
 struct mq_attr attr;

 attr.mq_maxmsg = 10;
 attr.mq_msgsize = sizeof(msgtp);
 attr.mq_flags = 0;

 //Cria e abre a fila para receber as mensagens, com os paramteros acima e permissão apenas de escrita para quem não for o dono
 if((receber = mq_open(userfila, O_RDWR|O_CREAT|O_EXCL, 0622, &attr))<0){
   printf("Usuario já existe\n");
   exit(1);
}

  
 pthread_t ids[2];


 pthread_create (&ids[0] , NULL , threceber , NULL ); //inicia a thread que recebe mensagens

  char oper[12]; //variavel para receber o comando
  msgtp msg;

  while(1){

     scanf("%s",oper);
     if(strcmp(oper, "exit")==0){//comando exit
       
       break;
     }
     else if(strcmp(oper, "list")==0){//comando list
         list();
     }
     else if(strcmp(oper, "enviar")==0){//comando enviar
         printf("Digite a mensagem no formato destino:mensagem\n");
         scanf(" %[^:]:%[^\n]",msg.para,msg.corpo);
         strcpy(msg.de,username);
         pthread_create (&ids[1] , NULL , thenviar , (void*) &msg);//cria uma thread que faz o envio da mensagem
     }
  }
 

 mq_unlink(userfila);

 return 0;

}
