# FSOchat

Até o Checkpoint 1:

   1. O protocolo proposto foi implementado até o último ponto definido para o ponto de controle 1, com todas as coisas requisitadas no enunciado do trabalho.

   2. Problemas já conhecidos:
	   - As vezes o broadcast demora um pouco
	   - Os erros retornados pela função de envio de mensagens não são todos corretamente descritos
	   - A rolagem de tela apresenta algumas numerações de linha erradas quando usando o comando list

   3. Dificuldades de implementação do modelo de threads:
	A principal dificuldade da dupla foi no momento inicial, de enxergar como seria a estrutura das threads do programa. E mais tarde de estruturar o funcionamento da função de broadcast de tal forma que a thread de envio de mensagens cuidasse do envio da mensagem para cada usuário disponível.

   Além disso, também houve uma dificuldade em como organizar os outputs do recebimento de mensagens para que elas não atrapalhassem o uso do programa e o envio de mensagens. Para solucionar isto, foi adotada a biblioteca ncurses

No Checkpoint 2:

   1. A assinatura de mensagens foi implementada e funcionou, mas apenas com o nosso chat, pela forma como foi feita não ficou compatível com os de outros.
   
   2. Os problemas do grupo começaram nas salas de chat. Da forma como estava a função das salas de chat haviam sido criadas e estavam funcionando. Mas, para utilizar o bot correteitor na entrega do trabalho, o grupo viu que seria necessário remover a Ncurses, pois a biblioteca gráfica, que havia sido adicionada para melhorar a apresentação do trabalho, não usa a saída padrão. E, nesse ponto foi notado que o nosso protocolo de assinatura de mensagens funcionava mas era incompatível com o dos demais, por conta disso, muito tempo se perdeu para remover todo o uso da biblioteca gráfica do trabalho e alterar a forma com que a assinatura de mensagens havia sido feita. Nessa segunda parte, o principal problema foi adaptar a nova assinatura de mensagens ao funcionamento das salas de chat, o que resultou na tentativa falha de refazer as salas de chat na última hora. Sendo assim, o código que está sendo entregue é o resultado disso, contendo tudo que havia no checkpoint 1 com a remoção da biblioteca gráfica, com a adição da assinatura de mensagens e de um funcionamento parcial das salas de chat (com apenas a criação da sala e a adição de um membro fixo, que foi colocado para fins de testes). Além disso, há também uma pasta com a versão antiga  do trabalho, contendo a biblioteca ncurses, para mostrar o que o grupo havia feito para o checkpoint 1.


Manual do chat:
    
   Ao abrir o programa, basta colocar o nome de usuario desejado e apertar enter.
   Depois disso, o programa irá mostrar quaisquer mensagens que receber e esperar por comandos, os comandos são:
   * list,  para listar usuarios disponíveis
   * exit, para sair do chat
   * canal, para criar uma sala de chat
   * Destinatário:MSG, Para enviar uma mensagem a alguém 
   * all:MSG, para enviar um broadcast


   Na versão do trabalho com a ncurses, para enviar uma mensagem direta ou broadcast o usuário deve primeiro usar o comando "enviar".  