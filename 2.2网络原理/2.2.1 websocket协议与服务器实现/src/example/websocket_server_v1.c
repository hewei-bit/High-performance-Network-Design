
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <sys/epoll.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>



#define KING_SERVER_PORT		9000
#define MAX_BUFFER_LENGTH		1024
#define MAX_EPOLL_SIZE			1024
#define GUID 					"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


struct _nty_ophdr {

	unsigned char opcode:4,
		 rsv3:1,
		 rsv2:1,
		 rsv1:1,
		 fin:1;
	unsigned char payload_length:7,
		mask:1;

} __attribute__ ((packed));

struct _nty_websocket_head_126 {
	unsigned short payload_length;
	char mask_key[4];
	unsigned char data[8];
} __attribute__ ((packed));

struct _nty_websocket_head_127 {

	unsigned long long payload_length;
	char mask_key[4];

	unsigned char data[8];
	
} __attribute__ ((packed));

#define UNION_HEADER(type1, type2) 	\
	struct {						\
		struct type1 hdr;			\
		struct type2 len;			\
	}

typedef struct _nty_websocket_head_127 nty_websocket_head_127;
typedef struct _nty_websocket_head_126 nty_websocket_head_126;
typedef struct _nty_ophdr nty_ophdr;

static int ntySetNonblock(int fd) {
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) return -1;
	return 0;
}

static int ntySetBlock(int fd)  
{
    int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return flags; 
    flags &=~O_NONBLOCK;  
    if (fcntl(fd, F_SETFL, flags) < 0) return -1;

	return 0;
} 


int send_buffer(int sockfd, char *buffer, int length) {
	
	int idx = 0;

	while (idx < length) {

		int count = 0;
		if ((idx + MAX_BUFFER_LENGTH) < length) {
			count = send(sockfd, buffer+idx, MAX_BUFFER_LENGTH, 0);
		} else {
			count = send(sockfd, buffer+idx, length-idx, 0);
		}
		if (count <= 0) break;
		
		idx += count;
	}
	return idx;
}


int recv_buffer(int sockfd, char *buffer, int length, int *ret) {
	
	int idx = 0;

	while (1) {
		int count = recv(sockfd, buffer+idx, length-idx, 0);
			printf("recv: %d\n", count);
		if (count == 0) {
			*ret = -1;
			close(sockfd);
			break;
		} else if (count == -1) {
			printf("recv success --> count : %d\n", idx);
			*ret = 0;
			break;
		} else {			
			idx += count;
			if (idx >= length) break;
		}
	}

	return idx;
}


int init_server(void) {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("socket failed\n");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(KING_SERVER_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		printf("bind failed\n");
		return -2;
	}

	if (listen(sockfd, 5) < 0) {
		printf("listen failed\n");
		return -3;
	}

	return sockfd;
	
}


int base64_encode(char *in_str, int in_len, char *out_str) {    
	BIO *b64, *bio;    
	BUF_MEM *bptr = NULL;    
	size_t size = 0;    

	if (in_str == NULL || out_str == NULL)        
		return -1;    

	b64 = BIO_new(BIO_f_base64());    
	bio = BIO_new(BIO_s_mem());    
	bio = BIO_push(b64, bio);
	
	BIO_write(bio, in_str, in_len);    
	BIO_flush(bio);    

	BIO_get_mem_ptr(bio, &bptr);    
	memcpy(out_str, bptr->data, bptr->length);    
	out_str[bptr->length-1] = '\0';    
	size = bptr->length;    

	BIO_free_all(bio);    
	return size;
}


int readline(char* allbuf,int level,char* linebuf){    
	int len = strlen(allbuf);    

	for (;level < len; ++level)    {        
		if(allbuf[level]=='\r' && allbuf[level+1]=='\n')            
			return level+2;        
		else            
			*(linebuf++) = allbuf[level];    
	}    

	return -1;
}

void umask(char *data,int len,char *mask){    
	int i;    
	for (i = 0;i < len;i ++)        
		*(data+i) ^= *(mask+(i%4));
}


int handshake(int cli_fd) {    
	int level = 0;     
	char buffer[MAX_BUFFER_LENGTH];    
	char linebuf[256];    //Sec-WebSocket-Accept    
	char sec_accept[32] = {0};    //sha1 data    
	unsigned char sha1_data[SHA_DIGEST_LENGTH+1] = {0};    //reponse head buffer    
	char head[MAX_BUFFER_LENGTH] = {0};    

#if 1
	if (read(cli_fd, buffer, sizeof(buffer))<=0)    //read / recv    
		perror("read");
#else

	int ret = 0;
	int length = recv_buffer(cli_fd, buffer, MAX_BUFFER_LENGTH, &ret);
	if (ret < 0) perror("read");

#endif
	printf("request\n");    
	printf("%s\n",buffer);   
	
	do {        
		memset(linebuf, 0, sizeof(linebuf));        
		level = readline(buffer,level,linebuf); 

		if (strstr(linebuf,"Sec-WebSocket-Key") != NULL)        {   
			
			strcat(linebuf, GUID);    
			
			SHA1((unsigned char*)&linebuf+19,strlen(linebuf+19),(unsigned char*)&sha1_data);  
			
			base64_encode(sha1_data,strlen(sha1_data),sec_accept);           
			sprintf(head, "HTTP/1.1 101 Switching Protocols\r\n" \
				"Upgrade: websocket\r\n" \
				"Connection: Upgrade\r\n" \
				"Sec-WebSocket-Accept: %s\r\n" \
				"\r\n", sec_accept);            

			printf("response\n");            
			printf("%s",head);            
#if 1
			if (write(cli_fd, head, strlen(head)) < 0)     //write ---> send            
				perror("write");            
#else

			length = send_buffer(cli_fd, head, strlen(head));
			assert(length == strlen(head));

#endif
			break;        
		}    

	}while((buffer[level]!='\r' || buffer[level+1]!='\n') && level!=-1);    

	return 0;

}

char* decode_packet(unsigned char *stream, char *mask, int length, int *ret) {

	nty_ophdr *hdr =  (nty_ophdr*)stream;
	unsigned char *data = stream + sizeof(nty_ophdr);
	int size = 0;
	int start = 0;
	//char mask[4] = {0};
	int i = 0;

	if ((hdr->payload_length & 0x7F) == 126) {

		nty_websocket_head_126 *hdr126 = (nty_websocket_head_126*)data;
		size = ntohs(hdr126->payload_length);

		printf("size: %d\n", size);
		
		for (i = 0;i < 4;i ++) {
			mask[i] = hdr126->mask_key[i];
		}
		
		start = 8;
		
	} else if ((hdr->payload_length & 0x7F) == 127) {

		nty_websocket_head_127 *hdr127 = (nty_websocket_head_127*)data;
		size = ntohl(hdr127->payload_length);

		printf("size: %d\n", size);
		
		for (i = 0;i < 4;i ++) {
			mask[i] = hdr127->mask_key[i];
		}
		
		start = 14;

	} else {
		size = hdr->payload_length;

		memcpy(mask, data, 4);
		start = 6;
	}

	*ret = size;
	umask(stream+start, size, mask);

	return stream + start;
	
}

int encode_packet(char *buffer,char *mask, char *stream, int length) {

	nty_ophdr head = {0};
	head.fin = 1;
	head.opcode = 1;
	head.mask = 0;
	int size = 0;

	if (length < 126) {
		head.payload_length = length;
		memcpy(buffer, &head, sizeof(nty_ophdr));
		size = 2;
		memcpy(buffer+2, stream, length);
	} else if (length < 0xffff) {

		head.payload_length = 126;
		memcpy(buffer, &head, sizeof(nty_ophdr));

		nty_websocket_head_126 hdr = {0};
		unsigned short len = (unsigned short)length;
		hdr.payload_length = htons(len);

		memcpy(buffer+sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_126));
		size = 4;

		memcpy(buffer+4, stream, length);
		
	} else {
		
		head.payload_length = 127;

		nty_websocket_head_127 hdr = {0};
		hdr.payload_length = htonl(length);
		//memcpy(hdr.mask_key, mask, 4);
		
		memcpy(buffer, &head, sizeof(nty_ophdr));
		memcpy(buffer+sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_127));

		size = 10;
		memcpy(buffer+10, stream, length);
		
	}

	

	return length + size;
}


int main() {
	int sockfd = init_server();
	if (sockfd < 0) {
		printf("init_server failed\n");
		return -1;
	}
	
	int epoll_fd = epoll_create(MAX_EPOLL_SIZE);
	struct epoll_event ev, events[MAX_EPOLL_SIZE] = {0};

	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);

	while (1) {

		int nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_SIZE, -1);
		if (nfds < 0) {
			printf("epoll_wait failed\n");
			break;
		}

		int i = 0;
		for (i = 0;i < nfds;i ++) {
			int connfd = events[i].data.fd;

			if (connfd == sockfd) {

				struct sockaddr_in client_addr = {0};
				socklen_t client_len = sizeof(struct sockaddr_in);

				int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
				if (clientfd < 0) continue;

				printf("New Client Comming\n");
				
				handshake(clientfd);
				ntySetNonblock(clientfd); 

				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = clientfd;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientfd, &ev);
				
			} else {

				int ret = 0;
				char stream[MAX_BUFFER_LENGTH] = {0};
				
				int length = recv_buffer(connfd, stream, MAX_BUFFER_LENGTH, &ret);
				if (ret < 0) {
					ev.events = EPOLLIN | EPOLLET;
					ev.data.fd = connfd;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connfd, &ev);
					
				} else if (ret == 0) {

					char mask[4] = {0};
					char *data = decode_packet(stream, mask, length, &ret);

					printf("data : %s , length : %d\n", data, ret);

#if 1			
					char *buffer = (char*)calloc(1, length+14);
					ret = encode_packet(buffer, mask, data, ret);
					
					ret = send_buffer(connfd, buffer, ret);

					free(buffer);
#endif
				}

			}
		}
		
	}
}



