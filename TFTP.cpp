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
int MAXLINE = 517;
int CurTime = 0;

//Alarmhandler for checking timeout, if not receiving for 10 seconds, end the process
void alarmhandler(int signo){
    CurTime++;
    //Disconnect
    if (CurTime == 10){
        cout << "TIMEOUT disconnecting..." << endl;
        exit(0);
    }
}

//sendDATA is to handle the RRQ request. It read the desired file and sends its DATA to the client,
// then receive ack. It keeps doing this until the server has sent all of the file
//This function is called once for every request
void sendDATA(int sockfd,  ifstream &infile, unsigned short int &blocknum, struct sockaddr_in &cliaddr,
        socklen_t len){
    //initialize variables
    int readBytes = 0;
    //buffer is to store data from the file (with header)
    char buffer[MAXLINE];
    //DATA is the actual packet that will be send to the client (without header)
    char DATA[MAXLINE];
    //CACK is the received client ACK
    char CACK[MAXLINE];
    do{
        infile.read(buffer,512);
        readBytes = infile.gcount();
        //Counstruct the DATA header
        DATA[0] = 0;
        DATA[1] = 3;
        DATA[2] = blocknum/256;
        DATA[3] = blocknum % 256;
        //Assemble the header and the data from the file
        int i;
        for (i = 4; i < 4 + readBytes; ++i) {
            DATA[i] = buffer[i-4];
        }
        DATA[i] = '\0';
        //Send DATA Packet to the client
        sendto(sockfd, DATA, readBytes+4, 0, (struct sockaddr *) &cliaddr, len);
        //free buffer
        bzero(buffer, sizeof(buffer));
        //Receive ACK from the client
        recvfrom(sockfd, CACK, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len);
        //Add blocknumber
        blocknum++;
    }
    //If readBytes is less than 512, then we know it is the last packet, we end the process
    while(readBytes>=512);
    //free client ACK
    bzero(CACK, sizeof(CACK));
    infile.close();
}

//receiveDATA is to handle WRQ , only to receive DATA packet from the client and store it to the file
//This function will be called multiple times for a request
//It returns the number of byte the server received
int receiveDATA(int sockfd, ofstream &outfile, unsigned short int &blocknum){
    //Initialize variables
    sockaddr cliaddr;
    socklen_t len = sizeof(cliaddr);
    unsigned short int * opcode_ptr;
    //Actual packet from the client(with header)
    char buffer[MAXLINE];
    //Only DATA part of the packet(without header)
    char DATA[MAXLINE];
    //Receive DATA Packet from the client and store in buffer
    int receivebyte = recvfrom(sockfd, buffer, MAXLINE, 0, &cliaddr, &len);
    buffer[receivebyte] = '\0';
    //Parse only DATA packet and store in DATA
    strcpy(DATA,buffer+4);
    string temp(DATA);
    //Wrtie data to the file
    outfile<<temp;
    temp.clear();
    //Get buffer pointer and the blocknumber
    opcode_ptr = (unsigned short int *) &buffer;
    blocknum = ntohs(*(opcode_ptr+1));
    bzero(buffer, MAXLINE);
    bzero(DATA, MAXLINE);
    return receivebyte;
}

int main(int argc, char* argv[]) {
    //MIN and MAX ports
    int MINPORT;
    int MAXPORT;
    //Signal
    signal(SIGALRM, alarmhandler);
    //Check argument number
    if(argc == 3){
        //Read MINPORT and MAXPORT
        string temp = argv[1];
        MINPORT = atoi(temp.c_str());
        temp = argv[2];
        MAXPORT = atoi(temp.c_str());
    }else{
        perror("INVALID arguments");
    }
    socklen_t len;
    int sockfd = 0;
    struct sockaddr_in servaddr, cliaddr;
    //socket descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd == -1 )
    {
        perror( "Server socket error" );
        return EXIT_FAILURE;
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port = htons(MINPORT);
    //bind server
    if ( bind( sockfd, (struct sockaddr *) &servaddr, sizeof( servaddr ) ) < 0 )
    {
        perror( "Server bind failed" );
        return EXIT_FAILURE;
    }
    int currentport = MINPORT;
    int f = 0;
    while(1){
        //Initialize variables
        unsigned short int opcode;
        unsigned short int * opcode_ptr;
        char buffer[MAXLINE];
        char filename[MAXLINE];
        len = sizeof(cliaddr);
        //Receive Client Request
        recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len);
        //Add port number
        currentport++;
        opcode_ptr = (unsigned short int *) &buffer;
        //Read opcode
        opcode = ntohs(*opcode_ptr);
        //Read file name
        strcpy(filename, buffer+2);
        struct sockaddr_in child_addr;
        socklen_t len_child = sizeof(child_addr);
        int childfd = socket(AF_INET, SOCK_DGRAM,0);
        bzero(&child_addr,len_child);
        child_addr.sin_family = AF_INET;
        child_addr.sin_addr.s_addr=htonl(INADDR_ANY);
        child_addr.sin_port = htons(currentport);
        //bind child client
        if ( bind( childfd, (struct sockaddr *) &child_addr, sizeof( child_addr ) ) < 0 )
        {
            perror( "CLIENT BIND FAILED" );
            return EXIT_FAILURE;
        }
        //If we received the request
        if(opcode == 1 || opcode == 2) {
            //Start a child process
            f = fork();
            if(f == -1) {
                perror("Fork fail");
            }else if(f == 0) {
                //CLIENT OPERATION
                //Read Request, Server Send File
                if(opcode==1){
                    ifstream infile;
                    infile.open(filename);
                    unsigned short int blocknum = 1;
                    //Send all data packets and receive client ack
                    sendDATA(childfd,infile,blocknum,cliaddr,len);
                    exit(0);
                }
                //Write Request, Server Get File
                else if(opcode == 2){
                    //Open file
                    ofstream outfile;
                    outfile.open(filename);
                    int n;
                    unsigned short int blocknum = 0;
                    //Build OACK Packet
                    char ACKOP[5];
                    ACKOP[0] = 0;
                    ACKOP[1] = 4;
                    ACKOP[2] = 0;
                    ACKOP[3] = 0;
                    ACKOP[4] = '\0';
                    //Send OACK Packet
                    sendto(childfd, ACKOP, 5, 0, (struct sockaddr *) &cliaddr, len);
                    bzero(ACKOP,5);
                    do{
                        //Read and save file from the client
                        n = receiveDATA(childfd,outfile,blocknum);
                        //Construct ACK
                        char ACK[5];
                        ACK[0] = 0;
                        ACK[1] = 4;
                        ACK[2] = blocknum/256;
                        ACK[3] = blocknum % 256;
                        ACK[4] = '\0';
                        //Send ACK back to the client
                        sendto(childfd, ACK, 5, 0, (struct sockaddr *) &cliaddr, len);
                        bzero(ACK,5);
                    }
                    //If the server gets the last packet, end the process
                    while( n >= 516 );
                    outfile.close();
                    exit(0);
                }
            }
        }
        if(currentport == MAXPORT){
            exit(0);
        }
    }
}
