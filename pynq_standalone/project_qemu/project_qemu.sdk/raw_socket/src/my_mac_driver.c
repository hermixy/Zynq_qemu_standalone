//#include "enc28j60.h"
#include "my_mac_driver.h"
/************************** Variable Definitions *****************************/
static XScuGic IntcInstance;    /* The instance of the SCUGic Driver */
static XScuTimer TimerInstance; /* The instance of the SCUTimer Driver */

/* Detected link speed goes here. */
int Link_Speed = 100;
/* Detected PHY address goes here. */
int PhyAddress;
u32 PtpNewPktRecd = 0;

int init_finish_flag=0;

unsigned int recvlen_temp = 0;
unsigned char  MyRecvbuf_temp[1500] = {0};
/*
 * Aligned memory segments to be used for Rx buffer descriptors
 */

u8 RxBuf[RX_DESCS][XEMACPS_PACKET_LEN + 2]
		__attribute__ ((aligned(XEMACPS_RX_BUF_ALIGNMENT)));


volatile u8 PDelayRespSent;
volatile u8 SyncSent;
volatile u32 PTPSendPacket = 0;

int count_rx,count_tx=0;

unsigned int my_protocol_flag = 0;
unsigned int count = 0;

unsigned char src_mac[6] = {0x01,0x02,0x03,0x04,0x05,0x06}; //Pynq
unsigned char dst_mac[6] = {0X00,0x0C,0x29,0x92,0x9F,0x96}; //VMware

int mymacinit(unsigned char *mymac, XEmacPs *EmacPsInstancePtr)
{
	XEmacPs_Config *Cfg;
	int Status = XST_SUCCESS;
	u32 Emac_option = 0;

	xil_printf("Entering into main() \r\n");

	Xil_DisableMMU();
	Xil_SetTlbAttributes(0x0FF00000, 0xc02); // addr, attr
	Xil_EnableMMU();

	/* Initialize SCUTIMER */
	if (XEmacPs_InitScuTimer()  != XST_SUCCESS) while(1);
	/*
	 * Get the configuration of EmacPs hardware.
	 */
	Cfg = XEmacPs_LookupConfig(EMACPS_DEVICE_ID);
	/*
	 * Initialize EmacPs hardware.
	 */
	Status = XEmacPs_CfgInitialize(EmacPsInstancePtr, Cfg,
							Cfg->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	/*
	 * Set the MAC address
	 */
	Status = XEmacPs_SetMacAddress(EmacPsInstancePtr,
					(unsigned char*)mymac, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XEmacPs_SetMdioDivisor(EmacPsInstancePtr, MDC_DIV_224);

	/*
	 * Detect and initialize the PHY
	 */
	EmacPs_PHY_init(EmacPsInstancePtr);
	sleep(2);
	/*
	 * Set the operating speed in EmacPs hardware.
	 */
	XEmacPs_SetOperatingSpeed(EmacPsInstancePtr, 1000);
	sleep(2);

	XEmacPs_Stop(EmacPsInstancePtr);
	/*
	 * Enable the promiscuous mode in EmacPs hardware.
	 */
	Status = XEmacPs_SetOptions(EmacPsInstancePtr, XEMACPS_PROMISC_OPTION);
	if (Status != XST_SUCCESS) {
		xil_printf("XEmacPs_SetOptions fail\n");
		return XST_FAILURE;
	}
	Emac_option = XEmacPs_GetOptions(EmacPsInstancePtr);
	xil_printf("Emac_option = %x\n",Emac_option);
/*	Status = XEmacPs_ClearOptions(EmacPsInstancePtr, XEMACPS_BROADCAST_OPTION);
	if (Status != XST_SUCCESS) {
		xil_printf("XEmacPs_ClearOptions fail\n");
		return XST_FAILURE;
	}
	Emac_option = XEmacPs_GetOptions(EmacPsInstancePtr);
	xil_printf("Emac_option = %x\n",Emac_option);
	//不注释这部分代码的话，会卡死
*/
	/*
	 * Register Ethernet Rx, Tx and Error handlers with the EmacPs driver.
	 */
	Status = XEmacPs_SetHandler (EmacPsInstancePtr,
				XEMACPS_HANDLER_DMARECV,
				(void *)XEmacPs_Rx_InterruptHandler,
				EmacPsInstancePtr);
	Status |= XEmacPs_SetHandler (EmacPsInstancePtr,
				XEMACPS_HANDLER_DMASEND,
				(void *)XEmacPs_Tx_InterruptHandler,
				EmacPsInstancePtr);
	Status |= XEmacPs_SetHandler (EmacPsInstancePtr,
				XEMACPS_HANDLER_ERROR,
				(void *)XEmacPs_Error_InterruptHandler,
				EmacPsInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect to the interrupt controller and enable interrupts in
	 * interrupt controller.
	 */
	Status = XEmacPs_SetupIntrSystem(&IntcInstance, EmacPsInstancePtr,
					&TimerInstance, EMACPS_IRPT_INTR,
					TIMER_IRPT_INTR);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//STOP

	/*
	 * Initialize the DMA and buffer descriptors
	 */
	Status = XEmacPs_InitializeEmacPsDma (EmacPsInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	/*
	 * Enable the timer interrupt in the timer module
	 */
	XScuTimer_EnableInterrupt(&TimerInstance);
	init_finish_flag = 1;
	xil_printf("MAC init successfully\n");
	return XST_SUCCESS;
}
/*****************************************************************************/
/**
*
* This function is the timer ISR that is invoked every 500 mseconds. The Tx of
* PTP packets are initiated from here at appropriate intervals. When the PTP
* node is Master, the Tx of SYNC frame is triggered from here. Similarly
* Announce frame and PDelayReq frame Tx are triggered from here.
* When the PTP node is Slave, the PDelayReq frame Tx is triggered from here.
*
*
* @param	InstancePntr is a pointer to the instance of the
*		XEmacPs_Ieee1588.
*
* @return	None.
*
* @note		The intervals at which SYNC, ANNOUNCE and PDelayReq are
*		triggered are hard coded to 1 sec, 5 seconds and 4 seconds
*		respectively. When signalling frames are implemented
*		the hardcoded values can be replaced with proper signalling
*		frame values.
*
******************************************************************************/
static unsigned char MySendbuf[1500];  //All Ethernet
void XEmacPs_TimerInterruptHandler_MY(XEmacPs *EmacPsInstancePtr)
{
	XScuTimer *TimerInstancePntr = &TimerInstance;
	/*
	 * Clear the Timer interrupt.
	 */
	XScuTimer_ClearInterruptStatus(TimerInstancePntr);
	static unsigned int count_5ms=0;
	int datalen=0;
	count_5ms++;
	if(count_5ms==400)  //2000ms
	{
		count_5ms = 0;
	    // 发送的data，长度可以任意，但是抓包时看到最小数据长度为46，这是以太网协议规定以太网帧数据域部分最小为46字节，不足的自动补零处理
		DATA_SEND *data_send;        //Payload of Ethernet
		strcpy(data_send->data_1,"I am zynq");
		strcpy(data_send->data_2,"Who are you?");
		strcpy(data_send->data_3,"How are you?");
		data_send->data_4 = 666;
	    datalen = sizeof(DATA_SEND)/sizeof(unsigned char);
	    printf("datalen = %d\n",datalen);

		memcpy (MySendbuf, dst_mac, 6);
		memcpy (MySendbuf + 6, src_mac, 6);

		MySendbuf[12] = ETH_P_DEAN / 256;
		MySendbuf[13] = ETH_P_DEAN % 256;

		// data
		memcpy (MySendbuf + 14 , data_send, datalen);
		Mac_driver_PacketSend(14+datalen, MySendbuf,EmacPsInstancePtr);		   //调用网卡发送函数

	}
}
/*****************************************************************************/
/**
*
* This function is the EmacPs Rx interrupt callback invoked from the EmacPs
* driver. Here we set the flag PtpNewPktRecd to true. This flag is checked for
* in the function XEmacPs_RunIEEE1588Protocol for further processing of
* packets.
*
* @param	InstancePntr is a pointer to the instance of the
*		XEmacPs_Ieee1588.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static unsigned char  MyRecvbuf[1500];
void XEmacPs_Rx_InterruptHandler(XEmacPs *EmacPsInstancePtr)
{
	count_rx++;

	PtpNewPktRecd = TRUE;

/******************************************/
	unsigned int recvlen = 0;
	char unsigned *buf;
	ETH_HEADER *eth;
	DATA_HEADER *data;
	recvlen = Mac_driver_PacketReceive(1500, MyRecvbuf, EmacPsInstancePtr);
//		xil_printf("count_rx = %d\n\r",count_rx);
   //接收数据不包括数据链路帧头
	buf = MyRecvbuf;
	eth = ( ETH_HEADER *)(buf);
	analyseETH(eth);
	size_t ethlen =  14;
	if(my_protocol_flag == 1)
	{
		data = (DATA_HEADER *)(buf + ethlen);
		analyseDATA(data);
	}
	return;
}
/*****************************************************************************/
/**
*
* This function is the Tx Done interrupt callback invoked from the EmacPs
* driver.
* For some PTP packets, upon getting a Tx done interrupt some actions need
* to be taked. For example, when SYNC frame is successfully sent and Tx Done
* interrupt is received, the time stamp for the SYNC frame needs to be stored.
* For all such processing the function XEmacPs_PtpTxDoFurtherProcessing is
* invoked from here.
*
* @param	InstancePntr is a pointer to the instance of the
*		XEmacPs_Ieee1588.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XEmacPs_Tx_InterruptHandler (XEmacPs *InstancePtr)
{
	unsigned int NumBds;
	unsigned int NumBdsToProcess;
	XEmacPs_Bd *BdPtr, *CurBdPtr;
	void *BufAddr;
	int BufLen;
	int Status;
	XEmacPs_BdRing *TxRingPtr;
	unsigned int *Temp;
	count_tx++;
//	xil_printf("			count_tx = %d\n\r",count_tx);
	TxRingPtr = &XEmacPs_GetTxRing(InstancePtr);

	NumBds = XEmacPs_BdRingFromHwTx( TxRingPtr,TX_DESCS, &BdPtr);
	if (NumBds == 0) {
			return;
	}
	NumBdsToProcess = NumBds;
	CurBdPtr=BdPtr;
	while (NumBdsToProcess > 0)
	{
		BufAddr = (void*)(INTPTR)(XEmacPs_BdGetBufAddr(CurBdPtr));
		BufLen = XEmacPs_BdGetLength(CurBdPtr);

		Temp = (unsigned int *)CurBdPtr;
		Temp++;
		*Temp &= XEMACPS_TXBUF_WRAP_MASK;
		*Temp |= XEMACPS_TXBUF_USED_MASK;

		CurBdPtr = XEmacPs_BdRingNext(TxRingPtr, CurBdPtr);
		NumBdsToProcess--;
		dmb();
		dsb();
	}
	Status = XEmacPs_BdRingFree( TxRingPtr, NumBds, BdPtr);
	if (Status != XST_SUCCESS) {
		return;
	}
	return;
}
/*****************************************************************************/
/**
*
* This function is the Error interrupt callback invoked from the EmacPs driver.
*
* @param	InstancePntr is a pointer to the instance of the
*		XEmacPs_Ieee1588.
* @param	Direction can be Rx or Tx
* @param	ErrorWord gives further information about the exact error type.
*
* @return	None.
*
* @note		This function needs to be revisited. Probably upon an error
*		we need to reset the EmacPs hardware and reinitialize the BDs.
*		However further study is needed. Whether for all errors we
*		need to reset or some errors can be ignored.
*
******************************************************************************/
void XEmacPs_Error_InterruptHandler (XEmacPs *InstancePtr,
						u8 Direction, u32 ErrorWord)
{
	XEmacPs_Config *Cfg;
	int Status = XST_SUCCESS;

	xil_printf("In %s: EMAC Error Interrupt, Direction is %d and ErrorWord is %x \r\n",
					__func__, Direction, ErrorWord);
	XScuTimer_Stop(&TimerInstance);
	XEmacPs_Stop(InstancePtr);
	Xil_ExceptionDisable();
	PDelayRespSent = 0;
	SyncSent = 0;
	PTPSendPacket = 0;
	memset(RxBuf, 0, sizeof(RxBuf));
	Link_Speed = 100;
	/* Initialize SCUTIMER */
	if (XEmacPs_InitScuTimer()  != XST_SUCCESS) while(1);
	/*
	 * Get the configuration of EmacPs hardware.
	 */
	Cfg = XEmacPs_LookupConfig(EMACPS_DEVICE_ID);

	/*
	 * Initialize EmacPs hardware.
	 */
	Status = XEmacPs_CfgInitialize(InstancePtr, Cfg, Cfg->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("In function %s: XEmacPs_CfgInitialize failure \r\n",__func__);
	}

	/*
	 * Set the MAC address
	 */
	Status = XEmacPs_SetMacAddress(InstancePtr,
					(unsigned char*)src_mac, 1);
	if (Status != XST_SUCCESS)
	{
		xil_printf("In function %s: XEmacPs_SetMacAddress failure \r\n",__func__);
	}
	XEmacPs_SetMdioDivisor(InstancePtr, MDC_DIV_224);

	/*
	 * Detect and initialize the PHY
	 */
	EmacPs_PHY_init(InstancePtr);
	sleep(1);

	/*
	 * Set the operating speed in EmacPs hardware.
	 */
	XEmacPs_SetOperatingSpeed(InstancePtr, 1000);
	sleep(1);

	/*
	 * Enable the promiscuous mode in EmacPs hardware.
	 */
/*	Status = XEmacPs_SetOptions(InstancePtr, XEMACPS_PROMISC_OPTION);
	if (Status != XST_SUCCESS)
	{
		xil_printf("In function %s: XEmacPs_SetOptions failure \r\n",__func__);
		return;
	}
*/
	/*
	 * Register Ethernet Rx, Tx and Error handlers with the EmacPs driver.
	 */
	Status = XEmacPs_SetHandler (InstancePtr,
				XEMACPS_HANDLER_DMARECV,
				(void *)XEmacPs_Rx_InterruptHandler,
				InstancePtr);
	Status |= XEmacPs_SetHandler (InstancePtr,
				XEMACPS_HANDLER_DMASEND,
				(void *)XEmacPs_Tx_InterruptHandler,
				InstancePtr);
	Status |= XEmacPs_SetHandler (InstancePtr,
				XEMACPS_HANDLER_ERROR,
				(void *)XEmacPs_Error_InterruptHandler,
				InstancePtr);
	if (Status != XST_SUCCESS)
	{
		xil_printf("In function %s: XEmacPs_SetHandler failure \r\n",
							__func__);
	}
	/*
	 * Connect to the interrupt controller and enable interrupts in
	 * interrupt controller.
	 */
	Status = XEmacPs_SetupIntrSystem(&IntcInstance,
					InstancePtr,
					&TimerInstance, EMACPS_IRPT_INTR,
					TIMER_IRPT_INTR);
	if (Status != XST_SUCCESS)
	{
		xil_printf("In function %s: XEmacPs_SetupIntrSystem failure \r\n",__func__);
	}
	Xil_ExceptionEnable();
	/*
	 * Enable the timer interrupt in the timer module
	 */
	XScuTimer_EnableInterrupt(&TimerInstance);

	XEmacPs_Stop(InstancePtr);

	/*
	 * Initialize the DMA and buffer descriptors
	 */
	Status = XEmacPs_InitializeEmacPsDma (InstancePtr);
	if (Status != XST_SUCCESS) {
		xil_printf("In function %s: XEmacPs_InitializeEmacPsDma failure \r\n",__func__);
	}
}

/****************************************************************************
* 名    称：void enc28j60PacketSend(unsigned int len, unsigned char* packet)
* 功    能：通过ENC28J60发送数据
* 入口参数：
* 出口参数：
* 说    明：
* 调用方法：
****************************************************************************/
void Mac_driver_PacketSend(unsigned int PacketLen, unsigned char* PacketBuf ,XEmacPs *EmacPsInstancePtr)
{
	int Status;
	XEmacPs_Bd *BdPtr;
	XEmacPs_BdRing *TxRingPtr;
	Xil_ExceptionDisable();

    while((XEmacPs_ReadReg(EmacPsInstancePtr->Config.BaseAddress,
		XEMACPS_TXSR_OFFSET)) & 0x08);

	TxRingPtr = &(XEmacPs_GetTxRing(EmacPsInstancePtr));
	Status = XEmacPs_BdRingAlloc(TxRingPtr, 1, &BdPtr);
	if (Status != XST_SUCCESS) {
		Xil_ExceptionEnable();
	}
	/*
	 * Fill the BD entries for the Tx1!!1`
	 */

	Xil_DCacheFlushRange((u32)PacketBuf, sizeof(EthernetFrame));//128

	XEmacPs_BdSetAddressTx (BdPtr, PacketBuf);
	XEmacPs_BdSetLength(BdPtr, PacketLen);
	XEmacPs_BdClearTxUsed(BdPtr);
	XEmacPs_BdSetLast(BdPtr);
	dmb();
	dsb();

	Status = XEmacPs_BdRingToHw(TxRingPtr, 1, BdPtr);
	if (Status != XST_SUCCESS) {
		Xil_ExceptionEnable();
	}
	dmb();
	dsb();
	/*
	 * Start the Tx
	 */
	XEmacPs_Transmit(EmacPsInstancePtr);
	Xil_ExceptionEnable();
}
/****************************************************************************
* 名    称：unsigned int enc28j60PacketReceive(unsigned int maxlen, unsigned char* )
* 功    能：
* 入口参数：从网络接收缓冲区获取一包
			maxlen： 检索到的数据包的最大可接受的长度
			packet:  数据包的指针
* 出口参数: 如果一个数据包收到返回数据包长度，以字节为单位，否则为零。
* 说    明：
* 调用方法：
****************************************************************************/
unsigned int Mac_driver_PacketReceive(unsigned int maxlen, unsigned char* packet ,XEmacPs *EmacPsInstancePtr)
{
	XEmacPs_Bd *BdPtr;
	XEmacPs_Bd *CurBdPtr;
	unsigned int NumBds;
	unsigned int FreeBds;
	int i;
	int j;
	u8 *BufAddr;
	unsigned int BufLen;
	int Status;
	XEmacPs_BdRing *RxRingPtr;

	/*
	 * Get the ring pointers from EmacPs instance
	 */
	RxRingPtr = &XEmacPs_GetRxRing(EmacPsInstancePtr);

	/*
	 * Extract all available BDs from EmacPs.
	 */
	NumBds = XEmacPs_BdRingFromHwRx( RxRingPtr,RX_DESCS, &BdPtr);
	if (NumBds == 0)
		return (0);
	for (i = 0, CurBdPtr=BdPtr; i < NumBds; i++)
	{
		/*
		 * Get the buffer address in which the PTP packet is
		 * stored from the BD.
		 */
		BufAddr = (void*)(INTPTR)(XEmacPs_BdGetBufAddr(CurBdPtr) &
		~(XEMACPS_RXBUF_WRAP_MASK | XEMACPS_RXBUF_NEW_MASK));
		BufLen = XEmacPs_BdGetLength(CurBdPtr);
		// 限制检索的长度
	    if (BufLen>maxlen-1)
		{
	    	BufLen=maxlen-1;
	   	}

		Xil_DCacheInvalidateRange((u32)BufAddr, BufLen);//132

		memcpy(packet, BufAddr, BufLen);
		/*
		 * Clear the used bit in the buffer so that it can
		 * be reused.
		 */
		BufAddr[XEMACPS_PACKET_LEN - 2] = 0;
		CurBdPtr = XEmacPs_BdRingNext( RxRingPtr, CurBdPtr);
	}

	/*
	 * Time to free the BDs
	 */
	XEmacPs_BdRingFree(RxRingPtr, NumBds, BdPtr);
	/*
	 * Time to reallocate the BDs
	 */
	FreeBds = XEmacPs_BdRingGetFreeCnt (RxRingPtr);
	Status = XEmacPs_BdRingAlloc (RxRingPtr, FreeBds,
							&BdPtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	CurBdPtr = BdPtr;
	for (i = 0; i < FreeBds; i++) {
		for (j = 0; j < RX_DESCS; j++) {
			if ((RxBuf[j][XEMACPS_PACKET_LEN - 2]) == 0) {
				XEmacPs_BdSetAddressRx
					(CurBdPtr, &(RxBuf[j][0]));
				/*
				 * Set the used bit in the Buffer
				 */
				RxBuf[j][XEMACPS_PACKET_LEN - 2] = 1;
				/*
				 * Clear the used bit so that it
				 * can be reused.
				 */
				XEmacPs_BdClearRxNew (CurBdPtr);
				break;
			}
		}
		CurBdPtr = XEmacPs_BdRingNext (RxRingPtr, CurBdPtr);
	}
	/*
	 * Submit the BDs to the hardware
	 */
	Status = XEmacPs_BdRingToHw (RxRingPtr, FreeBds, BdPtr);
	Xil_ExceptionEnable();
	return(BufLen);

}
int dest_flag,src_flag=0;
void analyseETH(ETH_HEADER *eth)
{
	unsigned char* p = (unsigned char*)&eth->type;
	if(eth->dest_1==0x01 && eth->dest_2==0x02 && eth->dest_3==0x03 && eth->dest_4==0x04 && eth->dest_5==0x05 && eth->dest_6==0x06)
	{
		dest_flag=1;
	}
	else
	{
		dest_flag=0;
	}
	if(eth->src_1==0x00 && eth->src_2==0x0C && eth->src_3==0x29 && eth->src_4==0x92 && eth->src_5==0x9F && eth->src_6==0x96)
	{
		src_flag=1;
	}
	else
	{
		src_flag=0;
	}
	if(p[0] == 0x88 && p[1] == 0x74 )//&& dest_flag == 1 && src_flag==1)
	{
		count++;
		my_protocol_flag = 1;
		printf("EHT my protocol-----%d\n",count);
		printf("dest: %02x.%02x.%02x.%02x.%02x.%02x\n", eth->dest_1,eth->dest_2,eth->dest_3,eth->dest_4,eth->dest_5,eth->dest_6);
		printf("src: %02x.%02x.%02x.%02x.%02x.%02x\n", eth->src_1,eth->src_2,eth->src_3,eth->src_4,eth->src_5,eth->src_6);
		printf("EtherType: %02x%02x\n", p[0],p[1]);
	}
	else
	{
		my_protocol_flag = 0;
	}
}
void analyseDATA(DATA_HEADER *data)
{
    printf("Payload is: %s\n\n", data->payload);
}
void analyseIP(IP_HEADER *ip)
{
    unsigned char* p = (unsigned char*)&ip->sourceIP;
    printf("Source IP\t: %u.%u.%u.%u\n",p[0],p[1],p[2],p[3]);
    p = (unsigned char*)&ip->destIP;
    printf("Destination IP\t: %u.%u.%u.%u\n",p[0],p[1],p[2],p[3]);

}
void analyseICMP(ICMP_HEADER *icmp)
{
    printf("ICMP -----\n");
    printf("type: %u\n", icmp->icmp_type);
    printf("sub code: %u\n", icmp->icmp_code);
}

/*****************************************************************************/
/**
*
* This function initializes the SCUTimer.
*
* @param	None.
*
* @return	- XST_SUCCESS to indicate success.
*		- XST_FAILURE to indicate failure
*
* @note		None.
*
******************************************************************************/
int XEmacPs_InitScuTimer(void)
{
	int Status = XST_SUCCESS;
	XScuTimer_Config *ConfigPtr;

	/*
	 * Get the configuration of Timer hardware.
	 */
	ConfigPtr = XScuTimer_LookupConfig(TIMER_DEVICE_ID);

	/*
	 * Initialize ScuTimer hardware.
	 */
	Status = XScuTimer_CfgInitialize(&TimerInstance, ConfigPtr,
			ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XScuTimer_EnableAutoReload(&TimerInstance);

	/*
	 * Initialize ScuTimer with a count so that the interrupt
	 * comes every 500 msec.
	 */
	XScuTimer_LoadTimer(&TimerInstance, TIMER_LOAD_VALUE); //500ms
	return XST_SUCCESS;
}



int EmacPs_PHY_init(XEmacPs * EmacPsInstancePtr)
{
	int Status;
	u16 PhyIdentity;
	u32 PhyAddr;

	/*
	 * Detect the PHY address
	 */
	PhyAddr = XEmacPsDetectPHY(EmacPsInstancePtr);

	if (PhyAddr >= 32) {
		xil_printf("Error detect phy");
		return XST_FAILURE;
	}

	XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, PHY_DETECT_REG1, &PhyIdentity);


	if (PhyIdentity == PHY_ID_REALTEK) {
		xil_printf(" The Realtek PHY detected successflly.\r\n");
		Status = EmacPsUtilRealtekPhy(EmacPsInstancePtr, PhyAddr);
	}

	if (Status != XST_SUCCESS) {
		xil_printf("Error setup phy loopback");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}
int EmacPsUtilRealtekPhy(XEmacPs * EmacPsInstancePtr, u32 PhyAddr)
{

	//上电自动进入Auto-negotiation 模式，并且默认设置支持10/100/1000速度 ，半/全双工
	LONG Status;
	u16 PhyReg1  = 0;
	int i=0;

	xil_printf("Start PHY Auto-negotiation \r\n");
	Status = XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 1, &PhyReg1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	while (!(PhyReg1 & PHY_R1_AN_COMPLETE)) {
	  XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 1, &PhyReg1);
	  sleep(1);
	}
	xil_printf("Auto-negotiation Complete\r\n");


	while (!(PhyReg1 & PHY_R1_LINK_UP)) {
	XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 1, &PhyReg1);
	  for(i=0;i<0xfffff;i++);
	  sleep(1);
	}
	xil_printf("Link is Up\r\n");

	return XST_SUCCESS;
}
int XEmacPsDetectPHY(XEmacPs * EmacPsInstancePtr)
{
	u32 PhyAddr;
	int Status;
	u16 PhyReg1;
	u16 PhyReg2;

	for (PhyAddr = 0; PhyAddr <= 31; PhyAddr++) {
		Status = XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr,
					  PHY_DETECT_REG1, &PhyReg1);

		Status |= XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr,
					   PHY_DETECT_REG2, &PhyReg2);

		if ((Status == XST_SUCCESS) &&
		    (PhyReg1 > 0x0000) && (PhyReg1 < 0xffff) &&
		    (PhyReg2 > 0x0000) && (PhyReg2 < 0xffff)) {
			/* Found a valid PHY address */
			return PhyAddr;
		}
	}

	return PhyAddr;		/* default to 32(max of iteration) */
}
/*****************************************************************************/
/**
*
* This function sets up interrupts. It registers interrupt handlers and then
* enables them..
*
* @param	IntcInstancePtr is a pointer to the instance of the SCUGic..
* @param	EmacPsInstancePtr is a pointer to the instance of the EmacPs.
* @param	TimerInstancePtr is a pointer to the instance of the SCUTimer.
* @param	EmacPsIntrId is the Interrupt ID for EmacPs and the value
*		used is taken from xparameters_ps.h.
* @param	TimerIntrId is the Interrupt ID for SCUTimer and the value
*		used is taken from xparameters_ps.h.
*
* @return	- XST_SUCCESS to indicate success.
*		- XST_FAILURE to indicate failure
*
* @note		None.
*
******************************************************************************/
int XEmacPs_SetupIntrSystem(XScuGic *IntcInstancePtr,
		XEmacPs *EmacPsInstancePtr, XScuTimer *TimerInstancePtr,
			u16 EmacPsIntrId, u16 TimerIntrId)
{
	int Status = XST_SUCCESS;
	XScuGic_Config *GicConfig;

	Xil_ExceptionInit();

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	GicConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == GicConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&IntcInstance, GicConfig,
	GicConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler)XScuGic_InterruptHandler,
			IntcInstancePtr);

	/*
	 * Connect the EmacPs device driver handler that will be called when an
	 * interrupt for the device occurs. The device driver handler performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, EmacPsIntrId,
			(Xil_InterruptHandler) XEmacPs_IntrHandler,
			(void *) EmacPsInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the handler for timer interrupt that will be called when the
	 * timer.expires.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, TimerIntrId,
			(Xil_ExceptionHandler)XEmacPs_TimerInterruptHandler_MY,
			(void *)EmacPsInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable interrupts from the hardware
	 */
	XScuGic_Enable(IntcInstancePtr, EmacPsIntrId);
	XScuGic_Enable(IntcInstancePtr, TimerIntrId);

	/*
	 * Enable interrupts in the processor
	 */
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);

	return XST_SUCCESS;
}




/*****************************************************************************/
/**
*
* This function initializes the EmacPs DMA buffer descriptors. 16 BDs are used
* on the Tx path and 16 on the Rx path. On the Rx path a 2-dimensional array
* RxBuf[16][1540] is used. The last byte in each of the buffers is used to mark
* whether the RxBuf is already submitted or not. For example, if the location
* RxBuf[1][1539] is 1, then it means the RxBuf[1] is already submitted. During
* initialization, for 16 BDs, 16 RxBufs are submitted (RxBuf[0, RxBuf[1], ...
* RxBuf[15]]) and the corresponding entries RxBuf[0][1539], RxBuf[1][1539], ...
* RxBuf[15][1539] are marked as 1.
* On the Rx path, all 16 BDs are submitted to the hardware.
* Once that is done, the timer is started and so is the EmacPs.
*
* @param	EmacPsInstancePtr is a pointer to the instance of the EmacPs.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
int XEmacPs_InitializeEmacPsDma (XEmacPs *InstancePntr)
{
	XEmacPs_BdRing *TxRingPtr;
	XEmacPs_BdRing *RxRingPtr;
	XEmacPs_Bd *RxBdPtr;
	XEmacPs_Bd *CurrBdPtr;
	int Status;
	int Index;
	XEmacPs_Bd BdTemplate;

	Status = XST_SUCCESS;

	TxRingPtr = &XEmacPs_GetTxRing(InstancePntr);
	RxRingPtr = &XEmacPs_GetRxRing(InstancePntr);

	/*
	 * BdTemplate is used for cloning. Hence it is cleared so that
	 * all 16 BDs can be cleared.
	 */
	XEmacPs_BdClear(&BdTemplate);
	Status = XEmacPs_BdRingCreate (RxRingPtr,
					(u32)RX_BD_START_ADDRESS,
					(u32)RX_BD_START_ADDRESS,
					XEMACPS_BD_ALIGNMENT,
					RX_DESCS);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: BD Ring Creation failed for Rx path \r\n",
								__func__);
		return XST_FAILURE;
	}

	/*
	 * Clone the 16 BDs with BdTemplate. This will clear all the 16 BDs.
	 */
	Status = XEmacPs_BdRingClone(RxRingPtr, &BdTemplate, XEMACPS_RECV);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * BdTemplate is used for cloning on Tx path. Hence it is cleared so
	 * that all 16 BDs can be cleared.
	 */
	XEmacPs_BdClear(&BdTemplate);
	/*
	 * Set the Used Bit.
	 */
	XEmacPs_BdSetStatus(&BdTemplate, XEMACPS_TXBUF_USED_MASK);

	/*
	 * Create 16 BDs for Tx path.
	 */
	Status = XEmacPs_BdRingCreate (TxRingPtr,
					(u32)TX_BD_START_ADDRESS,
					(u32)TX_BD_START_ADDRESS,
					XEMACPS_BD_ALIGNMENT,
					TX_DESCS);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: BD Ring Creation failed for Tx path \r\n",
								__func__);
		return XST_FAILURE;
	}

	/*
	 * Clone the 16 BDs with BdTemplate. This will clear all the 16 BDs
	 * and set the Used bit in all of them.
	 */
	Status = XEmacPs_BdRingClone (TxRingPtr, &BdTemplate, XEMACPS_SEND);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Allocate the 16 BDs on Rx  path.
	 */
	Status = XEmacPs_BdRingAlloc (RxRingPtr,RX_DESCS,&RxBdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: BD Ring allocation failed for Rx path \r\n",
								__func__);
		return XST_FAILURE;
	}
	/*
	 * Mark the RxBufs as used.
	 */
	CurrBdPtr = RxBdPtr;
	for (Index = 0; Index < RX_DESCS; Index++) {
		XEmacPs_BdSetAddressRx (CurrBdPtr, &(RxBuf[Index][0]));
		XEmacPs_BdSetLast(CurrBdPtr);
		RxBuf[Index][XEMACPS_PACKET_LEN - 2] = 1;
		CurrBdPtr = XEmacPs_BdRingNext (RxRingPtr, CurrBdPtr);
	}
	/*
	 * Submit the BDs on the Rx path.
	 */
	Status = XEmacPs_BdRingToHw (RxRingPtr,RX_DESCS,RxBdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: BD Ring submission failed for Rx path \r\n",
								__func__);
		return XST_FAILURE;
	}
	sleep(3);
	/*
	 * Start the timer and EmacPs.
	 */
	XEmacPs_Start(InstancePntr);
	XScuTimer_Start(&TimerInstance);
	return XST_SUCCESS;
}






