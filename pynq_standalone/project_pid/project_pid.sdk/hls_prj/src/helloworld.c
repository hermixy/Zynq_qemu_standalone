
/*
 * �ù�������pynq��PC��ͨ����̫��ͨ�ţ�û��������Э�飬��������Ethernet��������·������ݰ���
 * ��̫��������
 * pynq:
 * 		EtherType:0x8874
 * 		MAC:01-02-03-04-05-06
 * 		PC :00-0C-29-92-9F-96
 * Pynq��FPGA�˲�����HLS��д��IPģ�飬����FPGA����
 * ĿǰHLS��IP������
 * 1.pid_regulator
 *
 * ARM�˵��жϷ����������£�
 * ��̫����
 * --��̫�������жϣ�XEmacPs_Rx_InterruptHandler
 * --��̫�������жϣ�XEmacPs_Tx_InterruptHandler
 * --��̫��Error�жϣ�XEmacPs_Error_InterruptHandler
 * ��ʱ���жϣ�XEmacPs_TimerInterruptHandler_MY
 * FPGA�����жϣ�
 * --pid_regulator�����жϣ�hls_pid_return_isr
 *
 * �������̣�
 * ��̫�������ж����ڽ�����������PC�˵���̫��֡������main()������while(1)ѭ���в��ϼ����յ�����̫��֡�ĸ�ʽ�Ƿ���
 * �����0x8874���͵���̫��֡,���ǣ���������FPGA�˵�IPд�����ݣ�Ȼ��ȴ���IP������������жϣ���ARM��⵽����FPGA
 * ��ģ����ж��ź�ʱ��������ģ������������ʱARM����ģ����������ͨ����̫�����ͺ�����������PC�ˡ�
 * ����ͨ·��
 * 		PC--��̫������--ARM--����FPGA--FPGA��������ж�--ARM--����������--PC
 *
 * ���Ľ��ط���
 * 1.û�п��Ź���������û�취�ָ���
 * 2.while(1)�в�������̫��ʱ���������ױ��жϴ�ϣ��Ӷ� ���������Կ���ʹ��RTOS����ϵͳ��
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
		 *pid_regulatorģ��Ľ��������ɺ󣬴����жϣ�ARM���յ�FPGA���жϺ󣬽�ResultAvailHls_PID��Ϊ1
		 *����ִ�����´��룬�����ǽ��õ���HLSģ��ļ�����ͨ����̫��������PC Linux�ˡ�
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
			Mac_driver_PacketSend(14+datalen, MySendbuf,EmacPsInstancePtr);		   //�����������ͺ���
			//printf("Return_real = %f\n\r",Return_real);  //����ɼ���������0��˵��������
		}
		else{;}

		/*
		 *����̫�����յ�����PC����̫��֡������֡���󣬻��ڽ����ж��н�NewPktRecd��Ϊ1
		 *����ִ�����´��룬�����Ǽ����յ�����̫��֡�������ʽ����Ԥ���趨��0X8874,�������ݴ�ӡ
		 *����ִ��pid_input()�������ú���������pid_regulatorģ����д������
		*/
		if(NewPktRecd == TRUE)
		{
			NewPktRecd = 0;
			char unsigned *buf;
			ETH_HEADER *eth;
			DATA_HEADER *data;
		   //�������ݲ�����������·֡ͷ
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
