
/*
 * 该工程用于pynq和PC机通过以太网通信（没有以外网协议，仅仅传输Ethernet在数据链路层的数据包）
 * 以太网参数：
 * pynq:
 * 		EtherType:0x8874
 * 		MAC:01-02-03-04-05-06
 * 		PC :00-0C-29-92-9F-96
 * Pynq在FPGA端部署了HLS编写的IP模块，用于FPGA加速
 * 目前HLS的IP包括：
 * 1.pid_regulator
 *
 * ARM端的中断服务函数有以下：
 * 以太网：
 * --以太网接收中断：XEmacPs_Rx_InterruptHandler
 * --以太网发送中断：XEmacPs_Tx_InterruptHandler
 * --以太网Error中断：XEmacPs_Error_InterruptHandler
 * 定时器中断：XEmacPs_TimerInterruptHandler_MY
 * FPGA外设中断：
 * --pid_regulator接收中断：hls_pid_return_isr
 *
 * 代码流程：
 * 以太网接收中断用于接收所有来自PC端的以太网帧，且在main()函数的while(1)循环中不断检测接收到的以太网帧的格式是否是
 * 所需的0x8874类型的以太网帧,若是，则立即向FPGA端的IP写入数据，然后等待该IP计算结束触发中断，当ARM检测到来自FPGA
 * 端模块的中断信号时，代表该模块计算结束，此时ARM将此模块结果读出，通过以太网发送函数，发送至PC端。
 * 数据通路：
 * 		PC--以太网接收--ARM--传入FPGA--FPGA计算完成中断--ARM--以外网发送--PC
 *
 * 待改进地方：
 * 1.没有看门狗，卡死就没办法恢复了
 * 2.while(1)中操作不能太耗时，否则容易被中断打断，从而 卡死，可以考虑使用RTOS操作系统。
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include "my_mac_driver.h"

int Prepare_send_packet(float Return_real)
{
	int datalen=0;
	DATA_SEND *data_send;        //Payload of Ethernet
	strcpy(data_send->data_1,"I am zynq");
	strcpy(data_send->data_2,"Who are you?");
	strcpy(data_send->data_3,"How are you?");
	data_send->data_4 = 666;
	data_send->data_5 = Return_real;
    datalen = sizeof(DATA_SEND)/sizeof(unsigned char);
    printf("datalen = %d\n",datalen);

	memcpy (MySendbuf, dst_mac, 6);
	memcpy (MySendbuf + 6, src_mac, 6);

	MySendbuf[12] = ETH_P_DEAN / 256;
	MySendbuf[13] = ETH_P_DEAN % 256;

	// data
	memcpy (MySendbuf + 14 , data_send, datalen);
	return datalen;
}

int main()
{
    init_platform();
    int Status = XST_SUCCESS;
    int status;
	u32 Return_temp;
	float Return_real;
	int datalen=0;
    XEmacPs *EmacPsInstancePtr = &Mac;
    print("Hello World\n\r");

    // Setup the PID instances
    status=hls_PID_init(&Hls_pid);
    if(status != XST_SUCCESS){
    	print("HLS peripheral setup failed\n\r");
    	return(-1);
    }
    ResultAvailHls_PID = 0;

    // Setup the MAC instances
    Status = mymacinit(src_mac,EmacPsInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

    // Enable Global and instance interrupts
    XPid_regulator_InterruptEnable(&Hls_pid,1);
    XPid_regulator_InterruptGlobalEnable(&Hls_pid);


    DATA_SEND *data_send;        //Payload of Ethernet
	strcpy(data_send->data_1,"I am zynq");
	strcpy(data_send->data_2,"Who are you?");
	strcpy(data_send->data_3,"How are you?");
	data_send->data_4 = 666;
	memcpy (MySendbuf, dst_mac, 6);
	memcpy (MySendbuf + 6, src_mac, 6);

	MySendbuf[12] = ETH_P_DEAN / 256;
	MySendbuf[13] = ETH_P_DEAN % 256;


	while(1)
	{
		/*
		 *pid_regulator模块的结果计算完成后，触发中断，ARM接收到FPGA端中断后，将ResultAvailHls_PID置为1
		 *于是执行以下代码，功能是将得到的HLS模块的计算结果通过以太网发送至PC Linux端。
		*/
		if( ResultAvailHls_PID == 1 )
		{
			ResultAvailHls_PID = 0;
			Return_temp = XPid_regulator_Get_return(&Hls_pid);
			Return_real = *((float*)&Return_temp);

			data_send->data_5 = Return_real;
		    datalen = sizeof(DATA_SEND)/sizeof(unsigned char);
		    //printf("datalen = %d\n",datalen);
			// data
			memcpy (MySendbuf + 14 , data_send, datalen);
			Mac_driver_PacketSend(14+datalen, MySendbuf,EmacPsInstancePtr);		   //调用网卡发送函数
			//printf("Return_real = %f\n\r",Return_real);  //如果采集到的数是0，说明有问题
		}
		else{;}

		/*
		 *当以太网接收到来自PC的以太网帧（各种帧）后，会在接收中断中将NewPktRecd置为1
		 *于是执行以下代码，功能是检测接收到的以太网帧，若其格式符合预先设定的0X8874,则将其内容打印
		 *进而执行pid_input()函数，该函数用于向pid_regulator模块中写入数据
		*/
		if(NewPktRecd == TRUE)
		{
			NewPktRecd = 0;
			char unsigned *buf;
			ETH_HEADER *eth;
			DATA_HEADER *data;
		   //接收数据不包括数据链路帧头
			buf = MyRecvbuf;
			eth = ( ETH_HEADER *)(buf);
			analyseETH(eth);
			size_t ethlen =  14;
			if(my_protocol_flag == 1)
			{
				pid_input();
				data = (DATA_HEADER *)(buf + ethlen);
				analyseDATA(data);
			}
		}
		else{;}
	}

    cleanup_platform();
    return 0;
}

