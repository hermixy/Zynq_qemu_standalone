#include "hls_driver.h"

XPid_regulator Hls_pid;
int ResultAvailHls_PID;

void hls_pid_return_isr(void *InstancePtr);
int hls_PID_init(XPid_regulator *hls_Ptr_pid);
void pid_input(void);

void hls_pid_return_isr(void *InstancePtr)
{
		XPid_regulator *pAccelerator = (XPid_regulator *)InstancePtr;

	   // clear the local interrupt
		XPid_regulator_InterruptClear(pAccelerator,1);
		print("ResultAvailHls_PID = 1\n\r");
		ResultAvailHls_PID = 1;
}

int hls_PID_init(XPid_regulator *hls_Ptr_pid)
{
	XPid_regulator_Config *cfg_Ptr_pid;
   int status;

   cfg_Ptr_pid = XPid_regulator_LookupConfig(XPAR_PID_REGULATOR_0_DEVICE_ID);
   if (!cfg_Ptr_pid) {
      print("ERROR: Lookup of PID accelerator configuration failed.\n\r");
      return XST_FAILURE;
   }
   status = XPid_regulator_CfgInitialize(hls_Ptr_pid, cfg_Ptr_pid);
   if (status != XST_SUCCESS) {
      print("ERROR: Could not initialize PID accelerator.\n\r");
      return XST_FAILURE;
   }

   return status;
}

void pid_input(void)
{
	float Ref,Fdb;

	Ref = 7.3;
	Fdb = 5.2;

	//cast float "Ref" and "Fdb" to typr u32
	XPid_regulator_Set_Ref(&Hls_pid, *((u32*)&Ref));
	XPid_regulator_Set_Fdb(&Hls_pid, *((u32*)&Fdb));

	// Clear done flags
	ResultAvailHls_PID = 0;

	// issue start
	XPid_regulator_Start(&Hls_pid);
	//printf("HLS is starting \n\r");

}
/*int setup_interrupt()
{
	   //This functions sets up the interrupt on the ARM
	   int result;
	   XScuGic_Config *pCfg = XScuGic_LookupConfig(XPAR_SCUGIC_0_DEVICE_ID);
	   if (pCfg == NULL){
	      print("Interrupt Configuration Lookup Failed\n\r");
	      return XST_FAILURE;
	   }
	   result = XScuGic_CfgInitialize(&ScuGic,pCfg,pCfg->CpuBaseAddress);
	   if(result != XST_SUCCESS){
	      return result;
	   }
	   // self test
	   result = XScuGic_SelfTest(&ScuGic);
	   if(result != XST_SUCCESS){
	      return result;
	   }
	   // Initialize the exception handler
	   Xil_ExceptionInit();
	   // Register the exception handler
	   Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,&ScuGic);
	   //Enable the exception handler
	   Xil_ExceptionEnable();
	   // Connect the Left FIR ISR to the exception table
	   result = XScuGic_Connect(&ScuGic,XPAR_FABRIC_PID_REGULATOR_0_INTERRUPT_INTR,(Xil_InterruptHandler)hls_pid_return_isr ,&Hls_pid);
	   if(result != XST_SUCCESS){
	      return result;
	   }
	   // Enable the Left FIR ISR
	   XScuGic_Enable(&ScuGic,XPAR_FABRIC_PID_REGULATOR_0_INTERRUPT_INTR);

	   return XST_SUCCESS;
}
*/
