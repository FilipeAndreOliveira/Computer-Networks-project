#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_IP "192.136.138.142"
#define DEFAULT_UDP 59000

#define MAX_CONTENTS 10

struct tabuleta
{
    int tableActive;
    int connection;
    int destiny;
};

typedef struct tabuleta tabuleta;

struct otherNode
{
    int otherActive;         // flag atividade
    int otherTcp_fd;
    char otherIP[16];        // ip deste vizinho
    unsigned short otherTCP; // porto deste vizinho
};

typedef struct otherNode otherNode;

struct inputNode
{
    char serverIP[16];      // ip do servidor
    unsigned short portUDP; // porto UDP do servidor
    // struct in_addr nodeIP;   // ip da maquina q aloja a app
    char nodeIP[16];
    unsigned short portTCP; // porto TCP da maquina q aloja a app

    int myTcpListen_fd;
    int myUdp_fd;
    int myTcpMystery_fd[100];
    int mysteryNodes;

    int net, id, bootid;    // bootNode é o vizinho externo!
    char bootIP[16];        // ip obtido no modo djoin
    unsigned short bootTCP; // porto obtido no modo djoin

    struct sockaddr_in addr, udpAddr, tcpAddr;
    socklen_t addrlen;

    otherNode neighbours[100];
    int availableNetneigbours;

    int backupid;
    char backupIP[16];
    unsigned short backupTCP;

    tabuleta expedTable[100];
     // Convencionamos que o nosso nó guarda até MAX_CONTENTS contúdos
    char contents[MAX_CONTENTS][100];

    int getDest;          // lido no get
    int getOrig;          // lido no get
    char getContent[100]; // lido no get
};

typedef struct inputNode inputNode;

int parseParameters(inputNode *input, char **readParameters, int argc);
int initializeCom(fd_set *inputs, inputNode *myNode);
int setServerUDP(inputNode *myNode);
int setServerTCP(inputNode *myNode);
int getCommands(char in_strTeclado[], inputNode *inputNode);
int joinCheck(char *nodeList, int id, inputNode *inputNode);
int initializeNeighbours(inputNode *myNode);
int initializeExpedition(inputNode *myNode);
int initializeNode(inputNode *inputNode);
int process(inputNode *input);
int djoin(inputNode *inputNode, fd_set *inputs);
int join(inputNode *inputNode);
int randomPicker(int checkList[], int valid, inputNode *inputNode);
int sendREG(inputNode *inputNode);
int readNODELISTline(char *line, inputNode *inputNode, int *idTemp, int *valid, int *checkList);
int sendUNREG(inputNode *inputNode);
int leave(inputNode *inputNode, fd_set *inputs);
int showTopology(inputNode *inputNode);
int showRouting(inputNode *inputNode);
int showNames(inputNode *inputNode);
int sendNEWDES(inputNode *inputNode, int neighbourSocket);
int sendEXTERN(inputNode *inputNode, int neighbourSocket);
int sendCONTENT(inputNode *inputNode);
int sendWITHDRAW(inputNode *inputNode, int withdrawid, int emissor);
int sendNOCONTENT(inputNode *inputNode);
int sendQUERY(inputNode *inputNode, int emissor);
int withdraw(inputNode *inputNode, int withdrawid, int emissor);
int deleteContent(inputNode *inputNode, char *content);
int initializeContents(inputNode *myNode);

struct sockaddr_in addr;
socklen_t addrlen;

int main(int argc, char **argv)
{

    if (!(argc == 3 || argc == 5))
    {
        printf("Invalid input\nTry: cot IP TCP regIP regUDP argc < 5\n");
        return 0;
    }

    inputNode input;
    initializeNode(&input);

    if (parseParameters(&input, argv, argc)) // Valida se os parametros e coloca-os na estrutura input
    {
        printf("Invalid input\nTry: cot IP TCP regIP regUDP\n");
        return 0;
    }
    printf("Vamos ao processo!\n");

    if (~(process(&input)))
    {
        printf("ADEUS\n");
    }

    return 0;
}

int process(inputNode *inputNode)
{
    fd_set inputs, testfds;
    struct timeval timeout;
    int out_fds;
    int n;

    FD_ZERO(&inputs);   // Clear inputs
    FD_SET(0, &inputs); // Mapear os FDs

    // Inicializar os sockets com os dados do parseParameters
    initializeCom(&inputs, inputNode);

    while (1)
    {
        // A cada while o inputs vai adicionar e remover fds
        testfds = inputs;
        timeout.tv_sec = 120;
        timeout.tv_usec = 0;

        // printf("testfds byte Antes Select: %d\n",((char *)&testfds)[0]);

        out_fds = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);

        // printf("testfds byte Depois Select: %d\n",((char *)&testfds)[0]);
        // printf("VALOR FD_TCPLISTEN: %d\n", nodeBoot.tcpListen_fd);

        switch (out_fds)
        {
        case 0:
            // SE o buffer tiver cenas para o UDP e não obtiver resposta enviar de novo REG, UNREG, etc...
            printf("Timeout event\n");
            break;
        case -1:
            perror("select");
            exit(1);
        default:
            if (FD_ISSET(0, &testfds))
            {
                char inputTeclado[128];

                if ((n = read(0, inputTeclado, 127)) != 0)
                {
                    if (n == -1)
                    {
                        printf("Erro ao ler do teclado!\n");
                        exit(1);
                    }

                    inputTeclado[n] = '\0';

                    // printf("Leu %i caracteres do teclado: %s\n", n, inputTeclado);
                    // printf("........................................(KEYBOARD IN)\n");

                    if (!(strcmp(inputTeclado, "exit\n")))
                    {
                        printf("Fechando o Programa...\n");
                        leave(inputNode, &inputs);
                        exit(1);
                    }
                    else
                    {
                        inputTeclado[n - 1] = 0;

                        switch (getCommands(inputTeclado, inputNode))
                        {
                        case 1:
                            // printf("Modo join\n");
                            join(inputNode);
                            break;
                        case 2:
                            // printf("Modo leave\n");
                            leave(inputNode, &inputs);
                            break;
                        case 3:
                            // printf("Modo djoin\n");
                            djoin(inputNode, &inputs);
                            if (sendREG(inputNode))
                            {
                                printf("Something Went Wrong generating REG response!\n");
                                exit(1);
                            }
                            break;
                        case 4:
                            //printf("Modo show topology\n");
                            showTopology(inputNode);
                            break;
                        case 5:
                            //printf("Modo show routing\n");
                            showRouting(inputNode);
                            break;
                        case 6:
                            //printf("If You Stand For Nothing You'll Fall For Anything! Live Up To Your NAME\n");
                            showNames(inputNode);
                            break;
                        case 7:
                            printf("God Inspires. Man CREATEs. The Job Gets Done\n");
                            // tudo tratado dentro da getCommands()
                            break;
                        case 8:
                            //printf("DELETE The Soy Mentality\n");
                            // tudo tratado dentro da getCommands()
                            break;
                        case 9:
                            //printf("Stop Trying To GET Hoes! GET Money $$$\n");
                            // algumas cenas tratadas dentro da getCommands()
                            sendQUERY(inputNode, inputNode->id); // Neste caso estou a enviar o primeiro QUERY, é para enviar a todó mundo,
                            break;
                        case -1:
                            // printf("Houve algum erro...\n");
                            printf("**********\n"); // bad input cmd
                            break;
                        default:
                            printf("Default Case\n");
                            break;

                            // nc -u ipUDPserver portaUDPserver MANDAR UNREG xxx yy
                        }
                    }
                }
            }
            else if ((inputNode->myUdp_fd > 0) && FD_ISSET(inputNode->myUdp_fd, &testfds)) // UDP para receber NODELIST, OKREG, OKUNREG
            {
                // GUARDA AS LEITURAS NUM BUFFER
                char buffer[1024], bufferCopy[1024], *ptr, tok[] = " ";
                char modo[9];

                if ((n = recvfrom(inputNode->myUdp_fd, buffer, 1023, 0, (struct sockaddr *)&addr, &addrlen)) != 0)
                {
                    buffer[n] = '\0';
                    strcpy(bufferCopy, buffer);
                    // PQ o strtok afeta o buffer !!!

                    if (n == -1)
                    {
                        perror("ERRO Recvfrom");
                        exit(1);
                    }

                    ptr = strtok(buffer, tok);
                    if (ptr == NULL)
                    {
                        printf("Falha MODO da mensagem UDP\n");
                        return -1;
                    }

                    n = strlen(ptr);
                    if (n > 9)
                    {
                        strncpy(modo, ptr, n);
                        modo[n] = '\0';
                    }
                    else
                    {
                        strcpy(modo, ptr);
                    }

                    if (!(strcmp(modo, "NODESLIST")))
                    {
                        // printf("Recibi NODESLIST\n");
                        printf("%s\n", bufferCopy);
                        inputNode->id = joinCheck(bufferCopy, inputNode->id, inputNode); // ESCOLHER A QUEM ME VOU LIGAR NESTA LISTA

                        // VERIFICA SE HA NOS DISPONIVEIS NA REDE
                        if (!(inputNode->availableNetneigbours))
                        {
                            // Se a lista está vazia apenas tenho de me registar na rede!
                            // SEND REG e mai nada!
                            sendREG(inputNode);
                        }
                        else
                        {
                            djoin(inputNode, &inputs);
                            // SEND REG
                            if (sendREG(inputNode))
                            {
                                printf("Something Went Wrong generating REG response!\n");
                                exit(1);
                            }
                        }
                    }
                    else if (!(strcmp(modo, "OKREG")))
                    {
                        printf("Registado com sucesso!\n");
                    }
                    else if (!(strcmp(modo, "OKUNREG")))
                    {
                        printf("Desregistado com sucesso!\n");
                    }
                    else
                    {
                        printf("Resposta do servidor desconhecida\n");
                    }
                }
            }
            else if ((inputNode->myTcpListen_fd > 0) && FD_ISSET(inputNode->myTcpListen_fd, &testfds)) // RECEBER NOVAS LIGAÇOES E NAO LER LOGO PARA NAO FICAR STUCK
            {
                int newSocket, i;

                // Cria o socket temporario para depois ler e atribuir a um nó vizinho
                if ((newSocket = accept(inputNode->myTcpListen_fd, (struct sockaddr *)&addr, &addrlen)) != 0)
                {
                    if (newSocket == -1)
                        exit(1);
                }
                for (i = 0; i < 100; i++)
                {
                    if (inputNode->myTcpMystery_fd[i] == -1) // Percorre a lista e vê se ainda há algum slot temporário q ainda esteja livre
                    {
                        inputNode->myTcpMystery_fd[i] = newSocket;
                        FD_SET(inputNode->myTcpMystery_fd[i], &inputs);
                        inputNode->mysteryNodes++;
                        break;
                    }
                }
            }
            else if ((inputNode->mysteryNodes > 0))
            {
                int i, id;
                char tcpMessage[128], *ptr, tok[] = " ", *mensagemTCP;

                for (i = 0; i < 100; i++)
                {
                    if (FD_ISSET(inputNode->myTcpMystery_fd[i], &testfds))
                    {
                        if ((n = read(inputNode->myTcpMystery_fd[i], tcpMessage, 127)) != 0)
                        {
                            if (n == -1)
                            {
                                printf("Uma das Ligações mistério foi terminada COM erro\n");
                                FD_CLR(inputNode->myTcpMystery_fd[i], &inputs);
                                close(inputNode->myTcpMystery_fd[i]);
                                inputNode->myTcpMystery_fd[i] = -1;
                                inputNode->mysteryNodes--;
                                break;
                            }
                            // Ver se é NEW
                            ptr = strtok(tcpMessage, tok);
                            if (ptr == NULL)
                            {
                                printf("Falha MODO da mensagem vinda do TCP mistério\n");
                                return -1;
                            }
                            else if (!(strcmp(ptr, "NEW")))
                            {
                                printf("Recebi um NEW\n");
                                ptr = strtok(NULL, tok);

                                // neighbourid
                                if (ptr == NULL)
                                {
                                    printf("Falha TCP ACP NEW, valor id nulo\n");
                                    return -1;
                                }

                                if (strlen(ptr) > 2)
                                {
                                    printf("Falha TCP ACP NEW, valor id supeiror a 99\n");
                                    return -1;
                                }
                                else
                                {
                                    id = atoi(ptr);
                                    inputNode->neighbours[atoi(ptr)].otherActive = 1; // Vizinho interno ativo !!!

                                    // Atualizar a tabela de expedição:
                                    // inputNode->expedTable[id].tableActive = 1;
                                    // inputNode->expedTable[id].connection = id;
                                }

                                // neighbourIP
                                ptr = strtok(NULL, tok);

                                if (ptr == NULL)
                                {
                                    printf("Falha TCP ACP NEW, valor IP nulo\n");
                                    return -1;
                                }

                                if (strlen(ptr) > 15 || (!(inet_pton(AF_INET, ptr, &(inputNode->neighbours[id].otherIP)))))
                                {
                                    printf("Falha TCP ACP NEW, valor IP nulo\n");
                                    return -1;
                                }

                                else
                                {
                                    strcpy(inputNode->neighbours[id].otherIP, ptr);
                                }

                                // neighbourTCP
                                ptr = strtok(NULL, tok);

                                if (ptr == NULL)
                                {
                                    printf("Falha TCP ACP NEW, valor bootTCP nulo\n");
                                    return -1;
                                }
                                int portaInt = atoi(ptr);
                                if (portaInt < 65535 && portaInt > 0)
                                {
                                    inputNode->neighbours[id].otherTCP = (unsigned short)atoi(ptr);
                                    // printf("Porta bootTCP Correta: %d\n", inputNode->neighbours[id].otherTCP);
                                }
                                else
                                {
                                    printf("ERRO! TCP ACP NEW TCP incorreto\n");
                                    return -1;
                                }

                                printf("NEW vindo de: %d %s %d\n", id, inputNode->neighbours[id].otherIP, inputNode->neighbours[id].otherTCP);
                            }
                        }
                        else // n == 0
                        {
                            printf("Uma das Ligações mistério foi terminada SEM erro\n");
                            FD_CLR(inputNode->myTcpMystery_fd[i], &inputs);
                            close(inputNode->myTcpMystery_fd[i]);
                            inputNode->myTcpMystery_fd[i] = -1;
                            inputNode->mysteryNodes--;
                            break;
                        }

                        // PASSAR MYSTERY A VIZINHO E DAR RESET AO MYSTERY
                        // ISTO VAI TER DE IR PARA DENTRO DOS IFS PROVAVELMENTE!!!
                        inputNode->neighbours[id].otherTcp_fd = inputNode->myTcpMystery_fd[i]; // AQUI ESTOU A GURADAR O SOCKET 55378 <-> 58011 !!!
                        inputNode->myTcpMystery_fd[i] = -1;
                        inputNode->mysteryNodes--;

                        // NÓ ESTAVA SOZINHO NA REDE?
                        if ((inputNode->bootid == inputNode->backupid) && (inputNode->bootid == inputNode->id) && (inputNode->id == inputNode->backupid))
                        {
                            printf("I WAS LONELY!\n");
                            inputNode->bootid = id;
                            strcpy(inputNode->bootIP, inputNode->neighbours[id].otherIP);
                            inputNode->bootTCP = inputNode->neighbours[id].otherTCP;
                            // "(...) antes de responder com mensagem de nó externo, ele promove o vizinho entrante a vizinho externo (...)"
                        }

                        printf("LISTA VIZINHOS\n");
                        for (int z = 0; z < 100; z++)
                        {
                            if (inputNode->neighbours[z].otherActive == 1)
                            {
                                printf("Vizinho: %d, fd: %d\n", z, inputNode->neighbours[z].otherTcp_fd);
                            }
                        }

                        // ENVIAR EXTERN --- PASSAR A FUNÇÃO
                        mensagemTCP = malloc(33 * sizeof(char)); // 33 Bytes max: EXTERN
                        sprintf(mensagemTCP, "EXTERN %d %s %03d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);

                        n = write(inputNode->neighbours[id].otherTcp_fd, mensagemTCP, strlen(mensagemTCP));
                        if (n == -1 || (n != strlen(mensagemTCP)))
                        {
                            perror("Erro a enviar o EXTERN\n");
                        }
                        printf("EXTERN enviado a: %d %s %d\n", id, inputNode->neighbours[id].otherIP, inputNode->neighbours[id].otherTCP);
                    }
                }
            }

            else
            {
                // TCP vizinhos
                for (int i = 0; i < 100; i++)
                {
                    if (inputNode->neighbours[i].otherActive == 1)
                    {
                        printf("Vizinho que ativou: %d\n", i);

                        if ((inputNode->neighbours[i].otherTcp_fd > 0) && FD_ISSET(inputNode->neighbours[i].otherTcp_fd, &testfds))
                        {
                            int n;
                            char Message[128], *ptr, tok[] = " ";

                            if ((n = read(inputNode->neighbours[i].otherTcp_fd, Message, 127)) != 0)
                            {
                                // SE NAO TERMINOU A LIGAÇÂO
                                // ENTAO AQUI POSSO RECEBER EXTERN, QUERY, CONTENT, NOCONTENT, WITHDRAW

                                ptr = strtok(Message, tok);
                                // printf("CENAITA NO PTR:%s\n", ptr);
                                if (ptr == NULL)
                                {
                                    printf("Falha ler EXTERN TCP\n");
                                    return -1;
                                }
                                else if (!(strcmp(ptr, "EXTERN")))
                                {

                                    ptr = strtok(NULL, tok);

                                    // backupid
                                    if (ptr == NULL)
                                    {
                                        printf("Falha backupid nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha backupid supeiror a 99\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->backupid = atoi(ptr);

                                        // Atualiza a tabela de expedição
                                        // inputNode->expedTable[inputNode->backupid].tableActive = 1;
                                        // inputNode->expedTable[inputNode->backupid].tableActive = inputNode->backupid;
                                    }

                                    // backupIP
                                    ptr = strtok(NULL, tok);

                                    if (ptr == NULL)
                                    {
                                        printf("Falha backupIP nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 15 || (!(inet_pton(AF_INET, ptr, &(inputNode->backupIP)))))
                                    {
                                        printf("Falha backupIP nulo\n");
                                        return -1;
                                    }

                                    else
                                    {
                                        strcpy(inputNode->backupIP, ptr);
                                    }

                                    // backupTCP
                                    ptr = strtok(NULL, tok);

                                    if (ptr == NULL)
                                    {
                                        printf("Falha backupTCP nulo\n");
                                        return -1;
                                    }

                                    int portaInt = atoi(ptr);
                                    if (portaInt < 65535 && portaInt > 0)
                                    {
                                        inputNode->backupTCP = (unsigned short)atoi(ptr);
                                        // printf("Porta bootTCP Correta: %d\n", inputNode->backupTCP);
                                    }
                                    else
                                    {
                                        printf("ERRO! backupTCP incorreto\n");
                                        return -1;
                                    }

                                    printf("EXTERN recebido de: %d %s %d\n", i, inputNode->neighbours[i].otherIP, inputNode->neighbours[i].otherTCP);
                                    printf("Atualizei o backup com esta info!\n");
                                    // printf("Backup Node Info:\nid: %d\nIP: %s\nPortTCP: %d\n", inputNode->backupid, inputNode->backupIP, inputNode->backupTCP);
                                }
                                else if (!(strcmp(ptr, "QUERY")))
                                {
                                    //char name[100];
                                    // QUERY dest orig name\n

                                    // dest
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha dest nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha dest supeiror a 99: %s\n", ptr);
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getDest = atoi(ptr); // O NÓ QUE O QUERY QUER ALCANÇAR!
                                    }

                                    // origin
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha origin nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha origin supeiror a 99\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getOrig = atoi(ptr); // O NÓ QUE LANÇOU O QUERY NA REDE
                                    }

                                    // name 
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha name nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 100)
                                    {
                                        printf("Falha name superior a 100\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        memset(inputNode->getContent, 0, sizeof(inputNode->getContent)); // N SEI SE É PRECISO!!
                                        strncpy(inputNode->getContent, ptr, 100); // copia até aos 100 caractéres
                                        printf("INPUT->GETCONTEN Recebido no QUERY: %s", inputNode->getContent);
                                    }
                                    // ATUALIZAR TABELA DE EXPEDIÇAO:
                                    // QUANDO QUISER COMUNICAR COM O NÓ QUE ORIGINOU O QUERY, COMUNICO PELO NÓ QUE RECEBI O QUERY
                                    inputNode->expedTable[inputNode->getOrig].tableActive = 1;
                                    inputNode->expedTable[inputNode->getOrig].connection = i;

                                    // COMPARAR SE SOU O QUERY DEST
                                    // SE FOR: MANDO MENSAGEM CONTENT / NOCONTENT
                                    // SE NÃO FOR: MANDO QUERY AOS VIZINHOS
                                    if (inputNode->id == inputNode->getDest)
                                    {

                                        
                                        //TENHO DE TROCAR A ORDEM DO getDest COM getOrig
                                        int tempDest = inputNode->getDest;
                                        inputNode->getDest = inputNode->getOrig;
                                        inputNode->getOrig = tempDest;

                                        int hasContent = 0; // ATIVA A FLAG SE TIVER NAME NOS CONTENTS!
                                        for (int y = 0; y < MAX_CONTENTS; y++)
                                        {
                                            if (!(strcmp(inputNode->contents[y], inputNode->getContent))) //name
                                            {
                                                hasContent = 1;
                                                printf("TENHO O NAME!\n");
                                                break;
                                            }
                                        }

                                        // RESPONDER CONSOANTE A PRESENÇA DO CONTEUDO:
                                        if (hasContent)
                                        {
                                            printf("MANDAR CONTENT\n"); // FUNDS ARE SAFU !!!
                                            sendCONTENT(inputNode);
                                        }
                                        else
                                        {
                                            printf("MANDAR NOCONTENT\n"); // BOGDANOV STOLE YOUR FUNDS !!!
                                            sendNOCONTENT(inputNode);
                                        }
                                    }
                                    else // NAO SOU O DESTINO DO QUERY
                                    {
                                        printf("NO SOY EL DESTINO\n");
                                        sendQUERY(inputNode, i);
                                    }
                                }
                                else if (!(strcmp(ptr, "CONTENT")))
                                {
                                    // CONTENT dest orig

                                    // dest
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha dest nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha dest supeiror a 99: %s\n", ptr);
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getDest = atoi(ptr); // O NÓ QUE O CONTENT QUER ALCANÇAR!
                                    }

                                    // origin
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha origin nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha origin supeiror a 99\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getOrig = atoi(ptr); // O NÓ QUE LANÇOU O CONTENT NA REDE
                                    }

                                    // name
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha name nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 100)
                                    {
                                        printf("Falha name superior a 100\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        memset(inputNode->getContent, 0, sizeof(inputNode->getContent));
                                        strncpy(inputNode->getContent, ptr, 100); 
                                    }

                                    // ATUALIZAR TABELA DE EXPEDIÇAO:
                                    // QUANDO QUISER COMUNICAR COM O NÓ QUE ORIGINOU O CONTENT, COMUNICO PELO NÓ QUE RECEBI O CONTENT
                                    inputNode->expedTable[inputNode->getOrig].tableActive = 1;
                                    inputNode->expedTable[inputNode->getOrig].connection = i;


                                    if(inputNode->getDest ==inputNode->id)
                                    {
                                        // O DESTINO DO CONTENT SOU EU !
                                        // OBTENHO A CONFIRMAÇÃO DE QUE O NÓ DE QUERY TINHA O CONTEUDO
                                        printf("Recebi confirmação de que o nó de pesquisa tinha o name que eu estava a procurar!\n");
                                    }
                                    else
                                    {
                                        sendCONTENT(inputNode); // CONTENT NÃO SE DESTINA A MIM, VOU PROPAGAR A MENSAGEM 
                                        printf("Mandei CONTENT DE VOLTA\n");
                                    }
                                    
                                }
                                else if (!(strcmp(ptr, "NOCONTENT")))
                                {
                                    // NOCONTENT dest orig

                                    // dest
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha dest nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha dest supeiror a 99: %s\n", ptr);
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getDest = atoi(ptr); // O NÓ QUE O NOCONTENT QUER ALCANÇAR!
                                    }

                                    // origin
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha origin nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 2)
                                    {
                                        printf("Falha origin supeiror a 99\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        inputNode->getOrig = atoi(ptr); // O NÓ QUE LANÇOU O NOCONTENT NA REDE
                                    }

                                    // name
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha name nulo\n");
                                        return -1;
                                    }

                                    if (strlen(ptr) > 100)
                                    {
                                        printf("Falha name superior a 100\n");
                                        return -1;
                                    }
                                    else
                                    {
                                        strncpy(inputNode->getContent, ptr, 100);
                                        //int contentLen = strlen(inputNode->getContent);
                                        //inputNode->getContent[contentLen + 1] = '\0';
                                    }

                                    // ATUALIZAR TABELA DE EXPEDIÇAO:
                                    // QUANDO QUISER COMUNICAR COM O NÓ QUE ORIGINOU O NOCONTENT, COMUNICO PELO NÓ QUE RECEBI O NOCONTENT
                                    inputNode->expedTable[inputNode->getOrig].tableActive = 1;
                                    inputNode->expedTable[inputNode->getOrig].connection = i;

                                    if(inputNode->getDest == inputNode->id)
                                    {
                                        // O DESTINO DO NOCONTENT SOU EU !
                                        // OBTENHO A CONFIRMAÇÃO DE QUE O NÓ DE QUERY TINHA O CONTEUDO
                                        printf("Recebi confirmação de que o nó de pesquisa não tinha o name que eu estava a procurar!\n");
                                    }
                                    else
                                    {
                                        sendNOCONTENT(inputNode); // CONTENT NÃO SE DESTINA A MIM, VOU PROPAGAR A MENSAGEM 
                                        printf("Mandei NOCONTENT DE VOLTA\n");
                                    }
                                }
                                else if (!(strcmp(ptr, "WITHDRAW")))
                                {
                                    // WITHDRAW id

                                    int withdrawid;
                                    // QUANDO UM NÓ SE RETIRA, OS SEUS VIZINHOS DIRETOS ENVIAM O WITHDRAW PARA NOS LIMPARMOS TAMBEM A TABELA DE EXPEDIÇÃO
                                    // DESATIVAR DA TABELA DE EXPEDIÇÃO TODOS OS QUE TENHAM: DESTINO = "id" / CONNECTION "id"

                                    // id
                                    ptr = strtok(NULL, tok);
                                    if (ptr == NULL)
                                    {
                                        printf("Falha withdraw id nulo\n");
                                        break;
                                        //return -1;
                                    }

                                    if (strlen(ptr) > 3)
                                    {
                                        printf("Falha withdraw id supeiror a 99: %s\n", ptr);
                                        break;
                                        //return -1;
                                    }
                                    else
                                    {
                                        withdrawid = atoi(ptr);
                                        printf("withdrawid: %d\n", withdrawid);
                                    }

                                    withdraw(inputNode, withdrawid, i);
                                    printf("Vizinho retirado da tabela de expedição: %d\n", withdrawid);
                                }

                                else
                                {
                                    printf("----- AREA 51 -----\n");
                                }
                            }
                            else if (n == -1 || n == 0)
                            {

                                FD_CLR(inputNode->neighbours[i].otherTcp_fd, &inputs);
                                close(inputNode->neighbours[i].otherTcp_fd);
                                inputNode->neighbours[i].otherActive = 0; // Vizinho externo desativado!
                                // Atualizar a tabela de expedição:
                                withdraw(inputNode, i, inputNode->id);
                                printf("Ligação vizinho terminada!\n");


                                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                // TENHO DE VER SE O NÒ Q SAIU ERA O BOOT (VIZINHO EXTERNO)
                                // SE FOR O BOOT TENHO DE METER O BACKUP NO BOOT
                                // CRIAR NOVO SOCKET PARA COMUNICAR COM O NOVO BOOT!
                                // E MANDAR "NEW" AO BOOT & "EXTERN" AOS INTERNOS
                                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                                if (i == inputNode->bootid) // O NO QUE TERMINOU A LIGAÇAO ERA O EXTERNO
                                {
                                    printf("O VIZINHO Q SE DESLIGOU ERA O EXTERNO (inputNode->boot)\n");

                                    // O MEU EXTERNO FECHOU E SOU UM NÓ ÂNCORA
                                    if (inputNode->id == inputNode->backupid)
                                    {
                                        int lonely = 0; // Se mantiver a 0 tou lonely
                                        for (int j = 0; j < 100; j++)
                                        {
                                            // Selecionar um nó dos internos que esteja livre para ser o novo externo!
                                            if (inputNode->neighbours[j].otherActive == 1 && (inputNode->id != j) && (inputNode->bootid != j)) // Estas 2 ultimas por segurança
                                            {
                                                inputNode->bootid = j;
                                                strcpy(inputNode->bootIP, inputNode->neighbours[j].otherIP);
                                                inputNode->bootTCP = inputNode->neighbours[j].otherTCP;
                                                lonely = 1;

                                                // djoin(inputNode, &inputs);
                                                //  Cria nova ligaçao para externo
                                                //  SEND NUDES
                                                sendNEWDES(inputNode, inputNode->neighbours[j].otherTcp_fd);

                                                // MANDAR "EXTERN" AOS INTERNOS
                                                for (int p = 0; p < 100; p++)
                                                {
                                                    if (inputNode->neighbours[p].otherActive == 1 && (inputNode->id != p)) // NÓ É VIZINHO INTERNO, NAO ELE PROPRIO
                                                    {
                                                        printf("EXTERN enviado a: %d %s %d\n", p, inputNode->neighbours[p].otherIP, inputNode->neighbours[p].otherTCP);
                                                        sendEXTERN(inputNode, inputNode->neighbours[p].otherTcp_fd);
                                                    }
                                                }

                                                break; // Já promovi um dos internos a externo e atualizei os backups dos internos, parou o loop!!!
                                            }
                                        }
                                        if (!lonely)
                                        {
                                            printf("Não tinha vizinhos internos, devo atualizar o boot para mim mesmo!\n");
                                            inputNode->bootid = inputNode->id;
                                            strcpy(inputNode->bootIP, inputNode->nodeIP);
                                            inputNode->bootTCP = inputNode->portTCP;
                                            // join() automatico !!!! O nó não pode ficar abandonado na rede!!!!!!!!!!
                                        }
                                    }
                                    else // O MEU EXTERNO FECHOU E SOU UM NÓ REGULAR
                                    {
                                        // NOVO BOOT, QUE ERA O BACKUP
                                        inputNode->bootid = inputNode->backupid;
                                        strcpy(inputNode->backupIP, inputNode->nodeIP);
                                        inputNode->bootTCP = inputNode->backupTCP;
                                        // ATUALIZEI O EXTERNO COM A INFO DE BACKUP MAS....
                                        // NÃO TENHO NEHUMA SOCKET, NEM FIZ NENHUM CONNECT PARA ESTE NOVO BOOT!!!
                                        // DEVO USAR djoin() em vez de sendNEWDES() !!!

                                        if (djoin(inputNode, &inputs) == -1) // djoin() = CRIAR SOCKET PARA FALAR COM O NOVO BOOT + MANDAR-LHE "NEW"
                                        {
                                            // O Nó backup pelos vistos já não faz parte da rede
                                            printf("O nó backup não está dísponivel!\n");
                                            // ficou sozinho, sem ligações
                                            // procura novo nó para se ligar dentro da rede
                                            // join() automatico !!!! O nó não pode ficar abandonado na rede!!!!!!!!!!
                                            break;
                                        }

                                        /*if (sendNEWDES(inputNode, inputNode->neighbours[inputNode->bootid].otherTcp_fd) == -1)
                                        {
                                            // O Nó backup pelos vistos já não faz parte da rede
                                            printf("sendNEWDES para o backup correu mal\n");
                                            break;
                                            // join();
                                        } */

                                        // MANDAR "EXTERN" AOS INTERNOS
                                        for (int j = 0; j < 100; j++)
                                        {
                                            if (inputNode->neighbours[j].otherActive == 1 && (inputNode->id != j)) // NO É VIZINHO INTERNO, NAO EXTERNO, NAO ELE PROPRIO
                                            {
                                                printf("EXTERN enviado a: %d %s %d\n", j, inputNode->neighbours[j].otherIP, inputNode->neighbours[j].otherTCP);
                                                sendEXTERN(inputNode, inputNode->neighbours[j].otherTcp_fd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

int parseParameters(inputNode *input, char **readParameters, int argc)
{

    // Inicializar o backup com os dados do externo, foi inicializado a zeros!
    // "os nós âncoras têm-se a si próprios como nós de recuperação"
    // Aqui só atualizámos o IP e porta! O id é inicializado no processamento do comando join

    // Validar IP: SE tem 4 pontos, se tem so numeros, se nao tem mais de 16 caracters
    struct in_addr test;
    int retVal = inet_pton(AF_INET, readParameters[1], &(test)); // &(input->serverIP) guarda logo no serverIP q é do tipo "struct in_addr"

    if (retVal)
    {
        // printf("IP da app esta Correto: %s\n", readParameters[1]);
        strcpy(input->nodeIP, readParameters[1]);
        strcpy(input->bootIP, input->nodeIP);
        strcpy(input->backupIP, input->nodeIP);
    }
    else
    {
        printf("ERRO! IP da app invalida!\n");
        return 1;
    }

    // Validar Porta TCP: SE vai de 0 a 65535
    // unsigned long int  retPort = strtoul(readParameters[2], NULL, 10);

    int portaInt = atoi(readParameters[2]);
    if (portaInt < 65535 && portaInt > 0)
    {
        input->portTCP = (unsigned short)atoi(readParameters[2]);
        input->bootTCP = input->portTCP;
        input->backupTCP = input->portTCP;
        // printf("Porta TCP/bootTCP Correta: %d\n", input->bootTCP);
    }
    else
    {
        printf("ERRO! PORT TCP incorreto\n");
        return -1;
    }
    input->portUDP = DEFAULT_UDP;
    strcpy(input->serverIP, DEFAULT_IP);

    // Validar IP do servidor dos docentes
    if(argc == 5)
    {
        retVal = inet_pton(AF_INET, readParameters[3], &test);

        if (retVal)
        {
            // printf("IP do servidor de nos esta Correto: %s\n", readParameters[3]);
            strcpy(input->serverIP, readParameters[3]);
        }
        else
        {
            printf("ERRO! IP da app invalida!\n");
            return 1;
        }

        // Validar Porta UDP: SE vai de 0 a 65575
        portaInt = atoi(readParameters[4]);
        if (portaInt < 65535 && portaInt > 0)
        {
            input->portUDP = (unsigned short)atoi(readParameters[4]);
            // printf("Porta UDP Correta: %d\n", input->portUDP);
        }
        else
        {
            printf("ERRO! PORT UDP incorreto\n");
            return -1;
        }
    }
    return 0;
}

int getCommands(char in_strTeclado[], inputNode *inputNode)
{
    // getCommands vai retornar Flags para ativar o switch case fora da função

    char *ptr, tok[] = " ", modo[10];

    ptr = strtok(in_strTeclado, tok);
    if (ptr == NULL)
    {
        // printf("Falha a obter modo\n");
        return -1;
    }

    if (strlen(ptr) > 9)
    {
        strncpy(modo, ptr, 9);
        modo[9] = '\0';
    }
    else
    {
        strcpy(modo, ptr);
    }
    // Ao chegar aqui já lemos o modo do teclado

    if ((!(strcmp(modo, "join")))) // join net id
    {

        ptr = strtok(NULL, tok); // ptr: ponteiro temporario para ler as partes q compoem a string q vem do teclado

        // Obtemos o valor de net
        if (ptr == NULL)
        {
            printf("Falha join, valor net nulo\n");
            return -1;
        }

        if (strlen(ptr) > 3)
        {
            printf("Falha join, valor net superior a 999\n");
            return -1;
        }
        else
        {
            inputNode->net = atoi(ptr); // valor de net obtido (int) strtol(ptr, NULL, 10)
        }

        // Valor do id

        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha join, valor id nulo\n");
            return -1;
        }

        if (strlen(ptr) > 2)
        {
            printf("Falha join, valor id supeiror a 99, ptr: %s\n", ptr);
            return -1;
        }
        else
        {
            inputNode->id = atoi(ptr);
            inputNode->bootid = inputNode->id;
            inputNode->backupid = inputNode->id;
        }

        return 1;
    }
    else if (!(strcmp(modo, "leave")))
    {
        return 2;
    }
    else if (!(strcmp(modo, "djoin")))
    {

        ptr = strtok(NULL, tok);

        // net
        if (ptr == NULL)
        {
            printf("Falha djoin, valor net nulo\n");
            return -1;
        }

        if (strlen(ptr) > 3)
        {
            printf("Falha djoin, valor net superior a 999\n");
            return -1;
        }
        else
        {
            inputNode->net = atoi(ptr); // valor de net obtido
        }

        // id
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha djoin, valor id nulo\n");
            return -1;
        }

        if (strlen(ptr) > 2)
        {
            printf("Falha djoin, valor id supeiror a 99\n");
            return -1;
        }
        else
        {
            inputNode->id = atoi(ptr);
        }

        // bootid
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha djoin, valor id nulo\n");
            return -1;
        }

        if (strlen(ptr) > 2)
        {
            printf("Falha djoin, valor id supeiror a 99\n");
            return -1;
        }
        else
        {
            inputNode->bootid = atoi(ptr);
        }

        // bootIP
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha djoin, valor IP nulo\n");
            return -1;
        }

        if (strlen(ptr) > 15 || (!(inet_pton(AF_INET, ptr, &(inputNode->bootIP)))))
        {
            printf("Falha djoin, valor IP nulo\n");
            return -1;
        }

        else
        {
            strcpy(inputNode->bootIP, ptr);
            // strncpy(inputNode->bootIP,ptr,15);
            // inputNode->bootIP[15] = '\0';
        }

        // bootTCP
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha djoin, valor bootTCP nulo\n");
            return -1;
        }

        int portaInt = atoi(ptr);
        if (portaInt < 65535 && portaInt > 0)
        {
            inputNode->bootTCP = (unsigned short)atoi(ptr);
        }
        else
        {
            printf("ERRO! djoin bootUDP incorreto\n");
            return -1;
        }

        return 3;
    }
    else if (!(strcmp(modo, "st")))
    {
        return 4;
    }
    else if (!(strcmp(modo, "sr")))
    {
        return 5;
    }
    else if (!(strcmp(modo, "sn")))
    {
        return 6;
    }
    else if (!(strcmp(modo, "create")))
    {
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha create, sem conteudo!\n");
            return -1;
        }
        else
        {
            if (strlen(ptr) > 100)
            {
                printf("Nome do conteúdo excede os 100 caracteres\n");
                return -1;
            }

            int avalability = 0; // Se não entrar no if dentro do ciclo não há nenhuma slot dísponivel, mantém-se a zero!
            for (int i = 0; i < MAX_CONTENTS; i++)
            {
                if (strlen(inputNode->contents[i]) == 0) // string slot está vazia guarda o conteúdo nessa slot
                {
                    avalability = 1;
                    strcpy(inputNode->contents[i], ptr);
                    //printf("%s\n", inputNode->contents[i]);
                    break;
                }
            }
            if(!avalability)
            {
                printf("Não havia slots de conteúdo disponíveis! Não foi possível criar o nome!\n");
            }
        }
        return 7;
    }
    else if (!(strcmp(modo, "delete")))
    {
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha delete, sem conteudo!\n");
            return -1;
        }
        else
        {
            if (strlen(ptr) > 100)
            {
                printf("Nome do conteúdo excede os 100 caracteres\n");
                return -1;
            }

            deleteContent(inputNode, ptr);
        }
        return 8;
    }
    else if (!(strcmp(modo, "get")))
    {
        // id dest
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha get, valor id dest nulo\n");
            return -1;
        }

        if (strlen(ptr) > 2)
        {
            printf("Falha get, valor id dest supeiror a 99\n");
            return -1;
        }

        else
        {
            inputNode->getDest = atoi(ptr);
        }

        // string dest
        ptr = strtok(NULL, tok);

        if (ptr == NULL)
        {
            printf("Falha get, sem destino!\n");
            return -1;
        }
        if (strlen(ptr) > 100)
        {
            printf("Nome do conteúdo excede os 100 caracteres\n");
            return -1;
        }
        else
        {
            strcpy(inputNode->getContent, ptr);
        }

        // id Orig - não estava inicializado
        inputNode->getOrig = inputNode->id;

        return 9;
    }
    else
    {
        // printf("Comando não reconhecido\n");
        return -1;
    }

    return 0;
}

int initializeCom(fd_set *inputs, inputNode *myNode)
{
    if (setServerUDP(myNode))
    {
        printf("setServerUDP Error\n");
        exit(1);
    }
    FD_SET(myNode->myUdp_fd, inputs);

    if (setServerTCP(myNode))
    {
        perror("setServerTCP Error\n");
        exit(1);
    }
    FD_SET(myNode->myTcpListen_fd, inputs);

    return 0;
}

int setServerUDP(inputNode *myNode)
{
    int bindUDP;
    struct sockaddr_in udpAddrDummy;
    unsigned short portUDPDummy = myNode->portTCP; ///////// UDP e TCP podem ter a mesma porta!

    myNode->myUdp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (myNode->myUdp_fd == -1)
    {
        perror("UDP SOCKET\n");
        exit(1);
    }

    memset(&udpAddrDummy, 0, sizeof(udpAddrDummy));
    udpAddrDummy.sin_family = AF_INET;
    udpAddrDummy.sin_port = htons(portUDPDummy);
    inet_aton("0.0.0.0", &udpAddrDummy.sin_addr);

    bindUDP = bind(myNode->myUdp_fd, (struct sockaddr *)&udpAddrDummy, sizeof(udpAddrDummy));
    if (bindUDP == -1)
    {
        perror("UDP BIND\n");
        exit(1);
    }

    return 0;
}

int setServerTCP(inputNode *myNode)
{
    int bindTCP;
    struct sockaddr_in tcpAddrDummy;

    myNode->myTcpListen_fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket para ficar a escuta
    if (myNode->myTcpListen_fd == -1)
    {
        perror("TCP SOCKET\n");
        exit(1);
    }

    memset(&tcpAddrDummy, 0, sizeof(tcpAddrDummy));
    tcpAddrDummy.sin_family = AF_INET;
    tcpAddrDummy.sin_port = htons(myNode->portTCP);
    inet_aton("0.0.0.0", &tcpAddrDummy.sin_addr); // !!!! "0.0.0.0"
    // inet_aton(newNode->addressIP,&tcpAddr.sin_addr);

    bindTCP = bind(myNode->myTcpListen_fd, (struct sockaddr *)&tcpAddrDummy, sizeof(tcpAddrDummy));
    if (bindTCP == -1)
    {
        perror("TCP BIND\n");
        exit(1);
    }

    if (listen(myNode->myTcpListen_fd, 5) == -1) // TCP fica a escuta com backlog = 5
    {
        perror("TCP LISTEN\n");
        exit(1);
    }

    return 0;
}

int joinCheck(char *nodeList, int id, inputNode *inputNode)
{
    // printf("string recebida no joinckeck: %s\n", nodeList);

    char *ptr, *saveptr, buffer[1024];
    int checkList[100] = {0}, n, valid = 0, idTemp;

    char tok2[] = "\n";
    ptr = strtok_r(nodeList, tok2, &saveptr);

    if (ptr == NULL)
    {
        // LISTA VAZIA !!!! FUTURO PROBLEMS
        printf("Falha joinCheck!\n");
        return -1;
    }

    while (1)
    {
        ptr = strtok_r(NULL, tok2, &saveptr);
        // printf("PTR inicio do while = %s\n", ptr);
        if (ptr == NULL)
        {
            // printf("ACABOU TUDO O Q HAVIA PARA LER!\n");
            break;
        }
        // printf("Strtok dentro do while: %s\n", ptr);

        n = strlen(ptr);
        strcpy(buffer, ptr);
        buffer[n] = '\0';

        if (readNODELISTline(buffer, inputNode, &idTemp, &valid, checkList) == -1)
        {
            printf("LEITURA DE LINHA INVALIDA!!!");
        }
    }

    idTemp = id; // Guarda o id passado à função para se alterarmos já sabermos!

    while (1)
    {
        if (checkList[id] == 1)
        {
            id++;
            if (id == 100)
            {
                id = 0;
            }
        }
        else
        {
            break;
        }
    }

    if (idTemp != id)
    {
        printf("id: %i Ocupado -> Novo id: %i\n", idTemp, id);
    }
    
    // ESCOLHER O BOOT NODE RANDOMLY
    int random;
    random = randomPicker(checkList, valid, inputNode);

    if (random == -1)
    {
        printf("Lista de nós vazia!\n");
        return id;
    }

    inputNode->bootid = random;
    strcpy(inputNode->bootIP, inputNode->neighbours[random].otherIP);
    inputNode->bootTCP = inputNode->neighbours[random].otherTCP;

    return id;
}

int readNODELISTline(char *line, inputNode *inputNode, int *idTemp, int *valid, int *checkList)
{
    char tok[] = " ", *ptr;

    ptr = strtok(line, tok);
    // printf("Primeiro srtrtok dentro da readNODES, devia ter o ip: %s\n", ptr);

    // Obtemos o id
    if (ptr == NULL || strlen(ptr) > 2)
    {
        printf("Falha readNODESLIST, valor id null ou id superior a 99, ptr: -->%s\n", ptr);
        return -1;
    }

    else
    {
        *idTemp = atoi(ptr); // Obtem o id de cada linha da list
        if (*idTemp < 0 || *idTemp > 99)
        {
            printf("FALHA readNODESLIST: id fora dos limites!\n");
            return -1;
        }

        checkList[*idTemp] = 1; // indice checkado!
        (*valid)++;
    }

    // Obtemos o IP
    ptr = strtok(NULL, tok);
    // printf("Segundo strtok dentro da readNODES, devia ter o ip: %s\n", ptr);

    if (strlen(ptr) > 15 || (!(inet_pton(AF_INET, ptr, &(inputNode->neighbours[*idTemp].otherIP)))))
    {
        printf("Falha joinCheck, valor IP nulo\n");
        return -1;
    }

    else
    {
        strcpy(inputNode->neighbours[*idTemp].otherIP, ptr);
    }

    // Obtemos a Port
    ptr = strtok(NULL, tok);
    // printf("Terceiro strtok dentro da readNODES, devia ter a porta: %s\n", ptr);

    if (ptr == NULL)
    {
        printf("Falha joinCheck, valor bootTCP nulo\n");
        return -1;
    }

    int portaInt = atoi(ptr);
    if (portaInt < 65535 && portaInt > 0)
    {
        inputNode->neighbours[*idTemp].otherTCP = (unsigned short)atoi(ptr);
        // printf("Porta bootTCP Correta: %d\n", inputNode->bootTCP);
    }
    else
    {
        printf("ERRO! joinCheck bootTCP incorreto\n");
        return -1;
    }

    return 0;
}

int randomPicker(int checkList[], int valid, inputNode *inputNode)
{
    int validList[valid], i, ctr = 0, index;

    // Preenche o vetor de nós válidos a partir dos que estão ativos na checkList
    for (i = 0; i < 100; i++)
    {
        if (checkList[i] == 1)
        {
            validList[ctr] = i;
            // printf("VALIDLIST[%d] = %d\n", ctr, i);
            ctr++;
        }
    }

    if (!ctr) // NAO HAVIA NOS NA LISTA! ctr = 0
    {
        inputNode->availableNetneigbours = 0;
        return -1;
    }
    inputNode->availableNetneigbours = ctr;

    // Escolhe indice do vetor de nós válidos ao calha
    srand(time(0));
    index = rand() % valid;

    // printf("VOU ME LIGAR AO: %d\n", validList[index]);
    return validList[index];
}

int initializeNode(inputNode *inputNode)
{
    for (int i = 0; i < 100; i++)
    {
        memset(inputNode->serverIP, '0', 16);
        memset(inputNode->nodeIP, '0', 16);
        memset(inputNode->bootIP, '0', 16);
        memset(inputNode->backupIP, '0', 16);
        inputNode->myTcpMystery_fd[i] = -1;
    }

    inputNode->portUDP = 0;
    inputNode->portTCP = 0;
    inputNode->bootTCP = 0;
    inputNode->backupTCP = 0;

    inputNode->myTcpListen_fd = -1;
    inputNode->myUdp_fd = -1;

    inputNode->net = -1;
    inputNode->id = -1;
    inputNode->bootid = -1;
    inputNode->backupid = -1;
    inputNode->availableNetneigbours = 0;
    inputNode->mysteryNodes = 0;

    initializeNeighbours(inputNode);
    initializeExpedition(inputNode);
    initializeContents(inputNode);

    inputNode->getDest = -1;
    inputNode->getOrig = -1;

    return 0;
}

int initializeNeighbours(inputNode *myNode)
{
    for (int i = 0; i < 100; i++)
    {
        myNode->neighbours[i].otherActive = 0; // 0 = off
        myNode->neighbours[i].otherTcp_fd = -1;
        memset(myNode->neighbours[i].otherIP, '0', 16);
        myNode->neighbours[i].otherTCP = 0;
    }

    return 0;
}

int initializeExpedition(inputNode *myNode)
{
    for (int i = 0; i < 100; i++)
    {
        myNode->expedTable[i].tableActive = 0; // 0 = off
        myNode->expedTable[i].destiny = i;
        myNode->expedTable[i].connection = -1;
    }
    return 0;
}

int initializeContents(inputNode *myNode)
{
    for (int i = 0; i < MAX_CONTENTS; i++)
    {
        myNode->contents[i][0] = '\0';
    }
    return 0;
}

int djoin(inputNode *inputNode, fd_set *inputs)
{

    char *mensagemTCP;
    int connectTCP, n;

    // Parte do TCP!
    // Comunicar com o nosso vizinho externo, a quem nos vamos ligar

    struct sockaddr_in addrTCP;

    memset(&addrTCP, 0, sizeof(addrTCP));
    addrTCP.sin_family = AF_INET;
    addrTCP.sin_port = htons(inputNode->bootTCP);
    inet_aton(inputNode->bootIP, &addrTCP.sin_addr);

    int neighbourSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (neighbourSocket == -1)
    {
        perror("TCP SOCKET VIZINHO\n");
        return -1;
    }

    // Guardar o fd no vetor de vizinhos, atualizar vizinho correspondente
    inputNode->neighbours[inputNode->bootid].otherTcp_fd = neighbourSocket;

    //  Compor uma mensagem NEW id nodeIP portTCP
    mensagemTCP = malloc(30 * sizeof(char)); // 29 Bytes max: NEW
    sprintf(mensagemTCP, "NEW %d %s %d\n", inputNode->id, inputNode->nodeIP, inputNode->portTCP);

    // printf("Mensagem a enviar: %s\n", mensagemTCP);

    connectTCP = connect(neighbourSocket, (struct sockaddr *)&addrTCP, sizeof(addrTCP));
    if (connectTCP == -1)
    {
        perror("TCP NEW");
        return -1;
    }
    // printf("Connect: DONE!\n");

    // PASSA A SER UM VIZINHO A QUEM TENHO DE ESTAR ATENTO! NESTE CASO VIZINHO EXTERNO!
    inputNode->neighbours[inputNode->bootid].otherActive = 1; // flag deste nó fica ativa!
    strcpy(inputNode->neighbours[inputNode->bootid].otherIP, inputNode->bootIP);
    inputNode->neighbours[inputNode->bootid].otherTCP = inputNode->bootTCP;

    // para ser bullet prof, visto q retorna n bytes sent
    n = write(neighbourSocket, mensagemTCP, strlen(mensagemTCP));
    if (n == -1 || (n != strlen(mensagemTCP))) // SE FOR -1 faço close e fd_clear
    {
        perror("Erro a enviar o NEW");
        return -1;
    }

    printf("NEW enviado a: %d %s %d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);

    FD_SET(neighbourSocket, inputs);
    free(mensagemTCP);
    return 0;
}

int sendREG(inputNode *inputNode)
{
    // Vamos registar o nosso nó no servidor
    struct sockaddr_in addrUDP;
    char *mensagemUDP;
    int n;

    // REG net id nodeIP portTCP

    memset(&addrUDP, 0, sizeof(addrUDP));
    addrUDP.sin_family = AF_INET;
    addrUDP.sin_port = htons(inputNode->portUDP);
    inet_aton(inputNode->serverIP, &addrUDP.sin_addr);

    mensagemUDP = malloc(33 * sizeof(char)); // 30 Bytes max: REG
    sprintf(mensagemUDP, "REG %03d %02d %s %d", inputNode->net, inputNode->id, inputNode->nodeIP, inputNode->portTCP);

    n = sendto(inputNode->myUdp_fd, mensagemUDP, (strlen(mensagemUDP) +1 ), 0, (struct sockaddr *)&addrUDP, sizeof(addrUDP));
    if (n == -1)
    {
        perror("ERRO sendto");
        exit(1);
    }
    printf("Registado na rede: %d\n", inputNode->net);
    free(mensagemUDP);
    return 0;
}

int join(inputNode *inputNode)
{
    // printf("Vamos pedir a lista de nós ao servidor numero: %ld, endereço %s:%d\n", inputNode->net, inputNode->serverIP, inputNode->portUDP);
    struct sockaddr_in addrUDP;
    char *mensagemUDP;
    int n;

    memset(&addrUDP, 0, sizeof(addrUDP));
    addrUDP.sin_family = AF_INET;
    addrUDP.sin_port = htons(inputNode->portUDP);
    inet_aton(inputNode->serverIP, &addrUDP.sin_addr);

    mensagemUDP = malloc(10 * sizeof(char)); // 9 Bytes max: NODES 999
    // snprintf(mensagemUPD, 10, "NODES %li",inputNode->net)
    sprintf(mensagemUDP, "NODES %03i", inputNode->net);

    // printf("Mensagem a enviar: %s\n", mensagemUDP);
    // printf("UDP fd:%d\n", inputNode->myUdp_fd);
    n = sendto(inputNode->myUdp_fd, mensagemUDP, strlen(mensagemUDP), 0, (struct sockaddr *)&addrUDP, sizeof(addrUDP));
    if (n == -1)
    {
        perror("ERRO sendto");
        exit(1);
    }
    // printf("Já enviei mensagem NODES ao servidor!!!\nEsperando receber NODESLIST...\n");

    printf("Pedido de lista de nós à rede: %d\n", inputNode->net);
    free(mensagemUDP);
    return 0;
}

int leave(inputNode *inputNode, fd_set *inputs)
{
    // O NÓ Q SAI SÓ FECHA TUDO, AGORA OS OUTROS Q SE AMANHEM

    // LIMPA VIZINHOS INTERNOS
    for (int i = 0; i < 100; i++)
    {
        // LIMPA O SOCKET
        FD_CLR(inputNode->neighbours[i].otherTcp_fd, inputs);
        if((inputNode->neighbours[i].otherTcp_fd) != -1)
        {
            close(inputNode->neighbours[i].otherTcp_fd); // para não estar a dar close a cenas inválidas
        }
        inputNode->neighbours[i].otherTcp_fd = -1;

        // LIMPA A INFO
        inputNode->neighbours[i].otherActive = 0; // TENHO DE POR INTERNOS .otherActive = 0
        memset(inputNode->neighbours[i].otherIP, '0', 16);
        inputNode->neighbours[i].otherTCP = 0;
    }

    // LIMPA VIZINHO EXTERNO, VOLTA A SER ELE MESMO
    inputNode->bootid = inputNode->id;
    inputNode->bootTCP = inputNode->portTCP;
    strcpy(inputNode->bootIP, inputNode->nodeIP);

    // LIMPA VIZINHO BACKUP, VOLTA A SER ELE MESMO
    inputNode->backupid = inputNode->id;
    inputNode->backupTCP = inputNode->portTCP;
    strcpy(inputNode->backupIP, inputNode->nodeIP);

    sendUNREG(inputNode);

    // LIMPA A TABELA DE EXPEDIÇÃO
    initializeExpedition(inputNode);

    return 0;
}

int sendNEWDES(inputNode *inputNode, int neighbourSocket)
{
    int n;
    char *mensagemTCP;

    mensagemTCP = malloc(30 * sizeof(char)); // 29 Bytes max: NEW
    sprintf(mensagemTCP, "NEW %d %s %d\n", inputNode->id, inputNode->nodeIP, inputNode->portTCP);

    n = write(neighbourSocket, mensagemTCP, (strlen(mensagemTCP)+1));
    if (n == -1 || (n != (strlen(mensagemTCP)+1))) // SE FOR -1 faço close e fd_clear
    {
        perror("Erro a enviar o NEW");
        return -1;
    }

    printf("NEW enviado a: %d %s %d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);

    free(mensagemTCP);
    return 0;
}
int sendEXTERN(inputNode *inputNode, int neighbourSocket)
{
    char *mensagemTCP;
    int n;

    mensagemTCP = malloc(30 * sizeof(char)); // 30 Bytes max: EXTERN
    sprintf(mensagemTCP, "EXTERN %d %s %03d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);
    printf("enviado: EXTERN %d %s %03d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);
    printf("LIXARADA ENVIADA no sendEXTERN: %s\n", mensagemTCP);

    n = write(neighbourSocket, mensagemTCP, (strlen(mensagemTCP)+1));
    if (n == -1 || (n != (strlen(mensagemTCP)+1)))
    {
        perror("Erro a enviar o EXTERN\n");
    }
    free(mensagemTCP);
    return 0;
}

int sendUNREG(inputNode *inputNode)
{
    // Vamos registar o nosso nó no servidor
    struct sockaddr_in addrUDP;
    char *mensagemUDP;
    int n;

    // UNREG net id

    memset(&addrUDP, 0, sizeof(addrUDP));
    addrUDP.sin_family = AF_INET;
    addrUDP.sin_port = htons(inputNode->portUDP);
    inet_aton(inputNode->serverIP, &addrUDP.sin_addr);

    mensagemUDP = malloc(15 * sizeof(char)); // 12 Bytes max: UNREG
    sprintf(mensagemUDP, "UNREG %03d %02d", inputNode->net, inputNode->id);

    n = sendto(inputNode->myUdp_fd, mensagemUDP, (strlen(mensagemUDP) +1), 0, (struct sockaddr *)&addrUDP, sizeof(addrUDP));
    if (n == -1)
    {
        perror("ERRO sendto");
        exit(1);
    }
    printf("Desregistado na rede: %d\n", inputNode->net);
    free(mensagemUDP);
    return 0;
}

int showTopology(inputNode *inputNode)
{
    int flag = 0;
    printf("----------- BACKUP -----------\n\n");
    printf("id: %d || IP: %s  || PortTCP: %d\n", inputNode->backupid, inputNode->backupIP, inputNode->backupTCP);
    printf("\n----------- EXTERN -----------\n\n");
    printf("id: %d || IP: %s || PortTCP: %d\n", inputNode->bootid, inputNode->bootIP, inputNode->bootTCP);
    printf("\n----------- INTERN  ----------\n\n");
    for (int i = 0; i < 100; i++)
    {
        if (inputNode->neighbours[i].otherActive == 1)
        {

            if (inputNode->bootid != i)
            {
                printf("id: %d || IP: %s || PortTCP: %d\n", i, inputNode->neighbours[i].otherIP, inputNode->neighbours[i].otherTCP);
                flag = 1;
            }
            else if (inputNode->id == inputNode->backupid) // SÓ TEMOS UM VIZINHO LOGO É O ÂNCORA
            {
                printf("id: %d || IP: %s || PortTCP: %d (ANCHOR)\n", i, inputNode->neighbours[i].otherIP, inputNode->neighbours[i].otherTCP);
                flag = 1;
            }
        }
    }

    if (flag == 0)
    {
        printf("!!! NO INTERNAL NODES YET !!!\n");
    }
    printf("\n------------------------------\n\n");
    return 0;
}

int showRouting(inputNode *inputNode)
{
    int empty = 1;
    printf("----------- ROUTES -----------\n\n");
    printf("Destino:       |      Vizinho:\n");

    for (int i = 0; i < 100; i++)
    {
        if (inputNode->expedTable[i].tableActive == 1)
        {
            empty = 0;
            printf("%d              |      %d\n", inputNode->expedTable[i].destiny, inputNode->expedTable[i].connection);
        }
    }
    if(empty)
    {
        printf("\n!!! NO AVAILABLE ROUTES YET !!!\n");                       // ATUALIZAR TABELAS QUANDO RECEBO UM WITHDRAW TMB!!! 
    }
    printf("\n------------------------------\n\n");
    return 0;
}

int showNames(inputNode *inputNode)
{
    int empty = 1;
    printf("----------- NAMES  -----------\n\n");
    for (int i = 0; i < MAX_CONTENTS; i++)
    {
        if (strlen(inputNode->contents[i]) != 0) // string slot está vazia guarda o conteúdo nessa slot
        {
            empty = 0;
            printf("--> %s\n", inputNode->contents[i]);
        }
    }
    if(empty)
    {
        printf("!!! NO CONTENT NAMES YET !!!\n");
    }
    printf("\n------------------------------\n\n");
    return 0;
}

int sendQUERY(inputNode *inputNode, int emissor)
{
    // ENVIAR UMA MENSAGEM QUERY AOS VIZINHOS!!! MENOS AO Q ME MANDOU O QUERY (emissor)
    char *mensagemTCP;
    int i, n, connectionID;
    int len = snprintf(NULL, 0, "QUERY %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
    mensagemTCP = malloc(len +1);
    if (mensagemTCP != NULL)
    {
        snprintf(mensagemTCP, len+1, "QUERY %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
        mensagemTCP[len+1] = '\0';
    }
    else
    {
        printf("Erro a alocar memória para enviar o QUERY\n");
        return -1;
    }
    printf("Prestes a enviar: %s", mensagemTCP); // Já inclui o "\n"
    
    //mensagemTCP = malloc(124 * sizeof(char)); 
    //sprintf(mensagemTCP, "QUERY %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);

    // VERIFICAR SE TEM O DESTINO NA TABELA DE EXPEDIÇÃO, QUE NÃO SEJA O Q ME MANDOU O QUERY!
    for (i = 0; i < 100; i++)
    {
        if ((inputNode->expedTable[i].tableActive == 1) && (i != emissor))
        {
            if (inputNode->getDest == inputNode->expedTable[i].destiny)
            {
                connectionID = inputNode->expedTable[i].connection;
                n = write(inputNode->neighbours[connectionID].otherTcp_fd, mensagemTCP, (strlen(mensagemTCP) + 1 ));
                if (n == -1 || (n != strlen(mensagemTCP)))
                {
                    perror("Erro a enviar o QUERY\n"); // COUNTER COM ERROS ?!?!?!
                }
                printf("QUERY enviado pela tabela ao: %d\n", i);
                return 0;
            } 
        }
    }

    // SE NÃO TIVER, ESPALHA POR TODOS, MENOS PELO Q ME MANDOU O QUERY
    // ENVIAR UMA MENSAGEM DE PROCURA DE CONTEÙDOS A TODOS OS VIZINHOS
    for (int i = 0; i < 100; i++)
    {
        if ((inputNode->neighbours[i].otherActive == 1 ) && (i != emissor))
        {
            n = write(inputNode->neighbours[i].otherTcp_fd, mensagemTCP, (strlen(mensagemTCP) + 1 ));
            if (n == -1 || (n != (strlen(mensagemTCP) + 1)))
            {
                perror("Erro a enviar o QUERY\n");
            }
            printf("QUERY enviado ao interno: %d\n", i);
        }
    }
    free(mensagemTCP);
    return 0; // COUNTER COM ERROS ?!?!?!
}

int sendCONTENT(inputNode *inputNode)
{
    // ENVIAR CONTENT DE VOLTA PELO NÓ QUE ME MANDOU O QUERY!!!
    char *mensagemTCP;
    int n;

    int len = snprintf(NULL, 0, "CONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
    mensagemTCP = malloc(len +1);
    if (mensagemTCP != NULL)
    {
        snprintf(mensagemTCP, len+1, "CONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);   
        mensagemTCP[len+1] = '\0';
    }
    else
    {
        printf("Erro a alocar memória para enviar o CONTENT\n");
        return -1;
    }
    printf("Prestes a enviar: %s", mensagemTCP); // Já inclui o "\n"

    //mensagemTCP = malloc(122 * sizeof(char)); // 122 Bytes max
    //sprintf(mensagemTCP, "CONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
    //printf("enviado: CONTENT %d %d %s", inputNode->getDest, inputNode->getOrig, inputNode->getContent); // a string enviada já leva p \n !!!

    // RESPONDER COM CONTENT AO MENSAGEIRO DA PROCURA QUE ME CONTACTOU
    // AQUI NEM ESTAMOS A VERIFICAR SE  EXPEDTABLE.ACTIVE = 1 MAS SE DEREM WITHDRAW PODE SER PROBLEMÀTICO
    int neighbourSocketID = inputNode->expedTable[inputNode->getDest].connection; // NÓ DE CONTACTO DO NÓ DESTINO DA MENSAGEM
    int neighbourSocket_fd = inputNode->neighbours[neighbourSocketID].otherTcp_fd;

    n = write(neighbourSocket_fd, mensagemTCP, (strlen(mensagemTCP) + 1 ));
    if (n == -1 || (n != (strlen(mensagemTCP) + 1)))
    {
        perror("Erro a enviar o CONTENT\n");
    }
    free(mensagemTCP);
    return 0;
}

int sendNOCONTENT(inputNode *inputNode)
{
    // ENVIAR UMA MENSAGEM QUERY AOS VIZINHOS!!!
    char *mensagemTCP;
    int n;
    
    int len = snprintf(NULL, 0, "NOCONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
    mensagemTCP = malloc(len +1);
    if (mensagemTCP != NULL)
    {
        snprintf(mensagemTCP, len+1, "NOCONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);  
        mensagemTCP[len+1] = '\0'; 
    }
    else
    {
        printf("Erro a alocar memória para enviar o NOCONTENT\n");
        return -1;
    }
    printf("Prestes a enviar: %s", mensagemTCP); // Já inclui o "\n"

    //mensagemTCP = malloc(124 * sizeof(char)); // 124 Bytes max
    //sprintf(mensagemTCP, "NOCONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);
    //printf("enviado: NOCONTENT %d %d %s\n", inputNode->getDest, inputNode->getOrig, inputNode->getContent);

    // RESPONDER COM CONTENT AO MENSAGEIRO DA PROCURA QUE ME CONTACTOU
    int neighbourSocket = inputNode->expedTable[inputNode->getDest].connection; // NÓ DE CONTACTO DO NÓ DESTINO DA MENSAGEM
    int neighbourSocket_fd = inputNode->neighbours[neighbourSocket].otherTcp_fd;

    n = write(neighbourSocket_fd, mensagemTCP, strlen(mensagemTCP)+1);
    if (n == -1 || (n != (strlen(mensagemTCP) + 1)))
    {
        perror("Erro a enviar o CONTENT\n");
    }
    free(mensagemTCP);
    return 0;
}

int withdraw(inputNode *inputNode, int withdrawid, int emissor)
{
    // withdrawid tem o id do nó que saiu da rede

    // APAGAR NA TABELA OS QUE TẼM COMO CONECTION withdrawid
    for (int i = 0; i < 100; i++)
    {
        if (inputNode->expedTable[i].tableActive == 1) 
        {
            if (inputNode->expedTable[i].connection == withdrawid)
            {
                inputNode->expedTable[i].connection = -1;
                inputNode->expedTable[i].tableActive = 0;
            }
        }
    }
    // APAGAR NA TABELA O DESTINO withdrawid
    inputNode->expedTable[withdrawid].connection = -1;
    inputNode->expedTable[withdrawid].tableActive = 0;

    if(sendWITHDRAW(inputNode, withdrawid, emissor) != 0)
    {
        printf("Não foi possível enviar withdraw a todos os vizinhos\n");
    }

    return 0;
}

int sendWITHDRAW(inputNode *inputNode, int withdrawid, int emissor)
{
    char *mensagemTCP;
    int i, n, neighbourSocket_fd, erros = 0;

    int len = snprintf(NULL, 0, "WITHDRAW %d\n", withdrawid);
    mensagemTCP = malloc(len +1);
    if (mensagemTCP != NULL)
    {
        snprintf(mensagemTCP, len+1, "WITHDRAW %d\n", withdrawid); 
        mensagemTCP[len+1] = '\0';  
    }
    else
    {
        printf("Erro a alocar memória para enviar o WITHDRAW\n");
        return -1;
    }
    printf("Prestes a enviar: %s", mensagemTCP); // Já inclui o "\n"
    //mensagemTCP = malloc(14 * sizeof(char)); // 13 Bytes max: EXTERN
    //sprintf(mensagemTCP, "WIDTHDRAW %d\n", withdrawid);

    for ( i = 0; i < 100; i++)
    {
        if((i != emissor) && (inputNode->expedTable[i].tableActive == 1) && (inputNode->neighbours[i].otherActive == 1) )
        {
            neighbourSocket_fd = inputNode->neighbours[i].otherTcp_fd;
            n = write(neighbourSocket_fd, mensagemTCP, strlen(mensagemTCP)+1);
            if (n == -1 || (n != (strlen(mensagemTCP) + 1)))
            {
                printf("Erro a enviar o WITHDRAW ao vizinho: %d\n", i);
                erros++;
                // return -1; pode ser só um erro num, deve continuar a enviar aos outros, tirar o return mais tarde
            }
            printf("enviado: WIDTHDRAW %d ao nó: %d\n", withdrawid, i);
        }
    }
    free(mensagemTCP);
    return erros;
}

int deleteContent(inputNode *inputNode, char *content)
{
    int found = 0; // Se encontrar o conteúdo esta flag passa a 1
    for (int i = 0; i < MAX_CONTENTS; i++)
    {
        if (strlen(inputNode->contents[i]) != 0) // string slot está não está vazia 
        {
            if (!(strcmp(inputNode->contents[i], content))) // o conteudo foi encontrado numa slot
            {
                found = 1;
                //printf("conteúdo a apagar: %s\n", inputNode->contents[i]);
                inputNode->contents[i][0] = '\0'; // a string ficou nula
                break;
            }
        }
    }
    if (!found)
    {
        printf("Não encontro o conteúdo: %s na tabela!\n", content);
    }    
    return 0;
}
