# Zynq_qemu_standalone
这次是zynq裸机实现以太网与PC端linux通信

Vivado:
	1.选择芯片型号 xc7z020clg400-1，新建工程
	2.添加Zynq IP
	3.双击后点击Prests,导入pynq-revC.tcl(不能有中文路径）
	4.PS-PL Configuration
		->AXI Non Secure Enablement
			->GP Master AXI Interface
				->M AXI GP0 interface
	5.加一个AXI_GPIO  4bit的位宽，其他选项默认
	6.综合后绑定AXI_GPIO引脚：
		[3]-M14 [2]-N16 [1]-P14 [0]-R14

SDK:
	目前为纯裸机程序，实现以太网的传输。
	1.Timer中断为5ms
	2.MAC发送放到了timer中断中，2s发一次
	3.MAC接收放到了MAC接收中断中。
	4.没有丢包率
	
