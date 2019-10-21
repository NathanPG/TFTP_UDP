#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <fstream>
#include <signal.h>
#include <iostream>
#include <string>

using namespace std;
int MAXLINE = 516;
int CurTime = 0;

//generate ack
char* GACK(int blocknum){
    char ack[5];
    ack[0] = 0;
    ack[1] = 4;
    ack[2] = blocknum/256;
    ack[3] = blocknum % 256;
    ack[4] = '\0';
    char* ack_made = ack;
    return ack_made;
}

void alarmhandler(int signo){
    CurTime++;
    if (CurTime == 10){
        cout << "TIMEOUT disconnecting..." << endl;
        exit(0);
    }
}

void sendDATA(int sockfd,  ifstream &infile, unsigned short int &blocknum, string &lastPacket, struct sockaddr_in
&cliaddr, socklen_t len){
    int n = 0;
    int readBytes = 0;
    int debug = 0;
    char buffer[MAXLINE];
    string DATA(MAXLINE+5, ' ');
    char CACK[5];
    //TODO: UPLOAD FILE WITH 512 BYTES OR LESS
    do{
        infile.read(buffer,512);
        readBytes = infile.gcount();
        //buffer[readBytes] = '\0';
        DATA[0] = 0;
        DATA[1] = 3;
        DATA[2] = blocknum/256;
        DATA[3] = blocknum % 256;
        int i;
        for (i = 4; i < 4 + readBytes; ++i) {
            DATA[i] = buffer[i-4];
        }
        DATA[i] = '\0';
        cout<<"BYTES SENT: "<<readBytes<<endl;
        sendto(sockfd, DATA.c_str(), readBytes+4, 0, (struct sockaddr *) &cliaddr, len);
        lastPacket = DATA;

        bzero(buffer, sizeof(buffer));
        DATA.clear();

        n = recvfrom(sockfd, &CACK, 5, 0, (struct sockaddr *) &cliaddr, &len);
        //TODO: CLIENT ACK CHECK
        //bzero(CACK,MAXLINE);
        //IF THERE IS NO ACK BACK
        while(n == -1){
            //WAIT ONE SECOND
            //alarm(1);
            //pause();
            sendto(sockfd, &lastPacket, readBytes+4, 0, (struct sockaddr *) &cliaddr, len);
            n = recvfrom(sockfd, &CACK, 5, 0, (struct sockaddr *) &cliaddr, &len);
            //TODO: CLIENT ACK CHECK
            bzero(CACK, sizeof(CACK));
        }
        //alarm(0);
        CurTime = 0;
        blocknum++;
    }while(readBytes>=512);
    bzero(CACK, sizeof(CACK));
    infile.close();
}


//WRQ , server receive files
int receiveDATA(int sockfd, ofstream &outfile, unsigned short int &blocknum, string &lastACK){
    sockaddr cliaddr;
    socklen_t len = sizeof(cliaddr);
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    char buffer[MAXLINE];
    char DATA[MAXLINE];
    //Receive DATA From the Client
    int receivebyte = recvfrom(sockfd, &buffer, MAXLINE, 0, &cliaddr, &len);
    //TODO: TIMEOUT, SEND LAST ACK / END CONNECTION
    while(receivebyte == -1){
        //alarm(1);
        //pause();
        sendto(sockfd, &lastACK, 5, 0, &cliaddr, len);
        //Receive DATA From the Client
        receivebyte = recvfrom(sockfd, &buffer, MAXLINE, 0, &cliaddr, &len);
    }
    //alarm(0);
    CurTime = 0;
    memcpy(DATA, buffer+4, receivebyte-4);
    string temp(DATA);
    outfile<<temp;

    cout<<"DATA SIZE: "<<temp.size()<<endl;
    temp.clear();

    opcode_ptr = (unsigned short int *) &buffer;
    opcode = ntohs(*opcode_ptr);
    blocknum = ntohs(*(opcode_ptr+1));
    //CLEAR BUFFER AND DATA
    bzero(buffer, MAXLINE);
    bzero(DATA, MAXLINE);

    if(opcode != 3){
        //cout<<"MIAOMIAOMIAO"<<endl;
    }
    return receivebyte;
}

//int main(int argc, char* argv[])
int main(int argc, char* argv[]) {
    int MINPORT;
    int MAXPORT;
    string lastPacket(MAXLINE+5, ' ');
    signal(SIGALRM, alarmhandler);
    if(argc == 3){
        string temp = argv[1];
        MINPORT = atoi(temp.c_str());
        temp = argv[2];
        MAXPORT = atoi(temp.c_str());
    }

    //WHERE SHOULD WE PUT THESE
    socklen_t len;
    int sockfd = 0;
    int SERV_PORT = 9877;

    //int SERV_PORT = MINPORT;
    struct sockaddr_in servaddr, cliaddr;
    //socket descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd == -1 )
    {
        perror( "socket() failed" );
        return EXIT_FAILURE;
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    if ( bind( sockfd, (struct sockaddr *) &servaddr, sizeof( servaddr ) ) < 0 )
    {
        perror( "bind() failed" );
        return EXIT_FAILURE;
    }
    cout<<"SERVER ONLINE"<<endl;
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    char buffer[MAXLINE];
    char filename[MAXLINE];

    int currentport = 9877 + 1;
    int f;
    while(1){
        len = sizeof(cliaddr);
        recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len);
        opcode_ptr = (unsigned short int *) &buffer;
        opcode = ntohs(*opcode_ptr);
        strcpy(filename, buffer+2);


        //IF WE RECEIVE CLIENT QUESTS
        if(opcode == 1 || opcode == 2) {

            f = fork();
            if(f == -1) {
                //ERROR
            }else if(f == 0) {
                int childfd = socket(AF_INET, SOCK_DGRAM,0);
                if(childfd == -1) {

                }
                struct sockaddr_in child_addr;
                socklen_t len_child = sizeof(child_addr);
                bzero(&child_addr,len_child);
                child_addr.sin_family = AF_INET;
                child_addr.sin_addr.s_addr=htonl(INADDR_ANY);
                child_addr.sin_port = htons(currentport);
                cout<<currentport<<endl;
                currentport++;


                if ( bind( childfd, (struct sockaddr *) &child_addr, sizeof( child_addr ) ) < 0 )
                {
                    perror( "CLIENT BIND FAILED" );
                    return EXIT_FAILURE;
                }
                //CLIENT OPERATION
                //Read Request, Server Send File
                if(opcode==1){
                    ifstream infile;
                    infile.open(filename);
                    unsigned short int blocknum = 1;
                    cout<<"SERVER STARTED TO SEND DATA"<<endl;
                    sendDATA(sockfd,infile,blocknum,lastPacket,cliaddr,len);
                    exit(0);
                }
                    //Write Request, Server Get File
                else if(opcode == 2){
                    ofstream outfile;
                    outfile.open(filename);
                    int n;
                    unsigned short int blocknum = 0;
                    //Send First ACK Packet
                    char ACKOP[5];
                    ACKOP[0] = 0;
                    ACKOP[1] = 4;
                    ACKOP[2] = 0;
                    ACKOP[3] = 0;
                    ACKOP[4] = '\0';
                    sendto(sockfd, &ACKOP, 5, 0, (struct sockaddr *) &cliaddr, len);
                    lastPacket = ACKOP;
                    bzero(ACKOP,5);
                    do{
                        //Read and save file from the client
                        n = receiveDATA(sockfd,outfile,blocknum,lastPacket);
                        //Generate ACK
                        char ACK[5];
                        ACK[0] = 0;
                        ACK[1] = 4;
                        ACK[2] = blocknum/256;
                        ACK[3] = blocknum % 256;
                        ACK[4] = '\0';
                        //Send ACK
                        sendto(sockfd, &ACK, 5, 0, (struct sockaddr *) &cliaddr, len);
                        lastPacket = ACK;
                        bzero(ACK,5);
                    }while( n >= 516 );
                    outfile.close();
                    exit(0);
                }
                else{

                }
            }
            //PARENT PROCESS
            else {
                //DO NOTHING
            }
        }
        /*
        if(currentport == MAXPORT){
            exit(0);
        }
         */
    }
}
