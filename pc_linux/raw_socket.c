#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/if_packet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#define ETH_P_QEMU 0x8874 //自定义的以太网协议type

typedef struct _ehthdr //定义eth首部
{
    unsigned char dest_1; //Destination MAC Address
    unsigned char dest_2;
    unsigned char dest_3;
    unsigned char dest_4;
    unsigned char dest_5;
    unsigned char dest_6;
    unsigned char src_1;  //Source MAC Address
    unsigned char src_2;
    unsigned char src_3;
    unsigned char src_4;
    unsigned char src_5;
    unsigned char src_6;
    unsigned short type; //EtherType
}ETH_HEADER;

typedef struct _datahdr //定义data
{
    char payload[1500-14];
}DATA_HEADER;
typedef struct _data_recv //定义data_recv
{
    char data_1[16];
    char data_2[16];
    char data_3[32];
    int  data_4;
}DATA_RECV;


void analyseETH(ETH_HEADER *eth);
void analyseDATA(DATA_RECV *data);

int my_protocol_flag = 0;
int count = 0;
unsigned char src_mac[6] = {0x00,0x0c,0x29,0x92,0x9F,0x96};  //VMware
unsigned char dst_mac[6] = {0x01,0x02,0x03,0x04,0x05,0x06};  //Pynq
unsigned char send_data[1500];
int main(void)
{
    int sockfd;
	 ETH_HEADER *eth;
	 //DATA_HEADER *data;
	 DATA_RECV *data;
	char *interface="eth0";
    char rec_buf[1500];
	char send_buf[1500];
	int datalen=0; 
	struct sockaddr_ll device_send;
	struct sockaddr_ll device_recv;
    ssize_t n;
    /* capture ip datagram without ethernet header */
    if ((sockfd = socket(PF_PACKET,  SOCK_RAW, htons(ETH_P_QEMU)))== -1)
    {    
        perror("socket error!\n");
        return 1;
    }
	/**************************send device init***************************/
    memset (&device_send, 0, sizeof (device_send));
	device_send.sll_family    = AF_PACKET;//填写AF_PACKET,不再经协议层处理
	device_send.sll_protocol  = htons(ETH_P_ALL);
	//网卡eth0的index，非常重要，系统把数据往哪张网卡上发，就靠这个标识
    if ((device_send.sll_ifindex = if_nametoindex (interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
	//printf ("Index for interface %s is %i\n", interface, device_send.sll_ifindex);

	device_send.sll_pkttype   = PACKET_OTHERHOST;//标识包的类型为发出去的包
	device_send.sll_halen     = 6;    //目标MAC地址长度为6

	//填写目标MAC地址
	device_send.sll_addr[0]   = 0x01;
	device_send.sll_addr[1]   = 0x02;
	device_send.sll_addr[2]   = 0x03;
	device_send.sll_addr[3]   = 0x04;
	device_send.sll_addr[4]   = 0x05;
	device_send.sll_addr[5]   = 0x06;

	/**************************recv device init***************************/

    while (1)
    {
		/******************************reveive*************************/
        //n = recv(sockfd, rec_buf, sizeof(rec_buf), NULL);
		n = recvfrom(sockfd, rec_buf, sizeof(rec_buf), 0,NULL, NULL);
        if (n == -1)
        {
            perror("recv error!\n");
            break;
        }
        else if (n==0)
            continue;
        //接收数据包括数据链路帧头
		eth = ( ETH_HEADER *)(rec_buf);
		analyseETH(eth);
		size_t ethlen =  14;
		if(my_protocol_flag == 1)
		{
			data = (DATA_RECV *)(rec_buf + ethlen);
			analyseDATA(data);
		}
		printf ("recv_n = %d ,Now begin to send\n",n);
		/******************************send*************************/
//		printf ("Index for interface %s is %i\n", interface, device.sll_ifindex);

		datalen = 12;
		send_data[0] = 'I';
		send_data[1] = 'a';
		send_data[2] = 'm';
		send_data[3] = 'V';
		send_data[4] = 'M';
		send_data[5] = 'w';
		send_data[6] = 'a';
		send_data[7] = 'r';
		send_data[8] = 'e';
		send_data[9] = '!';
		send_data[10] = '!';
		send_data[11] = '!';
		//MAC
		memcpy (send_buf, dst_mac, 6);
		memcpy (send_buf + 6, src_mac, 6);
		//Type
		send_buf[12] = ETH_P_QEMU / 256;
		send_buf[13] = ETH_P_QEMU % 256;
		//Data
		memcpy (send_buf + 14 , send_data, datalen);
		n = sendto(sockfd, send_buf, ethlen+datalen, 0,(struct sockaddr *) &device_send, sizeof (device_send));
		//n = send(sockfd, send_buf, sizeof(send_buf), 0);
		printf ("\n",n);
        if (n == -1)
        {
            perror("send error!\n");
            break;
        }
        else if (n==0)
            continue;
    }
    close(sockfd);
    return 0;
}
void analyseETH(ETH_HEADER *eth)
{
	unsigned char* p = (unsigned char*)&eth->type;
	if(p[0] == 0x88 && p[1] == 0x74)
	{
		count++;
		my_protocol_flag = 1;
		printf("EHT my protocol-----%d\n",count);
		printf("dest: %02x.%02x.%02x.%02x.%02x.%02x\n", eth->dest_1,eth->dest_2,eth->dest_3,eth->dest_4,eth->dest_5,eth->dest_6);
		printf("src: %02x.%02x.%02x.%02x.%02x.%02x\n", eth->src_1,eth->src_2,eth->src_3,eth->src_4,eth->src_5,eth->src_6);
		printf("type: %02x%02x\n", p[0],p[1]);
	}
	else
	{
		my_protocol_flag = 0;
	}
}
void analyseDATA(DATA_RECV *data)
{
    printf("DATA -----\n");
    printf("data_1 is: %s\n", data->data_1);
	printf("data_2 is: %s\n", data->data_2);
	printf("data_3 is: %s\n", data->data_3);
	printf("data_4 is: %d\n", data->data_4);
}

