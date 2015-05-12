/**************************************************************
rdt-part3.h
Student name: Han Lin
Student No. : 3035027851
Date and version:
Development platform:
Development language:
Compilation:
	Can be compiled with
*****************************************************************/

#ifndef RDT3_H
#define RDT3_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

#define PAYLOAD 1000		//size of data payload of the RDT layer
#define TIMEOUT 50000		//50 milliseconds
#define TWAIT 10*TIMEOUT	//Each peer keeps an eye on the receiving  
							//end for TWAIT time units before closing
							//For retransmission of missing last ACK
#define W 5					//For Extended S&W - define pipeline window size


//----- Type defines ----------------------------------------------------------
typedef unsigned char		u8b_t;    	// a char
typedef unsigned short		u16b_t;  	// 16-bit word
typedef unsigned int		u32b_t;		// 32-bit word 

extern float LOSS_RATE, ERR_RATE;


/* this function is for simulating packet loss or corruption in an unreliable channel */
/***
Assume we have registered the target peer address with the UDP socket by the connect()
function, udt_send() uses send() function (instead of sendto() function) to send 
a UDP datagram.
***/
int udt_send(int fd, void * pkt, int pktLen, unsigned int flags) {
	double randomNum = 0.0;

	/* simulate packet loss */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < LOSS_RATE){
		//simulate packet loss of unreliable send
		printf("WARNING: udt_send: Packet lost in unreliable layer!!!!!!\n");
		return pktLen;
	}

	/* simulate packet corruption */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < ERR_RATE){
		//clone the packet
		u8b_t errmsg[pktLen];
		memcpy(errmsg, pkt, pktLen);
		//change a char of the packet
		int position = rand() % pktLen;
		if (errmsg[position] > 1) errmsg[position] -= 2;
		else errmsg[position] = 254;
		printf("WARNING: udt_send: Packet corrupted in unreliable layer!!!!!!\n");
		return send(fd, errmsg, pktLen, 0);
	} else 	// transmit original packet
		return send(fd, pkt, pktLen, 0);
}

/* this function is for calculating the 16-bit checksum of a message */
/***
Source: UNIX Network Programming, Vol 1 (by W.R. Stevens et. al)
***/
u16b_t checksum(u8b_t *msg, u16b_t bytecount)
{
	u32b_t sum = 0;
	u16b_t * addr = (u16b_t *)msg;
	u16b_t word = 0;
	
	// add 16-bit by 16-bit
	while(bytecount > 1)
	{
		sum += *addr++;
		bytecount -= 2;
	}
	
	// Add left-over byte, if any
	if (bytecount > 0) {
		*(u8b_t *)(&word) = *(u8b_t *)addr;
		sum += word;
	}
	
	// Fold 32-bit sum to 16 bits
	while (sum>>16) 
		sum = (sum & 0xFFFF) + (sum >> 16);
	
	word = ~sum;
	
	return word;
}

//----- Type defines ----------------------------------------------------------

// define your data structures and global variables in here
#define HEADER_SIZE 4	// the size if header
u8b_t send_window = 0;  //256 width
u8b_t rcv_window = 0;
struct timeval timer;
fd_set master;
fd_set read_fds;
int fdmax;
int send_seqnum = 0;	//seq# for send
int rcv_count = 0;		//seq# for receive

/*********Originally defined functions************/ 
int rdt_socket();
int rdt_bind(int fd, u16b_t port);
int rdt_target(int fd, char * peer_name, u16b_t peer_port);
int rdt_send(int fd, char * msg, int length);
int rdt_recv(int fd, char * msg, int length);
int rdt_close(int fd);

/****************Auxiliary Fucntions*************/
void make_pkt(char* pkt, int type, u8b_t seq, u16b_t checksum, char* msg, int lenghth);
u16b_t make_ack(char* pkt, u8b_t seq);
u16b_t make_dgm(char* pkt, u8b_t seq, char* msg, int length);
bool isACK(char* pkt, u8b_t seq, int count);
bool corrupt(char* pkt, u16b_t length);
/* Application process calls this function to create the RDT socket.
   return	-> the socket descriptor on success, -1 on error 
*/
int rdt_socket() {
	int fd = 0;
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("rdt_socket: \n");
		return -1;
	}
	
	printf("rdt_socket: socket established\n");
	return fd;
	
}

/* Application process calls this function to specify the IP address
   and port number used by itself and assigns them to the RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_bind(int fd, u16b_t port){
	//same as part 1
	int rcv;
    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);	
    memset(&(my_addr.sin_zero),'\0',8);		
    
    if((rcv = bind(fd,(struct sockaddr *)& my_addr,sizeof(my_addr))) ==-1 ){
        perror("bind error:");
        return -1;
    }
	printf("rdt_bind: success!\n");
    return rcv;
}

/* Application process calls this function to specify the IP address
   and port number used by remote process and associates them to the 
   RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_target(int fd, char * peer_name, u16b_t peer_port){

	struct hostent *he;
    int rcv;
    if((he = gethostbyname(peer_name)) == NULL){
        perror("gethostbyname");
        return -1;
    }

    struct sockaddr_in peer_addr;	
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    peer_addr.sin_addr = *((struct in_addr *) he->h_addr);	
    memset(&(peer_addr.sin_zero),'\0',8);
    
    if ((rcv = connect(fd,(struct sockaddr *) &peer_addr, sizeof(struct sockaddr_in))) == -1) {
        perror("connect error:");
        return -1;
    }
    printf("rdt_target success\n");
    return 0;
}

/* Application process calls this function to transmit a message to
   target (rdt_target) remote process through RDT socket; this call will
   not return until the whole message has been successfully transmitted
   or when encountered errors.
   msg		-> pointer to the application's send buffer
   length	-> length of application message
   return	-> size of data sent on success, -1 on error
*/
int rdt_send(int fd, char * msg, int length){ //implement the Extended Stop-and-Wait ARQ logic

	
	int i,rv, send_status = 0;
	fdmax = fd;
    FD_ZERO(&master);
    FD_SET(fd,&master);
    FD_ZERO(&read_fds);
    FD_SET(fd,&read_fds);
    
    timer.tv_sec = 0;
    timer.tv_usec = TIMEOUT;
	
	int num_of_pkts = (int) ceil( (float)length / PAYLOAD); //calc the num of packets
	u8b_t ack_target = send_seqnum + num_of_pkts -1;				
	u8b_t c_wind = send_window;
	u16b_t *data_length = (u16b_t *) malloc(num_of_pkts * sizeof(u16b_t));
	int* sent_length = (int *) malloc(num_of_pkts * sizeof(int));	
	char** sent_pkts = (char **) malloc(num_of_pkts*sizeof(char *));

	for( i = 0; i<num_of_pkts ;i++){
		data_length[i] = (PAYLOAD > length? length + HEADER_SIZE:HEADER_SIZE+ PAYLOAD);
		length -= PAYLOAD;
		sent_pkts[i] = (char*) malloc(data_length[i] * sizeof(char));
		make_dgm(sent_pkts[i], (send_window+i) ,msg+i*PAYLOAD,data_length[i]-HEADER_SIZE);
		sent_length[i] = udt_send(fd,sent_pkts[i],(int) data_length[i],0);
		printf("rdt_send: Sent one message of size:%d seqNo:%d\n",sent_length[i],send_seqnum + i);
}	
    char* recv_data = (char*) malloc((HEADER_SIZE + PAYLOAD)*sizeof(char));
    char* recv_ACK = (char*) malloc((HEADER_SIZE)*sizeof(char));
    
    for(;;){
    	
    	read_fds = master;
    	timer.tv_sec  = 0;
    	timer.tv_usec = TIMEOUT;

    	send_status = select(fdmax+1,&read_fds,NULL,NULL,&timer);
        if(send_status == 0){
            
    		// timeout, retransmit and do another forloop.
    		printf("rdt_send: Timeout!!");
			for(i = (int) ((u8b_t) (c_wind - send_window)); i < num_of_pkts; i++){
				sent_length[i] = udt_send(fd,sent_pkts[i],(int) data_length[i],0);
				printf("%d \n",i + send_seqnum);
			}
			continue;
			}
    	else if( send_status == -1){
    		perror("select error");
    		exit(0);
    	}

		rv = recv(fd,recv_data, sizeof recv_data, 0);
		if(rv < 4){
			//impossible
		}
		else if(rv > 4){
			if( recv_data[0] =='0'&& (u8b_t) recv_data[1]== (rcv_window - 1) ){
				char *ACK = (char*) malloc(HEADER_SIZE * sizeof(char));
				make_ack(ACK, (rcv_window -1));
				udt_send(fd,ACK,HEADER_SIZE,0);
				free(ACK);
				continue;
			}else{
				printf("No need to care\n");
			}
		}
		else{
			
			if(corrupt(recv_data,rv)){
			printf("rdt_send: Received a corrupt packet: Type = %s, Length = %d\n",(recv_data[0] == '0'? "DATA":"ACK"),rv);
			printf("rdt_send: Drop the packet\n");
			continue;
			}

			memcpy(recv_ACK,recv_data,HEADER_SIZE);
			if(!isACK(recv_ACK,send_window,num_of_pkts)){
				printf("rdt_send: Received a wrong ACK%u packet\n",256*((send_seqnum+1)/256) +(u8b_t) recv_ACK[1] );
				continue;
			}
			else{
				// Correct ACK is received.
				if( (u8b_t) recv_ACK[1] == ack_target){
					printf("rdt_send: Received the ACK:%u (%u-%u)\n",256*((send_seqnum+1)/256) +(u8b_t) recv_ACK[1] ,
							256*((send_seqnum+1)/256) +send_window,256*((send_seqnum+1)/256) + ack_target);
					send_window = ack_target + 1;
					send_seqnum += num_of_pkts;
					// Stop timer.
					timer.tv_usec = 0;
					timer.tv_sec  = 0;
					send_status = select(fdmax+1,&read_fds,NULL,NULL,&timer);
					break;
				}
				else if((u8b_t) recv_ACK[1] - send_seqnum >= c_wind - send_seqnum){
					c_wind =(u8b_t) recv_ACK[1] + 1;
					printf("rdt_send: Received the ACK:%u (%u-%u)\n", 256*((send_seqnum+1)/256) + (u8b_t) recv_ACK[1] ,
							256*((send_seqnum+1)/256) +send_window, 256*((send_seqnum+1)/256) + ack_target);
				}

				else{
					
					printf("rdt_send: Received duplicated ACK:%u (expect %u)\n",256*((send_seqnum+1)/256) + (u8b_t) recv_ACK[1] ,256*(send_seqnum/256) + c_wind);
					
				}
			}
		}
    }

    // Clean intermediate variable.
	int sent_total = 0;
	for( i = 0;i<num_of_pkts;i++){
		free(sent_pkts[i]);
		sent_total += sent_length[i];
	}
	free(sent_length);
	free(data_length);
    free(sent_pkts);
    free(recv_data);
    free(recv_ACK);
    return sent_total;

}

/* Application process calls this function to wait for a message of any
   length from the remote process; the caller will be blocked waiting for
   the arrival of the message. 
   msg		-> pointer to the receiving buffer
   length	-> length of receiving buffer
   return	-> size of data received on success, -1 on error
*/
int rdt_recv(int fd, char * msg, int length){
	//implement the Extended Stop-and-Wait ARQ logic
	int rcv, type;
	u8b_t seqnum;
	char* received = (char*) malloc((HEADER_SIZE + PAYLOAD)*sizeof(char));
	char* reply_ACK = (char*) malloc((HEADER_SIZE) * sizeof(char));

	//loop to wait for a message coming in
	while(true){
		
		if((rcv = recv(fd, received, HEADER_SIZE+PAYLOAD, 0)) < 0){
			perror("Recv: \n");
			free(reply_ACK);
			free(received);
			return -1;
		}
		//no recv error
		type = received[0] - '0';
		seqnum = (u8b_t) received[1];
		// a corrupted message
		if(corrupt(received, (u16b_t)rcv)){
			printf("rdt_recv: received a corrupted message of Type\n");
			continue; //Just ignore this message
		}
		//Good message
		if(seqnum == rcv_window&&type == 0){
			make_ack(reply_ACK, rcv_window);
			//reply the corresponding ACK
			udt_send(fd, reply_ACK, HEADER_SIZE, 0);
			memcpy(msg, received + 4, rcv - HEADER_SIZE);
			printf("rdt_recv: Expected packet - seq#%d\n", rcv_count);
			rcv_count++;
			rcv_window++;
			free(reply_ACK);
			free(received);
			return rcv - HEADER_SIZE;
		}else if(type==0&&seqnum!=rcv_window){
			printf("rdt_recv: Received a unexpected out of order packet of seq#%d\n", 256*(rcv_count/256)+seqnum);
			printf("Send the previous ACK# to require desired one");
			make_ack(reply_ACK, rcv_window-1); //the previous one' ACK
			udt_send(fd, reply_ACK,HEADER_SIZE, 0);
		}else{
			continue;
		}

	}//end of while loop

}

/* Application process calls this function to close the RDT socket.
*/
int rdt_close(int fd){
	//implement the Extended Stop-and-Wait ARQ logic
	fdmax = fd;
	FD_ZERO(&master);
	FD_SET(fd, &master);
	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);
	struct timeval timer;
	//int count = 0;
	int status;
	int rcv;

	char* data = (char*)malloc((PAYLOAD + HEADER_SIZE) * sizeof(char));
	char* ACK = (char*)malloc(HEADER_SIZE * sizeof(char));

	while(true){
		timer.tv_sec = 0;
		timer.tv_usec = TWAIT;
		read_fds = master;
		status = select(fdmax+1, &read_fds, NULL, NULL, &timer);
		if(status == 0){
			//if(count == 0){
			//timeout
				if((rcv = close(fd))!=0){
					perror("close(fd)");
					free(data);
					free(ACK);
					return -1;
				}else{
					free(data);
					free(ACK);
					printf("rdt_close: Close the socket\n");
					return 0;
				}
			//}
		}else{ 
			//Simply reply that receive the message and do nothing for this message
			//which must be a ACK or retransimited message
			rcv = recv(fd, data, sizeof data, 0);
			if(rcv <= HEADER_SIZE){ // safely close the socket
				printf("rdt_close: final ACK received\n");
				
				make_ack(ACK, rcv_window - 1);
				udt_send(fd, ACK, HEADER_SIZE, 0);
				if((rcv = close(fd))!=0){
					perror("close(fd)");
					free(data);
					free(ACK);
					return -1;
				}else{
					free(data);
					free(ACK);
					printf("rdt_close: Close the socket\n");
					return 0;
				}
				continue;
			}else{
				printf("rdt_close: Received a retransimited packet\n");
				//to let another side know that the pakcet has been accepted
				make_ack(ACK, rcv_window - 1);
				udt_send(fd, ACK, HEADER_SIZE, 0);
				printf("rdt_close: send the ACK of retransimited packet\n");
			}

		}
	}//end of while loop
}

/****************Definition of auxiliary functions***********************/
void make_pkt(char* pkt, int type, u8b_t seq, u16b_t checksum, char* msg, int length){
	pkt[0] = type + '0'; //the type identifier
	pkt[1] = (char)seq;
	unsigned char* ckm = (unsigned char*)&checksum;
	pkt[2] = ckm[0];
	pkt[3] = ckm[1];
	if(length>0)
		memcpy(pkt+4, msg, length);
	return;
}

u16b_t make_ack(char* pkt, u8b_t seq){
	char* temp = (char*)malloc(PAYLOAD * sizeof(char));
	memset(temp, '\0', PAYLOAD * sizeof(char));
	char* _temp = (char*)malloc((HEADER_SIZE + PAYLOAD) * sizeof(char));
	make_pkt(_temp, 1, seq, 0, temp, PAYLOAD);
	_temp[2] = '0';
	_temp[3] = '0';
	u16b_t ckm = checksum((unsigned char*)_temp, (u16b_t)HEADER_SIZE+PAYLOAD);
	make_pkt(pkt, 1, seq, ckm, NULL, 0);
	free(temp);
	free(_temp);
	return ckm; 
}

u16b_t make_dgm(char* pkt, u8b_t seq, char* msg, int length){
	char* temp = (char*)malloc(PAYLOAD * sizeof(char));
	if(PAYLOAD > length){
		memcpy(temp, msg, length);
		memset(temp + length, '\0', (PAYLOAD - length) * sizeof(char));
	}else{
		memcpy(temp, msg, PAYLOAD);
	}
	char* _temp = (char*)malloc((HEADER_SIZE+PAYLOAD) * sizeof(char));
	make_pkt(_temp, 0, seq, 0, temp, PAYLOAD);
	_temp[2] = '0';
	_temp[3] = '0';
	u16b_t ckm = checksum((unsigned char*)_temp, (u16b_t)HEADER_SIZE+PAYLOAD);
	make_pkt(pkt, 0,seq, ckm, msg, length);
	free(temp);
	free(_temp);
	return ckm;
}
bool isACK(char* pkt, u8b_t seq, int count){
	if((u8b_t)(pkt[1] - seq) < count && (u8b_t)(pkt[1] - seq) >= 0 && pkt[0] - '0' == 1){
		return true;
	}else{
		return false;
	}

}
bool corrupt(char* pkt, u16b_t length){
	u16b_t ckm = 0;
	u16b_t ckm2 = 0;
	u8b_t seqnum = (u8b_t)pkt[1];
	char* msg = NULL;

	unsigned char* _ckm = (unsigned char*)&ckm;
	_ckm[0] = pkt[2];
	_ckm[1] = pkt[3];

	if(length < (u16b_t)HEADER_SIZE){
		return true;
	}

	char* temp = (char*)malloc((PAYLOAD + HEADER_SIZE) * sizeof(char));
	if(length > HEADER_SIZE){
		msg = (char*)malloc((length - HEADER_SIZE) * sizeof(char));
		memcpy(msg, pkt+HEADER_SIZE, length - HEADER_SIZE);
		ckm2 = make_dgm(temp, seqnum, msg, length - HEADER_SIZE);
		free(msg);
	}else{
		ckm2 = make_ack(temp,seqnum);
	}

	if(ckm2 != ckm){
		free(temp);
		return true;
	}else{
		free(temp);
		return false;
	}

}

#endif